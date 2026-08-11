#include "nvse/PluginAPI.h"

#include "pbvp/d3d_renderer.hpp"
#include "pbvp/configuration.hpp"
#include "pbvp/ffmpeg_runtime.hpp"
#include "pbvp/log.hpp"
#include "pbvp/log_privacy.hpp"
#include "pbvp/media_catalog.hpp"
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
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <limits>
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
pbvp::PlayerSettings g_settings{};
std::wstring g_configuration_path;
std::wstring g_media_root;
pbvp::PlaybackState g_last_playback_state{pbvp::PlaybackState::idle};

enum class VideosPageState : std::uint32_t {
    data_page,
    catalog,
    playback,
};

VideosPageState g_videos_page_state{VideosPageState::data_page};
pbvp::MediaCatalogResult g_media_catalog{};
pbvp::MediaCatalogSelection g_catalog_selection{pbvp::kUiCatalogVisibleRows};
std::wstring g_current_display_title;
bool g_current_title_metadata_checked{};
bool g_current_stream_summary_logged{};
std::uint64_t g_playback_sequence{};
bool g_playback_session_summary_logged{true};
std::int64_t g_last_display_second{-1ll};
std::int64_t g_last_display_duration_second{-1ll};
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

std::wstring PrivateConfigurationPath(const char* runtime_directory) noexcept {
    try {
        std::wstring path = WidenRuntimeDirectory(runtime_directory);
        if (path.empty()) {
            return {};
        }
        if (path.back() != L'\\' && path.back() != L'/') {
            path.push_back(L'\\');
        }
        path.append(L"Data\\Config\\PipBoyVideoPlayer.ini");
        return path;
    } catch (...) {
        return {};
    }
}

pbvp::PlaybackControllerConfig PlaybackConfiguration(
    const pbvp::PlayerSettings& settings) noexcept {
    pbvp::PlaybackControllerConfig config{};
    config.volume = settings.volume;
    config.muted = settings.muted;
    config.decoder.payload_limits.maximum_width =
        settings.resources.maximum_source_width;
    config.decoder.payload_limits.maximum_height =
        settings.resources.maximum_source_height;
    config.decoder.io_limits.maximum_file_bytes =
        settings.resources.maximum_media_file_bytes;
    config.decoder.output_video_edge_limit =
        settings.resources.maximum_queued_video_edge;
#if defined(PBVP_ENABLE_PLAYBACK_DIAGNOSTIC)
    config.volume = kPlaybackSmokeVolume;
#endif
    return config;
}

bool CreatePlaybackController(const pbvp::PlayerSettings& settings) noexcept {
    try {
        auto replacement = std::make_unique<pbvp::PlaybackController>(
            g_ffmpeg_runtime, PlaybackConfiguration(settings));
        if (replacement->Snapshot().playback.state == pbvp::PlaybackState::unavailable) {
            return false;
        }
        if (g_playback_controller != nullptr) {
            g_playback_controller->Shutdown();
        }
        g_playback_controller = std::move(replacement);
        g_last_playback_state = pbvp::PlaybackState::idle;
        return true;
    } catch (...) {
        return false;
    }
}

void LogConfigurationResult(const pbvp::ConfigurationResult& result) noexcept {
    if (result.status != pbvp::ConfigurationStatus::ok) {
        PBVP_LOG_WARN(
            "Configuration unavailable; compiled defaults retained: status=%s win32=%lu",
            pbvp::ConfigurationStatusName(result.status),
            static_cast<unsigned long>(result.windows_error));
        return;
    }
    if (result.unknown_settings != 0u || result.invalid_settings != 0u ||
        result.malformed_lines != 0u) {
        PBVP_LOG_WARN(
            "Configuration loaded with ignored values: unknown=%u invalid=%u malformed=%u",
            result.unknown_settings, result.invalid_settings, result.malformed_lines);
    }
    PBVP_LOG_INFO(
        "Configuration accepted: enabled=%u aspect=%s tint=%s volume=%.2f muted=%u seek_seconds=%u catalog=%zu display_chars=%zu source=%ux%u queued_edge=%u file_limit=%llu logging=%s",
        result.settings.enabled ? 1u : 0u,
        pbvp::AspectModeName(result.settings.aspect_mode),
        pbvp::TintModeName(result.settings.tint_mode),
        static_cast<double>(result.settings.volume),
        result.settings.muted ? 1u : 0u,
        result.settings.seek_seconds,
        result.settings.catalog.maximum_entries,
        result.settings.catalog.maximum_display_characters,
        result.settings.resources.maximum_source_width,
        result.settings.resources.maximum_source_height,
        result.settings.resources.maximum_queued_video_edge,
        static_cast<unsigned long long>(
            result.settings.resources.maximum_media_file_bytes),
        pbvp::LoggingDetailName(result.settings.logging_detail));
}

