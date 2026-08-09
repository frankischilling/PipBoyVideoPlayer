#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace pbvp {

enum class HookProbeResult {
    supported,
    occupied,
    unknown,
    unreadable,
};

inline constexpr std::size_t kHookProbeBytes = 16;
using HookBytes = std::array<std::uint8_t, kHookProbeBytes>;

HookProbeResult ClassifyHookTarget(
    std::span<const std::uint8_t> bytes,
    std::span<const HookBytes> supported_signatures) noexcept;

} // namespace pbvp
