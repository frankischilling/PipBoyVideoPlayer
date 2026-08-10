#include "pbvp/d3d_renderer.hpp"

#include "pbvp/log.hpp"
#include "pbvp/recreate_result.hpp"
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
constexpr std::uint32_t kMaximumCadenceSamples = 8u;

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
    ++frame_callback_count_;
    if (!frame_callback_logged_) {
        PBVP_LOG_INFO("xNVSE frame-present texture upload boundary active");
        frame_callback_logged_ = true;
    }
    if (device_lost_) {
        cadence_tracker_.Reset();
        return;
    }
    if (!ui_rect.visible) {
        cadence_tracker_.Reset();
        last_surface_ = 0u;
        last_surface_status_ = 0u;
        return;
    }
    ++visible_frame_count_;

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
    RecordVisibleCadence();

    const UiSurfaceSnapshot surface =
        UiBridge::Instance().ResolveSurfaceOnSharedThread(ui_rect.game_thread_id);
    if (surface.status != UiSurfaceStatus::available) {
        const auto status = static_cast<std::uint32_t>(surface.status) + 1u;
        if (last_surface_status_ != status && error_count_++ < 8u) {
            PBVP_LOG_WARN("Pip-Boy engine texture unavailable: %s", UiSurfaceStatusName(surface.status));
            if (surface.direct_texture != 0u || surface.shader_property != 0u ||
                surface.shader_source_texture != 0u) {
                PBVP_LOG_INFO(
                    "UI surface chain: direct=0x%08X direct-vtbl=0x%08X shader=0x%08X shader-vtbl=0x%08X source=0x%08X source-vtbl=0x%08X",
                    static_cast<unsigned>(surface.direct_texture),
                    static_cast<unsigned>(surface.direct_texture_vtable),
                    static_cast<unsigned>(surface.shader_property),
                    static_cast<unsigned>(surface.shader_property_vtable),
                    static_cast<unsigned>(surface.shader_source_texture),
                    static_cast<unsigned>(surface.shader_source_texture_vtable));
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
    ++upload_attempt_count_;
    if (!UploadCheckerboard(device, surface.d3d_texture)) {
        ++upload_failure_count_;
        if (error_count_++ < 8u) {
            PBVP_LOG_WARN("Generated checkerboard upload to the engine image failed");
        }
        return;
    }
    ++upload_success_count_;
    last_surface_ = surface.d3d_texture;
    PBVP_LOG_INFO("Generated checkerboard uploaded to PBVP_VideoSurface");
}

void D3dRenderer::RecordVisibleCadence() noexcept {
    if (cadence_sample_count_ >= kMaximumCadenceSamples) {
        return;
    }
    if (cadence_frequency_ <= 0) {
        LARGE_INTEGER frequency{};
        if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0) {
            return;
        }
        cadence_frequency_ = frequency.QuadPart;
    }

    LARGE_INTEGER counter{};
    if (!QueryPerformanceCounter(&counter)) {
        cadence_tracker_.Reset();
        return;
    }
    const FrameCadenceSample sample =
        cadence_tracker_.Observe(counter.QuadPart, cadence_frequency_);
    if (!sample.ready || sample.frames_per_second < 0.0) {
        return;
    }

    if (cadence_sample_count_ == 0u) {
        cadence_minimum_fps_ = sample.frames_per_second;
        cadence_maximum_fps_ = sample.frames_per_second;
    } else {
        if (sample.frames_per_second < cadence_minimum_fps_) {
            cadence_minimum_fps_ = sample.frames_per_second;
        }
        if (sample.frames_per_second > cadence_maximum_fps_) {
            cadence_maximum_fps_ = sample.frames_per_second;
        }
    }
    cadence_total_fps_ += sample.frames_per_second;
    ++cadence_sample_count_;
    PBVP_LOG_INFO(
        "Visible frame cadence: frames=%u elapsed-ms=%.2f fps=%.2f",
        sample.frames, sample.elapsed_seconds * 1000.0, sample.frames_per_second);
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
    const RecreateResult classification = ClassifyRecreateResult(result);
    if (classification == RecreateResult::failed) {
        ++recreate_failure_count_;
        PBVP_LOG_WARN("D3D engine recreation failed; texture uploads remain disabled");
        return;
    }
    if (classification == RecreateResult::unknown) {
        ++recreate_failure_count_;
        PBVP_LOG_ERROR(
            "D3D engine recreation returned unexpected value %u; texture uploads remain disabled",
            result);
        return;
    }
    if (DeviceFromRenderer(renderer) == nullptr) {
        ++recreate_failure_count_;
        PBVP_LOG_ERROR(
            "D3D engine recreation returned %u without publishing a device; texture uploads remain disabled",
            result);
        return;
    }

    device_ = nullptr;
    device_lost_ = false;
    ++recreate_success_count_;
    if (classification == RecreateResult::recovered) {
        PBVP_LOG_INFO(
            "D3D engine recreation recovered the original presentation parameters; resources will be reacquired");
    } else {
        PBVP_LOG_INFO(
            "D3D engine recreation applied the requested presentation parameters; resources will be reacquired");
    }
}

void D3dRenderer::RequestShutdown() noexcept {
    shutdown_requested_ = true;
    ReleaseResources();
    LogSessionSummary();
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
    ++device_validation_count_;
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
        RecordUploadDuration(microseconds);
        PBVP_LOG_INFO("Engine texture checkerboard upload took %.2f microseconds", microseconds);
    }
    return true;
}