void ReloadConfiguration() noexcept {
    if (g_playback_controller == nullptr || !pbvp::ConfigurationReloadAllowed(
            g_playback_controller->Snapshot().playback.state)) {
        PBVP_LOG_WARN("Configuration reload rejected because playback is not idle");
        return;
    }
    const pbvp::ConfigurationResult loaded =
        pbvp::LoadConfiguration(g_configuration_path);
    if (loaded.status != pbvp::ConfigurationStatus::ok) {
        LogConfigurationResult(loaded);
        PBVP_LOG_WARN("Configuration reload kept the previous settings");
        return;
    }
    if (!CreatePlaybackController(loaded.settings)) {
        PBVP_LOG_ERROR("Configuration reload rejected an invalid playback configuration");
        return;
    }
    g_settings = loaded.settings;
    pbvp::D3dRenderer::Instance().ConfigurePresentation(
        g_settings.aspect_mode, g_settings.tint_mode);
    if (!pbvp::UiBridge::Instance().SetInputBindings(g_settings.input)) {
        PBVP_LOG_ERROR("Configuration reload rejected invalid input bindings");
        return;
    }
    LogConfigurationResult(loaded);
    pbvp::D3dRenderer::Instance().ClearVideoFrame();
    g_videos_page_state = VideosPageState::data_page;
    g_media_catalog = {};
    g_catalog_selection.Reset(0u);
    g_current_display_title.clear();
    PBVP_LOG_INFO("Configuration reloaded while playback was idle");
}

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

struct ProcessAddressSpaceSnapshot {
    std::uint64_t private_bytes{};
    std::uint64_t working_set_bytes{};
    std::uint64_t largest_free_region_bytes{};
    std::uint64_t total_free_region_bytes{};
};

ProcessAddressSpaceSnapshot MeasureProcessAddressSpace() noexcept {
    ProcessAddressSpaceSnapshot result{};
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = static_cast<DWORD>(sizeof(counters));
    if (GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
            static_cast<DWORD>(sizeof(counters))) != FALSE) {
        result.private_bytes = static_cast<std::uint64_t>(counters.PrivateUsage);
        result.working_set_bytes = static_cast<std::uint64_t>(counters.WorkingSetSize);
    }

    SYSTEM_INFO system_info{};
    GetSystemInfo(&system_info);
    const std::uintptr_t maximum_address = reinterpret_cast<std::uintptr_t>(
        system_info.lpMaximumApplicationAddress);
    std::uintptr_t address{};
    while (address < maximum_address) {
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(
                reinterpret_cast<const void*>(address),
                &region, sizeof(region)) == 0u) {
            break;
        }
        if (region.State == MEM_FREE) {
            const std::uint64_t region_bytes = static_cast<std::uint64_t>(region.RegionSize);
            result.largest_free_region_bytes = (std::max)(
                result.largest_free_region_bytes, region_bytes);
            if (region_bytes <=
                (std::numeric_limits<std::uint64_t>::max)() -
                    result.total_free_region_bytes) {
                result.total_free_region_bytes += region_bytes;
            }
        }
        const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
        if (region.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - base) {
            break;
        }
        const std::uintptr_t next = base + region.RegionSize;
        if (next <= address) {
            break;
        }
        address = next;
    }
    return result;
}
#endif

bool PlaybackActive(const pbvp::PlaybackState state) noexcept {
    return state == pbvp::PlaybackState::opening ||
           state == pbvp::PlaybackState::buffering ||
           state == pbvp::PlaybackState::playing ||
           state == pbvp::PlaybackState::paused;
}

