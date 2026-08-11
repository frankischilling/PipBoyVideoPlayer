#pragma once

#include "pbvp/media_limits.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace pbvp {

constexpr std::size_t kMaximumCatalogEntries = 500u;
constexpr std::size_t kMaximumCatalogDisplayCharacters = 512u;

struct MediaCatalogConfig final {
    std::size_t maximum_entries{kMaximumCatalogEntries};
    std::size_t maximum_display_characters{128u};
};

struct MediaCatalogEntry final {
    std::wstring relative_name{};
    std::wstring display_name{};
    std::uint64_t file_bytes{};
    std::uint64_t session_id{};
};

enum class MediaCatalogStatus : std::uint32_t {
    ok,
    invalid_configuration,
    root_not_absolute,
    root_missing,
    root_not_directory,
    root_reparse_point,
    enumeration_failed,
    allocation_failed,
    unexpected_failure,
};

struct MediaCatalogResult final {
    MediaCatalogStatus status{MediaCatalogStatus::ok};
    std::vector<MediaCatalogEntry> entries{};
    std::uint32_t windows_error{};
    bool truncated{};
};

class MediaCatalogSelection final {
public:
    explicit MediaCatalogSelection(std::size_t visible_rows) noexcept;

    void Reset(std::size_t entry_count) noexcept;
    [[nodiscard]] bool Previous() noexcept;
    [[nodiscard]] bool Next() noexcept;
    [[nodiscard]] bool SelectVisibleRow(std::size_t row) noexcept;

    [[nodiscard]] std::size_t EntryCount() const noexcept;
    [[nodiscard]] std::size_t SelectedIndex() const noexcept;
    [[nodiscard]] std::size_t FirstVisibleIndex() const noexcept;
    [[nodiscard]] std::size_t VisibleCount() const noexcept;
    [[nodiscard]] std::size_t SelectedVisibleRow() const noexcept;

private:
    void KeepVisible() noexcept;

    std::size_t visible_rows_{};
    std::size_t entry_count_{};
    std::size_t selected_index_{};
    std::size_t first_visible_index_{};
};

[[nodiscard]] const char* MediaCatalogStatusName(MediaCatalogStatus status) noexcept;
[[nodiscard]] bool NaturalCatalogLess(
    const MediaCatalogEntry& left,
    const MediaCatalogEntry& right) noexcept;
[[nodiscard]] bool ApplyCatalogMetadataTitle(
    MediaCatalogEntry& entry,
    std::string_view title_utf8,
    const MediaCatalogConfig& config = {}) noexcept;
[[nodiscard]] MediaCatalogResult ScanMediaCatalog(
    const std::wstring& media_root,
    const MediaCatalogConfig& config = {}) noexcept;

} // namespace pbvp
