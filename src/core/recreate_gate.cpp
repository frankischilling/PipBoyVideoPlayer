#include "pbvp/recreate_gate.hpp"

#include <algorithm>

namespace pbvp {
namespace {

constexpr DeferredRecreateGateBytes kSupportedGate{
    0x0Fu, 0xB6u, 0x05u, 0xBBu, 0x6Fu, 0x1Cu, 0x01u, 0x85u,
    0xC0u, 0x74u, 0x0Cu, 0xE8u, 0x5Bu, 0xD5u, 0xC6u, 0xFFu,
    0xC6u, 0x05u, 0xBBu, 0x6Fu, 0x1Cu, 0x01u, 0x00u};

} // namespace

bool MatchesDeferredRecreateGate(const std::span<const std::uint8_t> bytes) noexcept {
    return bytes.size() >= kSupportedGate.size() &&
           std::equal(kSupportedGate.begin(), kSupportedGate.end(), bytes.begin());
}

} // namespace pbvp
