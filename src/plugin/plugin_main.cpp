#include "nvse/PluginAPI.h"

#include "pbvp/d3d_renderer.hpp"
#include "pbvp/ffmpeg_runtime.hpp"
#include "pbvp/log.hpp"
#include "pbvp/ui_bridge.hpp"

#include <Windows.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>

namespace {

constexpr char kPluginName[] = "Pip-Boy Video Player";
constexpr std::uint32_t kPluginVersion = 1u;
constexpr std::uint32_t kMinimumNvseVersion = 0x06040050u;

PluginHandle g_plugin_handle = static_cast<PluginHandle>(kPluginHandle_Invalid);
NVSEMessagingInterface* g_messaging = nullptr;
std::atomic<bool> g_shutdown{false};
std::atomic<bool> g_presentation_ready{false};
pbvp::FfmpegRuntime g_ffmpeg_runtime;

std::wstring WidenRuntimeDirectory(const char* path) noexcept {
    try {
        if (path == nullptr || *path == '\0') {
            return {};
        }
        const int required = MultiByteToWideChar(CP_ACP, 0u, path, -1, nullptr, 0);
        if (required <= 1) {
            return {};
        }
        std::wstring output(static_cast<std::size_t>(required), L'\0');
        if (MultiByteToWideChar(CP_ACP, 0u, path, -1, output.data(), required) != required) {
            return {};
        }
        output.pop_back();
        return output;
    } catch (...) {
        return {};
    }
}

std::wstring PrivateFfmpegDirectory(const char* runtime_directory) noexcept {
    try {
        std::wstring path = WidenRuntimeDirectory(runtime_directory);
        if (path.empty()) {
            return {};
        }
        if (path.back() != L'\\' && path.back() != L'/') {
            path.push_back(L'\\');
        }
        path.append(L"Data\\NVSE\\Plugins\\PipBoyVideoPlayer\\bin");
        return path;
    } catch (...) {
        return {};
    }
}

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
            g_presentation_ready.store(true, std::memory_order_release);
            PBVP_LOG_INFO("xNVSE frame-present presentation path enabled without executable hooks");
            break;
        case NVSEMessagingInterface::kMessage_MainGameLoop:
            if (!g_shutdown.load(std::memory_order_acquire)) {
                pbvp::UiBridge::Instance().UpdateOnGameThread();
            }
            break;
        case NVSEMessagingInterface::kMessage_OnFramePresent: {
            bool loading_screen = true;
            if (message->data != nullptr && message->dataLen == sizeof(int)) {
                loading_screen = *static_cast<const int*>(message->data) != 0;
            }
            if (!loading_screen && g_presentation_ready.load(std::memory_order_acquire) &&
                !g_shutdown.load(std::memory_order_acquire)) {
                pbvp::D3dRenderer::Instance().OnFrame(
                    pbvp::UiBridge::Instance().ReadForRenderThread());
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
            g_presentation_ready.store(false, std::memory_order_release);
            pbvp::UiBridge::Instance().Clear();
            pbvp::D3dRenderer::Instance().RequestShutdown();
            g_ffmpeg_runtime.Unload();
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
    pbvp::FfmpegLoadFailure ffmpeg_failure{};
    const std::wstring ffmpeg_directory = PrivateFfmpegDirectory(nvse->GetRuntimeDirectory());
    if (ffmpeg_directory.empty()) {
        ffmpeg_failure.status = pbvp::FfmpegLoadStatus::path_not_absolute;
    }
    if (ffmpeg_directory.empty() || !g_ffmpeg_runtime.Load(ffmpeg_directory, ffmpeg_failure)) {
        PBVP_LOG_ERROR(
            "Private FFmpeg runtime rejected: reason=%s module=%ls symbol=%s win32=%lu",
            pbvp::FfmpegLoadStatusName(ffmpeg_failure.status),
            ffmpeg_failure.module != nullptr ? ffmpeg_failure.module : L"none",
            ffmpeg_failure.symbol != nullptr ? ffmpeg_failure.symbol : "none",
            static_cast<unsigned long>(ffmpeg_failure.windows_error));
        pbvp::log::Close();
        return false;
    }
    const pbvp::FfmpegVersions ffmpeg_versions = g_ffmpeg_runtime.Versions();
    PBVP_LOG_INFO(
        "Private FFmpeg runtime accepted: avcodec=0x%06X avformat=0x%06X avutil=0x%06X swresample=0x%06X swscale=0x%06X",
        ffmpeg_versions.avcodec, ffmpeg_versions.avformat, ffmpeg_versions.avutil,
        ffmpeg_versions.swresample, ffmpeg_versions.swscale);
    g_messaging = static_cast<NVSEMessagingInterface*>(nvse->QueryInterface(kInterface_Messaging));
    if (g_messaging == nullptr || g_messaging->version < NVSEMessagingInterface::kVersion ||
        g_messaging->RegisterListener == nullptr ||
        !g_messaging->RegisterListener(g_plugin_handle, "NVSE", &HandleMessage)) {
        PBVP_LOG_ERROR("Required xNVSE messaging interface is unavailable");
        g_ffmpeg_runtime.Unload();
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
