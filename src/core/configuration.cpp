#include "pbvp/configuration.hpp"

#include <Windows.h>

#include <array>
#include <charconv>
#include <cmath>
#include <cwctype>
#include <limits>
#include <new>
#include <string_view>
#include <vector>

namespace pbvp {
namespace {

bool IsAbsoluteLocalPath(const std::wstring& path) noexcept {
    return path.size() >= 3u && std::iswalpha(path[0]) != 0 && path[1] == L':' &&
           (path[2] == L'\\' || path[2] == L'/');
}

std::wstring_view Trim(const std::wstring_view input) noexcept {
    std::size_t begin = 0u;
    while (begin < input.size() && (input[begin] == L' ' || input[begin] == L'\t')) {
        ++begin;
    }
    std::size_t end = input.size();
    while (end > begin && (input[end - 1u] == L' ' || input[end - 1u] == L'\t')) {
        --end;
    }
    return input.substr(begin, end - begin);
}

std::wstring LowerAscii(const std::wstring_view input) {
    std::wstring output(input);
    for (wchar_t& value : output) {
        if (value >= L'A' && value <= L'Z') {
            value = static_cast<wchar_t>(value + (L'a' - L'A'));
        }
    }
    return output;
}

bool NarrowAscii(const std::wstring_view input, std::string& output) {
    output.clear();
    output.reserve(input.size());
    for (const wchar_t value : input) {
        if (value > 0x7Fu) {
            output.clear();
            return false;
        }
        output.push_back(static_cast<char>(value));
    }
    return true;
}

bool ParseBool(const std::wstring_view input, bool& output) {
    const std::wstring value = LowerAscii(Trim(input));
    if (value == L"1" || value == L"true" || value == L"yes" || value == L"on") {
        output = true;
        return true;
    }
    if (value == L"0" || value == L"false" || value == L"no" || value == L"off") {
        output = false;
        return true;
    }
    return false;
}

template <typename Integer>
bool ParseInteger(
    const std::wstring_view input,
    const Integer minimum,
    const Integer maximum,
    Integer& output) {
    std::string ascii;
    if (!NarrowAscii(Trim(input), ascii) || ascii.empty()) {
        return false;
    }
    Integer parsed{};
    const auto converted = std::from_chars(
        ascii.data(), ascii.data() + ascii.size(), parsed, 10);
    if (converted.ec != std::errc{} || converted.ptr != ascii.data() + ascii.size() ||
        parsed < minimum || parsed > maximum) {
        return false;
    }
    output = parsed;
    return true;
}

bool ParseFloat(
    const std::wstring_view input,
    const float minimum,
    const float maximum,
    float& output) {
    std::string ascii;
    if (!NarrowAscii(Trim(input), ascii) || ascii.empty()) {
        return false;
    }
    float parsed{};
    const auto converted = std::from_chars(
        ascii.data(), ascii.data() + ascii.size(), parsed, std::chars_format::general);
    if (converted.ec != std::errc{} || converted.ptr != ascii.data() + ascii.size() ||
        !std::isfinite(parsed) || parsed < minimum || parsed > maximum) {
        return false;
    }
    output = parsed;
    return true;
}

void RecordInvalid(ConfigurationResult& result, const bool valid) noexcept {
    if (!valid && result.invalid_settings != (std::numeric_limits<std::uint32_t>::max)()) {
        ++result.invalid_settings;
    }
}

bool ApplySetting(
    const std::wstring& section,
    const std::wstring& key,
    const std::wstring_view value,
    ConfigurationResult& result,
    bool& input_value_invalid) {
    if (section == L"general" && key == L"enabled") {
        bool parsed{};
        const bool valid = ParseBool(value, parsed);
        if (valid) {
            result.settings.enabled = parsed;
        }
        RecordInvalid(result, valid);
        return true;
    }
    if (section == L"rendering" && key == L"aspectmode") {
        const std::wstring parsed = LowerAscii(Trim(value));
        const bool valid = parsed == L"fit" || parsed == L"fill";
        if (valid) {
            result.settings.aspect_mode = parsed == L"fill" ? AspectMode::fill : AspectMode::fit;
        }
        RecordInvalid(result, valid);
        return true;
    }
    if (section == L"rendering" && key == L"tintmode") {
        const std::wstring parsed = LowerAscii(Trim(value));
        const bool valid = parsed == L"pipboy" || parsed == L"fullcolor";
        if (valid) {
            result.settings.tint_mode = parsed == L"fullcolor"
                ? TintMode::full_color
                : TintMode::pipboy;
        }
        RecordInvalid(result, valid);
        return true;
    }
    if (section == L"playback" && key == L"volume") {
        float parsed{};
        const bool valid = ParseFloat(value, 0.0f, 1.0f, parsed);
        if (valid) {
            result.settings.volume = parsed;
        }
        RecordInvalid(result, valid);
        return true;
    }
    if (section == L"playback" && key == L"muted") {
        bool parsed{};
        const bool valid = ParseBool(value, parsed);
        if (valid) {
            result.settings.muted = parsed;
        }
        RecordInvalid(result, valid);
        return true;
    }
    if (section == L"playback" && key == L"seekseconds") {
        std::uint32_t parsed{};
        const bool valid = ParseInteger(value, 1u, 60u, parsed);
        if (valid) {
            result.settings.seek_seconds = parsed;
        }
        RecordInvalid(result, valid);
        return true;
    }
    if (section == L"catalog" && key == L"maximumentries") {
        std::size_t parsed{};
        const bool valid = ParseInteger(
            value, std::size_t{1u}, kMaximumCatalogEntries, parsed);
        if (valid) {
            result.settings.catalog.maximum_entries = parsed;
        }
        RecordInvalid(result, valid);
        return true;
    }
    if (section == L"input") {
        std::uint32_t* destination = nullptr;
        if (key == L"selectorplay") {
            destination = &result.settings.input.select_or_play;
        } else if (key == L"pauseresume") {
            destination = &result.settings.input.pause_resume;
        } else if (key == L"backorstop") {
            destination = &result.settings.input.back_or_stop;
        } else if (key == L"seekbackward") {
            destination = &result.settings.input.seek_backward;
        } else if (key == L"seekforward") {
            destination = &result.settings.input.seek_forward;
        } else if (key == L"previousitem") {
            destination = &result.settings.input.previous_item;
        } else if (key == L"nextitem") {
            destination = &result.settings.input.next_item;
        } else if (key == L"togglecolor") {
            destination = &result.settings.input.toggle_color;
        }
        if (destination != nullptr) {
            std::uint32_t parsed{};
            const bool valid = ParseInteger(value, 1u, 255u, parsed);
            if (valid) {
                *destination = parsed;
            } else {
                input_value_invalid = true;
            }
            RecordInvalid(result, valid);
            return true;
        }
    }
    if (section == L"catalog" && key == L"maximumdisplaycharacters") {
        std::size_t parsed{};
        const bool valid = ParseInteger(
            value, std::size_t{16u}, kMaximumCatalogDisplayCharacters, parsed);
        if (valid) {
            result.settings.catalog.maximum_display_characters = parsed;
        }
        RecordInvalid(result, valid);
        return true;
    }
    if (section == L"resources" && key == L"maximumsourcewidth") {
        std::uint32_t parsed{};
        const bool valid = ParseInteger(value, 320u, 1920u, parsed);
        if (valid) {
            result.settings.resources.maximum_source_width = parsed;
        }
        RecordInvalid(result, valid);
        return true;
    }
    if (section == L"resources" && key == L"maximumsourceheight") {
        std::uint32_t parsed{};
        const bool valid = ParseInteger(value, 240u, 1080u, parsed);
        if (valid) {
            result.settings.resources.maximum_source_height = parsed;
        }
        RecordInvalid(result, valid);
        return true;
    }
    if (section == L"resources" && key == L"maximumqueuedvideoedge") {
        std::uint32_t parsed{};
        const bool valid = ParseInteger(value, 64u, 512u, parsed);
        if (valid) {
            result.settings.resources.maximum_queued_video_edge = parsed;
        }
        RecordInvalid(result, valid);
        return true;
    }
    if (section == L"resources" && key == L"maximummediafilemib") {
        std::uint64_t parsed{};
        constexpr std::uint64_t maximum_mib =
            kMaximumConfiguredMediaBytes / (1024ull * 1024ull);
        const bool valid = ParseInteger(value, 1ull, maximum_mib, parsed);
        if (valid) {
            result.settings.resources.maximum_media_file_bytes =
                parsed * 1024ull * 1024ull;
        }
        RecordInvalid(result, valid);
        return true;
    }
    if (section == L"logging" && key == L"detail") {
        const std::wstring parsed = LowerAscii(Trim(value));
        const bool valid = parsed == L"normal" || parsed == L"diagnostic";
        if (valid) {
            result.settings.logging_detail = parsed == L"diagnostic"
                ? LoggingDetail::diagnostic
                : LoggingDetail::normal;
        }
        RecordInvalid(result, valid);
        return true;
    }
    return false;
}

bool DecodeUtf8(const std::vector<unsigned char>& bytes, std::wstring& output) {
    std::size_t offset = 0u;
    if (bytes.size() >= 3u && bytes[0] == 0xEFu && bytes[1] == 0xBBu && bytes[2] == 0xBFu) {
        offset = 3u;
    }
    if (offset == bytes.size()) {
        output.clear();
        return true;
    }
    const std::size_t remaining = bytes.size() - offset;
    if (remaining > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return false;
    }
    const int required = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS,
        reinterpret_cast<const char*>(bytes.data() + offset),
        static_cast<int>(remaining), nullptr, 0);
    if (required <= 0) {
        return false;
    }
    output.resize(static_cast<std::size_t>(required));
    return MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS,
        reinterpret_cast<const char*>(bytes.data() + offset),
        static_cast<int>(remaining), output.data(), required) == required;
}

