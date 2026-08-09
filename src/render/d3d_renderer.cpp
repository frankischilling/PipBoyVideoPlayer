#include "pbvp/d3d_renderer.hpp"

#include "pbvp/log.hpp"
#include "pbvp/rect_math.hpp"
#include "pbvp/ui_bridge.hpp"

#include <Windows.h>
#include <d3d9.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace pbvp {
namespace {

constexpr std::uintptr_t kRendererSingletonPointer = 0x011C73B4u;
constexpr std::size_t kRendererDeviceOffset = 0x288u;
constexpr UINT kTextureWidth = 256u;
constexpr UINT kTextureHeight = 256u;
constexpr DWORD kVertexFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;

struct Vertex {
    float x;
    float y;
    float z;
    float reciprocal_w;
    D3DCOLOR color;
    float u;
    float v;
};

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
        PBVP_LOG_INFO("Non-loading xNVSE frame-present callback active");
        frame_callback_logged_ = true;
    }
    if (device_lost_) {
        return;
    }

    IDirect3DDevice9* device = FindDevice();
    if (!ValidateDevice(device)) {
        if (error_count_++ < 8u) {
            PBVP_LOG_WARN("D3D frame skipped after device validation failure");
        }
        return;
    }
    if (!ui_rect.visible) {
        return;
    }
    if (!EnsureResources(device)) {
        if (error_count_++ < 8u) {
            PBVP_LOG_WARN("D3D frame skipped after resource creation failure");
        }
        return;
    }

    LARGE_INTEGER started{};
    LARGE_INTEGER finished{};
    QueryPerformanceCounter(&started);
    const bool drew = Draw(device, ui_rect);
    QueryPerformanceCounter(&finished);
    if (!drew) {
        if (error_count_++ < 8u) {
            PBVP_LOG_WARN("D3D frame skipped after draw failure");
        }
        return;
    }
    RecordDrawTiming(finished.QuadPart - started.QuadPart);
    if (!first_draw_logged_) {
        PBVP_LOG_INFO("First Pip-Boy checkerboard draw succeeded");
        first_draw_logged_ = true;
    }
    ++frame_count_;
}

