#include "pbvp/hook_probe.hpp"

#include <MinHook.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>

namespace {

using TestFunction = int(__cdecl*)(int);
TestFunction g_original = nullptr;
volatile int g_target_bias = 7;

__declspec(noinline) int __cdecl TestTarget(const int value) {
    return value + g_target_bias;
}

__declspec(noinline) int __cdecl TestDetour(const int value) {
    return g_original != nullptr ? g_original(value) + 100 : -1;
}

bool ReadTarget(pbvp::HookBytes& bytes) {
    std::memcpy(bytes.data(), reinterpret_cast<const void*>(&TestTarget), bytes.size());
    return true;
}

int Fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}

} // namespace

int main() {
    pbvp::HookBytes original_bytes{};
    if (!ReadTarget(original_bytes)) {
        return Fail("Could not read the test target.");
    }
    const std::array<pbvp::HookBytes, 1> signatures{original_bytes};
    if (pbvp::ClassifyHookTarget(original_bytes, signatures) !=
        pbvp::HookProbeResult::supported) {
        return Fail("The unmodified test target was not accepted.");
    }
    if (MH_Initialize() != MH_OK) {
        return Fail("MinHook initialization failed.");
    }

    void* trampoline = nullptr;
    if (MH_CreateHook(
            reinterpret_cast<void*>(&TestTarget), reinterpret_cast<void*>(&TestDetour),
            &trampoline) != MH_OK) {
        MH_Uninitialize();
        return Fail("MinHook could not create the test hook.");
    }
    g_original = reinterpret_cast<TestFunction>(trampoline);
    if (MH_EnableHook(reinterpret_cast<void*>(&TestTarget)) != MH_OK) {
        MH_RemoveHook(reinterpret_cast<void*>(&TestTarget));
        MH_Uninitialize();
        return Fail("MinHook could not enable the test hook.");
    }

    pbvp::HookBytes redirected_bytes{};
    ReadTarget(redirected_bytes);
    const bool occupied = pbvp::ClassifyHookTarget(redirected_bytes, signatures) ==
                          pbvp::HookProbeResult::occupied;
    volatile TestFunction redirected = &TestTarget;
    const bool detour_called = redirected(5) == 112;

    const bool disabled = MH_DisableHook(reinterpret_cast<void*>(&TestTarget)) == MH_OK;
    const bool removed = MH_RemoveHook(reinterpret_cast<void*>(&TestTarget)) == MH_OK;
    const bool uninitialized = MH_Uninitialize() == MH_OK;
    g_original = nullptr;

    volatile TestFunction restored = &TestTarget;
    if (!occupied) {
        return Fail("The live MinHook redirect was not classified as occupied.");
    }
    if (!detour_called) {
        return Fail("The isolated test detour did not execute.");
    }
    if (!disabled || !removed || !uninitialized || restored(5) != 12) {
        return Fail("The isolated test hook was not restored cleanly.");
    }

    std::cout << "Live hook-conflict fixture passed.\n";
    return 0;
}
