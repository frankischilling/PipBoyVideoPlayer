#include "pbvp/audio_smoke_test.hpp"

#include "pbvp/log.hpp"
#include "pbvp/media_decoder.hpp"
#include "pbvp/playback_clock.hpp"
#include "pbvp/xaudio_stream.hpp"

#include <Windows.h>

#include <cstdint>
#include <deque>
#include <memory>
#include <new>
#include <optional>
#include <utility>

namespace pbvp {
namespace {

constexpr wchar_t kAudioSmokeFile[] = L"PBVP-Phase3-Audio.mp4";
constexpr ULONGLONG kSmokeTimeoutMs = 15'000u;

} // namespace

struct AudioSmokeTest::Impl final {
    const FfmpegRuntime* runtime{};
    std::unique_ptr<MediaDecoder> decoder{};
    std::unique_ptr<XAudioStream> audio{};
    std::deque<DecodedAudioChunk> lookahead{};
    ULONGLONG deadline{};
    std::uint64_t submitted_samples{};
    std::int64_t expected_end_us{};
    bool started{};
    bool submitted_end{};
    bool finished{};
};

AudioSmokeTest& AudioSmokeTest::Instance() noexcept {
    static AudioSmokeTest instance;
    return instance;
}

void AudioSmokeTest::Start(
    const FfmpegRuntime& runtime,
    std::wstring media_root) noexcept {
    if (impl_ != nullptr) {
        return;
    }

    std::unique_ptr<Impl> candidate(new (std::nothrow) Impl{});
    if (!candidate) {
        PBVP_LOG_ERROR("Audio smoke failed before start: allocation failed");
        return;
    }
    candidate->runtime = &runtime;
    try {
        candidate->decoder = std::make_unique<MediaDecoder>(runtime);
        candidate->audio = std::make_unique<XAudioStream>();
    } catch (...) {
        PBVP_LOG_ERROR("Audio smoke failed before start: component allocation failed");
        return;
    }

    XAudioStreamConfig audio_config{};
    const XAudioStreamStatus audio_status = candidate->audio->Initialize(audio_config);
    if (audio_status != XAudioStreamStatus::ok) {
        PBVP_LOG_ERROR(
            "Audio smoke failed before decoder start: audio_status=%s",
            XAudioStreamStatusName(audio_status));
        return;
    }
    if (candidate->audio->SetVolume(0.10f) != XAudioStreamStatus::ok) {
        PBVP_LOG_ERROR("Audio smoke failed before decoder start: volume setup failed");
        return;
    }

    MediaDecodeFailure failure{};
    if (!candidate->decoder->Start(media_root, kAudioSmokeFile, failure)) {
        PBVP_LOG_ERROR(
            "Audio smoke failed before worker start: status=%s win32=%lu",
            MediaDecodeStatusName(failure.status),
            static_cast<unsigned long>(failure.io.windows_error));
        return;
    }

    candidate->deadline = GetTickCount64() + kSmokeTimeoutMs;
    impl_ = candidate.release();
    PBVP_LOG_INFO(
        "PBVP_AUDIO_SMOKE_TEST_ARMED: playing the generated Phase 3 tone at volume 0.10");
}

void AudioSmokeTest::Update() noexcept {
    if (impl_ == nullptr || impl_->finished) {
        return;
    }
    if (GetTickCount64() >= impl_->deadline) {
        PBVP_LOG_ERROR("Audio smoke timed out before the end-of-stream callback");
        Stop();
        return;
    }

    for (;;) {
        auto frame = impl_->decoder->TryPopVideo();
        if (frame.status != QueuePopStatus::item) {
            break;
        }
    }
    while (impl_->lookahead.size() < 2u) {
        auto chunk = impl_->decoder->TryPopAudio();
        if (chunk.status != QueuePopStatus::item) {
            break;
        }
        if (!chunk.value.has_value()) {
            PBVP_LOG_ERROR("Audio smoke received an empty audio queue item");
            Stop();
            return;
        }
        impl_->lookahead.push_back(std::move(*chunk.value));
    }

    const DecoderSnapshot decoder_snapshot = impl_->decoder->Snapshot();
    if (decoder_snapshot.state == DecoderState::failed ||
        decoder_snapshot.state == DecoderState::stopped) {
        PBVP_LOG_ERROR(
            "Audio smoke decoder failed: status=%s ffmpeg=%d win32=%lu",
            MediaDecodeStatusName(decoder_snapshot.failure.status),
            decoder_snapshot.failure.ffmpeg_error,
            static_cast<unsigned long>(decoder_snapshot.failure.io.windows_error));
        Stop();
        return;
    }

    const bool decoder_ended = decoder_snapshot.state == DecoderState::end_of_stream;
    const bool final_pending =
        decoder_ended && impl_->decoder->BufferUsage().audio_items == 0u &&
        impl_->lookahead.size() == 1u;
    if (impl_->lookahead.size() >= 2u || final_pending) {
        DecodedAudioChunk& chunk = impl_->lookahead.front();
        const XAudioStreamStatus submit_status = impl_->audio->SubmitPcm(
            chunk.samples,
            chunk.pts_us,
            chunk.generation,
            final_pending);
        if (submit_status == XAudioStreamStatus::ok) {
            impl_->submitted_samples += chunk.samples_per_channel;
            AudioSampleClock end_clock;
            const auto chunk_end = end_clock.Reset(
                chunk.sample_rate, 0u, chunk.pts_us)
                ? end_clock.MediaTimeUs(chunk.samples_per_channel)
                : std::nullopt;
            if (!chunk_end.has_value()) {
                PBVP_LOG_ERROR("Audio smoke rejected an audio timestamp");
                Stop();
                return;
            }
            impl_->expected_end_us = *chunk_end;
            impl_->submitted_end = final_pending;
            impl_->lookahead.pop_front();
        } else if (submit_status != XAudioStreamStatus::queue_full) {
            PBVP_LOG_ERROR(
                "Audio smoke submit failed: status=%s",
                XAudioStreamStatusName(submit_status));
            Stop();
            return;
        }
    }

    const XAudioStreamSnapshot audio_snapshot = impl_->audio->Snapshot();
    if (audio_snapshot.status == XAudioStreamStatus::audio_device_failed) {
        PBVP_LOG_ERROR(
            "Audio smoke device failed: error=0x%08X",
            static_cast<unsigned int>(audio_snapshot.error_code));
        Stop();
        return;
    }
    if (!impl_->started && audio_snapshot.ready_to_start) {
        const XAudioStreamStatus start_status = impl_->audio->Start();
        if (start_status != XAudioStreamStatus::ok) {
            PBVP_LOG_ERROR(
                "Audio smoke start failed: status=%s",
                XAudioStreamStatusName(start_status));
            Stop();
            return;
        }
        impl_->started = true;
        PBVP_LOG_INFO(
            "Audio smoke playback started: prebuffer_ms=200 pool_bytes=%zu",
            audio_snapshot.pool_bytes);
    }
    if (!impl_->submitted_end || !audio_snapshot.end_of_stream_reached) {
        return;
    }

    const auto media_time = impl_->audio->MediaTimeUs();
    const bool passed =
        decoder_snapshot.failure.status == MediaDecodeStatus::ok &&
        decoder_snapshot.info.source_audio_rate == 44'100u &&
        decoder_snapshot.info.source_audio_channels == 2u &&
        decoder_snapshot.info.output_audio_rate == 48'000u &&
        decoder_snapshot.info.output_audio_channels == 2u &&
        impl_->started && impl_->submitted_samples >= 96'000u &&
        impl_->submitted_samples <= 98'000u &&
        audio_snapshot.submitted_samples == impl_->submitted_samples &&
        audio_snapshot.underruns == 0u &&
        media_time == impl_->expected_end_us;
    if (passed) {
        PBVP_LOG_INFO(
            "Audio smoke passed: source_rate=44100 source_channels=2 output_rate=48000 output_channels=2 samples=%llu clock_us=%lld underruns=0 completions=%llu stream_ends=%llu pool_bytes=%zu generation=1",
            static_cast<unsigned long long>(impl_->submitted_samples),
            static_cast<long long>(media_time.value_or(-1)),
            static_cast<unsigned long long>(audio_snapshot.completed_buffers),
            static_cast<unsigned long long>(audio_snapshot.stream_end_callbacks),
            audio_snapshot.pool_bytes);
    } else {
        PBVP_LOG_ERROR(
            "Audio smoke output mismatch: source_rate=%u source_channels=%u output_rate=%u output_channels=%u samples=%llu clock_us=%lld expected_us=%lld underruns=%llu generation=%llu",
            decoder_snapshot.info.source_audio_rate,
            decoder_snapshot.info.source_audio_channels,
            decoder_snapshot.info.output_audio_rate,
            decoder_snapshot.info.output_audio_channels,
            static_cast<unsigned long long>(impl_->submitted_samples),
            static_cast<long long>(media_time.value_or(-1)),
            static_cast<long long>(impl_->expected_end_us),
            static_cast<unsigned long long>(audio_snapshot.underruns),
            static_cast<unsigned long long>(audio_snapshot.generation));
    }
    Stop();
}

void AudioSmokeTest::Stop() noexcept {
    if (impl_ == nullptr) {
        return;
    }
    impl_->finished = true;
    if (impl_->audio != nullptr) {
        impl_->audio->StopAndFlush();
    }
    if (impl_->decoder != nullptr) {
        impl_->decoder->Stop();
        impl_->decoder.reset();
        PBVP_LOG_INFO("Audio smoke decoder worker joined before private FFmpeg unload");
    }
    impl_->audio.reset();
    PBVP_LOG_INFO("Audio smoke voices and callback targets released");
    delete impl_;
    impl_ = nullptr;
}

} // namespace pbvp