void D3dRenderer::BeforeDeviceRecreate(void* renderer) noexcept {
    IDirect3DDevice9* incoming = DeviceFromRenderer(renderer);
    if (incoming == nullptr || device_ == nullptr || incoming == device_) {
        ReleaseResources();
    }
    device_lost_ = true;
    ++reset_count_;
    PBVP_LOG_INFO("D3D default-pool resources released before engine recreation %u", reset_count_);
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

bool D3dRenderer::EnsureResources(IDirect3DDevice9* device) noexcept {
    if (texture_ != nullptr) {
        return true;
    }
    const HRESULT result = device->CreateTexture(
        kTextureWidth, kTextureHeight, 1u, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT, &texture_, nullptr);
    if (FAILED(result)) {
        texture_ = nullptr;
        PBVP_LOG_ERROR("CreateTexture failed with HRESULT 0x%08X", static_cast<unsigned>(result));
        return false;
    }
    if (!UploadCheckerboard()) {
        texture_->Release();
        texture_ = nullptr;
        return false;
    }
    PBVP_LOG_INFO("Generated 256x256 D3D checkerboard texture created");
    return true;
}

bool D3dRenderer::UploadCheckerboard() noexcept {
    LARGE_INTEGER started{};
    LARGE_INTEGER finished{};
    QueryPerformanceCounter(&started);
    D3DLOCKED_RECT locked{};
    const HRESULT result = texture_->LockRect(0u, &locked, nullptr, D3DLOCK_DISCARD);
    if (FAILED(result) || locked.pBits == nullptr || locked.Pitch < static_cast<INT>(kTextureWidth * 4u)) {
        PBVP_LOG_ERROR("Texture LockRect failed with HRESULT 0x%08X", static_cast<unsigned>(result));
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
    const bool succeeded = SUCCEEDED(texture_->UnlockRect(0u));
    QueryPerformanceCounter(&finished);
    LARGE_INTEGER frequency{};
    if (succeeded && QueryPerformanceFrequency(&frequency) && frequency.QuadPart > 0) {
        const double microseconds = static_cast<double>(finished.QuadPart - started.QuadPart) *
                                    1000000.0 / static_cast<double>(frequency.QuadPart);
        PBVP_LOG_INFO("Generated texture upload took %.2f microseconds", microseconds);
    }
    return succeeded;
}

bool D3dRenderer::Draw(IDirect3DDevice9* device, const UiRectSnapshot& ui_rect) noexcept {
    D3DVIEWPORT9 viewport{};
    if (FAILED(device->GetViewport(&viewport)) || viewport.Width == 0u || viewport.Height == 0u) {
        return false;
    }

    FloatRect pixels{};
    if (!ConvertUiRectToPixels(
            ui_rect.rect, ui_rect.ui_extent,
            {static_cast<float>(viewport.Width), static_cast<float>(viewport.Height)}, pixels)) {
        return false;
    }

    IDirect3DStateBlock9* state = nullptr;
    IDirect3DSurface9* render_target = nullptr;
    IDirect3DSurface9* depth_surface = nullptr;
    if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &state)) || state == nullptr) {
        return false;
    }
    if (FAILED(state->Capture())) {
        state->Release();
        return false;
    }
    if (FAILED(device->GetRenderTarget(0u, &render_target)) || render_target == nullptr) {
        state->Release();
        return false;
    }
    device->GetDepthStencilSurface(&depth_surface);

    const std::array<Vertex, 4> vertices{{
        {pixels.left - 0.5f, pixels.top - 0.5f, 0.0f, 1.0f, 0xFFFFFFFFu, 0.0f, 0.0f},
        {pixels.right - 0.5f, pixels.top - 0.5f, 0.0f, 1.0f, 0xFFFFFFFFu, 1.0f, 0.0f},
        {pixels.left - 0.5f, pixels.bottom - 0.5f, 0.0f, 1.0f, 0xFFFFFFFFu, 0.0f, 1.0f},
        {pixels.right - 0.5f, pixels.bottom - 0.5f, 0.0f, 1.0f, 0xFFFFFFFFu, 1.0f, 1.0f},
    }};

    device->SetVertexShader(nullptr);
    device->SetPixelShader(nullptr);
    device->SetVertexDeclaration(nullptr);
    device->SetFVF(kVertexFvf);
    device->SetTexture(0u, texture_);
    device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    device->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0Fu);
    device->SetSamplerState(0u, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0u, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0u, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    device->SetTextureStageState(0u, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0u, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0u, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0u, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(1u, D3DTSS_COLOROP, D3DTOP_DISABLE);
    const HRESULT draw_result = device->DrawPrimitiveUP(
        D3DPT_TRIANGLESTRIP, 2u, vertices.data(), sizeof(Vertex));

    const HRESULT apply_result = state->Apply();
    const HRESULT render_target_result = device->SetRenderTarget(0u, render_target);
    render_target->Release();
    HRESULT depth_result = D3D_OK;
    if (depth_surface != nullptr) {
        depth_result = device->SetDepthStencilSurface(depth_surface);
        depth_surface->Release();
    }
    state->Release();
    return SUCCEEDED(draw_result) && SUCCEEDED(apply_result) &&
           SUCCEEDED(render_target_result) && SUCCEEDED(depth_result);
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

void D3dRenderer::RecordDrawTiming(const std::int64_t elapsed_ticks) noexcept {
    if (elapsed_ticks < 0) {
        return;
    }
    if (performance_frequency_ == 0) {
        LARGE_INTEGER frequency{};
        if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0) {
            performance_frequency_ = -1;
            return;
        }
        performance_frequency_ = frequency.QuadPart;
    }
    if (performance_frequency_ < 0) {
        return;
    }

    timing_total_ticks_ += elapsed_ticks;
    if (elapsed_ticks > timing_max_ticks_) {
        timing_max_ticks_ = elapsed_ticks;
    }
    if (++timing_samples_ >= 300u) {
        const double average_microseconds = static_cast<double>(timing_total_ticks_) * 1000000.0 /
                                            static_cast<double>(performance_frequency_) /
                                            static_cast<double>(timing_samples_);
        const double maximum_microseconds = static_cast<double>(timing_max_ticks_) * 1000000.0 /
                                            static_cast<double>(performance_frequency_);
        PBVP_LOG_INFO(
            "D3D draw timing over %u frames: average=%.2f microseconds maximum=%.2f microseconds",
            timing_samples_, average_microseconds, maximum_microseconds);
        timing_samples_ = 0u;
        timing_total_ticks_ = 0;
        timing_max_ticks_ = 0;
    }
}

void D3dRenderer::ReleaseResources() noexcept {
    if (texture_ != nullptr) {
        texture_->Release();
        texture_ = nullptr;
    }
    device_ = nullptr;
}

} // namespace pbvp
