#include "pbvp/hook_probe.hpp"

#include "test_support.hpp"

#include <array>
#include <span>

void RunHookProbeTests() {
    using namespace pbvp;
    constexpr HookBytes signature{0x83u, 0xECu, 0x38u, 0x56u, 0x57u, 0x8Bu, 0xF9u, 0x8Bu,
                                  0x8Fu, 0x84u, 0x08u, 0x00u, 0x00u, 0x8Bu, 0x01u, 0x8Bu};
    constexpr std::array<HookBytes, 1> signatures{signature};
    PBVP_CHECK(ClassifyHookTarget(signature, signatures) == HookProbeResult::supported);

    HookBytes relative_jump = signature;
    relative_jump[0] = 0xE9u;
    PBVP_CHECK(ClassifyHookTarget(relative_jump, signatures) == HookProbeResult::occupied);

    HookBytes indirect_jump = signature;
    indirect_jump[0] = 0xFFu;
    indirect_jump[1] = 0x25u;
    PBVP_CHECK(ClassifyHookTarget(indirect_jump, signatures) == HookProbeResult::occupied);

    HookBytes unknown{};
    unknown.fill(0xCCu);
    PBVP_CHECK(ClassifyHookTarget(unknown, signatures) == HookProbeResult::unknown);

    const std::array<std::uint8_t, 4> short_input{};
    PBVP_CHECK(ClassifyHookTarget(short_input, signatures) == HookProbeResult::unreadable);
}
