#include "pbvp/hook_probe.hpp"

#include <algorithm>

namespace pbvp {
namespace {

bool IsJumpAt(const std::span<const std::uint8_t> bytes, const std::size_t offset) noexcept {
    if (offset + 1u >= bytes.size()) {
        return false;
    }
    const std::uint8_t first = bytes[offset];
    return first == 0xE9u || first == 0xEBu || first == 0xEAu ||
           (first == 0xFFu && (bytes[offset + 1u] == 0x25u || bytes[offset + 1u] == 0x15u));
}

bool IsPushReturnStub(const std::span<const std::uint8_t> bytes) noexcept {
    return bytes[0] == 0x68u && bytes[5] == 0xC3u;
}

bool IsRegisterJumpStub(const std::span<const std::uint8_t> bytes) noexcept {
    if (bytes[0] >= 0xB8u && bytes[0] <= 0xBFu && bytes[5] == 0xFFu) {
        const std::uint8_t expected_jump = static_cast<std::uint8_t>(0xE0u + (bytes[0] - 0xB8u));
        return bytes[6] == expected_jump;
    }
    return bytes[0] == 0xA1u && bytes[5] == 0xFFu && bytes[6] == 0xE0u;
}

} // namespace

HookProbeResult ClassifyHookTarget(
    const std::span<const std::uint8_t> bytes,
    const std::span<const HookBytes> supported_signatures) noexcept {
    if (bytes.size() < kHookProbeBytes) {
        return HookProbeResult::unreadable;
    }

    const bool hotpatch_prefix = bytes[0] == 0x8Bu && bytes[1] == 0xFFu;
    const bool padding_prefix = bytes[0] == 0x90u || bytes[0] == 0xCCu;
    if (IsJumpAt(bytes, 0u) || (hotpatch_prefix && IsJumpAt(bytes, 2u)) ||
        (padding_prefix && IsJumpAt(bytes, 1u)) || IsPushReturnStub(bytes) ||
        IsRegisterJumpStub(bytes)) {
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
