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

namespace pbvp {

struct PlaybackControllerTestAccess final {
    static bool BeginAudioRebuffer(PlaybackController& controller) noexcept {
        return controller.BeginAudioRebuffer();
    }
};

} // namespace pbvp

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
    PBVP_CHECK(snapshot.metrics.last_presented_video_pts_us >= 1'800'000);
    PBVP_CHECK(snapshot.metrics.last_presented_video_end_us >= 1'900'000);
    PBVP_CHECK(snapshot.metrics.last_media_time_us >=
               snapshot.metrics.last_presented_video_end_us);
    PBVP_CHECK(snapshot.metrics.last_media_time_us -
                   snapshot.metrics.last_presented_video_end_us <= 50'000);
    std::printf(
        "integrated audio completion: frames=%llu delivered=%llu dropped=%llu samples=%llu clock_us=%lld underruns=%llu\n",
        static_cast<unsigned long long>(snapshot.metrics.presented_video_frames),
        static_cast<unsigned long long>(delivered_frames),
        static_cast<unsigned long long>(snapshot.metrics.dropped_video_frames),
        static_cast<unsigned long long>(snapshot.metrics.submitted_audio_samples),
        static_cast<long long>(snapshot.metrics.last_media_time_us),
        static_cast<unsigned long long>(snapshot.audio.underruns));
}