void D3dRenderer::RecordUploadDuration(const double microseconds) noexcept {
    if (microseconds < 0.0) {
        return;
    }
    if (upload_timing_count_ == 0u) {
        upload_minimum_microseconds_ = microseconds;
        upload_maximum_microseconds_ = microseconds;
    } else {
        if (microseconds < upload_minimum_microseconds_) {
            upload_minimum_microseconds_ = microseconds;
        }
        if (microseconds > upload_maximum_microseconds_) {
            upload_maximum_microseconds_ = microseconds;
        }
    }
    upload_total_microseconds_ += microseconds;
    ++upload_timing_count_;
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

void D3dRenderer::LogSessionSummary() noexcept {
    if (summary_logged_) {
        return;
    }
    summary_logged_ = true;
    const double average = upload_timing_count_ > 0u
                               ? upload_total_microseconds_ /
                                     static_cast<double>(upload_timing_count_)
                               : 0.0;
    PBVP_LOG_INFO(
        "Phase 1 renderer summary: callbacks=%llu visible=%llu devices=%u upload-successes=%llu upload-attempts=%llu upload-failures=%llu upload-us=%.2f/%.2f/%.2f recreation-successes=%u recreation-starts=%u recreation-failures=%u",
        static_cast<unsigned long long>(frame_callback_count_),
        static_cast<unsigned long long>(visible_frame_count_), device_validation_count_,
        static_cast<unsigned long long>(upload_success_count_),
        static_cast<unsigned long long>(upload_attempt_count_),
        static_cast<unsigned long long>(upload_failure_count_), upload_minimum_microseconds_,
        average, upload_maximum_microseconds_, recreate_success_count_, reset_count_,
        recreate_failure_count_);
    const double cadence_average = cadence_sample_count_ > 0u
                                       ? cadence_total_fps_ /
                                             static_cast<double>(cadence_sample_count_)
                                       : 0.0;
    PBVP_LOG_INFO(
        "Phase 1 cadence summary: samples=%u fps=%.2f/%.2f/%.2f",
        cadence_sample_count_, cadence_minimum_fps_, cadence_average,
        cadence_maximum_fps_);
}

void D3dRenderer::ReleaseResources() noexcept {
    cadence_tracker_.Reset();
    device_ = nullptr;
    last_surface_ = 0u;
    last_surface_status_ = 0u;
}

} // namespace pbvp