void LogPlaybackSessionSummary(
    const pbvp::PlaybackControllerSnapshot& snapshot) noexcept {
    if (g_playback_session_summary_logged || g_playback_sequence == 0u) {
        return;
    }
    PBVP_LOG_INFO(
        "Playback session summary: playback=%llu terminal=%s state=%s error=%s failure_site=%s generation=%llu decoded=%llu presented=%llu dropped=%llu stale_video=%llu audio_samples=%llu stale_audio=%llu clock_us=%lld video_end_us=%lld underruns=%llu seeks=%llu forward_seeks=%llu backward_seeks=%llu pauses=%llu resumes=%llu buffering=%llu max_update_gap_ms=%llu staged_peak=%zu decoder_video_peak=%zu decoder_audio_peak=%zu",
        static_cast<unsigned long long>(g_playback_sequence),
        pbvp::PlaybackTerminalReasonName(snapshot.terminal_reason),
        pbvp::PlaybackStateName(snapshot.playback.state),
        pbvp::PlaybackErrorName(snapshot.playback.error),
        pbvp::PlaybackFailureSiteName(snapshot.failure_site),
        static_cast<unsigned long long>(snapshot.generation),
        static_cast<unsigned long long>(snapshot.metrics.decoded_video_frames),
        static_cast<unsigned long long>(snapshot.metrics.presented_video_frames),
        static_cast<unsigned long long>(snapshot.metrics.dropped_video_frames),
        static_cast<unsigned long long>(snapshot.metrics.stale_video_frames),
        static_cast<unsigned long long>(snapshot.metrics.submitted_audio_samples),
        static_cast<unsigned long long>(snapshot.metrics.stale_audio_chunks),
        static_cast<long long>(snapshot.metrics.last_media_time_us),
        static_cast<long long>(snapshot.metrics.last_presented_video_end_us),
        static_cast<unsigned long long>(snapshot.audio.underruns),
        static_cast<unsigned long long>(snapshot.metrics.seek_count),
        static_cast<unsigned long long>(snapshot.metrics.forward_seek_count),
        static_cast<unsigned long long>(snapshot.metrics.backward_seek_count),
        static_cast<unsigned long long>(snapshot.metrics.pause_count),
        static_cast<unsigned long long>(snapshot.metrics.resume_count),
        static_cast<unsigned long long>(snapshot.metrics.buffering_events),
        static_cast<unsigned long long>(snapshot.metrics.maximum_update_gap_ms),
        snapshot.metrics.peak_staged_video_bytes,
        snapshot.metrics.peak_decoder_video_bytes,
        snapshot.metrics.peak_decoder_audio_bytes);
    g_playback_session_summary_logged = true;
}

void StopPlayback(const pbvp::PlaybackTerminalReason reason) noexcept {
    if (g_playback_controller == nullptr) {
        return;
    }
    const pbvp::PlaybackState state = g_playback_controller->Snapshot().playback.state;
    if (state != pbvp::PlaybackState::idle && state != pbvp::PlaybackState::unavailable) {
        g_playback_controller->Stop(reason);
        LogPlaybackSessionSummary(g_playback_controller->Snapshot());
    }
    pbvp::D3dRenderer::Instance().ClearVideoFrame();
}

bool HasUiAction(
    const pbvp::UiInputSnapshot& input,
    const pbvp::UiInputAction action) noexcept {
    return (input.actions & static_cast<std::uint32_t>(action)) != 0u;
}

pbvp::UiVideosMode CurrentUiVideosMode() noexcept {
#if defined(PBVP_ENABLE_PLAYBACK_DIAGNOSTIC)
    return pbvp::UiVideosMode::playback;
#else
    switch (g_videos_page_state) {
        case VideosPageState::data_page: return pbvp::UiVideosMode::data_page;
        case VideosPageState::catalog: return pbvp::UiVideosMode::catalog;
        case VideosPageState::playback: return pbvp::UiVideosMode::playback;
    }
    return pbvp::UiVideosMode::data_page;
#endif
}

void PublishCatalogRows() noexcept {
    std::array<std::wstring, pbvp::kUiCatalogVisibleRows> rows{};
    std::size_t row_count = 0u;
    std::size_t selected_row = 0u;
    try {
        if (g_media_catalog.status != pbvp::MediaCatalogStatus::ok) {
            rows[0] = L"VIDEO FOLDER UNAVAILABLE";
            row_count = 1u;
        } else if (g_media_catalog.entries.empty()) {
            rows[0] = L"NO MP4 FILES FOUND";
            row_count = 1u;
        } else {
            row_count = g_catalog_selection.VisibleCount();
            selected_row = g_catalog_selection.SelectedVisibleRow();
            for (std::size_t row = 0u; row < row_count; ++row) {
                rows[row] = g_media_catalog.entries[
                    g_catalog_selection.FirstVisibleIndex() + row].display_name;
            }
        }
        static_cast<void>(pbvp::UiBridge::Instance().SetCatalogRows(
            rows, row_count, selected_row));
    } catch (...) {
        PBVP_LOG_WARN("Catalog rows could not be prepared for the Pip-Boy UI");
    }
}

