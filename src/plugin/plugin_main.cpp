#include "nvse/PluginAPI.h"

#include "pbvp/d3d_renderer.hpp"
#include "pbvp/ffmpeg_runtime.hpp"
#include "pbvp/log.hpp"
#include "pbvp/playback_controller.hpp"
#if defined(PBVP_ENABLE_AUDIO_SMOKE_TEST)
#include "pbvp/audio_smoke_test.hpp"
#endif
#if defined(PBVP_ENABLE_MEDIA_SMOKE_TEST)
#include "pbvp/media_decoder.hpp"
#endif
#include "pbvp/ui_bridge.hpp"

#include <Windows.h>
#include <Psapi.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace {

constexpr char kPluginName[] = "Pip-Boy Video Player";
constexpr std::uint32_t kPluginVersion = 1u;
constexpr std::uint32_t kMinimumNvseVersion = 0x06040050u;

PluginHandle g_plugin_handle = static_cast<PluginHandle>(kPluginHandle_Invalid);
NVSEMessagingInterface* g_messaging = nullptr;
std::atomic<bool> g_shutdown{false};
std::atomic<bool> g_presentation_ready{false};
pbvp::FfmpegRuntime g_ffmpeg_runtime;
std::unique_ptr<pbvp::PlaybackController> g_playback_controller;
pbvp::PlaybackState g_last_playback_state{pbvp::PlaybackState::idle};
#if defined(PBVP_ENABLE_PLAYBACK_SMOKE_TEST) || defined(PBVP_ENABLE_PLAYBACK_LONG_TEST)
#define PBVP_ENABLE_PLAYBACK_DIAGNOSTIC 1
#endif
#if defined(PBVP_ENABLE_AUDIO_SMOKE_TEST)
std::wstring g_audio_smoke_root;
#endif
#if defined(PBVP_ENABLE_PLAYBACK_DIAGNOSTIC)
#if defined(PBVP_ENABLE_PLAYBACK_LONG_TEST)
constexpr wchar_t kPlaybackSmokeFile[] = L"PBVP-Phase4-30Minute.mp4";
constexpr ULONGLONG kPlaybackSmokeTimeoutMs = 31u * 60u * 1000u;
constexpr float kPlaybackSmokeVolume = 0.03f;
#else
constexpr wchar_t kPlaybackSmokeFile[] = L"PBVP-Phase4-Playback.mp4";
constexpr ULONGLONG kPlaybackSmokeTimeoutMs = 20'000u;
constexpr float kPlaybackSmokeVolume = 0.10f;
#endif
std::wstring g_playback_smoke_root;
ULONGLONG g_playback_smoke_deadline{};
ULONGLONG g_playback_smoke_next_progress{};
bool g_playback_smoke_attempted{};
bool g_playback_smoke_reported{};
std::uint64_t g_playback_smoke_private_baseline{};
std::uint64_t g_playback_smoke_peak_private_delta{};
#endif

#if defined(PBVP_ENABLE_MEDIA_SMOKE_TEST)
constexpr wchar_t kMediaSmokeFile[] = L"PBVP-Phase2-Smoke.mp4";
enum class MediaSmokeStage : std::uint32_t {
    idle,
    settling,
    control,
    decoding,
    finished,
};
std::unique_ptr<pbvp::MediaDecoder> g_media_smoke_decoder;
std::wstring g_media_smoke_root;
bool g_media_smoke_attempted{};
bool g_media_smoke_generations_valid{true};
std::uint64_t g_media_smoke_video_frames{};
std::uint64_t g_media_smoke_audio_chunks{};
std::uint64_t g_media_smoke_audio_samples{};
std::uint64_t g_media_smoke_private_baseline{};
std::uint64_t g_media_smoke_peak_private_delta{};
std::uint64_t g_media_smoke_control_peak_delta{};
std::size_t g_media_smoke_peak_video_bytes{};
std::size_t g_media_smoke_peak_audio_bytes{};
MediaSmokeStage g_media_smoke_stage{MediaSmokeStage::idle};
LARGE_INTEGER g_media_smoke_frequency{};
LARGE_INTEGER g_media_smoke_stage_started{};
#endif

std::wstring WidenRuntimeDirectory(const char* path) noexcept {
    try {
        if (path == nullptr || *path == '\0') {
            return {};
        }
        const int required = MultiByteToWideChar(CP_ACP, 0u, path, -1, nullptr, 0);
        if (required <= 1) {
            return {};
        }
        std::wstring output(static_cast<std::size_t>(required), L'\0');
        if (MultiByteToWideChar(CP_ACP, 0u, path, -1, output.data(), required) != required) {
            return {};
        }
        output.pop_back();
        return output;
    } catch (...) {
        return {};
    }
}

