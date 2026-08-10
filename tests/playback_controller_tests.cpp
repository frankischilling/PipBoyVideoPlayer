#include "pbvp/ffmpeg_runtime.hpp"
#include "pbvp/playback_controller.hpp"

#include "test_support.hpp"

#include <Windows.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <thread>

namespace {

using namespace std::chrono_literals;

pbvp::PlaybackControllerConfig TestConfig() {
    pbvp::PlaybackControllerConfig config{};
    config.muted = true;
    config.maximum_underruns = 3u;
    return config;
}

bool AdvanceUntil(
    pbvp::PlaybackController& controller,
    const pbvp::PlaybackState expected,
    const std::chrono::steady_clock::time_point deadline,
    std::uint64_t& delivered_frames) {
    while (std::chrono::steady_clock::now() < deadline) {
        if (!controller.Update(true)) {
            return false;
        }
        if (controller.TakeVideoFrame().has_value()) {
            ++delivered_frames;
        }
        const pbvp::PlaybackControllerSnapshot snapshot = controller.Snapshot();
        if (snapshot.playback.state == expected) {
            return true;
        }
        if (snapshot.playback.state == pbvp::PlaybackState::error) {
            return false;
        }
        Sleep(1u);
    }
    return false;
}

void TestAudioCompletion(
    const pbvp::FfmpegRuntime& runtime,
    const std::wstring& fixture_root) {
    pbvp::PlaybackController controller(runtime, TestConfig());
    PBVP_CHECK(controller.Open(fixture_root, L"h264-aac-44100-stereo.mp4"));

    std::uint64_t delivered_frames = 0u;
    PBVP_CHECK(AdvanceUntil(
        controller, pbvp::PlaybackState::idle,
        std::chrono::steady_clock::now() + 10s, delivered_frames));
    const pbvp::PlaybackControllerSnapshot snapshot = controller.Snapshot();
    PBVP_CHECK(snapshot.playback.state == pbvp::PlaybackState::idle);
    PBVP_CHECK(snapshot.playback.error == pbvp::PlaybackError::none);
    PBVP_CHECK(snapshot.terminal_reason == pbvp::PlaybackTerminalReason::completed);
    PBVP_CHECK(snapshot.decoder.state == pbvp::DecoderState::end_of_stream);
    PBVP_CHECK(snapshot.metrics.submitted_audio_samples >= 96'000u);
    PBVP_CHECK(snapshot.metrics.submitted_audio_samples <= 98'000u);
    PBVP_CHECK(snapshot.audio.underruns == 0u);
    PBVP_CHECK(snapshot.audio.end_of_stream_reached);
    PBVP_CHECK(delivered_frames > 0u);
    PBVP_CHECK(snapshot.metrics.presented_video_frames >= delivered_frames);
    PBVP_CHECK(snapshot.metrics.peak_staged_video_bytes <= 8u * 1024u * 1024u);
    PBVP_CHECK(snapshot.metrics.peak_decoder_video_bytes <= 32u * 1024u * 1024u);
    PBVP_CHECK(snapshot.metrics.peak_decoder_audio_bytes <= 4u * 1024u * 1024u);
    std::printf(
        "integrated audio completion: frames=%llu delivered=%llu dropped=%llu samples=%llu clock_us=%lld underruns=%llu\n",
        static_cast<unsigned long long>(snapshot.metrics.presented_video_frames),
        static_cast<unsigned long long>(delivered_frames),
        static_cast<unsigned long long>(snapshot.metrics.dropped_video_frames),
        static_cast<unsigned long long>(snapshot.metrics.submitted_audio_samples),
        static_cast<long long>(snapshot.metrics.last_media_time_us),
        static_cast<unsigned long long>(snapshot.audio.underruns));
}

void TestPauseAndSeeks(
    const pbvp::FfmpegRuntime& runtime,
    const std::wstring& fixture_root) {
    pbvp::PlaybackController controller(runtime, TestConfig());
    PBVP_CHECK(controller.Open(fixture_root, L"h264-aac-44100-stereo.mp4"));
    std::uint64_t delivered_frames = 0u;
    PBVP_CHECK(AdvanceUntil(
        controller, pbvp::PlaybackState::playing,
        std::chrono::steady_clock::now() + 5s, delivered_frames));

    PBVP_CHECK(controller.Pause());
    PBVP_CHECK(controller.Update(true));
    const std::int64_t paused_time = controller.Snapshot().metrics.last_media_time_us;
    Sleep(50u);
    PBVP_CHECK(controller.Update(true));
    PBVP_CHECK(controller.Snapshot().metrics.last_media_time_us == paused_time);
    PBVP_CHECK(controller.Resume());

    PBVP_CHECK(controller.Seek(1'000'000));
    PBVP_CHECK(controller.Snapshot().generation == 2u);
    PBVP_CHECK(AdvanceUntil(
        controller, pbvp::PlaybackState::playing,
        std::chrono::steady_clock::now() + 5s, delivered_frames));
    PBVP_CHECK(controller.Seek(200'000));
    PBVP_CHECK(controller.Snapshot().generation == 3u);
    PBVP_CHECK(AdvanceUntil(
        controller, pbvp::PlaybackState::playing,
        std::chrono::steady_clock::now() + 5s, delivered_frames));

    controller.Stop();
    const pbvp::PlaybackControllerSnapshot snapshot = controller.Snapshot();
    PBVP_CHECK(snapshot.playback.state == pbvp::PlaybackState::idle);
    PBVP_CHECK(snapshot.terminal_reason == pbvp::PlaybackTerminalReason::stopped);
    PBVP_CHECK(snapshot.metrics.seek_count == 2u);
    PBVP_CHECK(snapshot.metrics.pause_count == 1u);
    PBVP_CHECK(snapshot.metrics.resume_count == 1u);
}

void TestStopAndThreadOwnership(
    const pbvp::FfmpegRuntime& runtime,
    const std::wstring& fixture_root) {
    {
        pbvp::PlaybackController controller(runtime, TestConfig());
        PBVP_CHECK(controller.Open(fixture_root, L"h264-aac-1080p.mp4"));
        bool foreign_update = true;
        std::thread foreign([&controller, &foreign_update]() {
            foreign_update = controller.Update(true);
        });
        foreign.join();
        PBVP_CHECK(!foreign_update);
        const auto started = std::chrono::steady_clock::now();
        controller.Stop();
        PBVP_CHECK(std::chrono::steady_clock::now() - started < 2s);
        PBVP_CHECK(controller.Snapshot().playback.state == pbvp::PlaybackState::idle);
    }
    {
        pbvp::PlaybackController controller(runtime, TestConfig());
        PBVP_CHECK(controller.Open(fixture_root, L"h264-aac-44100-stereo.mp4"));
        PBVP_CHECK(controller.Update(false));
        const auto snapshot = controller.Snapshot();
        PBVP_CHECK(snapshot.playback.state == pbvp::PlaybackState::idle);
        PBVP_CHECK(snapshot.terminal_reason ==
                   pbvp::PlaybackTerminalReason::presentation_hidden);
    }
}

void TestSilentCompletion(
    const pbvp::FfmpegRuntime& runtime,
    const std::wstring& fixture_root) {
    pbvp::PlaybackController controller(runtime, TestConfig());
    PBVP_CHECK(controller.Open(fixture_root, L"h264-vfr-silent.mp4"));
    std::uint64_t delivered_frames = 0u;
    PBVP_CHECK(AdvanceUntil(
        controller, pbvp::PlaybackState::idle,
        std::chrono::steady_clock::now() + 10s, delivered_frames));
    const auto snapshot = controller.Snapshot();
    PBVP_CHECK(snapshot.terminal_reason == pbvp::PlaybackTerminalReason::completed);
    PBVP_CHECK(snapshot.decoder.state == pbvp::DecoderState::end_of_stream);
    PBVP_CHECK(snapshot.metrics.submitted_audio_samples == 0u);
    PBVP_CHECK(delivered_frames > 0u);
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) {
        std::fputs("usage: pbvp_playback_controller_test <runtime-bin> <fixture-root>\n", stderr);
        return 2;
    }

    pbvp::FfmpegRuntime runtime;
    pbvp::FfmpegLoadFailure failure{};
    PBVP_CHECK(runtime.Load(argv[1], failure));
    if (!runtime.IsLoaded()) {
        return 1;
    }

    TestAudioCompletion(runtime, argv[2]);
    TestPauseAndSeeks(runtime, argv[2]);
    TestStopAndThreadOwnership(runtime, argv[2]);
    TestSilentCompletion(runtime, argv[2]);
    runtime.Unload();

    if (pbvp::test::failures != 0) {
        std::fprintf(stderr, "%d test check(s) failed\n", pbvp::test::failures);
        return 1;
    }
    std::puts("playback controller checks passed");
    return 0;
}
