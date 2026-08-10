#include "pbvp/recreate_gate.hpp"

#include "test_support.hpp"

#include <array>

void RunRecreateGateTests() {
    using namespace pbvp;
    constexpr DeferredRecreateGateBytes supported{
        0x0Fu, 0xB6u, 0x05u, 0xBBu, 0x6Fu, 0x1Cu, 0x01u, 0x85u,
        0xC0u, 0x74u, 0x0Cu, 0xE8u, 0x5Bu, 0xD5u, 0xC6u, 0xFFu,
        0xC6u, 0x05u, 0xBBu, 0x6Fu, 0x1Cu, 0x01u, 0x00u};

    PBVP_CHECK(MatchesDeferredRecreateGate(supported));

    auto wrong_flag = supported;
    wrong_flag[3] = 0xBCu;
    PBVP_CHECK(!MatchesDeferredRecreateGate(wrong_flag));

    auto wrong_call = supported;
    wrong_call[12] = 0x5Cu;
    PBVP_CHECK(!MatchesDeferredRecreateGate(wrong_call));

    const std::array<std::uint8_t, 8> short_gate{};
    PBVP_CHECK(!MatchesDeferredRecreateGate(short_gate));
}