std::wstring PrivateFfmpegDirectory(const char* runtime_directory) noexcept {
    try {
        std::wstring path = WidenRuntimeDirectory(runtime_directory);
        if (path.empty()) {
            return {};
        }
        if (path.back() != L'\\' && path.back() != L'/') {
            path.push_back(L'\\');
        }
        path.append(L"Data\\NVSE\\Plugins\\PipBoyVideoPlayer\\bin");
        return path;
    } catch (...) {
        return {};
    }
}

#if defined(PBVP_ENABLE_MEDIA_SMOKE_TEST) || defined(PBVP_ENABLE_AUDIO_SMOKE_TEST) || \
    defined(PBVP_ENABLE_PLAYBACK_DIAGNOSTIC)
std::wstring PrivateMediaDirectory(const char* runtime_directory) noexcept {
    try {
        std::wstring path = WidenRuntimeDirectory(runtime_directory);
        if (path.empty()) {
            return {};
        }
        if (path.back() != L'\\' && path.back() != L'/') {
            path.push_back(L'\\');
        }
        path.append(L"Data\\NVSE\\Plugins\\PipBoyVideoPlayer\\Videos");
        return path;
    } catch (...) {
        return {};
    }
}
#endif

#if defined(PBVP_ENABLE_MEDIA_SMOKE_TEST) || defined(PBVP_ENABLE_PLAYBACK_DIAGNOSTIC)
std::uint64_t ProcessPrivateBytes() noexcept {
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = static_cast<DWORD>(sizeof(counters));
    if (GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
            static_cast<DWORD>(sizeof(counters))) == FALSE) {
        return 0u;
    }
    return static_cast<std::uint64_t>(counters.PrivateUsage);
}
#endif

bool PlaybackActive(const pbvp::PlaybackState state) noexcept {
    return state == pbvp::PlaybackState::opening ||
           state == pbvp::PlaybackState::buffering ||
           state == pbvp::PlaybackState::playing ||
           state == pbvp::PlaybackState::paused;
}

void StopPlayback(const pbvp::PlaybackTerminalReason reason) noexcept {
    if (g_playback_controller == nullptr) {
        return;
    }
    const pbvp::PlaybackState state = g_playback_controller->Snapshot().playback.state;
    if (state != pbvp::PlaybackState::idle && state != pbvp::PlaybackState::unavailable) {
        g_playback_controller->Stop(reason);
    }
    pbvp::D3dRenderer::Instance().ClearVideoFrame();
}