void OpenVideosCatalog() noexcept {
    g_media_catalog = pbvp::ScanMediaCatalog(g_media_root, g_settings.catalog);
    g_catalog_selection.Reset(g_media_catalog.entries.size());
    g_videos_page_state = VideosPageState::catalog;
    PBVP_LOG_INFO(
        "Video catalog scan finished: status=%s entries=%zu truncated=%u win32=%lu",
        pbvp::MediaCatalogStatusName(g_media_catalog.status),
        g_media_catalog.entries.size(), g_media_catalog.truncated ? 1u : 0u,
        static_cast<unsigned long>(g_media_catalog.windows_error));
}

void CloseVideosPage() noexcept {
    StopPlayback(pbvp::PlaybackTerminalReason::stopped);
    g_videos_page_state = VideosPageState::data_page;
    g_current_display_title.clear();
}

void ReturnToCatalog() noexcept {
    StopPlayback(pbvp::PlaybackTerminalReason::stopped);
    g_videos_page_state = VideosPageState::catalog;
    g_current_display_title.clear();
}

void OpenSelectedCatalogEntry() noexcept {
    if (g_playback_controller == nullptr ||
        g_media_catalog.status != pbvp::MediaCatalogStatus::ok ||
        g_catalog_selection.SelectedIndex() >= g_media_catalog.entries.size()) {
        return;
    }
    const pbvp::MediaCatalogEntry& entry =
        g_media_catalog.entries[g_catalog_selection.SelectedIndex()];
    g_current_display_title = entry.display_name;
    g_current_title_metadata_checked = false;
    g_current_stream_summary_logged = false;
    g_last_display_second = -1ll;
    g_last_display_duration_second = -1ll;
    std::array<char, pbvp::kMaximumMediaLogNameBytes> media_name{};
    const bool media_name_available = pbvp::FormatPrivacySafeMediaName(
        entry.relative_name, g_settings.logging_detail, media_name);
    ++g_playback_sequence;
    g_playback_session_summary_logged = false;
    if (g_playback_controller->Open(g_media_root, entry.relative_name)) {
        g_videos_page_state = VideosPageState::playback;
        PBVP_LOG_INFO(
            "Playback opened catalog item: session=%llu bytes=%llu media=%s",
            static_cast<unsigned long long>(entry.session_id),
            static_cast<unsigned long long>(entry.file_bytes),
            media_name_available ? media_name.data() : "unavailable");
    } else {
        LogPlaybackSessionSummary(g_playback_controller->Snapshot());
        g_videos_page_state = VideosPageState::playback;
        PBVP_LOG_WARN(
            "Playback rejected catalog item: session=%llu media=%s",
            static_cast<unsigned long long>(entry.session_id),
            media_name_available ? media_name.data() : "unavailable");
    }
}

void SeekRelative(const std::int64_t direction) noexcept {
    if (g_playback_controller == nullptr || direction == 0) {
        return;
    }
    const pbvp::PlaybackControllerSnapshot snapshot =
        g_playback_controller->Snapshot();
    if (!PlaybackActive(snapshot.playback.state)) {
        return;
    }
    const std::int64_t step =
        static_cast<std::int64_t>(g_settings.seek_seconds) * 1'000'000ll;
    std::int64_t target = snapshot.metrics.last_media_time_us;
    if (direction < 0) {
        target = target > step ? target - step : 0ll;
    } else if (target <= (std::numeric_limits<std::int64_t>::max)() - step) {
        target += step;
    }
    static_cast<void>(g_playback_controller->Seek(target));
}