void ParseDocument(const std::wstring& document, ConfigurationResult& result) {
    std::wstring section;
    bool input_value_invalid = false;
    std::size_t offset = 0u;
    while (offset <= document.size()) {
        const std::size_t newline = document.find(L'\n', offset);
        const std::size_t end = newline == std::wstring::npos ? document.size() : newline;
        std::wstring_view line(document.data() + offset, end - offset);
        if (!line.empty() && line.back() == L'\r') {
            line.remove_suffix(1u);
        }
        line = Trim(line);
        if (!line.empty() && line.front() != L';' && line.front() != L'#') {
            if (line.front() == L'[' && line.back() == L']' && line.size() > 2u) {
                section = LowerAscii(Trim(line.substr(1u, line.size() - 2u)));
                if (section.empty()) {
                    ++result.malformed_lines;
                }
            } else {
                const std::size_t equals = line.find(L'=');
                if (equals == std::wstring_view::npos || section.empty()) {
                    ++result.malformed_lines;
                } else {
                    const std::wstring key = LowerAscii(Trim(line.substr(0u, equals)));
                    const std::wstring_view value = Trim(line.substr(equals + 1u));
                    if (key.empty()) {
                        ++result.malformed_lines;
                    } else if (!ApplySetting(
                                   section, key, value, result,
                                   input_value_invalid)) {
                        ++result.unknown_settings;
                    }
                }
            }
        }
        if (newline == std::wstring::npos) {
            break;
        }
        offset = newline + 1u;
    }

    const bool input_set_invalid = !InputSettingsValid(result.settings.input);
    if (input_value_invalid || input_set_invalid) {
        result.settings.input = InputSettings{};
        if (input_set_invalid) {
            RecordInvalid(result, false);
        }
    }
}

} // namespace

