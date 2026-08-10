#include "pbvp/ffmpeg_runtime.hpp"
#include "pbvp/media_decoder.hpp"
#include "pbvp/playback_clock.hpp"
#include "pbvp/xaudio_stream.hpp"

#include <Windows.h>

#include <cstdint>
#include <cstdio>
#include <deque>
#include <optional>
#include <string>

namespace {

struct FixtureCase final {
    const wchar_t* name;
    std::uint32_t source_rate;
    std::uint32_t source_channels;
};

bool StreamFixture(
    const pbvp::FfmpegRuntime& runtime,
    const std::wstring& fixture_root,
    const FixtureCase& fixture) {
    pbvp::MediaDecoder decoder(runtime);
    pbvp::MediaDecodeFailure decode_failure{};
    if (!decoder.Start(fixture_root, fixture.name, decode_failure)) {
        std::fprintf(
            stderr,
            "decoder start failed: %s\n",
            pbvp::MediaDecodeStatusName(decode_failure.status));
        return false;
    }

    pbvp::XAudioStream audio;
    pbvp::XAudioStreamConfig audio_config{};
    const auto initialized = audio.Initialize(audio_config);
    if (initialized != pbvp::XAudioStreamStatus::ok) {
        std::fprintf(
            stderr,
            "audio initialization failed: %s\n",
            pbvp::XAudioStreamStatusName(initialized));
        decoder.Stop();
        return false;
    }
    if (audio.SetMuted(true) != pbvp::XAudioStreamStatus::ok) {
        decoder.Stop();
        return false;
    }

    std::deque<pbvp::DecodedAudioChunk> lookahead{};
    std::uint64_t submitted_samples = 0u;
    std::int64_t expected_end_us = 0;
    bool started = false;
    bool submitted_end = false;
    bool failed = false;
    const ULONGLONG deadline = GetTickCount64() + 15'000u;

    while (GetTickCount64() < deadline) {
        for (;;) {
            auto video = decoder.TryPopVideo();
            if (video.status != pbvp::QueuePopStatus::item) {
                break;
            }
        }

        while (lookahead.size() < 2u) {
            auto chunk = decoder.TryPopAudio();
            if (chunk.status != pbvp::QueuePopStatus::item) {
                break;
            }
            if (!chunk.value.has_value()) {
                failed = true;
                break;
            }
            lookahead.push_back(std::move(*chunk.value));
        }
        if (failed) {
            break;
        }

        const pbvp::DecoderSnapshot decoder_snapshot = decoder.Snapshot();
        if (decoder_snapshot.state == pbvp::DecoderState::failed ||
            decoder_snapshot.state == pbvp::DecoderState::stopped) {
            std::fprintf(
                stderr,
                "decoder failed while streaming: %s\n",
                pbvp::MediaDecodeStatusName(decoder_snapshot.failure.status));
            failed = true;
            break;
        }
        const bool decoder_ended =
            decoder_snapshot.state == pbvp::DecoderState::end_of_stream;
        const bool final_pending =
            decoder_ended && decoder.BufferUsage().audio_items == 0u &&
            lookahead.size() == 1u;

        if (lookahead.size() >= 2u || final_pending) {
            pbvp::DecodedAudioChunk& chunk = lookahead.front();
            const bool end_of_stream = final_pending;
            const pbvp::XAudioStreamStatus submit_status = audio.SubmitPcm(
                chunk.samples,
                chunk.pts_us,
                chunk.generation,
                end_of_stream);
            if (submit_status == pbvp::XAudioStreamStatus::ok) {
                submitted_samples += chunk.samples_per_channel;
                pbvp::AudioSampleClock end_clock;
                if (!end_clock.Reset(chunk.sample_rate, 0u, chunk.pts_us)) {
                    failed = true;
                    break;
                }
                const auto chunk_end = end_clock.MediaTimeUs(chunk.samples_per_channel);
                if (!chunk_end.has_value()) {
                    failed = true;
                    break;
                }
                expected_end_us = *chunk_end;
                submitted_end = end_of_stream;
                lookahead.pop_front();
            } else if (submit_status != pbvp::XAudioStreamStatus::queue_full) {
                std::fprintf(
                    stderr,
                    "audio submit failed: %s\n",
                    pbvp::XAudioStreamStatusName(submit_status));
                failed = true;
                break;
            }
        }

        const pbvp::XAudioStreamSnapshot audio_snapshot = audio.Snapshot();
        if (audio_snapshot.status == pbvp::XAudioStreamStatus::audio_device_failed) {
            failed = true;
            break;
        }
        if (!started && audio_snapshot.ready_to_start) {
            if (audio.Start() != pbvp::XAudioStreamStatus::ok) {
                failed = true;
                break;
            }
            started = true;
        }
        if (submitted_end && audio_snapshot.end_of_stream_reached) {
            break;
        }
        Sleep(1u);
    }

    const pbvp::DecoderSnapshot decoder_snapshot = decoder.Snapshot();
    const pbvp::XAudioStreamSnapshot audio_snapshot = audio.Snapshot();
    const auto final_time = audio.MediaTimeUs();
    const bool result =
        !failed && submitted_end && started &&
        audio_snapshot.end_of_stream_reached &&
        audio_snapshot.underruns == 0u &&
        audio_snapshot.submitted_samples == submitted_samples &&
        final_time == expected_end_us &&
        decoder_snapshot.info.source_audio_rate == fixture.source_rate &&
        decoder_snapshot.info.source_audio_channels == fixture.source_channels &&
        decoder_snapshot.info.output_audio_rate == 48'000u &&
        decoder_snapshot.info.output_audio_channels == 2u;

    if (!result) {
        std::fprintf(
            stderr,
            "pipeline check failed: ended=%d started=%d stream-end=%d underruns=%llu submitted=%llu/%llu time=%lld/%lld\n",
            submitted_end ? 1 : 0,
            started ? 1 : 0,
            audio_snapshot.end_of_stream_reached ? 1 : 0,
            static_cast<unsigned long long>(audio_snapshot.underruns),
            static_cast<unsigned long long>(audio_snapshot.submitted_samples),
            static_cast<unsigned long long>(submitted_samples),
            static_cast<long long>(final_time.value_or(-1)),
            static_cast<long long>(expected_end_us));
    } else {
        std::printf(
            "streamed %ls: source=%u Hz/%u ch output=48000 Hz/2 ch samples=%llu end=%lld us underruns=0\n",
            fixture.name,
            fixture.source_rate,
            fixture.source_channels,
            static_cast<unsigned long long>(submitted_samples),
            static_cast<long long>(expected_end_us));
    }

    decoder.Stop();
    audio.StopAndFlush();
    return result;
}

} // namespace

int wmain(const int argc, wchar_t** argv) {
    if (argc != 3) {
        return 2;
    }

    pbvp::FfmpegRuntime runtime;
    pbvp::FfmpegLoadFailure load_failure{};
    if (!runtime.Load(argv[1], load_failure)) {
        std::fprintf(
            stderr,
            "FFmpeg load failed: %s\n",
            pbvp::FfmpegLoadStatusName(load_failure.status));
        return 1;
    }

    constexpr FixtureCase fixtures[]{
        {L"h264-aac-44100-stereo.mp4", 44'100u, 2u},
        {L"h264-aac-48000-mono.mp4", 48'000u, 1u},
        {L"h264-aac-48000-51.mp4", 48'000u, 6u},
    };
    bool passed = true;
    for (const FixtureCase& fixture : fixtures) {
        passed = StreamFixture(runtime, argv[2], fixture) && passed;
    }

    runtime.Unload();
    if (!passed) {
        return 1;
    }
    std::puts("decoded-audio pipeline checks passed");
    return 0;
}