void UpdatePlayback(const pbvp::UiRectSnapshot& ui_snapshot) noexcept {
    if (g_playback_controller == nullptr) {
        return;
    }

#if defined(PBVP_ENABLE_PLAYBACK_DIAGNOSTIC)
    if (!g_playback_smoke_attempted && ui_snapshot.visible) {
        g_playback_smoke_attempted = true;
#if defined(PBVP_ENABLE_PLAYBACK_LONG_TEST)
        PBVP_LOG_INFO(
            "PBVP_PLAYBACK_LONG_TEST_ARMED: opening the generated 30-minute 720p30 fixture at volume 0.03");
#else
        PBVP_LOG_INFO(
            "PBVP_PLAYBACK_SMOKE_TEST_ARMED: opening the generated Phase 4 fixture at volume 0.10");
#endif
        g_playback_smoke_private_baseline = ProcessPrivateBytes();
        g_playback_smoke_peak_private_delta = 0u;
        if (g_playback_smoke_root.empty() ||
            !g_playback_controller->Open(g_playback_smoke_root, kPlaybackSmokeFile)) {
            PBVP_LOG_ERROR("Integrated playback smoke could not open its private fixture");
            g_playback_smoke_reported = true;
        } else if (g_playback_smoke_private_baseline == 0u) {
            g_playback_controller->Stop(pbvp::PlaybackTerminalReason::failed);
            PBVP_LOG_ERROR("Integrated playback diagnostic could not read process memory");
            g_playback_smoke_reported = true;
        } else {
            const ULONGLONG now = GetTickCount64();
            g_playback_smoke_deadline = now + kPlaybackSmokeTimeoutMs;
            g_playback_smoke_next_progress = now + 5u * 60u * 1000u;
        }
    }
#endif

    pbvp::PlaybackControllerSnapshot before = g_playback_controller->Snapshot();
    if (PlaybackActive(before.playback.state)) {
        if (!g_playback_controller->Update(ui_snapshot.visible)) {
            const pbvp::PlaybackControllerSnapshot failed = g_playback_controller->Snapshot();
            PBVP_LOG_ERROR(
                "Playback update failed: state=%s error=%s site=%s decoder=%s audio=%s",
                pbvp::PlaybackStateName(failed.playback.state),
                pbvp::PlaybackErrorName(failed.playback.error),
                pbvp::PlaybackFailureSiteName(failed.failure_site),
                pbvp::MediaDecodeStatusName(failed.decoder.failure.status),
                pbvp::XAudioStreamStatusName(failed.audio.status));
        }
        std::optional<pbvp::DecodedVideoFrame> frame =
            g_playback_controller->TakeVideoFrame();
        if (frame.has_value() &&
            !pbvp::D3dRenderer::Instance().SubmitVideoFrame(std::move(*frame))) {
            g_playback_controller->NotifyRenderFailure();
            PBVP_LOG_ERROR("Playback stopped because the render mailbox rejected a decoded frame");
        }
    }

    const pbvp::PlaybackControllerSnapshot after = g_playback_controller->Snapshot();
#if defined(PBVP_ENABLE_PLAYBACK_DIAGNOSTIC)
    if (g_playback_smoke_attempted && !g_playback_smoke_reported) {
        const std::uint64_t private_bytes = ProcessPrivateBytes();
        if (private_bytes > g_playback_smoke_private_baseline) {
            g_playback_smoke_peak_private_delta = (std::max)(
                g_playback_smoke_peak_private_delta,
                private_bytes - g_playback_smoke_private_baseline);
        }
    }
#if defined(PBVP_ENABLE_PLAYBACK_LONG_TEST)
    if (!g_playback_smoke_reported && PlaybackActive(after.playback.state) &&
        GetTickCount64() >= g_playback_smoke_next_progress) {
        PBVP_LOG_INFO(
            "Integrated playback long test progress: clock_us=%lld decoded=%llu presented=%llu dropped=%llu underruns=%llu private_delta=%llu",
            static_cast<long long>(after.metrics.last_media_time_us),
            static_cast<unsigned long long>(after.metrics.decoded_video_frames),
            static_cast<unsigned long long>(after.metrics.presented_video_frames),
            static_cast<unsigned long long>(after.metrics.dropped_video_frames),
            static_cast<unsigned long long>(after.audio.underruns),
            static_cast<unsigned long long>(g_playback_smoke_peak_private_delta));
        g_playback_smoke_next_progress = GetTickCount64() + 5u * 60u * 1000u;
    }
#endif
#endif
    if (ui_snapshot.visible &&
        !pbvp::UiBridge::Instance().SetPlaybackStatus(after.playback) &&
        after.playback.state == pbvp::PlaybackState::error) {
        PBVP_LOG_WARN("The Pip-Boy status text could not display the playback error");
    }
    if (after.playback.state != g_last_playback_state) {
        PBVP_LOG_INFO(
            "Playback state changed: %s -> %s generation=%llu",
            pbvp::PlaybackStateName(g_last_playback_state),
            pbvp::PlaybackStateName(after.playback.state),
            static_cast<unsigned long long>(after.generation));
        g_last_playback_state = after.playback.state;
    }
    if (!PlaybackActive(after.playback.state) && PlaybackActive(before.playback.state)) {
        pbvp::D3dRenderer::Instance().ClearVideoFrame();
    }

#if defined(PBVP_ENABLE_PLAYBACK_DIAGNOSTIC)
    if (!g_playback_smoke_reported && g_playback_smoke_attempted &&
        g_playback_smoke_deadline != 0u && GetTickCount64() >= g_playback_smoke_deadline &&
        PlaybackActive(after.playback.state)) {
        g_playback_controller->Stop(pbvp::PlaybackTerminalReason::failed);
        pbvp::D3dRenderer::Instance().ClearVideoFrame();
        PBVP_LOG_ERROR("Integrated playback smoke timed out before clean completion");
        g_playback_smoke_reported = true;
    } else if (!g_playback_smoke_reported &&
               after.terminal_reason == pbvp::PlaybackTerminalReason::completed) {
        const pbvp::D3dRendererSnapshot render = pbvp::D3dRenderer::Instance().Snapshot();
        const std::int64_t sync_error_us = after.metrics.last_media_time_us >=
                after.metrics.last_presented_video_end_us
            ? after.metrics.last_media_time_us - after.metrics.last_presented_video_end_us
            : after.metrics.last_presented_video_end_us - after.metrics.last_media_time_us;
#if defined(PBVP_ENABLE_PLAYBACK_LONG_TEST)
        PBVP_LOG_INFO(
            "Integrated playback long test passed: decoded=%llu presented=%llu dropped=%llu audio_samples=%llu clock_us=%lld video_end_us=%lld sync_error_us=%lld underruns=%llu seeks=%llu video_submitted=%llu video_uploaded=%llu mailbox_replaced=%llu upload_us=%.2f/%.2f/%.2f private_delta=%llu staged_peak=%zu decoder_video_peak=%zu decoder_audio_peak=%zu generation=%llu",
            static_cast<unsigned long long>(after.metrics.decoded_video_frames),
            static_cast<unsigned long long>(after.metrics.presented_video_frames),
            static_cast<unsigned long long>(after.metrics.dropped_video_frames),
            static_cast<unsigned long long>(after.metrics.submitted_audio_samples),
            static_cast<long long>(after.metrics.last_media_time_us),
            static_cast<long long>(after.metrics.last_presented_video_end_us),
            static_cast<long long>(sync_error_us),
            static_cast<unsigned long long>(after.audio.underruns),
            static_cast<unsigned long long>(after.metrics.seek_count),
            static_cast<unsigned long long>(render.submitted_video_frames),
            static_cast<unsigned long long>(render.uploaded_video_frames),
            static_cast<unsigned long long>(render.replaced_mailbox_frames),
            render.upload_minimum_us, render.upload_average_us, render.upload_maximum_us,
            static_cast<unsigned long long>(g_playback_smoke_peak_private_delta),
            after.metrics.peak_staged_video_bytes,
            after.metrics.peak_decoder_video_bytes,
            after.metrics.peak_decoder_audio_bytes,
            static_cast<unsigned long long>(after.generation));
#else
        PBVP_LOG_INFO(
            "Integrated playback smoke passed: decoded=%llu presented=%llu dropped=%llu audio_samples=%llu clock_us=%lld underruns=%llu seeks=%llu video_submitted=%llu video_uploaded=%llu mailbox_replaced=%llu upload_us=%.2f/%.2f/%.2f generation=%llu sync_error_us=%lld private_delta=%llu staged_peak=%zu decoder_video_peak=%zu decoder_audio_peak=%zu",
            static_cast<unsigned long long>(after.metrics.decoded_video_frames),
            static_cast<unsigned long long>(after.metrics.presented_video_frames),
            static_cast<unsigned long long>(after.metrics.dropped_video_frames),
            static_cast<unsigned long long>(after.metrics.submitted_audio_samples),
            static_cast<long long>(after.metrics.last_media_time_us),
            static_cast<unsigned long long>(after.audio.underruns),
            static_cast<unsigned long long>(after.metrics.seek_count),
            static_cast<unsigned long long>(render.submitted_video_frames),
            static_cast<unsigned long long>(render.uploaded_video_frames),
            static_cast<unsigned long long>(render.replaced_mailbox_frames),
            render.upload_minimum_us, render.upload_average_us, render.upload_maximum_us,
            static_cast<unsigned long long>(after.generation),
            static_cast<long long>(sync_error_us),
            static_cast<unsigned long long>(g_playback_smoke_peak_private_delta),
            after.metrics.peak_staged_video_bytes,
            after.metrics.peak_decoder_video_bytes,
            after.metrics.peak_decoder_audio_bytes);
#endif
        g_playback_smoke_reported = true;
    }
#endif
}

