#include "pbvp/media_catalog.hpp"

#include "test_support.hpp"

#include <Windows.h>

#include <filesystem>
#include <string>
#include <vector>

namespace {

class TemporaryCatalog final {
public:
    TemporaryCatalog() {
        wchar_t temporary[MAX_PATH]{};
        wchar_t candidate[MAX_PATH]{};
        if (GetTempPathW(MAX_PATH, temporary) == 0u ||
            GetTempFileNameW(temporary, L"pbc", 0u, candidate) == 0u) {
            return;
        }
        DeleteFileW(candidate);
        if (CreateDirectoryW(candidate, nullptr) == FALSE) {
            return;
        }
        root_ = candidate;
    }

    ~TemporaryCatalog() {
        if (!root_.empty()) {
            std::error_code ignored;
            std::filesystem::remove_all(root_, ignored);
        }
    }

    const std::wstring& Root() const noexcept { return root_; }

    bool AddFile(const std::wstring& relative_name, const DWORD bytes = 1u) const {
        if (root_.empty()) {
            return false;
        }
        const std::wstring path = root_ + L"\\" + relative_name;
        HANDLE file = CreateFileW(
            path.c_str(), GENERIC_WRITE, 0u, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            return false;
        }
        DWORD written = 0u;
        std::vector<unsigned char> payload(bytes, 0x5Au);
        const bool succeeded = bytes == 0u ||
            (WriteFile(file, payload.data(), bytes, &written, nullptr) != FALSE &&
             written == bytes);
        CloseHandle(file);
        return succeeded;
    }

    bool AddDirectory(const std::wstring& relative_name) const {
        return !root_.empty() &&
            CreateDirectoryW((root_ + L"\\" + relative_name).c_str(), nullptr) != FALSE;
    }

private:
    std::wstring root_{};
};

void CheckCatalogSizes() {
    TemporaryCatalog empty;
    PBVP_CHECK(!empty.Root().empty());
    pbvp::MediaCatalogResult result = pbvp::ScanMediaCatalog(empty.Root());
    PBVP_CHECK(result.status == pbvp::MediaCatalogStatus::ok);
    PBVP_CHECK(result.entries.empty());
    PBVP_CHECK(!result.truncated);

    for (std::size_t index = 1u; index <= 500u; ++index) {
        wchar_t name[32]{};
        swprintf_s(name, L"Episode %03zu.mp4", index);
        PBVP_CHECK(empty.AddFile(name));
        if (index == 1u || index == 10u || index == 100u) {
            result = pbvp::ScanMediaCatalog(empty.Root());
            PBVP_CHECK(result.status == pbvp::MediaCatalogStatus::ok);
            PBVP_CHECK(result.entries.size() == index);
            PBVP_CHECK(!result.truncated);
        }
    }
    result = pbvp::ScanMediaCatalog(empty.Root());
    PBVP_CHECK(result.status == pbvp::MediaCatalogStatus::ok);
    PBVP_CHECK(result.entries.size() == 500u);
    PBVP_CHECK(!result.truncated);

    PBVP_CHECK(empty.AddFile(L"Episode 501.mp4"));
    result = pbvp::ScanMediaCatalog(empty.Root());
    PBVP_CHECK(result.status == pbvp::MediaCatalogStatus::ok);
    PBVP_CHECK(result.entries.size() == 500u);
    PBVP_CHECK(result.truncated);
}

void CheckNamesAndNaturalOrder() {
    TemporaryCatalog catalog;
    PBVP_CHECK(catalog.AddFile(L"Episode 10.mp4", 10u));
    PBVP_CHECK(catalog.AddFile(L"Episode 2.MP4", 2u));
    PBVP_CHECK(catalog.AddFile(L"Episode 1.mp4", 1u));
    PBVP_CHECK(catalog.AddFile(L"Courier's Cut.mp4", 3u));
    PBVP_CHECK(catalog.AddFile(L"日本語.mp4", 4u));
    PBVP_CHECK(catalog.AddFile(L"Cafe\x0301.mp4", 5u));
    PBVP_CHECK(catalog.AddFile(L"ignore.txt", 6u));
    PBVP_CHECK(catalog.AddDirectory(L"nested.mp4"));
    PBVP_CHECK(catalog.AddDirectory(L"subfolder"));

    const pbvp::MediaCatalogResult result = pbvp::ScanMediaCatalog(catalog.Root());
    PBVP_CHECK(result.status == pbvp::MediaCatalogStatus::ok);
    PBVP_CHECK(result.entries.size() == 6u);

    std::vector<std::wstring> episodes;
    bool found_japanese = false;
    bool found_combining = false;
    for (const auto& entry : result.entries) {
        PBVP_CHECK(entry.relative_name.find(L'\\') == std::wstring::npos);
        PBVP_CHECK(entry.session_id != 0u);
        if (entry.display_name.rfind(L"Episode", 0u) == 0u) {
            episodes.push_back(entry.display_name);
        }
        if (entry.relative_name == L"\u65E5\u672C\u8A9E.mp4") {
            PBVP_CHECK(entry.display_name == L"\u65E5\u672C\u8A9E");
            found_japanese = true;
        }
        if (entry.relative_name == L"Cafe\u0301.mp4") {
            PBVP_CHECK(entry.display_name == L"Cafe\u0301");
            found_combining = true;
        }
    }
    PBVP_CHECK(found_japanese);
    PBVP_CHECK(found_combining);
    PBVP_CHECK(episodes.size() == 3u);
    if (episodes.size() == 3u) {
        PBVP_CHECK(episodes[0] == L"Episode 1");
        PBVP_CHECK(episodes[1] == L"Episode 2");
        PBVP_CHECK(episodes[2] == L"Episode 10");
    }

    const pbvp::MediaCatalogResult repeated = pbvp::ScanMediaCatalog(catalog.Root());
    PBVP_CHECK(repeated.entries.size() == result.entries.size());
    if (repeated.entries.size() == result.entries.size()) {
        for (std::size_t index = 0u; index < result.entries.size(); ++index) {
            PBVP_CHECK(repeated.entries[index].relative_name == result.entries[index].relative_name);
            PBVP_CHECK(repeated.entries[index].session_id == result.entries[index].session_id);
        }
    }

    pbvp::MediaCatalogEntry metadata_entry{};
    metadata_entry.display_name = L"Filename";
    PBVP_CHECK(pbvp::ApplyCatalogMetadataTitle(
        metadata_entry, "Courier \xE2\x80\x93 Test", {}));
    PBVP_CHECK(metadata_entry.display_name == L"Courier \x2013 Test");
    PBVP_CHECK(pbvp::ApplyCatalogMetadataTitle(
        metadata_entry, "  Trimmed title  ", {}));
    PBVP_CHECK(metadata_entry.display_name == L"Trimmed title");
    PBVP_CHECK(!pbvp::ApplyCatalogMetadataTitle(
        metadata_entry, std::string_view("\xC3\x28", 2u), {}));
    PBVP_CHECK(!pbvp::ApplyCatalogMetadataTitle(
        metadata_entry, "Line\nBreak", {}));
    PBVP_CHECK(!pbvp::ApplyCatalogMetadataTitle(
        metadata_entry, "   ", {}));
    PBVP_CHECK(!pbvp::ApplyCatalogMetadataTitle(
        metadata_entry, std::string(2049u, 'A'), {}));
    pbvp::MediaCatalogConfig short_title{};
    short_title.maximum_display_characters = 4u;
    PBVP_CHECK(!pbvp::ApplyCatalogMetadataTitle(
        metadata_entry, "Title", short_title));
}

void CheckDisplayAndInputLimits() {
    TemporaryCatalog catalog;
    const std::wstring long_stem(180u, L'X');
    PBVP_CHECK(catalog.AddFile(long_stem + L".mp4"));

    pbvp::MediaCatalogConfig clipped{};
    clipped.maximum_display_characters = 32u;
    pbvp::MediaCatalogResult result = pbvp::ScanMediaCatalog(catalog.Root(), clipped);
    PBVP_CHECK(result.status == pbvp::MediaCatalogStatus::ok);
    PBVP_CHECK(result.entries.size() == 1u);
    if (result.entries.size() == 1u) {
        PBVP_CHECK(result.entries[0].display_name.size() == 32u);
        PBVP_CHECK(result.entries[0].display_name.substr(29u) == L"...");
        PBVP_CHECK(result.entries[0].relative_name == long_stem + L".mp4");
    }

    clipped.maximum_entries = 0u;
    result = pbvp::ScanMediaCatalog(catalog.Root(), clipped);
    PBVP_CHECK(result.status == pbvp::MediaCatalogStatus::invalid_configuration);
    result = pbvp::ScanMediaCatalog(L"relative\\Videos");
    PBVP_CHECK(result.status == pbvp::MediaCatalogStatus::root_not_absolute);
    result = pbvp::ScanMediaCatalog(catalog.Root() + L"\\missing");
    PBVP_CHECK(result.status == pbvp::MediaCatalogStatus::root_missing);

    const std::wstring ordinary_file = catalog.Root() + L"\\ordinary.bin";
    HANDLE handle = CreateFileW(
        ordinary_file.c_str(), GENERIC_WRITE, 0u, nullptr, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    PBVP_CHECK(handle != INVALID_HANDLE_VALUE);
    if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
        result = pbvp::ScanMediaCatalog(ordinary_file);
        PBVP_CHECK(result.status == pbvp::MediaCatalogStatus::root_not_directory);
    }
}

} // namespace

