#include "pbvp/log_privacy.hpp"

#include "test_support.hpp"

#include <array>
#include <string>

namespace {

std::string Text(const std::array<char, pbvp::kMaximumMediaLogNameBytes>& output) {
    return output.data();
}

} // namespace

void RunLogPrivacyTests() {
    std::array<char, pbvp::kMaximumMediaLogNameBytes> output{};

    PBVP_CHECK(pbvp::FormatPrivacySafeMediaName(
        L"Season 1\\Episode 2.mp4", pbvp::LoggingDetail::normal, output));
    PBVP_CHECK(Text(output) == "Episode 2.mp4");

    PBVP_CHECK(pbvp::FormatPrivacySafeMediaName(
        L"Season 1\\Episode 2.mp4", pbvp::LoggingDetail::diagnostic, output));
    PBVP_CHECK(Text(output) == "Season 1\\Episode 2.mp4");

    PBVP_CHECK(pbvp::FormatPrivacySafeMediaName(
        L"C:\\Users\\Francis\\Private.mp4",
        pbvp::LoggingDetail::diagnostic, output));
    PBVP_CHECK(Text(output) == "Private.mp4");
    PBVP_CHECK(pbvp::FormatPrivacySafeMediaName(
        L"\\\\server\\share\\Private.mp4",
        pbvp::LoggingDetail::diagnostic, output));
    PBVP_CHECK(Text(output) == "Private.mp4");
    PBVP_CHECK(pbvp::FormatPrivacySafeMediaName(
        L"..\\Private.mp4", pbvp::LoggingDetail::diagnostic, output));
    PBVP_CHECK(Text(output) == "Private.mp4");

    PBVP_CHECK(pbvp::FormatPrivacySafeMediaName(
        L"\u65E5\u672C\u8A9E.mp4", pbvp::LoggingDetail::normal, output));
    PBVP_CHECK(Text(output) == "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E.mp4");

    const std::wstring embedded_null{L"Private\0Title.mp4", 17u};
    PBVP_CHECK(!pbvp::FormatPrivacySafeMediaName(
        embedded_null, pbvp::LoggingDetail::normal, output));
    PBVP_CHECK(output.front() == '\0');
    PBVP_CHECK(!pbvp::FormatPrivacySafeMediaName(
        L"Private\nTitle.mp4", pbvp::LoggingDetail::normal, output));
    PBVP_CHECK(output.front() == '\0');
    const wchar_t invalid_surrogate[] = {static_cast<wchar_t>(0xD800u), L'.', L'm', L'p', L'4'};
    PBVP_CHECK(!pbvp::FormatPrivacySafeMediaName(
        std::wstring_view{invalid_surrogate, std::size(invalid_surrogate)},
        pbvp::LoggingDetail::normal, output));
    PBVP_CHECK(output.front() == '\0');

    std::array<char, 5u> short_output{};
    PBVP_CHECK(!pbvp::FormatPrivacySafeMediaName(
        L"Episode 2.mp4", pbvp::LoggingDetail::normal, short_output));
    PBVP_CHECK(short_output.front() == '\0');
    PBVP_CHECK(!pbvp::FormatPrivacySafeMediaName(
        L"Episode 2.mp4", pbvp::LoggingDetail::normal, std::span<char>{}));
}
