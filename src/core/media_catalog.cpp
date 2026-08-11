#include "pbvp/media_catalog.hpp"

#include <Windows.h>

#include <algorithm>
#include <cwctype>
#include <limits>
#include <new>
#include <string_view>

namespace pbvp {
namespace {

constexpr std::uint64_t kFnvOffset = 14'695'981'039'346'656'037ull;
constexpr std::uint64_t kFnvPrime = 1'099'511'628'211ull;

bool IsAbsoluteLocalPath(const std::wstring& path) noexcept {
    return path.size() >= 3u && std::iswalpha(path[0]) != 0 && path[1] == L':' &&
           (path[2] == L'\\' || path[2] == L'/');
}

bool IsMp4Name(const std::wstring_view name) noexcept {
    if (name.size() < 5u) {
        return false;
    }
    const std::wstring_view extension = name.substr(name.size() - 4u);
    return extension[0] == L'.' &&
           std::towlower(extension[1]) == L'm' &&
           std::towlower(extension[2]) == L'p' &&
           extension[3] == L'4';
}

std::wstring DisplayNameFor(
    const std::wstring& relative_name,
    const std::size_t maximum_characters) {
    std::wstring display = relative_name.substr(0u, relative_name.size() - 4u);
    if (display.size() <= maximum_characters) {
        return display;
    }
    if (maximum_characters <= 3u) {
        display.resize(maximum_characters);
        return display;
    }
    display.resize(maximum_characters - 3u);
    if (!display.empty() && display.back() >= 0xD800 && display.back() <= 0xDBFF) {
        display.pop_back();
    }
    display.append(L"...");
    return display;
}

std::uint64_t StableSessionId(
    const std::wstring_view relative_name,
    const std::uint64_t file_bytes) noexcept {
    std::uint64_t value = kFnvOffset;
    for (const wchar_t code_unit : relative_name) {
        const std::uint32_t folded = static_cast<std::uint32_t>(std::towlower(code_unit));
        value ^= folded & 0xFFu;
        value *= kFnvPrime;
        value ^= (folded >> 8u) & 0xFFu;
        value *= kFnvPrime;
    }
    std::uint64_t remaining = file_bytes;
    for (std::size_t byte = 0u; byte < sizeof(remaining); ++byte) {
        value ^= remaining & 0xFFu;
        value *= kFnvPrime;
        remaining >>= 8u;
    }
    return value == 0u ? 1u : value;
}

int NaturalCompare(const std::wstring& left, const std::wstring& right) noexcept {
    if (left.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()) ||
        right.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return left.compare(right);
    }
    const int compared = CompareStringEx(
        LOCALE_NAME_INVARIANT,
        NORM_IGNORECASE | SORT_DIGITSASNUMBERS,
        left.data(), static_cast<int>(left.size()),
        right.data(), static_cast<int>(right.size()),
        nullptr, nullptr, 0);
    if (compared == CSTR_LESS_THAN) {
        return -1;
    }
    if (compared == CSTR_GREATER_THAN) {
        return 1;
    }
    if (compared == CSTR_EQUAL) {
        return 0;
    }
    return left.compare(right);
}

HANDLE BeginEnumeration(const std::wstring& pattern, WIN32_FIND_DATAW& found) noexcept {
    HANDLE search = FindFirstFileExW(
        pattern.c_str(), FindExInfoBasic, &found, FindExSearchNameMatch,
        nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (search != INVALID_HANDLE_VALUE) {
        return search;
    }
    const DWORD first_error = GetLastError();
    if (first_error != ERROR_INVALID_PARAMETER && first_error != ERROR_NOT_SUPPORTED) {
        return INVALID_HANDLE_VALUE;
    }
    return FindFirstFileExW(
        pattern.c_str(), FindExInfoBasic, &found, FindExSearchNameMatch,
        nullptr, 0u);
}

} // namespace

const char* MediaCatalogStatusName(const MediaCatalogStatus status) noexcept {
    switch (status) {
        case MediaCatalogStatus::ok: return "ok";
        case MediaCatalogStatus::invalid_configuration: return "invalid configuration";
        case MediaCatalogStatus::root_not_absolute: return "media root is not absolute";
        case MediaCatalogStatus::root_missing: return "media root is missing";
        case MediaCatalogStatus::root_not_directory: return "media root is not a directory";
        case MediaCatalogStatus::root_reparse_point: return "media root is a reparse point";
        case MediaCatalogStatus::enumeration_failed: return "directory enumeration failed";
        case MediaCatalogStatus::allocation_failed: return "catalog allocation failed";
        case MediaCatalogStatus::unexpected_failure: return "unexpected catalog failure";
    }
    return "unknown catalog failure";
}

bool NaturalCatalogLess(
    const MediaCatalogEntry& left,
    const MediaCatalogEntry& right) noexcept {
    int compared = NaturalCompare(left.display_name, right.display_name);
    if (compared != 0) {
        return compared < 0;
    }
    compared = NaturalCompare(left.relative_name, right.relative_name);
    if (compared != 0) {
        return compared < 0;
    }
    return left.session_id < right.session_id;
}

MediaCatalogResult ScanMediaCatalog(
    const std::wstring& media_root,
    const MediaCatalogConfig& config) noexcept {
    MediaCatalogResult result{};
    if (config.maximum_entries == 0u || config.maximum_entries > kMaximumCatalogEntries ||
        config.maximum_display_characters == 0u ||
        config.maximum_display_characters > kMaximumCatalogDisplayCharacters) {
        result.status = MediaCatalogStatus::invalid_configuration;
        return result;
    }
    if (!IsAbsoluteLocalPath(media_root)) {
        result.status = MediaCatalogStatus::root_not_absolute;
        return result;
    }

    const DWORD root_attributes = GetFileAttributesW(media_root.c_str());
    if (root_attributes == INVALID_FILE_ATTRIBUTES) {
        result.windows_error = GetLastError();
        result.status = result.windows_error == ERROR_FILE_NOT_FOUND ||
                result.windows_error == ERROR_PATH_NOT_FOUND
            ? MediaCatalogStatus::root_missing
            : MediaCatalogStatus::enumeration_failed;
        return result;
    }
    if ((root_attributes & FILE_ATTRIBUTE_DIRECTORY) == 0u) {
        result.status = MediaCatalogStatus::root_not_directory;
        return result;
    }
    if ((root_attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u) {
        result.status = MediaCatalogStatus::root_reparse_point;
        return result;
    }

    try {
        std::wstring pattern = media_root;
        if (pattern.back() != L'\\' && pattern.back() != L'/') {
            pattern.push_back(L'\\');
        }
        pattern.push_back(L'*');

        WIN32_FIND_DATAW found{};
        HANDLE search = BeginEnumeration(pattern, found);
        if (search == INVALID_HANDLE_VALUE) {
            result.windows_error = GetLastError();
            if (result.windows_error == ERROR_FILE_NOT_FOUND) {
                return result;
            }
            result.status = MediaCatalogStatus::enumeration_failed;
            return result;
        }

        result.entries.reserve(config.maximum_entries);
        for (;;) {
            const DWORD rejected_attributes = FILE_ATTRIBUTE_DIRECTORY |
                FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_REPARSE_POINT;
            const std::wstring relative_name(found.cFileName);
            if ((found.dwFileAttributes & rejected_attributes) == 0u &&
                IsMp4Name(relative_name)) {
                if (result.entries.size() == config.maximum_entries) {
                    result.truncated = true;
                    FindClose(search);
                    break;
                }
                const std::uint64_t file_bytes =
                    (static_cast<std::uint64_t>(found.nFileSizeHigh) << 32u) |
                    static_cast<std::uint64_t>(found.nFileSizeLow);
                MediaCatalogEntry entry{};
                entry.relative_name = relative_name;
                entry.display_name = DisplayNameFor(
                    relative_name, config.maximum_display_characters);
                entry.file_bytes = file_bytes;
                entry.session_id = StableSessionId(relative_name, file_bytes);
                result.entries.push_back(std::move(entry));
            }
            if (FindNextFileW(search, &found) == FALSE) {
                result.windows_error = GetLastError();
                FindClose(search);
                if (result.windows_error != ERROR_NO_MORE_FILES) {
                    result.entries.clear();
                    result.status = MediaCatalogStatus::enumeration_failed;
                    return result;
                }
                result.windows_error = ERROR_SUCCESS;
                break;
            }
        }
        std::sort(result.entries.begin(), result.entries.end(), NaturalCatalogLess);
        return result;
    } catch (const std::bad_alloc&) {
        result.entries.clear();
        result.status = MediaCatalogStatus::allocation_failed;
        return result;
    } catch (...) {
        result.entries.clear();
        result.status = MediaCatalogStatus::unexpected_failure;
        return result;
    }
}

MediaCatalogSelection::MediaCatalogSelection(
    const std::size_t visible_rows) noexcept
    : visible_rows_(visible_rows) {
}

void MediaCatalogSelection::Reset(const std::size_t entry_count) noexcept {
    entry_count_ = entry_count;
    selected_index_ = 0u;
    first_visible_index_ = 0u;
}

bool MediaCatalogSelection::Previous() noexcept {
    if (selected_index_ == 0u || entry_count_ == 0u || visible_rows_ == 0u) {
        return false;
    }
    --selected_index_;
    KeepVisible();
    return true;
}

bool MediaCatalogSelection::Next() noexcept {
    if (visible_rows_ == 0u || selected_index_ + 1u >= entry_count_) {
        return false;
    }
    ++selected_index_;
    KeepVisible();
    return true;
}

bool MediaCatalogSelection::SelectVisibleRow(const std::size_t row) noexcept {
    if (visible_rows_ == 0u || row >= visible_rows_ ||
        first_visible_index_ + row >= entry_count_) {
        return false;
    }
    selected_index_ = first_visible_index_ + row;
    KeepVisible();
    return true;
}

std::size_t MediaCatalogSelection::EntryCount() const noexcept {
    return entry_count_;
}

std::size_t MediaCatalogSelection::SelectedIndex() const noexcept {
    return selected_index_;
}

std::size_t MediaCatalogSelection::FirstVisibleIndex() const noexcept {
    return first_visible_index_;
}

std::size_t MediaCatalogSelection::VisibleCount() const noexcept {
    if (visible_rows_ == 0u || first_visible_index_ >= entry_count_) {
        return 0u;
    }
    return (std::min)(visible_rows_, entry_count_ - first_visible_index_);
}

std::size_t MediaCatalogSelection::SelectedVisibleRow() const noexcept {
    return selected_index_ >= first_visible_index_
        ? selected_index_ - first_visible_index_
        : 0u;
}

void MediaCatalogSelection::KeepVisible() noexcept {
    if (entry_count_ == 0u || visible_rows_ == 0u) {
        selected_index_ = 0u;
        first_visible_index_ = 0u;
        return;
    }
    selected_index_ = (std::min)(selected_index_, entry_count_ - 1u);
    if (selected_index_ < first_visible_index_) {
        first_visible_index_ = selected_index_;
    } else if (selected_index_ >= first_visible_index_ + visible_rows_) {
        first_visible_index_ = selected_index_ - visible_rows_ + 1u;
    }
}

} // namespace pbvp