void RunMediaCatalogTests() {
    CheckCatalogSizes();
    CheckNamesAndNaturalOrder();
    CheckDisplayAndInputLimits();

    {
        pbvp::MediaCatalogSelection selection(8u);
        selection.Reset(10u);
        PBVP_CHECK(selection.EntryCount() == 10u);
        PBVP_CHECK(selection.SelectedIndex() == 0u);
        PBVP_CHECK(selection.FirstVisibleIndex() == 0u);
        PBVP_CHECK(selection.VisibleCount() == 8u);
        PBVP_CHECK(!selection.Previous());
        for (std::size_t index = 0u; index < 8u; ++index) {
            PBVP_CHECK(selection.Next());
        }
        PBVP_CHECK(selection.SelectedIndex() == 8u);
        PBVP_CHECK(selection.FirstVisibleIndex() == 1u);
        PBVP_CHECK(selection.SelectedVisibleRow() == 7u);
        PBVP_CHECK(selection.SelectVisibleRow(0u));
        PBVP_CHECK(selection.SelectedIndex() == 1u);
        PBVP_CHECK(!selection.SelectVisibleRow(8u));
        while (selection.Next()) {
        }
        PBVP_CHECK(selection.SelectedIndex() == 9u);
        PBVP_CHECK(selection.FirstVisibleIndex() == 2u);
        PBVP_CHECK(selection.VisibleCount() == 8u);
        PBVP_CHECK(!selection.Next());

        selection.Reset(0u);
        PBVP_CHECK(selection.VisibleCount() == 0u);
        PBVP_CHECK(!selection.Next());
        PBVP_CHECK(!selection.SelectVisibleRow(0u));
    }

    {
        pbvp::MediaCatalogSelection disabled(0u);
        disabled.Reset(5u);
        PBVP_CHECK(disabled.VisibleCount() == 0u);
        PBVP_CHECK(!disabled.Next());
        PBVP_CHECK(!disabled.Previous());
    }
}