void ProcessVideosInput(const pbvp::UiInputSnapshot& input) noexcept {
    if (g_videos_page_state == VideosPageState::data_page) {
        if (HasUiAction(input, pbvp::UiInputAction::open_page)) {
            OpenVideosCatalog();
        }
        return;
    }

    if (HasUiAction(input, pbvp::UiInputAction::close_page)) {
        if (g_videos_page_state == VideosPageState::playback) {
            ReturnToCatalog();
        } else {
            CloseVideosPage();
        }
        return;
    }

    if (g_videos_page_state == VideosPageState::catalog) {
        if (!g_media_catalog.entries.empty()) {
            const std::uint32_t clicked_row =
                (input.actions & pbvp::kUiCatalogRowMask) >>
                pbvp::kUiCatalogRowShift;
            if ((input.actions & pbvp::kUiCatalogRowMask) != 0u &&
                clicked_row > 0u) {
                static_cast<void>(
                    g_catalog_selection.SelectVisibleRow(clicked_row - 1u));
            }
            if (HasUiAction(input, pbvp::UiInputAction::previous_item)) {
                static_cast<void>(g_catalog_selection.Previous());
            }
            if (HasUiAction(input, pbvp::UiInputAction::next_item)) {
                static_cast<void>(g_catalog_selection.Next());
            }
            if (HasUiAction(input, pbvp::UiInputAction::activate)) {
                OpenSelectedCatalogEntry();
            }
        }
        return;
    }

    const pbvp::PlaybackControllerSnapshot playback =
        g_playback_controller != nullptr
            ? g_playback_controller->Snapshot()
            : pbvp::PlaybackControllerSnapshot{};
    if (HasUiAction(input, pbvp::UiInputAction::pause_resume)) {
        if (playback.playback.state == pbvp::PlaybackState::paused) {
            static_cast<void>(g_playback_controller->Resume());
        } else if (PlaybackActive(playback.playback.state)) {
            static_cast<void>(g_playback_controller->Pause());
        }
    }
    if (HasUiAction(input, pbvp::UiInputAction::stop)) {
        ReturnToCatalog();
        return;
    }
    if (HasUiAction(input, pbvp::UiInputAction::seek_backward)) {
        SeekRelative(-1ll);
    }
    if (HasUiAction(input, pbvp::UiInputAction::seek_forward)) {
        SeekRelative(1ll);
    }
    if (HasUiAction(input, pbvp::UiInputAction::toggle_presentation)) {
        g_settings.tint_mode = g_settings.tint_mode == pbvp::TintMode::pipboy
            ? pbvp::TintMode::full_color
            : pbvp::TintMode::pipboy;
        pbvp::D3dRenderer::Instance().ConfigurePresentation(
            g_settings.aspect_mode, g_settings.tint_mode);
        PBVP_LOG_INFO(
            "Playback tint changed to %s",
            pbvp::TintModeName(g_settings.tint_mode));
    }
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
#if defined(PBVP_ENABLE_PLAYBACK_LONG_TEST)
        const ProcessAddressSpaceSnapshot address_space = MeasureProcessAddressSpace();
        PBVP_LOG_INFO(
            "Integrated playback address space before open: private=%llu working_set=%llu largest_free=%llu total_free=%llu",
            static_cast<unsigned long long>(address_space.private_bytes),
            static_cast<unsigned long long>(address_space.working_set_bytes),
            static_cast<unsigned long long>(address_space.largest_free_region_bytes),
            static_cast<unsigned long long>(address_space.total_free_region_bytes));
#endif
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
#if defined(PBVP_ENABLE_PLAYBACK_DIAGNOSTIC)
            const ProcessAddressSpaceSnapshot address_space = MeasureProcessAddressSpace();
            PBVP_LOG_ERROR(
                "Playback update failed: state=%s error=%s site=%s decoder=%s media_site=%s ffmpeg=%d audio=%s updates=%llu max_update_gap_ms=%llu media_us=%lld decoded=%llu presented=%llu dropped=%llu submitted_chunks=%llu submitted_samples=%llu audio_queued=%u audio_played=%llu underruns=%llu decoder_video_items=%zu decoder_audio_items=%zu private=%llu working_set=%llu largest_free=%llu total_free=%llu",
                pbvp::PlaybackStateName(failed.playback.state),
                pbvp::PlaybackErrorName(failed.playback.error),
                pbvp::PlaybackFailureSiteName(failed.failure_site),
                pbvp::MediaDecodeStatusName(failed.decoder.failure.status),
                pbvp::MediaDecodeFailureSiteName(failed.decoder.failure.site),
                failed.decoder.failure.ffmpeg_error,
                pbvp::XAudioStreamStatusName(failed.audio.status),
                static_cast<unsigned long long>(failed.metrics.update_calls),
                static_cast<unsigned long long>(failed.metrics.maximum_update_gap_ms),
                static_cast<long long>(failed.metrics.last_media_time_us),
                static_cast<unsigned long long>(failed.metrics.decoded_video_frames),
                static_cast<unsigned long long>(failed.metrics.presented_video_frames),
                static_cast<unsigned long long>(failed.metrics.dropped_video_frames),
                static_cast<unsigned long long>(failed.metrics.submitted_audio_chunks),
                static_cast<unsigned long long>(failed.metrics.submitted_audio_samples),
                failed.audio.queued_buffers,
                static_cast<unsigned long long>(failed.audio.samples_played),
                static_cast<unsigned long long>(failed.audio.underruns),
                failed.decoder_buffers.video_items,
                failed.decoder_buffers.audio_items,
                static_cast<unsigned long long>(address_space.private_bytes),
                static_cast<unsigned long long>(address_space.working_set_bytes),
                static_cast<unsigned long long>(address_space.largest_free_region_bytes),
                static_cast<unsigned long long>(address_space.total_free_region_bytes));
#else
            PBVP_LOG_ERROR(
                "Playback update failed: state=%s error=%s site=%s decoder=%s media_site=%s ffmpeg=%d audio=%s updates=%llu media_us=%lld decoded=%llu presented=%llu dropped=%llu submitted_chunks=%llu submitted_samples=%llu audio_queued=%u audio_played=%llu underruns=%llu decoder_video_items=%zu decoder_audio_items=%zu",
                pbvp::PlaybackStateName(failed.playback.state),
                pbvp::PlaybackErrorName(failed.playback.error),
                pbvp::PlaybackFailureSiteName(failed.failure_site),
                pbvp::MediaDecodeStatusName(failed.decoder.failure.status),
                pbvp::MediaDecodeFailureSiteName(failed.decoder.failure.site),
                failed.decoder.failure.ffmpeg_error,
                pbvp::XAudioStreamStatusName(failed.audio.status),
                static_cast<unsigned long long>(failed.metrics.update_calls),
                static_cast<long long>(failed.metrics.last_media_time_us),
                static_cast<unsigned long long>(failed.metrics.decoded_video_frames),
                static_cast<unsigned long long>(failed.metrics.presented_video_frames),
                static_cast<unsigned long long>(failed.metrics.dropped_video_frames),
                static_cast<unsigned long long>(failed.metrics.submitted_audio_chunks),
                static_cast<unsigned long long>(failed.metrics.submitted_audio_samples),
                failed.audio.queued_buffers,
                static_cast<unsigned long long>(failed.audio.samples_played),
                static_cast<unsigned long long>(failed.audio.underruns),
                failed.decoder_buffers.video_items,
                failed.decoder_buffers.audio_items);
#endif
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
    if (!g_current_stream_summary_logged && after.decoder.info.source_width != 0u) {
        const pbvp::MediaInfo& info = after.decoder.info;
        PBVP_LOG_INFO(
            "Playback stream summary: source=%ux%u display=%ux%u rotation=%u duration_us=%lld audio=%u source_audio=%uch@%u output_audio=%uch@%u",
            info.source_width, info.source_height,
            info.display_width, info.display_height,
            info.clockwise_rotation_degrees,
            static_cast<long long>(info.duration_us),
            info.has_audio ? 1u : 0u,
            info.source_audio_channels, info.source_audio_rate,
            info.output_audio_channels, info.output_audio_rate);
        g_current_stream_summary_logged = true;
    }
#if !defined(PBVP_ENABLE_PLAYBACK_DIAGNOSTIC)
    if (!g_current_title_metadata_checked && after.decoder.info.source_width != 0u) {
        g_current_title_metadata_checked = true;
        const std::size_t title_bytes = after.decoder.info.title_utf8_bytes;
        if (title_bytes <= pbvp::kMaximumMediaTitleUtf8Bytes &&
            g_catalog_selection.SelectedIndex() < g_media_catalog.entries.size() &&
            pbvp::ApplyCatalogMetadataTitle(
                g_media_catalog.entries[g_catalog_selection.SelectedIndex()],
                std::string_view(
                    after.decoder.info.title_utf8.data(), title_bytes),
                g_settings.catalog)) {
            g_current_display_title = g_media_catalog.entries[
                g_catalog_selection.SelectedIndex()].display_name;
            g_last_display_second = -1ll;
            g_last_display_duration_second = -1ll;
        }
    }
#endif
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
            "Integrated playback long test progress: clock_us=%lld decoded=%llu presented=%llu dropped=%llu underruns=%llu private_delta=%llu max_update_gap_ms=%llu",
            static_cast<long long>(after.metrics.last_media_time_us),
            static_cast<unsigned long long>(after.metrics.decoded_video_frames),
            static_cast<unsigned long long>(after.metrics.presented_video_frames),
            static_cast<unsigned long long>(after.metrics.dropped_video_frames),
            static_cast<unsigned long long>(after.audio.underruns),
            static_cast<unsigned long long>(g_playback_smoke_peak_private_delta),
            static_cast<unsigned long long>(after.metrics.maximum_update_gap_ms));
        g_playback_smoke_next_progress = GetTickCount64() + 5u * 60u * 1000u;
    }