#if defined(PBVP_ENABLE_MEDIA_SMOKE_TEST)
void StopMediaSmoke() noexcept {
    if (g_media_smoke_decoder != nullptr) {
        g_media_smoke_decoder->Stop();
        g_media_smoke_decoder.reset();
        PBVP_LOG_INFO("Media smoke worker joined before private FFmpeg unload");
    }
    g_media_smoke_stage = MediaSmokeStage::finished;
}

std::int64_t MediaSmokeStageMicroseconds() noexcept {
    LARGE_INTEGER now{};
    if (g_media_smoke_frequency.QuadPart <= 0 ||
        QueryPerformanceCounter(&now) == FALSE ||
        now.QuadPart < g_media_smoke_stage_started.QuadPart) {
        return -1;
    }
    const std::int64_t ticks = now.QuadPart - g_media_smoke_stage_started.QuadPart;
    return ticks * 1'000'000ll / g_media_smoke_frequency.QuadPart;
}

void BeginMediaSmokeDecoder() noexcept {
    if (g_media_smoke_root.empty()) {
        PBVP_LOG_ERROR("Media smoke failed before worker start: media root unavailable");
        g_media_smoke_stage = MediaSmokeStage::finished;
        return;
    }
    try {
        g_media_smoke_private_baseline = ProcessPrivateBytes();
        g_media_smoke_decoder = std::make_unique<pbvp::MediaDecoder>(g_ffmpeg_runtime);
    } catch (...) {
        PBVP_LOG_ERROR("Media smoke failed before worker start: allocation failed");
        g_media_smoke_stage = MediaSmokeStage::finished;
        return;
    }
    pbvp::MediaDecodeFailure failure{};
    if (!g_media_smoke_decoder->Start(g_media_smoke_root, kMediaSmokeFile, failure)) {
        PBVP_LOG_ERROR(
            "Media smoke failed before worker start: status=%s win32=%lu",
            pbvp::MediaDecodeStatusName(failure.status),
            static_cast<unsigned long>(failure.io.windows_error));
        StopMediaSmoke();
        return;
    }
    g_media_smoke_stage = MediaSmokeStage::decoding;
    PBVP_LOG_INFO("Media smoke decoder worker started for PBVP-Phase2-Smoke.mp4");
}

