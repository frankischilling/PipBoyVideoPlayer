#include "nvse/PluginAPI.h"

#include "pbvp/d3d_renderer.hpp"
#include "pbvp/hook_manager.hpp"
#include "pbvp/log.hpp"
#include "pbvp/ui_bridge.hpp"

#include <Windows.h>

#include <atomic>
#include <cstdint>
#include <cstring>

namespace {

constexpr char kPluginName[] = "Pip-Boy Video Player";
constexpr std::uint32_t kPluginVersion = 1u;
constexpr std::uint32_t kMinimumNvseVersion = 0x06040050u;

PluginHandle g_plugin_handle = static_cast<PluginHandle>(kPluginHandle_Invalid);
NVSEMessagingInterface* g_messaging = nullptr;
std::atomic<bool> g_shutdown{false};

void HandleMessage(NVSEMessagingInterface::Message* message) {
    if (message == nullptr) {
        return;
    }
    switch (message->type) {
        case NVSEMessagingInterface::kMessage_PostLoad:
            PBVP_LOG_INFO("xNVSE PostLoad received");
            break;
        case NVSEMessagingInterface::kMessage_DeferredInit:
            PBVP_LOG_INFO("xNVSE DeferredInit received");
            pbvp::hooks::ProbeAndInstall();
            break;
        case NVSEMessagingInterface::kMessage_MainGameLoop:
            if (!g_shutdown.load(std::memory_order_acquire)) {
                pbvp::UiBridge::Instance().UpdateOnGameThread();
            }
            break;
        case NVSEMessagingInterface::kMessage_OnFramePresent: {
            static bool present_boundary_logged = false;
            bool loading_screen = true;
            if (message->data != nullptr && message->dataLen == sizeof(int)) {
                loading_screen = *static_cast<const int*>(message->data) != 0;
            }
            if (!loading_screen && !present_boundary_logged) {
                PBVP_LOG_INFO("xNVSE frame-present callback active; drawing is disabled at this boundary");
                present_boundary_logged = true;
            }
            break;
        }
        case NVSEMessagingInterface::kMessage_PreLoadGame:
        case NVSEMessagingInterface::kMessage_ExitToMainMenu:
        case NVSEMessagingInterface::kMessage_NewGame:
            pbvp::UiBridge::Instance().Clear();
            PBVP_LOG_INFO("Game transition cleared the Pip-Boy presentation snapshot");
            break;
        case NVSEMessagingInterface::kMessage_ExitGame:
        case NVSEMessagingInterface::kMessage_ExitGame_Console:
            g_shutdown.store(true, std::memory_order_release);
            pbvp::hooks::MarkShutdown();
            pbvp::UiBridge::Instance().Clear();
            pbvp::D3dRenderer::Instance().RequestShutdown();
            PBVP_LOG_INFO("Process shutdown requested");
            break;
        case NVSEMessagingInterface::kMessage_ReloadConfig:
            if (message->data != nullptr && message->dataLen > 0u &&
                std::strcmp(static_cast<const char*>(message->data), "PipBoyVideoPlayer") == 0) {
                PBVP_LOG_INFO("Configuration reload requested; Phase 1 has no runtime settings");
            }
            break;
        default:
            break;
    }
}

} // namespace

extern "C" bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info) {
    if (nvse == nullptr || info == nullptr) {
        return false;
    }
    info->infoVersion = PluginInfo::kInfoVersion;
    info->name = kPluginName;
    info->version = kPluginVersion;

    if (nvse->isEditor != 0u || nvse->runtimeVersion != RUNTIME_VERSION_1_4_0_525 ||
        nvse->nvseVersion < kMinimumNvseVersion) {
        return false;
    }
    return true;
}

extern "C" bool NVSEPlugin_Load(NVSEInterface* nvse) {
    if (nvse == nullptr || nvse->QueryInterface == nullptr ||
        nvse->GetPluginHandle == nullptr || nvse->GetRuntimeDirectory == nullptr) {
        return false;
    }

    g_plugin_handle = nvse->GetPluginHandle();
    auto* logging = static_cast<NVSELoggingInterface*>(nvse->QueryInterface(kInterface_Logging));
    const char* log_directory = "";
    if (logging != nullptr && logging->GetPluginLogPath != nullptr) {
        log_directory = logging->GetPluginLogPath();
    }
    pbvp::log::Open(nvse->GetRuntimeDirectory(), log_directory);
    PBVP_LOG_INFO(
        "Pip-Boy Video Player %s loading; runtime=0x%08X xNVSE=0x%08X",
        PBVP_VERSION_STRING, nvse->runtimeVersion, nvse->nvseVersion);

    g_messaging = static_cast<NVSEMessagingInterface*>(nvse->QueryInterface(kInterface_Messaging));
    if (g_messaging == nullptr || g_messaging->version < NVSEMessagingInterface::kVersion ||
        g_messaging->RegisterListener == nullptr ||
        !g_messaging->RegisterListener(g_plugin_handle, "NVSE", &HandleMessage)) {
        PBVP_LOG_ERROR("Required xNVSE messaging interface is unavailable");
        return false;
    }
    PBVP_LOG_INFO("Plugin lifecycle listener registered");
    return true;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
