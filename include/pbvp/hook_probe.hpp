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
inline constexpr std::size_t kRelativeCallBytes = 5;
using RelativeCallBytes = std::array<std::uint8_t, kRelativeCallBytes>;

HookProbeResult ClassifyHookTarget(
    std::span<const std::uint8_t> bytes,
    std::span<const HookBytes> supported_signatures) noexcept;

bool EncodeRelativeCall(
    std::uintptr_t call_site,
    std::uintptr_t target,
    RelativeCallBytes& output) noexcept;

bool DecodeRelativeCallTarget(
    std::uintptr_t call_site,
    std::span<const std::uint8_t> bytes,
    std::uintptr_t& target) noexcept;

HookProbeResult ClassifyRelativeCallSite(
    std::uintptr_t call_site,
    std::span<const std::uint8_t> bytes,
    std::uintptr_t expected_target) noexcept;

} // namespace pbvp