void StartMediaSmoke() noexcept {
    if (g_media_smoke_attempted) {
        return;
    }
    g_media_smoke_attempted = true;
    PBVP_LOG_INFO(
        "PBVP_MEDIA_SMOKE_TEST_ARMED: waiting for a stable main-menu memory baseline");
    if (QueryPerformanceFrequency(&g_media_smoke_frequency) == FALSE ||
        QueryPerformanceCounter(&g_media_smoke_stage_started) == FALSE ||
        g_media_smoke_frequency.QuadPart <= 0) {
        PBVP_LOG_ERROR("Media smoke failed before control start: performance counter unavailable");
        g_media_smoke_stage = MediaSmokeStage::finished;
        return;
    }
    g_media_smoke_stage = MediaSmokeStage::settling;
}

void UpdateMediaSmoke() noexcept {
    if (g_media_smoke_stage == MediaSmokeStage::settling) {
        const std::int64_t elapsed_us = MediaSmokeStageMicroseconds();
        if (elapsed_us < 0) {
            PBVP_LOG_ERROR("Media smoke failed during settle delay: performance counter unavailable");
            g_media_smoke_stage = MediaSmokeStage::finished;
            return;
        }
        if (elapsed_us < 5'000'000ll) {
            return;
        }
        g_media_smoke_private_baseline = ProcessPrivateBytes();
        g_media_smoke_control_peak_delta = 0u;
        if (g_media_smoke_private_baseline == 0u ||
            QueryPerformanceCounter(&g_media_smoke_stage_started) == FALSE) {
            PBVP_LOG_ERROR("Media smoke failed before control start: process memory unavailable");
            g_media_smoke_stage = MediaSmokeStage::finished;
            return;
        }
        g_media_smoke_stage = MediaSmokeStage::control;
        PBVP_LOG_INFO("Media smoke no-decode memory control started after five-second settle delay");
        return;
    }

    if (g_media_smoke_stage == MediaSmokeStage::control) {
        const std::uint64_t private_bytes = ProcessPrivateBytes();
        if (private_bytes > g_media_smoke_private_baseline) {
            g_media_smoke_control_peak_delta = (std::max)(
                g_media_smoke_control_peak_delta,
                private_bytes - g_media_smoke_private_baseline);
        }
        const std::int64_t elapsed_us = MediaSmokeStageMicroseconds();
        if (elapsed_us < 0) {
            PBVP_LOG_ERROR("Media smoke failed during no-decode control: performance counter unavailable");
            g_media_smoke_stage = MediaSmokeStage::finished;
            return;
        }
        if (elapsed_us < 1'000'000ll) {
            return;
        }
        if (g_media_smoke_control_peak_delta >= 32u * 1024u * 1024u) {
            PBVP_LOG_ERROR(
                "Media smoke no-decode control remained unstable: private_delta=%llu",
                static_cast<unsigned long long>(g_media_smoke_control_peak_delta));
            g_media_smoke_stage = MediaSmokeStage::finished;
            return;
        }
        PBVP_LOG_INFO(
            "Media smoke no-decode control passed: private_delta=%llu",
            static_cast<unsigned long long>(g_media_smoke_control_peak_delta));
        g_media_smoke_private_baseline = 0u;
        g_media_smoke_peak_private_delta = 0u;
        g_media_smoke_peak_video_bytes = 0u;
        g_media_smoke_peak_audio_bytes = 0u;
        BeginMediaSmokeDecoder();
        return;
    }

    if (g_media_smoke_stage != MediaSmokeStage::decoding ||
        g_media_smoke_decoder == nullptr) {
        return;
    }
    const pbvp::DecoderBufferUsage usage = g_media_smoke_decoder->BufferUsage();
    g_media_smoke_peak_video_bytes = (std::max)(
        g_media_smoke_peak_video_bytes, usage.video_bytes);
    g_media_smoke_peak_audio_bytes = (std::max)(
        g_media_smoke_peak_audio_bytes, usage.audio_bytes);
    const std::uint64_t private_bytes = ProcessPrivateBytes();
    if (private_bytes > g_media_smoke_private_baseline) {
        g_media_smoke_peak_private_delta = (std::max)(
            g_media_smoke_peak_private_delta,
            private_bytes - g_media_smoke_private_baseline);
    }

    for (;;) {
        auto frame = g_media_smoke_decoder->TryPopVideo();
        if (frame.status != pbvp::QueuePopStatus::item) {
            break;
        }
        if (!frame.value.has_value()) {
            g_media_smoke_generations_valid = false;
            continue;
        }
        ++g_media_smoke_video_frames;
        if (frame.value->generation != 1u) {
            g_media_smoke_generations_valid = false;
        }
    }
    for (;;) {
        auto chunk = g_media_smoke_decoder->TryPopAudio();
        if (chunk.status != pbvp::QueuePopStatus::item) {
            break;
        }
        if (!chunk.value.has_value()) {
            g_media_smoke_generations_valid = false;
            continue;
        }
        ++g_media_smoke_audio_chunks;
        g_media_smoke_audio_samples += chunk.value->samples_per_channel;
        if (chunk.value->generation != 1u) {
            g_media_smoke_generations_valid = false;
        }
    }

    const pbvp::DecoderSnapshot snapshot = g_media_smoke_decoder->Snapshot();
    if (snapshot.state == pbvp::DecoderState::failed) {
        PBVP_LOG_ERROR(
            "Media smoke failed: status=%s ffmpeg=%d win32=%lu video=%llu audio_chunks=%llu",
            pbvp::MediaDecodeStatusName(snapshot.failure.status),
            snapshot.failure.ffmpeg_error,
            static_cast<unsigned long>(snapshot.failure.io.windows_error),
            static_cast<unsigned long long>(g_media_smoke_video_frames),
            static_cast<unsigned long long>(g_media_smoke_audio_chunks));
        StopMediaSmoke();
        return;
    }
    if (snapshot.state != pbvp::DecoderState::end_of_stream ||
        g_media_smoke_video_frames != snapshot.video_frames ||
        g_media_smoke_audio_chunks != snapshot.audio_chunks) {
        return;
    }

    const bool expected = snapshot.failure.status == pbvp::MediaDecodeStatus::ok &&
        snapshot.info.source_width == 1920u && snapshot.info.source_height == 1080u &&
        snapshot.info.has_audio && snapshot.info.source_audio_channels == 2u &&
        snapshot.info.source_audio_rate == 48000u &&
        snapshot.info.output_audio_channels == 2u &&
        snapshot.info.output_audio_rate == 48000u &&
        g_media_smoke_video_frames == 30u &&
        g_media_smoke_audio_samples >= 47000u &&
        g_media_smoke_audio_samples <= 49000u &&
        g_media_smoke_peak_private_delta < 128u * 1024u * 1024u &&
        g_media_smoke_peak_video_bytes <= 32u * 1024u * 1024u &&
        g_media_smoke_peak_audio_bytes <= 4u * 1024u * 1024u &&
        g_media_smoke_generations_valid;
    if (expected) {
        PBVP_LOG_INFO(
            "Media smoke passed: source=1920x1080 video=30 audio_chunks=%llu audio_samples=%llu private_delta=%llu video_queue_peak=%zu audio_queue_peak=%zu generation=1",
            static_cast<unsigned long long>(g_media_smoke_audio_chunks),
            static_cast<unsigned long long>(g_media_smoke_audio_samples),
            static_cast<unsigned long long>(g_media_smoke_peak_private_delta),
            g_media_smoke_peak_video_bytes,
            g_media_smoke_peak_audio_bytes);
    } else {
        PBVP_LOG_ERROR(
            "Media smoke output mismatch: source=%ux%u video=%llu audio_chunks=%llu audio_samples=%llu private_delta=%llu video_queue_peak=%zu audio_queue_peak=%zu generation_ok=%u",
            snapshot.info.source_width, snapshot.info.source_height,
            static_cast<unsigned long long>(g_media_smoke_video_frames),
            static_cast<unsigned long long>(g_media_smoke_audio_chunks),
            static_cast<unsigned long long>(g_media_smoke_audio_samples),
            static_cast<unsigned long long>(g_media_smoke_peak_private_delta),
            g_media_smoke_peak_video_bytes,
            g_media_smoke_peak_audio_bytes,
            g_media_smoke_generations_valid ? 1u : 0u);
    }
    StopMediaSmoke();
}
#endif

