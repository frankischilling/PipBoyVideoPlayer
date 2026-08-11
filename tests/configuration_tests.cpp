#include "pbvp/configuration.hpp"

#include "test_support.hpp"

#include <Windows.h>

#include <filesystem>
#include <string>
#include <vector>

namespace {

class TemporaryConfiguration final {
public:
    TemporaryConfiguration() {
        wchar_t temporary[MAX_PATH]{};
        wchar_t candidate[MAX_PATH]{};
        if (GetTempPathW(MAX_PATH, temporary) == 0u ||
            GetTempFileNameW(temporary, L"pbc", 0u, candidate) == 0u) {
            return;
        }
        path_ = candidate;
    }

    ~TemporaryConfiguration() {
        if (!path_.empty()) {
            std::error_code ignored;
            std::filesystem::remove(path_, ignored);
        }
    }

    const std::wstring& Path() const noexcept { return path_; }

    bool Write(const std::vector<unsigned char>& bytes) const {
        HANDLE file = CreateFileW(
            path_.c_str(), GENERIC_WRITE, 0u, nullptr, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            return false;
        }
        DWORD written = 0u;
        const bool succeeded = bytes.empty() ||
            (WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()),
                       &written, nullptr) != FALSE &&
             written == static_cast<DWORD>(bytes.size()));
        CloseHandle(file);
        return succeeded;
    }

    bool Write(const std::string& text) const {
        return Write(std::vector<unsigned char>(text.begin(), text.end()));
    }

private:
    std::wstring path_{};
};

void CheckDefaultsAndValidSettings() {
    TemporaryConfiguration file;
    PBVP_CHECK(file.Write("; defaults\r\n"));
    pbvp::ConfigurationResult result = pbvp::LoadConfiguration(file.Path());
    PBVP_CHECK(result.status == pbvp::ConfigurationStatus::ok);
    PBVP_CHECK(result.settings.enabled);
    PBVP_CHECK(result.settings.aspect_mode == pbvp::AspectMode::fit);
    PBVP_CHECK(result.settings.tint_mode == pbvp::TintMode::pipboy);
    PBVP_CHECK(result.settings.volume == 1.0f);
    PBVP_CHECK(result.settings.catalog.maximum_entries == 500u);
    PBVP_CHECK(result.unknown_settings == 0u);
    PBVP_CHECK(result.invalid_settings == 0u);
    PBVP_CHECK(result.malformed_lines == 0u);

    const std::string valid =
        "\xEF\xBB\xBF[General]\r\nEnabled=off\r\n"
        "[Rendering]\r\nAspectMode=Fill\r\nTintMode=FullColor\r\n"
        "[Playback]\r\nVolume=0.25\r\nMuted=yes\r\nSeekSeconds=30\r\n"
        "[Catalog]\r\nMaximumEntries=100\r\nMaximumDisplayCharacters=256\r\n"
        "[Resources]\r\nMaximumSourceWidth=1280\r\nMaximumSourceHeight=720\r\n"
        "MaximumQueuedVideoEdge=384\r\nMaximumMediaFileMiB=4096\r\n"
        "[Logging]\r\nDetail=Diagnostic\r\n";
    PBVP_CHECK(file.Write(valid));
    result = pbvp::LoadConfiguration(file.Path());
    PBVP_CHECK(result.status == pbvp::ConfigurationStatus::ok);
    PBVP_CHECK(!result.settings.enabled);
    PBVP_CHECK(result.settings.aspect_mode == pbvp::AspectMode::fill);
    PBVP_CHECK(result.settings.tint_mode == pbvp::TintMode::full_color);
    PBVP_CHECK(result.settings.volume == 0.25f);
    PBVP_CHECK(result.settings.muted);
    PBVP_CHECK(result.settings.seek_seconds == 30u);
    PBVP_CHECK(result.settings.catalog.maximum_entries == 100u);
    PBVP_CHECK(result.settings.catalog.maximum_display_characters == 256u);
    PBVP_CHECK(result.settings.resources.maximum_source_width == 1280u);
    PBVP_CHECK(result.settings.resources.maximum_source_height == 720u);
    PBVP_CHECK(result.settings.resources.maximum_queued_video_edge == 384u);
    PBVP_CHECK(result.settings.resources.maximum_media_file_bytes ==
               4096ull * 1024ull * 1024ull);
    PBVP_CHECK(result.settings.logging_detail == pbvp::LoggingDetail::diagnostic);
    PBVP_CHECK(result.unknown_settings == 0u);
    PBVP_CHECK(result.invalid_settings == 0u);
    PBVP_CHECK(result.malformed_lines == 0u);
}

