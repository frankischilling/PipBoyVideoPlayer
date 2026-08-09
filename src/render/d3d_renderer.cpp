#include "pbvp/d3d_renderer.hpp"

#include "pbvp/log.hpp"
#include "pbvp/ui_bridge.hpp"

#include <Windows.h>
#include <d3d9.h>

#include <cstddef>
#include <cstdint>

namespace pbvp {
namespace {

constexpr std::uintptr_t kRendererSingletonPointer = 0x011C73B4u;
constexpr std::size_t kRendererDeviceOffset = 0x288u;
constexpr UINT kTextureWidth = 256u;
constexpr UINT kTextureHeight = 256u;

IDirect3DDevice9* DeviceFromRenderer(void* renderer) noexcept {
    if (renderer == nullptr) {
        return nullptr;
    }
    __try {
        auto* bytes = static_cast<std::byte*>(renderer);
        return *reinterpret_cast<IDirect3DDevice9**>(bytes + kRendererDeviceOffset);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

IDirect3DTexture9* AcquireTexture(const std::uintptr_t surface) noexcept {
    if (surface == 0u) {
        return nullptr;
    }
    void* queried = nullptr;
    __try {
        auto* base_texture = reinterpret_cast<IDirect3DBaseTexture9*>(surface);
        if (FAILED(base_texture->QueryInterface(__uuidof(IDirect3DTexture9), &queried))) {
            return nullptr;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
    return static_cast<IDirect3DTexture9*>(queried);
}

} // namespace

D3dRenderer& D3dRenderer::Instance() noexcept {
    static D3dRenderer renderer;
    return renderer;
}

void D3dRenderer::OnFrame(const UiRectSnapshot& ui_rect) noexcept {
    if (shutdown_requested_) {
        ReleaseResources();
        return;
    }
    if (!frame_callback_logged_) {
        PBVP_LOG_INFO("xNVSE frame-present texture upload boundary active");
        frame_callback_logged_ = true;
    }
    if (device_lost_) {
        return;
    }
    if (!ui_rect.visible) {
        last_surface_ = 0u;
        last_surface_status_ = 0u;
        return;
    }

    const std::uint32_t current_thread = GetCurrentThreadId();
    if (ui_rect.game_thread_id == 0u || current_thread != ui_rect.game_thread_id) {
        if (error_count_++ < 8u) {
            PBVP_LOG_ERROR(
                "Texture upload refused because game thread %u and render thread %u differ",
                ui_rect.game_thread_id, current_thread);
        }
        return;
    }
    if (!thread_identity_logged_) {
        PBVP_LOG_INFO("Game and Direct3D callbacks share thread %u", current_thread);
        thread_identity_logged_ = true;
    }

    const UiSurfaceSnapshot surface =
        UiBridge::Instance().ResolveSurfaceOnSharedThread(ui_rect.game_thread_id);
    if (surface.status != UiSurfaceStatus::available) {
        const auto status = static_cast<std::uint32_t>(surface.status) + 1u;
        if (last_surface_status_ != status && error_count_++ < 8u) {
            PBVP_LOG_WARN("Pip-Boy engine texture unavailable: %s", UiSurfaceStatusName(surface.status));
            if (surface.status == UiSurfaceStatus::texture_unavailable) {
                PBVP_LOG_INFO(
                    "UI image field check at upload: target[3C]=0x%08X target[40]=0x%08X reference[3C]=0x%08X reference[40]=0x%08X",
                    static_cast<unsigned>(surface.surface_texture_member),
                    static_cast<unsigned>(surface.surface_shader_member),
                    static_cast<unsigned>(surface.reference_texture_member),
                    static_cast<unsigned>(surface.reference_shader_member));
            }
        }
        last_surface_status_ = status;
        last_surface_ = 0u;
        return;
    }
    last_surface_status_ = 0u;

    IDirect3DDevice9* device = FindDevice();
    if (!ValidateDevice(device)) {
        if (error_count_++ < 8u) {
            PBVP_LOG_WARN("Texture upload skipped after device validation failure");
        }
        return;
    }
    if (surface.d3d_texture == last_surface_) {
        return;
    }
    if (!UploadCheckerboard(device, surface.d3d_texture)) {
        if (error_count_++ < 8u) {
            PBVP_LOG_WARN("Generated checkerboard upload to the engine image failed");
        }
        return;
    }
    last_surface_ = surface.d3d_texture;
    PBVP_LOG_INFO("Generated checkerboard uploaded to PBVP_VideoSurface");
}

void D3dRenderer::BeforeDeviceRecreate(void* renderer) noexcept {
    static_cast<void>(renderer);
    ReleaseResources();
    device_lost_ = true;
    ++reset_count_;
    PBVP_LOG_INFO(
        "Transient engine-surface state cleared before engine recreation %u", reset_count_);
}

void D3dRenderer::AfterDeviceRecreate(void* renderer, const std::uint32_t result) noexcept {
    if (result != 0u) {
        device_ = DeviceFromRenderer(renderer);
        device_lost_ = false;
        PBVP_LOG_INFO("D3D engine recreation succeeded with result %u", result);
    } else {
        PBVP_LOG_WARN("D3D engine recreation failed");
    }
}

void D3dRenderer::RequestShutdown() noexcept {
    shutdown_requested_ = true;
    ReleaseResources();
}

IDirect3DDevice9* D3dRenderer::FindDevice() noexcept {
    __try {
        void* renderer = *reinterpret_cast<void**>(kRendererSingletonPointer);
        return DeviceFromRenderer(renderer);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

bool D3dRenderer::ValidateDevice(IDirect3DDevice9* device) noexcept {
    if (device == nullptr) {
        return false;
    }
    if (device_ == device) {
        return true;
    }
    __try {
        D3DDEVICE_CREATION_PARAMETERS parameters{};
        if (FAILED(device->GetCreationParameters(&parameters)) || parameters.hFocusWindow == nullptr) {
            return false;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    ReleaseResources();
    device_ = device;
    LogDeviceProfile(device);
    return true;
}

bool D3dRenderer::UploadCheckerboard(
    IDirect3DDevice9* device,
    const std::uintptr_t surface) noexcept {
    IDirect3DTexture9* texture = AcquireTexture(surface);
    if (texture == nullptr) {
        PBVP_LOG_ERROR("PBVP_VideoSurface is not an IDirect3DTexture9 resource");
        return false;
    }

    IDirect3DDevice9* texture_device = nullptr;
    const HRESULT device_result = texture->GetDevice(&texture_device);
    const bool device_matches =
        SUCCEEDED(device_result) && texture_device != nullptr && texture_device == device;
    if (texture_device != nullptr) {
        texture_device->Release();
    }
    if (!device_matches) {
        texture->Release();
        PBVP_LOG_ERROR("PBVP_VideoSurface belongs to an unexpected Direct3D device");
        return false;
    }

    D3DSURFACE_DESC description{};
    if (FAILED(texture->GetLevelDesc(0u, &description))) {
        texture->Release();
        PBVP_LOG_ERROR("PBVP_VideoSurface level description is unavailable");
        return false;
    }
    PBVP_LOG_INFO(
        "PBVP_VideoSurface profile: size=%ux%u format=%u pool=%u usage=0x%08X",
        description.Width, description.Height, static_cast<unsigned>(description.Format),
        static_cast<unsigned>(description.Pool), static_cast<unsigned>(description.Usage));
    if (description.Width != kTextureWidth || description.Height != kTextureHeight ||
        (description.Format != D3DFMT_A8R8G8B8 && description.Format != D3DFMT_X8R8G8B8)) {
        texture->Release();
        PBVP_LOG_ERROR("PBVP_VideoSurface has an unsupported size or format");
        return false;
    }

    LARGE_INTEGER started{};
    LARGE_INTEGER finished{};
    QueryPerformanceCounter(&started);
    D3DLOCKED_RECT locked{};
    const HRESULT lock_result = texture->LockRect(0u, &locked, nullptr, 0u);
    if (FAILED(lock_result) || locked.pBits == nullptr ||
        locked.Pitch < static_cast<INT>(kTextureWidth * 4u)) {
        texture->Release();
        PBVP_LOG_ERROR(
            "PBVP_VideoSurface LockRect failed with HRESULT 0x%08X",
            static_cast<unsigned>(lock_result));
        return false;
    }

    auto* row = static_cast<std::uint8_t*>(locked.pBits);
    for (UINT y = 0; y < kTextureHeight; ++y) {
        auto* pixels = reinterpret_cast<std::uint32_t*>(row);
        for (UINT x = 0; x < kTextureWidth; ++x) {
            const bool light = ((x / 32u) + (y / 32u)) % 2u == 0u;
            pixels[x] = light ? 0xFF42F56Cu : 0xFF102818u;
        }
        row += locked.Pitch;
    }
    const HRESULT unlock_result = texture->UnlockRect(0u);
    texture->Release();
    QueryPerformanceCounter(&finished);
    if (FAILED(unlock_result)) {
        PBVP_LOG_ERROR(
            "PBVP_VideoSurface UnlockRect failed with HRESULT 0x%08X",
            static_cast<unsigned>(unlock_result));
        return false;
    }

    LARGE_INTEGER frequency{};
    if (QueryPerformanceFrequency(&frequency) && frequency.QuadPart > 0) {
        const double microseconds = static_cast<double>(finished.QuadPart - started.QuadPart) *
                                    1000000.0 / static_cast<double>(frequency.QuadPart);
        PBVP_LOG_INFO("Engine texture checkerboard upload took %.2f microseconds", microseconds);
    }
    return true;
}

void D3dRenderer::LogDeviceProfile(IDirect3DDevice9* device) noexcept {
    D3DDEVICE_CREATION_PARAMETERS creation{};
    D3DPRESENT_PARAMETERS presentation{};
    D3DADAPTER_IDENTIFIER9 adapter{};
    const char* description = "unknown";
    const char* driver = "unknown";
    const char* mode = "unknown";

    if (SUCCEEDED(device->GetCreationParameters(&creation))) {
        IDirect3D9* d3d = nullptr;
        if (SUCCEEDED(device->GetDirect3D(&d3d)) && d3d != nullptr) {
            if (SUCCEEDED(d3d->GetAdapterIdentifier(creation.AdapterOrdinal, 0u, &adapter))) {
                description = adapter.Description;
                driver = adapter.Driver;
            }
            d3d->Release();
        }
    }

    IDirect3DSwapChain9* swap_chain = nullptr;
    if (SUCCEEDED(device->GetSwapChain(0u, &swap_chain)) && swap_chain != nullptr) {
        if (SUCCEEDED(swap_chain->GetPresentParameters(&presentation))) {
            mode = presentation.Windowed ? "windowed" : "fullscreen";
        }
        swap_chain->Release();
    }

    PBVP_LOG_INFO(
        "D3D device validated: adapter=%s driver=%s mode=%s backbuffer=%ux%u format=%u interval=0x%08X",
        description, driver, mode, presentation.BackBufferWidth, presentation.BackBufferHeight,
        static_cast<unsigned>(presentation.BackBufferFormat),
        static_cast<unsigned>(presentation.PresentationInterval));
}

void D3dRenderer::ReleaseResources() noexcept {
    device_ = nullptr;
    last_surface_ = 0u;
    last_surface_status_ = 0u;
}

} // namespace pbvp