void TestLowCadenceFrameAccounting(
    const pbvp::FfmpegRuntime& runtime,
    const std::wstring& fixture_root) {
    pbvp::PlaybackController controller(runtime, TestConfig());
    PBVP_CHECK(controller.Open(fixture_root, L"h264-aac-44100-stereo.mp4"));

    std::uint64_t delivered_frames = 0u;
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while (std::chrono::steady_clock::now() < deadline) {
        PBVP_CHECK(controller.Update(true));
        if (controller.TakeVideoFrame().has_value()) {
            ++delivered_frames;
        }
        const auto state = controller.Snapshot().playback.state;
        if (state == pbvp::PlaybackState::idle || state == pbvp::PlaybackState::error) {
            break;
        }
        Sleep(250u);
    }

    const pbvp::PlaybackControllerSnapshot snapshot = controller.Snapshot();
    PBVP_CHECK(snapshot.playback.state == pbvp::PlaybackState::idle);
    PBVP_CHECK(snapshot.playback.error == pbvp::PlaybackError::none);
    PBVP_CHECK(snapshot.metrics.dropped_video_frames > 0u);
    PBVP_CHECK(snapshot.metrics.presented_video_frames == delivered_frames);
    PBVP_CHECK(snapshot.metrics.presented_video_frames +
                   snapshot.metrics.dropped_video_frames <=
               snapshot.metrics.decoded_video_frames);
    PBVP_CHECK(snapshot.metrics.decoded_video_frames -
                   (snapshot.metrics.presented_video_frames +
                    snapshot.metrics.dropped_video_frames) <=
               1u);
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

void TestForcedAudioRebuffer(
    const pbvp::FfmpegRuntime& runtime,
    const std::wstring& fixture_root) {
    pbvp::PlaybackController controller(runtime, TestConfig());
    PBVP_CHECK(controller.Open(fixture_root, L"h264-aac-44100-stereo.mp4"));

    std::uint64_t delivered_frames = 0u;
    PBVP_CHECK(AdvanceUntil(
        controller, pbvp::PlaybackState::playing,
        std::chrono::steady_clock::now() + 5s, delivered_frames));
    PBVP_CHECK(pbvp::PlaybackControllerTestAccess::BeginAudioRebuffer(controller));
    auto snapshot = controller.Snapshot();
    PBVP_CHECK(snapshot.playback.state == pbvp::PlaybackState::buffering);
    PBVP_CHECK(snapshot.audio_started);
    PBVP_CHECK(snapshot.audio.paused);
    PBVP_CHECK(snapshot.metrics.buffering_events == 2u);

    PBVP_CHECK(AdvanceUntil(
        controller, pbvp::PlaybackState::playing,
        std::chrono::steady_clock::now() + 5s, delivered_frames));
    snapshot = controller.Snapshot();
    PBVP_CHECK(!snapshot.audio.paused);

    PBVP_CHECK(pbvp::PlaybackControllerTestAccess::BeginAudioRebuffer(controller));
    PBVP_CHECK(controller.Pause());
    PBVP_CHECK(AdvanceUntil(
        controller, pbvp::PlaybackState::paused,
        std::chrono::steady_clock::now() + 5s, delivered_frames));
    snapshot = controller.Snapshot();
    PBVP_CHECK(snapshot.audio.paused);
    PBVP_CHECK(snapshot.playback.pause_after_buffering);
    PBVP_CHECK(controller.Resume());
    snapshot = controller.Snapshot();
    PBVP_CHECK(snapshot.playback.state == pbvp::PlaybackState::playing);
    PBVP_CHECK(!snapshot.audio.paused);
    controller.Stop();
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

void TestHighResolutionCompletion(
    const pbvp::FfmpegRuntime& runtime,
    const std::wstring& fixture_root) {
    pbvp::PlaybackController controller(runtime, TestConfig());
    PBVP_CHECK(controller.Open(fixture_root, L"h264-aac-1080p.mp4"));
    std::uint64_t delivered_frames = 0u;
    PBVP_CHECK(AdvanceUntil(
        controller, pbvp::PlaybackState::idle,
        std::chrono::steady_clock::now() + 10s, delivered_frames));
    const auto snapshot = controller.Snapshot();
    PBVP_CHECK(snapshot.playback.error == pbvp::PlaybackError::none);
    PBVP_CHECK(snapshot.terminal_reason == pbvp::PlaybackTerminalReason::completed);
    PBVP_CHECK(snapshot.failure_site == pbvp::PlaybackFailureSite::none);
    PBVP_CHECK(snapshot.metrics.decoded_video_frames == 30u);
    PBVP_CHECK(delivered_frames > 0u);
    PBVP_CHECK(snapshot.metrics.peak_staged_video_bytes <= 8u * 1024u * 1024u);
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

void TestOptionalLongStart(
    const pbvp::FfmpegRuntime& runtime,
    const std::wstring& fixture_root,
    const std::wstring& fixture_name) {
    pbvp::PlaybackController controller(runtime, TestConfig());
    PBVP_CHECK(controller.Open(fixture_root, fixture_name));
    std::uint64_t delivered_frames = 0u;
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < deadline) {
        if (!controller.Update(true)) {
            break;
        }
        if (controller.TakeVideoFrame().has_value()) {
            ++delivered_frames;
        }
        Sleep(1u);
    }
    const auto snapshot = controller.Snapshot();
    std::printf(
        "long startup: state=%s error=%s site=%s decoder=%s frames=%llu delivered=%llu dropped=%llu samples=%llu clock_us=%lld\n",
        pbvp::PlaybackStateName(snapshot.playback.state),
        pbvp::PlaybackErrorName(snapshot.playback.error),
        pbvp::PlaybackFailureSiteName(snapshot.failure_site),
        pbvp::MediaDecodeStatusName(snapshot.decoder.failure.status),
        static_cast<unsigned long long>(snapshot.metrics.decoded_video_frames),
        static_cast<unsigned long long>(delivered_frames),
        static_cast<unsigned long long>(snapshot.metrics.dropped_video_frames),
        static_cast<unsigned long long>(snapshot.metrics.submitted_audio_samples),
        static_cast<long long>(snapshot.metrics.last_media_time_us));
    PBVP_CHECK(snapshot.playback.state != pbvp::PlaybackState::error);
    PBVP_CHECK(delivered_frames > 0u);
    controller.Stop();
}

void TestOptionalLowFpsStart(
    const pbvp::FfmpegRuntime& runtime,
    const std::wstring& fixture_root,
    const std::wstring& fixture_name) {
    pbvp::PlaybackController controller(runtime, TestConfig());
    PBVP_CHECK(controller.Open(fixture_root, fixture_name));
    std::uint64_t delivered_frames = 0u;
    const auto deadline = std::chrono::steady_clock::now() + 300s;
    while (std::chrono::steady_clock::now() < deadline) {
        if (!controller.Update(true)) {
            break;
        }
        if (controller.TakeVideoFrame().has_value()) {
            ++delivered_frames;
        }
        Sleep(100u);
    }
    const auto snapshot = controller.Snapshot();
    std::printf(
        "low-fps startup: state=%s error=%s frames=%llu delivered=%llu dropped=%llu samples=%llu clock_us=%lld queued=%u underruns=%llu updates=%llu\n",
        pbvp::PlaybackStateName(snapshot.playback.state),
        pbvp::PlaybackErrorName(snapshot.playback.error),
        static_cast<unsigned long long>(snapshot.metrics.decoded_video_frames),
        static_cast<unsigned long long>(delivered_frames),
        static_cast<unsigned long long>(snapshot.metrics.dropped_video_frames),
        static_cast<unsigned long long>(snapshot.metrics.submitted_audio_samples),
        static_cast<long long>(snapshot.metrics.last_media_time_us),
        snapshot.audio.queued_buffers,
        static_cast<unsigned long long>(snapshot.audio.underruns),
        static_cast<unsigned long long>(snapshot.metrics.update_calls));
    PBVP_CHECK(snapshot.playback.state != pbvp::PlaybackState::error);
    PBVP_CHECK(snapshot.audio.underruns == 0u);
    PBVP_CHECK(delivered_frames >= 2'500u);
    PBVP_CHECK(snapshot.metrics.update_calls >= 2'700u);
    PBVP_CHECK(snapshot.metrics.maximum_update_gap_ms >= 90u);
    PBVP_CHECK(snapshot.metrics.maximum_update_gap_ms <= 250u);
    controller.Stop();
}

void TestOptionalServiceGap(
    const pbvp::FfmpegRuntime& runtime,
    const std::wstring& fixture_root,
    const std::wstring& fixture_name) {
    pbvp::PlaybackController controller(runtime, TestConfig());
    PBVP_CHECK(controller.Open(fixture_root, fixture_name));
    std::uint64_t delivered_frames = 0u;
    PBVP_CHECK(AdvanceUntil(
        controller, pbvp::PlaybackState::playing,
        std::chrono::steady_clock::now() + 5s, delivered_frames));

    const auto fill_deadline = std::chrono::steady_clock::now() + 2s;
    while (std::chrono::steady_clock::now() < fill_deadline &&
           controller.Snapshot().audio.queued_buffers < 15u) {
        PBVP_CHECK(controller.Update(true));
        if (controller.TakeVideoFrame().has_value()) {
            ++delivered_frames;
        }
        Sleep(10u);
    }
    PBVP_CHECK(controller.Snapshot().audio.queued_buffers >= 15u);

    const auto before_gap = controller.Snapshot();
    Sleep(450u);
    PBVP_CHECK(controller.Update(true));
    const auto snapshot = controller.Snapshot();
    const std::int64_t clock_advance_us =
        snapshot.metrics.last_media_time_us - before_gap.metrics.last_media_time_us;
    std::printf(
        "service gap: state=%s gap_ms=%llu clock_advance_us=%lld queued=%u underruns=%llu clock_us=%lld\n",
        pbvp::PlaybackStateName(snapshot.playback.state),
        static_cast<unsigned long long>(snapshot.metrics.maximum_update_gap_ms),
        static_cast<long long>(clock_advance_us),
        snapshot.audio.queued_buffers,
        static_cast<unsigned long long>(snapshot.audio.underruns),
        static_cast<long long>(snapshot.metrics.last_media_time_us));
    PBVP_CHECK(snapshot.metrics.maximum_update_gap_ms >= 400u);
    PBVP_CHECK(clock_advance_us > 250'000);
    PBVP_CHECK(clock_advance_us < 400'000);
    PBVP_CHECK(snapshot.audio.underruns == 0u);
    controller.Stop();
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 3 && argc != 5) {
        std::fputs(
            "usage: pbvp_playback_controller_test <runtime-bin> <fixture-root> [<long-root> <long-name>]\n",
            stderr);
        return 2;
    }

    pbvp::FfmpegRuntime runtime;
    pbvp::FfmpegLoadFailure failure{};
    PBVP_CHECK(runtime.Load(argv[1], failure));
    if (!runtime.IsLoaded()) {
        return 1;
    }

    TestAudioCompletion(runtime, argv[2]);
    TestLowCadenceFrameAccounting(runtime, argv[2]);
    TestPauseAndSeeks(runtime, argv[2]);
    TestForcedAudioRebuffer(runtime, argv[2]);
    TestStopAndThreadOwnership(runtime, argv[2]);
    TestHighResolutionCompletion(runtime, argv[2]);
    TestSilentCompletion(runtime, argv[2]);
    if (argc == 5) {
        TestOptionalLongStart(runtime, argv[3], argv[4]);
        TestOptionalLowFpsStart(runtime, argv[3], argv[4]);
        TestOptionalServiceGap(runtime, argv[3], argv[4]);
    }
    runtime.Unload();

    if (pbvp::test::failures != 0) {
        std::fprintf(stderr, "%d test check(s) failed\n", pbvp::test::failures);
        return 1;
    }
    std::puts("playback controller checks passed");
    return 0;
}