#endif
#endif
    if (ui_snapshot.visible &&
        !pbvp::UiBridge::Instance().SetPlaybackStatus(
            after.playback) &&
        after.playback.state == pbvp::PlaybackState::error) {
        PBVP_LOG_WARN("The Pip-Boy status text could not display the playback error");
    }
    if (ui_snapshot.visible) {
        const std::int64_t current_second =
            (std::max)(after.metrics.last_media_time_us, 0ll) / 1'000'000ll;
        const std::int64_t duration_second =
            (std::max)(after.decoder.info.duration_us, 0ll) / 1'000'000ll;
        if (current_second != g_last_display_second ||
            duration_second != g_last_display_duration_second) {
            if (pbvp::UiBridge::Instance().SetPlaybackDetails(
                    g_current_display_title,
                    (std::max)(after.metrics.last_media_time_us, 0ll),
                    (std::max)(after.decoder.info.duration_us, 0ll))) {
                g_last_display_second = current_second;
                g_last_display_duration_second = duration_second;
            }
        }
    }
    if (after.playback.state != g_last_playback_state) {
        PBVP_LOG_INFO(
            "Playback state changed: %s -> %s generation=%llu",
            pbvp::PlaybackStateName(g_last_playback_state),
            pbvp::PlaybackStateName(after.playback.state),
            static_cast<unsigned long long>(after.generation));
#if defined(PBVP_ENABLE_PLAYBACK_DIAGNOSTIC)
        if (after.playback.state == pbvp::PlaybackState::buffering) {
            PBVP_LOG_INFO(
                "Playback buffering diagnostic: max_update_gap_ms=%llu audio_queued=%u underruns=%llu decoder_video_items=%zu decoder_audio_items=%zu",
                static_cast<unsigned long long>(after.metrics.maximum_update_gap_ms),
                after.audio.queued_buffers,
                static_cast<unsigned long long>(after.audio.underruns),
                after.decoder_buffers.video_items,
                after.decoder_buffers.audio_items);
        }
#endif
        g_last_playback_state = after.playback.state;
    }
    if (!PlaybackActive(after.playback.state) && PlaybackActive(before.playback.state)) {
        LogPlaybackSessionSummary(after);
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
                static_cast<void>(
                    pbvp::UiBridge::Instance().SetLayerEnabled(g_settings.enabled));
#if defined(PBVP_ENABLE_PLAYBACK_DIAGNOSTIC)
                static_cast<void>(pbvp::UiBridge::Instance().SetVideosMode(
                    pbvp::UiVideosMode::playback));
                pbvp::UiBridge::Instance().UpdateOnGameThread();
#else
                static_cast<void>(pbvp::UiBridge::Instance().SetVideosMode(
                    CurrentUiVideosMode()));
                pbvp::UiBridge::Instance().UpdateInputOnGameThread(
                    g_settings.enabled &&
                    g_videos_page_state != VideosPageState::data_page);
                const pbvp::UiInputSnapshot input =
                    pbvp::UiBridge::Instance().TakeInputSnapshot();
                if (input.map_menu_visible && !input.menu_hook_available) {
                    static_cast<void>(
                        pbvp::UiBridge::Instance().SetLayerEnabled(false));
                }
                if (!input.map_menu_visible &&
                    g_videos_page_state != VideosPageState::data_page) {
                    StopPlayback(pbvp::PlaybackTerminalReason::presentation_hidden);
                    g_videos_page_state = VideosPageState::data_page;
                } else if (g_settings.enabled && input.menu_hook_available) {
                    ProcessVideosInput(input);
                }
                static_cast<void>(pbvp::UiBridge::Instance().SetVideosMode(
                    CurrentUiVideosMode()));
                pbvp::UiBridge::Instance().UpdateOnGameThread();
                if (g_videos_page_state == VideosPageState::catalog) {
                    PublishCatalogRows();
                }
#endif
                static_cast<void>(pbvp::UiBridge::Instance().SetPipBoyTintEnabled(
                    g_settings.tint_mode == pbvp::TintMode::pipboy));
                pbvp::UiRectSnapshot ui_snapshot =
                    pbvp::UiBridge::Instance().ReadForRenderThread();
                if (!g_settings.enabled) {
                    ui_snapshot.visible = false;
                }
                UpdatePlayback(ui_snapshot);
#if !defined(PBVP_ENABLE_PLAYBACK_DIAGNOSTIC)
                if (g_videos_page_state == VideosPageState::playback &&
                    g_playback_controller != nullptr &&
                    g_playback_controller->Snapshot().terminal_reason ==
                        pbvp::PlaybackTerminalReason::completed) {
                    g_videos_page_state = VideosPageState::catalog;
                    g_current_display_title.clear();
                    static_cast<void>(pbvp::UiBridge::Instance().SetVideosMode(
                        pbvp::UiVideosMode::catalog));
                    PublishCatalogRows();
                }
#endif
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
            g_videos_page_state = VideosPageState::data_page;
            pbvp::UiBridge::Instance().SetGameInputState(0u);
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
            if (message->data != nullptr && message->dataLen ==
                    sizeof(kPluginName) &&
                std::memcmp(message->data, kPluginName,
                            sizeof(kPluginName)) == 0) {
                ReloadConfiguration();
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
    g_configuration_path = PrivateConfigurationPath(nvse->GetRuntimeDirectory());
    g_media_root = PrivateMediaDirectory(nvse->GetRuntimeDirectory());
    const pbvp::ConfigurationResult configuration =
        pbvp::LoadConfiguration(g_configuration_path);
    if (configuration.status == pbvp::ConfigurationStatus::ok) {
        g_settings = configuration.settings;
    }
    pbvp::D3dRenderer::Instance().ConfigurePresentation(
        g_settings.aspect_mode, g_settings.tint_mode);
    if (!pbvp::UiBridge::Instance().SetInputBindings(g_settings.input)) {
        PBVP_LOG_ERROR("Configured input bindings failed validation");
        g_ffmpeg_runtime.Unload();
        return false;
    }
    LogConfigurationResult(configuration);
#if defined(PBVP_ENABLE_MEDIA_SMOKE_TEST)
    g_media_smoke_root = g_media_root;
#endif
#if defined(PBVP_ENABLE_AUDIO_SMOKE_TEST)
    g_audio_smoke_root = g_media_root;
#endif
#if defined(PBVP_ENABLE_PLAYBACK_DIAGNOSTIC)
    g_playback_smoke_root = g_media_root;
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
    if (!CreatePlaybackController(g_settings)) {
        PBVP_LOG_ERROR("Playback controller allocation failed");
        g_ffmpeg_runtime.Unload();
        return false;
    }
    auto* data = static_cast<NVSEDataInterface*>(
        nvse->QueryInterface(kInterface_Data));
    void* game_input_state = nullptr;
    if (data != nullptr && data->version >= NVSEDataInterface::kVersion &&
        data->GetSingleton != nullptr) {
        game_input_state = data->GetSingleton(
            NVSEDataInterface::kNVSEData_DIHookControl);
    }
    if (game_input_state == nullptr) {
        PBVP_LOG_ERROR("Required xNVSE game input state is unavailable");
        g_playback_controller->Shutdown();
        g_playback_controller.reset();
        g_ffmpeg_runtime.Unload();
        return false;
    }
    pbvp::UiBridge::Instance().SetGameInputState(
        reinterpret_cast<std::uintptr_t>(game_input_state));
    PBVP_LOG_INFO("xNVSE filtered game input state accepted for scoped Videos controls");
    g_messaging = static_cast<NVSEMessagingInterface*>(nvse->QueryInterface(kInterface_Messaging));
    if (g_messaging == nullptr || g_messaging->version < NVSEMessagingInterface::kVersion ||
        g_messaging->RegisterListener == nullptr ||
        !g_messaging->RegisterListener(g_plugin_handle, "NVSE", &HandleMessage)) {
        PBVP_LOG_ERROR("Required xNVSE messaging interface is unavailable");
        pbvp::UiBridge::Instance().SetGameInputState(0u);
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
