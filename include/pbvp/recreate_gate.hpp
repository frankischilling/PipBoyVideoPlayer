#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace pbvp {

inline constexpr std::size_t kDeferredRecreateGateBytes = 23u;
using DeferredRecreateGateBytes =
    std::array<std::uint8_t, kDeferredRecreateGateBytes>;

bool MatchesDeferredRecreateGate(std::span<const std::uint8_t> bytes) noexcept;

} // namespace pbvp
