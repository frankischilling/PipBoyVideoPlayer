#include "pbvp/hook_manager.hpp"

#include "pbvp/d3d_renderer.hpp"
#include "pbvp/hook_probe.hpp"
#include "pbvp/log.hpp"

#include <Windows.h>
#include <MinHook.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <span>

namespace pbvp::hooks {
namespace {

constexpr std::uintptr_t kRendererRecreateAddress = 0x00E73EB0u;

// This relocation-free entry was independently recovered from the supported
// 1.4.0.525 runtime. Runtime code is decrypted before DeferredInit, so the
// live bytes must still match exactly before MinHook receives the target.
constexpr std::array<HookBytes, 1> kSupportedRecreateSignatures{{
    {0x83u, 0xECu, 0x38u, 0x56u, 0x57u, 0x8Bu, 0xF9u, 0x8Bu,
     0x8Fu, 0x84u, 0x08u, 0x00u, 0x00u, 0x8Bu, 0x01u, 0x8Bu},
}};

using RecreateFunction = std::uint32_t(__thiscall*)(void*, std::uint32_t, std::uint32_t);
RecreateFunction g_original_recreate = nullptr;
std::atomic<bool> g_ready{false};
std::atomic<bool> g_shutting_down{false};
bool g_minhook_initialized = false;
bool g_reset_hook_created = false;
bool g_reset_hook_enabled = false;

std::uint32_t __fastcall RecreateDetour(
    void* renderer, void*, const std::uint32_t request_a, const std::uint32_t request_b) noexcept {
    if (g_original_recreate == nullptr) {
        return 0u;
    }
    if (!g_shutting_down.load(std::memory_order_acquire)) {
        D3dRenderer::Instance().BeforeDeviceRecreate(renderer);
    }
    const std::uint32_t result = g_original_recreate(renderer, request_a, request_b);
    if (!g_shutting_down.load(std::memory_order_acquire)) {
        D3dRenderer::Instance().AfterDeviceRecreate(renderer, result);
    }
    return result;
}

template <std::size_t Size>
bool ReadBytes(const std::uintptr_t address, std::array<std::uint8_t, Size>& output) noexcept {
    MEMORY_BASIC_INFORMATION information{};
    const void* target = reinterpret_cast<const void*>(address);
    if (VirtualQuery(target, &information, sizeof(information)) != sizeof(information) ||
        information.State != MEM_COMMIT || (information.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0u ||
        address + Size > reinterpret_cast<std::uintptr_t>(information.BaseAddress) + information.RegionSize) {
        return false;
    }
    __try {
        const auto* source = static_cast<const std::uint8_t*>(target);
        for (std::size_t index = 0; index < output.size(); ++index) {
            output[index] = source[index];
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void LogBytes(const HookBytes& bytes) noexcept {
    PBVP_LOG_INFO(
        "NiDX9Renderer::Recreate live bytes: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
        bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
        bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
}

void RemoveResetHook() noexcept {
    if (g_reset_hook_enabled) {
        MH_DisableHook(reinterpret_cast<void*>(kRendererRecreateAddress));
        g_reset_hook_enabled = false;
    }
    if (g_reset_hook_created) {
        MH_RemoveHook(reinterpret_cast<void*>(kRendererRecreateAddress));
        g_reset_hook_created = false;
    }
    g_original_recreate = nullptr;
    if (g_minhook_initialized) {
        MH_Uninitialize();
        g_minhook_initialized = false;
    }
}

} // namespace

bool ProbeAndInstall() noexcept {
    HookBytes bytes{};
    if (!ReadBytes(kRendererRecreateAddress, bytes)) {
        PBVP_LOG_ERROR("Reset hook target is unreadable; rendering is disabled");
        return false;
    }
    LogBytes(bytes);
    const HookProbeResult probe = ClassifyHookTarget(bytes, kSupportedRecreateSignatures);
    if (probe == HookProbeResult::occupied) {
        PBVP_LOG_ERROR("Reset hook target is already redirected; rendering is disabled");
        return false;
    }
    if (probe != HookProbeResult::supported) {
        PBVP_LOG_WARN("Reset hook target has no reviewed signature; diagnostic rendering is disabled");
        return false;
    }

    if (MH_Initialize() != MH_OK) {
        PBVP_LOG_ERROR("MinHook initialization failed");
        return false;
    }
    g_minhook_initialized = true;
    void* original = nullptr;
    if (MH_CreateHook(
            reinterpret_cast<void*>(kRendererRecreateAddress),
            reinterpret_cast<void*>(&RecreateDetour), &original) != MH_OK) {
        PBVP_LOG_ERROR("Reset hook creation failed; rendering is disabled");
        RemoveResetHook();
        return false;
    }
    g_reset_hook_created = true;
    if (original == nullptr) {
        PBVP_LOG_ERROR("Reset hook returned no original function; rendering is disabled");
        RemoveResetHook();
        return false;
    }
    g_original_recreate = reinterpret_cast<RecreateFunction>(original);
    if (MH_EnableHook(reinterpret_cast<void*>(kRendererRecreateAddress)) != MH_OK) {
        PBVP_LOG_ERROR("Reset hook activation failed; rendering is disabled");
        RemoveResetHook();
        return false;
    }
    g_reset_hook_enabled = true;

    g_ready.store(true, std::memory_order_release);
    PBVP_LOG_INFO("Verified NiDX9Renderer::Recreate hook installed");
    return true;
}

void MarkShutdown() noexcept {
    g_shutting_down.store(true, std::memory_order_release);
    g_ready.store(false, std::memory_order_release);
    RemoveResetHook();
}

bool IsReady() noexcept {
    return g_ready.load(std::memory_order_acquire);
}

} // namespace pbvp::hooks