void HandleMessage(NVSEMessagingInterface::Message* message) {
    if (message == nullptr) {
        return;
    }
    switch (message->type) {
        case NVSEMessagingInterface::kMessage_PostLoad:
            PBVP_LOG_INFO("xNVSE PostLoad received");
            break;
        case NVSEMessagingInterface::kMessage_DeferredInit:
            PBVP_LOG_INFO("xNVSE DeferredInit received");
            g_presentation_ready.store(true, std::memory_order_release);
            PBVP_LOG_INFO("xNVSE frame-present presentation path enabled without executable hooks");
#if defined(PBVP_ENABLE_MEDIA_SMOKE_TEST)
            StartMediaSmoke();
#endif
#if defined(PBVP_ENABLE_AUDIO_SMOKE_TEST)
            pbvp::AudioSmokeTest::Instance().Start(g_ffmpeg_runtime, g_audio_smoke_root);
#endif
            break;
        case NVSEMessagingInterface::kMessage_MainGameLoop:
            if (!g_shutdown.load(std::memory_order_acquire)) {
                pbvp::UiBridge::Instance().UpdateOnGameThread();
                UpdatePlayback(pbvp::UiBridge::Instance().ReadForRenderThread());
#if defined(PBVP_ENABLE_MEDIA_SMOKE_TEST)
                UpdateMediaSmoke();
#endif
#if defined(PBVP_ENABLE_AUDIO_SMOKE_TEST)
                pbvp::AudioSmokeTest::Instance().Update();
#endif
            }
            break;
        case NVSEMessagingInterface::kMessage_OnFramePresent: {
            bool loading_screen = true;
            if (message->data != nullptr && message->dataLen == sizeof(int)) {
                loading_screen = *static_cast<const int*>(message->data) != 0;
            }
            if (!loading_screen && g_presentation_ready.load(std::memory_order_acquire) &&
                !g_shutdown.load(std::memory_order_acquire)) {
                pbvp::D3dRenderer::Instance().OnFrame(
                    pbvp::UiBridge::Instance().ReadForRenderThread());
            }
            break;
        }
        case NVSEMessagingInterface::kMessage_PreLoadGame:
        case NVSEMessagingInterface::kMessage_ExitToMainMenu:
        case NVSEMessagingInterface::kMessage_NewGame:
            StopPlayback(pbvp::PlaybackTerminalReason::lifecycle_transition);
            pbvp::UiBridge::Instance().Clear();
            PBVP_LOG_INFO("Game transition cleared the Pip-Boy presentation snapshot");
            break;
        case NVSEMessagingInterface::kMessage_ExitGame:
        case NVSEMessagingInterface::kMessage_ExitGame_Console:
            g_shutdown.store(true, std::memory_order_release);
            g_presentation_ready.store(false, std::memory_order_release);
            if (g_playback_controller != nullptr) {
                g_playback_controller->Shutdown();
                g_playback_controller.reset();
                PBVP_LOG_INFO("Playback audio stopped and decoder worker joined");
            }
            pbvp::UiBridge::Instance().Clear();
            pbvp::D3dRenderer::Instance().RequestShutdown();
#if defined(PBVP_ENABLE_MEDIA_SMOKE_TEST)
            StopMediaSmoke();
#endif
#if defined(PBVP_ENABLE_AUDIO_SMOKE_TEST)
            pbvp::AudioSmokeTest::Instance().Stop();
#endif
            g_ffmpeg_runtime.Unload();
            PBVP_LOG_INFO("Process shutdown requested");
            break;
        case NVSEMessagingInterface::kMessage_ReloadConfig:
            if (message->data != nullptr && message->dataLen > 0u &&
                std::strcmp(static_cast<const char*>(message->data), "PipBoyVideoPlayer") == 0) {
                PBVP_LOG_INFO("Configuration reload requested; Phase 1 has no runtime settings");
            }
            break;
        default:
            break;
    }
}

} // namespace