const char* ConfigurationStatusName(const ConfigurationStatus status) noexcept {
    switch (status) {
        case ConfigurationStatus::ok: return "ok";
        case ConfigurationStatus::path_not_absolute: return "configuration path is not absolute";
        case ConfigurationStatus::file_missing: return "configuration file is missing";
        case ConfigurationStatus::file_not_regular: return "configuration path is not a regular file";
        case ConfigurationStatus::file_reparse_point: return "configuration file is a reparse point";
        case ConfigurationStatus::file_too_large: return "configuration file is too large";
        case ConfigurationStatus::file_open_failed: return "configuration file could not be opened";
        case ConfigurationStatus::file_read_failed: return "configuration file could not be read";
        case ConfigurationStatus::invalid_utf8: return "configuration file is not valid UTF-8";
        case ConfigurationStatus::allocation_failed: return "configuration allocation failed";
        case ConfigurationStatus::unexpected_failure: return "unexpected configuration failure";
    }
    return "unknown configuration failure";
}

const char* AspectModeName(const AspectMode mode) noexcept {
    return mode == AspectMode::fill ? "fill" : "fit";
}

const char* TintModeName(const TintMode mode) noexcept {
    return mode == TintMode::full_color ? "full-color" : "pipboy";
}

const char* LoggingDetailName(const LoggingDetail detail) noexcept {
    return detail == LoggingDetail::diagnostic ? "diagnostic" : "normal";
}

