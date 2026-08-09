#include "pbvp/hook_probe.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace pbvp {

HookProbeResult ClassifyHookTarget(
    const std::span<const std::uint8_t> bytes,
    const std::span<const HookBytes> supported_signatures) noexcept {
    if (bytes.size() < kHookProbeBytes) {
        return HookProbeResult::unreadable;
    }

    const auto first = bytes.front();
    if (first == 0xE9u || first == 0xEBu || first == 0xEAu ||
        (first == 0xFFu && (bytes[1] == 0x25u || bytes[1] == 0x15u))) {
        return HookProbeResult::occupied;
    }

    for (const auto& signature : supported_signatures) {
        if (std::equal(signature.begin(), signature.end(), bytes.begin())) {
            return HookProbeResult::supported;
        }
    }
    return HookProbeResult::unknown;
}

bool EncodeRelativeCall(
    const std::uintptr_t call_site,
    const std::uintptr_t target,
    RelativeCallBytes& output) noexcept {
    if (call_site > std::numeric_limits<std::uint32_t>::max() ||
        target > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    const auto next = static_cast<std::int64_t>(call_site) +
                      static_cast<std::int64_t>(kRelativeCallBytes);
    const auto displacement = static_cast<std::int64_t>(target) - next;
    if (displacement < std::numeric_limits<std::int32_t>::min() ||
        displacement > std::numeric_limits<std::int32_t>::max()) {
        return false;
    }

    output[0] = 0xE8u;
    const auto encoded = static_cast<std::int32_t>(displacement);
    std::memcpy(output.data() + 1, &encoded, sizeof(encoded));
    return true;
}

bool DecodeRelativeCallTarget(
    const std::uintptr_t call_site,
    const std::span<const std::uint8_t> bytes,
    std::uintptr_t& target) noexcept {
    if (bytes.size() < kRelativeCallBytes || bytes[0] != 0xE8u ||
        call_site > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }

    std::int32_t displacement = 0;
    std::memcpy(&displacement, bytes.data() + 1, sizeof(displacement));
    const auto decoded = static_cast<std::int64_t>(call_site) +
                         static_cast<std::int64_t>(kRelativeCallBytes) + displacement;
    if (decoded < 0 || decoded > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    target = static_cast<std::uintptr_t>(decoded);
    return true;
}

HookProbeResult ClassifyRelativeCallSite(
    const std::uintptr_t call_site,
    const std::span<const std::uint8_t> bytes,
    const std::uintptr_t expected_target) noexcept {
    if (bytes.size() < kRelativeCallBytes) {
        return HookProbeResult::unreadable;
    }
    std::uintptr_t actual_target = 0;
    if (DecodeRelativeCallTarget(call_site, bytes, actual_target)) {
        return actual_target == expected_target ? HookProbeResult::supported
                                                : HookProbeResult::occupied;
    }
    const auto first = bytes.front();
    if (first == 0xE9u || first == 0xEBu || first == 0xEAu || first == 0xE8u ||
        (first == 0xFFu && (bytes[1] == 0x25u || bytes[1] == 0x15u))) {
        return HookProbeResult::occupied;
    }
    return HookProbeResult::unknown;
}

} // namespace pbvp
