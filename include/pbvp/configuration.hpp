#pragma once

#include "pbvp/media_catalog.hpp"
#include "pbvp/playback_state.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace pbvp {

constexpr std::uint64_t kMaximumConfiguredMediaBytes =
    32ull * 1024ull * 1024ull * 1024ull;
constexpr std::size_t kMaximumConfigurationBytes = 64u * 1024u;

enum class AspectMode : std::uint32_t {
    fit,
    fill,
};

enum class TintMode : std::uint32_t {
    pipboy,
    full_color,
};

enum class LoggingDetail : std::uint32_t {
    normal,
    diagnostic,
};

struct ResourceSettings final {
    std::uint32_t maximum_source_width{1920u};
    std::uint32_t maximum_source_height{1080u};
    std::uint32_t maximum_queued_video_edge{512u};
    std::uint64_t maximum_media_file_bytes{kMaximumConfiguredMediaBytes};
};

struct InputSettings final {
    std::uint32_t select_or_play{0x1Cu};
    std::uint32_t pause_resume{0x39u};
    std::uint32_t back_or_stop{0x01u};
    std::uint32_t seek_backward{0xCBu};
    std::uint32_t seek_forward{0xCDu};
    std::uint32_t previous_item{0xC8u};
    std::uint32_t next_item{0xD0u};
    std::uint32_t toggle_color{0x14u};
};

struct PlayerSettings final {
    bool enabled{true};
    AspectMode aspect_mode{AspectMode::fit};
    TintMode tint_mode{TintMode::pipboy};
    LoggingDetail logging_detail{LoggingDetail::normal};
    float volume{1.0f};
    bool muted{};
    std::uint32_t seek_seconds{10u};
    MediaCatalogConfig catalog{};
    ResourceSettings resources{};
    InputSettings input{};
};

enum class ConfigurationStatus : std::uint32_t {
    ok,
    path_not_absolute,
    file_missing,
    file_not_regular,
    file_reparse_point,
    file_too_large,
    file_open_failed,
    file_read_failed,
    invalid_utf8,
    allocation_failed,
    unexpected_failure,
};

struct ConfigurationResult final {
    ConfigurationStatus status{ConfigurationStatus::ok};
    PlayerSettings settings{};
    std::uint32_t windows_error{};
    std::uint32_t unknown_settings{};
    std::uint32_t invalid_settings{};
    std::uint32_t malformed_lines{};
};

[[nodiscard]] const char* ConfigurationStatusName(ConfigurationStatus status) noexcept;
[[nodiscard]] const char* AspectModeName(AspectMode mode) noexcept;
[[nodiscard]] const char* TintModeName(TintMode mode) noexcept;
[[nodiscard]] const char* LoggingDetailName(LoggingDetail detail) noexcept;
[[nodiscard]] bool InputSettingsValid(const InputSettings& settings) noexcept;
[[nodiscard]] bool ConfigurationReloadAllowed(PlaybackState state) noexcept;
[[nodiscard]] ConfigurationResult LoadConfiguration(
    const std::wstring& configuration_path) noexcept;

} // namespace pbvp