extern "C" bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info) {
    if (nvse == nullptr || info == nullptr) {
        return false;
    }
    info->infoVersion = PluginInfo::kInfoVersion;
    info->name = kPluginName;
    info->version = kPluginVersion;

    if (nvse->isEditor != 0u || nvse->runtimeVersion != RUNTIME_VERSION_1_4_0_525 ||
        nvse->nvseVersion < kMinimumNvseVersion) {
        return false;
    }
    return true;
}

extern "C" bool NVSEPlugin_Load(NVSEInterface* nvse) {
    if (nvse == nullptr || nvse->QueryInterface == nullptr ||
        nvse->GetPluginHandle == nullptr || nvse->GetRuntimeDirectory == nullptr) {
        return false;
    }

    g_plugin_handle = nvse->GetPluginHandle();
    auto* logging = static_cast<NVSELoggingInterface*>(nvse->QueryInterface(kInterface_Logging));
    const char* log_directory = "";
    if (logging != nullptr && logging->GetPluginLogPath != nullptr) {
        log_directory = logging->GetPluginLogPath();
    }
    pbvp::log::Open(nvse->GetRuntimeDirectory(), log_directory);
    PBVP_LOG_INFO(
        "Pip-Boy Video Player %s loading; runtime=0x%08X xNVSE=0x%08X",
        PBVP_VERSION_STRING, nvse->runtimeVersion, nvse->nvseVersion);
    pbvp::FfmpegLoadFailure ffmpeg_failure{};
    const std::wstring ffmpeg_directory = PrivateFfmpegDirectory(nvse->GetRuntimeDirectory());
#if defined(PBVP_ENABLE_MEDIA_SMOKE_TEST)
    g_media_smoke_root = PrivateMediaDirectory(nvse->GetRuntimeDirectory());
#endif
#if defined(PBVP_ENABLE_AUDIO_SMOKE_TEST)
    g_audio_smoke_root = PrivateMediaDirectory(nvse->GetRuntimeDirectory());
#endif
#if defined(PBVP_ENABLE_PLAYBACK_DIAGNOSTIC)
    g_playback_smoke_root = PrivateMediaDirectory(nvse->GetRuntimeDirectory());
#endif
    if (ffmpeg_directory.empty()) {
        ffmpeg_failure.status = pbvp::FfmpegLoadStatus::path_not_absolute;
    }
    if (ffmpeg_directory.empty() || !g_ffmpeg_runtime.Load(ffmpeg_directory, ffmpeg_failure)) {
        PBVP_LOG_ERROR(
            "Private FFmpeg runtime rejected: reason=%s module=%ls symbol=%s win32=%lu",
            pbvp::FfmpegLoadStatusName(ffmpeg_failure.status),
            ffmpeg_failure.module != nullptr ? ffmpeg_failure.module : L"none",
            ffmpeg_failure.symbol != nullptr ? ffmpeg_failure.symbol : "none",
            static_cast<unsigned long>(ffmpeg_failure.windows_error));
        pbvp::log::Close();
        return false;
    }
    const pbvp::FfmpegVersions ffmpeg_versions = g_ffmpeg_runtime.Versions();
    PBVP_LOG_INFO(
        "Private FFmpeg runtime accepted: avcodec=0x%06X avformat=0x%06X avutil=0x%06X swresample=0x%06X swscale=0x%06X",
        ffmpeg_versions.avcodec, ffmpeg_versions.avformat, ffmpeg_versions.avutil,
        ffmpeg_versions.swresample, ffmpeg_versions.swscale);
    try {
        pbvp::PlaybackControllerConfig playback_config{};
#if defined(PBVP_ENABLE_PLAYBACK_DIAGNOSTIC)
        playback_config.volume = kPlaybackSmokeVolume;
#endif
        g_playback_controller = std::make_unique<pbvp::PlaybackController>(
            g_ffmpeg_runtime, playback_config);
    } catch (...) {
        PBVP_LOG_ERROR("Playback controller allocation failed");
        g_ffmpeg_runtime.Unload();
        return false;
    }
    if (g_playback_controller->Snapshot().playback.state ==
        pbvp::PlaybackState::unavailable) {
        PBVP_LOG_ERROR("Playback controller rejected its bounded configuration");
        g_playback_controller.reset();
        g_ffmpeg_runtime.Unload();
        return false;
    }
    g_messaging = static_cast<NVSEMessagingInterface*>(nvse->QueryInterface(kInterface_Messaging));
    if (g_messaging == nullptr || g_messaging->version < NVSEMessagingInterface::kVersion ||
        g_messaging->RegisterListener == nullptr ||
        !g_messaging->RegisterListener(g_plugin_handle, "NVSE", &HandleMessage)) {
        PBVP_LOG_ERROR("Required xNVSE messaging interface is unavailable");
        g_playback_controller.reset();
        g_ffmpeg_runtime.Unload();
        return false;
    }
    PBVP_LOG_INFO("Plugin lifecycle listener registered");
    return true;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
