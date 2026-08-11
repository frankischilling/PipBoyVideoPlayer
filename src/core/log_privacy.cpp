#include "pbvp/log_privacy.hpp"

#include <Windows.h>

#include <limits>

namespace pbvp {
namespace {

bool IsSeparator(const wchar_t character) noexcept {
    return character == L'\\' || character == L'/';
}

bool IsSafeRelativeName(const std::wstring_view name) noexcept {
    if (name.empty() || IsSeparator(name.front()) ||
        (name.size() >= 2u && name[1] == L':')) {
        return false;
    }

    std::size_t segment_start = 0u;
    for (std::size_t index = 0u; index <= name.size(); ++index) {
        if (index != name.size() && !IsSeparator(name[index])) {
            continue;
        }
        const std::wstring_view segment = name.substr(segment_start, index - segment_start);
        if (segment.empty() || segment == L"." || segment == L"..") {
            return false;
        }
        segment_start = index + 1u;
    }
    return true;
}

std::wstring_view BaseName(const std::wstring_view name) noexcept {
    const std::size_t separator = name.find_last_of(L"\\/");
    return separator == std::wstring_view::npos ? name : name.substr(separator + 1u);
}

bool ContainsUnsafeLogCharacter(const std::wstring_view name) noexcept {
    for (const wchar_t character : name) {
        if (character == L'\0' || character < L' ' || character == 0x7Fu) {
            return true;
        }
    }
    return false;
}

} // namespace

bool FormatPrivacySafeMediaName(
    const std::wstring_view relative_name,
    const LoggingDetail detail,
    const std::span<char> output) noexcept {
    if (output.empty()) {
        return false;
    }
    output.front() = '\0';

    const std::wstring_view base_name = BaseName(relative_name);
    const std::wstring_view selected =
        detail == LoggingDetail::diagnostic && IsSafeRelativeName(relative_name)
        ? relative_name
        : base_name;
    if (selected.empty() || ContainsUnsafeLogCharacter(selected) ||
        selected.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return false;
    }

    const int required = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, selected.data(),
        static_cast<int>(selected.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0 ||
        static_cast<std::size_t>(required) >= output.size()) {
        return false;
    }
    const int written = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, selected.data(),
        static_cast<int>(selected.size()), output.data(), required, nullptr, nullptr);
    if (written != required) {
        output.front() = '\0';
        return false;
    }
    output[static_cast<std::size_t>(written)] = '\0';
    return true;
}

} // namespace pbvp
