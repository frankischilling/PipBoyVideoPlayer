#include "pbvp/hook_probe.hpp"

#include <algorithm>

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

} // namespace pbvp