bool InputSettingsValid(const InputSettings& settings) noexcept {
    const std::array<std::uint32_t, 8u> bindings{{
        settings.select_or_play,
        settings.pause_resume,
        settings.back_or_stop,
        settings.seek_backward,
        settings.seek_forward,
        settings.previous_item,
        settings.next_item,
        settings.toggle_color,
    }};
    for (std::size_t left = 0u; left < bindings.size(); ++left) {
        if (bindings[left] == 0u || bindings[left] > 255u) {
            return false;
        }
        for (std::size_t right = left + 1u; right < bindings.size(); ++right) {
            if (bindings[left] == bindings[right]) {
                return false;
            }
        }
    }
    return true;
}

bool ConfigurationReloadAllowed(const PlaybackState state) noexcept {
    return state == PlaybackState::idle;
}

ConfigurationResult LoadConfiguration(
    const std::wstring& configuration_path) noexcept {
    ConfigurationResult result{};
    if (!IsAbsoluteLocalPath(configuration_path)) {
        result.status = ConfigurationStatus::path_not_absolute;
        return result;
    }

    const DWORD attributes = GetFileAttributesW(configuration_path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        result.windows_error = GetLastError();
        result.status = result.windows_error == ERROR_FILE_NOT_FOUND ||
                result.windows_error == ERROR_PATH_NOT_FOUND
            ? ConfigurationStatus::file_missing
            : ConfigurationStatus::file_open_failed;
        return result;
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0u ||
        (attributes & FILE_ATTRIBUTE_DEVICE) != 0u) {
        result.status = ConfigurationStatus::file_not_regular;
        return result;
    }
    if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u) {
        result.status = ConfigurationStatus::file_reparse_point;
        return result;
    }

    HANDLE file = CreateFileW(
        configuration_path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        result.windows_error = GetLastError();
        result.status = ConfigurationStatus::file_open_failed;
        return result;
    }

    const auto close_file = [&file]() noexcept {
        if (file != INVALID_HANDLE_VALUE) {
            CloseHandle(file);
            file = INVALID_HANDLE_VALUE;
        }
    };

    try {
        LARGE_INTEGER file_size{};
        if (GetFileSizeEx(file, &file_size) == FALSE || file_size.QuadPart < 0) {
            result.windows_error = GetLastError();
            result.status = ConfigurationStatus::file_read_failed;
            close_file();
            return result;
        }
        if (static_cast<std::uint64_t>(file_size.QuadPart) > kMaximumConfigurationBytes) {
            result.status = ConfigurationStatus::file_too_large;
            close_file();
            return result;
        }
        std::vector<unsigned char> bytes(static_cast<std::size_t>(file_size.QuadPart));
        DWORD total = 0u;
        while (total < bytes.size()) {
            DWORD read = 0u;
            const DWORD remaining = static_cast<DWORD>(bytes.size() - total);
            if (ReadFile(file, bytes.data() + total, remaining, &read, nullptr) == FALSE ||
                read == 0u) {
                result.windows_error = GetLastError();
                result.status = ConfigurationStatus::file_read_failed;
                close_file();
                return result;
            }
            total += read;
        }
        close_file();

        std::wstring document;
        if (!DecodeUtf8(bytes, document)) {
            result.status = ConfigurationStatus::invalid_utf8;
            return result;
        }
        ParseDocument(document, result);
        return result;
    } catch (const std::bad_alloc&) {
        close_file();
        result.status = ConfigurationStatus::allocation_failed;
        return result;
    } catch (...) {
        close_file();
        result.status = ConfigurationStatus::unexpected_failure;
        return result;
    }
}

} // namespace pbvp