void CheckFallbacksAndCounters() {
    TemporaryConfiguration file;
    const std::string invalid =
        "orphan=1\n"
        "[General]\nEnabled=maybe\nUnknown=1\n"
        "[Rendering]\nAspectMode=Stretch\nTintMode=Amber\n"
        "[Playback]\nVolume=nan\nMuted=2\nSeekSeconds=0\n"
        "[Catalog]\nMaximumEntries=501\nMaximumDisplayCharacters=15\n"
        "[Resources]\nMaximumSourceWidth=3840\nMaximumSourceHeight=2160\n"
        "MaximumQueuedVideoEdge=1024\nMaximumMediaFileMiB=32769\n"
        "[Logging]\nDetail=Verbose\n"
        "bad line\n";
    PBVP_CHECK(file.Write(invalid));
    const pbvp::ConfigurationResult result = pbvp::LoadConfiguration(file.Path());
    PBVP_CHECK(result.status == pbvp::ConfigurationStatus::ok);
    PBVP_CHECK(result.settings.enabled);
    PBVP_CHECK(result.settings.aspect_mode == pbvp::AspectMode::fit);
    PBVP_CHECK(result.settings.tint_mode == pbvp::TintMode::pipboy);
    PBVP_CHECK(result.settings.volume == 1.0f);
    PBVP_CHECK(!result.settings.muted);
    PBVP_CHECK(result.settings.seek_seconds == 10u);
    PBVP_CHECK(result.settings.catalog.maximum_entries == 500u);
    PBVP_CHECK(result.settings.resources.maximum_source_width == 1920u);
    PBVP_CHECK(result.settings.resources.maximum_source_height == 1080u);
    PBVP_CHECK(result.unknown_settings == 1u);
    PBVP_CHECK(result.invalid_settings == 13u);
    PBVP_CHECK(result.malformed_lines == 2u);
}

void CheckFileFailures() {
    pbvp::ConfigurationResult result = pbvp::LoadConfiguration(L"relative.ini");
    PBVP_CHECK(result.status == pbvp::ConfigurationStatus::path_not_absolute);

    TemporaryConfiguration missing;
    DeleteFileW(missing.Path().c_str());
    result = pbvp::LoadConfiguration(missing.Path());
    PBVP_CHECK(result.status == pbvp::ConfigurationStatus::file_missing);

    TemporaryConfiguration invalid_utf8;
    PBVP_CHECK(invalid_utf8.Write(std::vector<unsigned char>{0xC3u, 0x28u}));
    result = pbvp::LoadConfiguration(invalid_utf8.Path());
    PBVP_CHECK(result.status == pbvp::ConfigurationStatus::invalid_utf8);

    TemporaryConfiguration too_large;
    PBVP_CHECK(too_large.Write(std::vector<unsigned char>(
        pbvp::kMaximumConfigurationBytes + 1u, static_cast<unsigned char>('A'))));
    result = pbvp::LoadConfiguration(too_large.Path());
    PBVP_CHECK(result.status == pbvp::ConfigurationStatus::file_too_large);
}

} // namespace

void RunConfigurationTests() {
    CheckDefaultsAndValidSettings();
    CheckFallbacksAndCounters();
    CheckFileFailures();
}
