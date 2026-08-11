#pragma once

#include "pbvp/media_catalog.hpp"

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
[[nodiscard]] ConfigurationResult LoadConfiguration(
    const std::wstring& configuration_path) noexcept;

} // namespace pbvp
