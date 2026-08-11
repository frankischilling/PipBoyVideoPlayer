#pragma once

#include "pbvp/configuration.hpp"

#include <cstddef>
#include <span>
#include <string_view>

namespace pbvp {

constexpr std::size_t kMaximumMediaLogNameBytes = 2049u;

[[nodiscard]] bool FormatPrivacySafeMediaName(
    std::wstring_view relative_name,
    LoggingDetail detail,
    std::span<char> output) noexcept;

} // namespace pbvp
