#include "pbvp/d3d_renderer.hpp"

#include "pbvp/checked_math.hpp"
#include "pbvp/log.hpp"
#include "pbvp/texture_contract.hpp"
#include "pbvp/ui_bridge.hpp"
#include "pbvp/video_scaler.hpp"

#include <Windows.h>
#include <d3d9.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

namespace pbvp {
namespace {

constexpr std::uintptr_t kRendererSingletonPointer = 0x011C73B4u;
constexpr std::size_t kRendererDeviceOffset = 0x288u;
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

bool D3dRenderer::SubmitVideoFrame(DecodedVideoFrame frame) noexcept {
    std::size_t row_bytes = 0u;
    std::size_t required_bytes = 0u;
    std::size_t preceding_rows = 0u;
    if (frame.width == 0u || frame.height == 0u || frame.stride == 0u ||
        !CheckedMultiplySize(static_cast<std::size_t>(frame.width), 4u, row_bytes) ||
        frame.stride < row_bytes ||
        !CheckedMultiplySize(
            static_cast<std::size_t>(frame.height - 1u),
            static_cast<std::size_t>(frame.stride), preceding_rows) ||
        !CheckedAddSize(preceding_rows, row_bytes, required_bytes) ||
        frame.bgra.size() < required_bytes || frame.generation == 0u || frame.pts_us < 0) {
        return false;
    }
    try {
        std::scoped_lock lock(mailbox_mutex_);
        if (pending_frame_.has_value()) {
            ++mailbox_replacement_count_;
        }
        pending_frame_ = std::move(frame);
        ++submitted_video_frame_count_;
        clear_requested_.store(false, std::memory_order_release);
        return true;
    } catch (...) {
        return false;
    }
}

void D3dRenderer::ConfigurePresentation(
    const AspectMode aspect_mode,
    const TintMode tint_mode) noexcept {
    aspect_mode_.store(
        static_cast<std::uint32_t>(aspect_mode), std::memory_order_release);
    tint_mode_.store(
        static_cast<std::uint32_t>(tint_mode), std::memory_order_release);
}

void D3dRenderer::ClearVideoFrame() noexcept {
    try {
        std::scoped_lock lock(mailbox_mutex_);
        if (pending_frame_.has_value()) {
            ++mailbox_clear_count_;
        }
        pending_frame_.reset();
    } catch (...) {
    }
    clear_requested_.store(true, std::memory_order_release);
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
    if (clear_requested_.exchange(false, std::memory_order_acq_rel)) {
        presentation_pixels_valid_ = false;
        video_pixels_ready_ = false;
        last_surface_ = 0u;
        upload_retry_pending_ = true;
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

    bool new_video_pixels = false;
    std::optional<DecodedVideoFrame> pending = TakePendingFrame();
    if (pending.has_value()) {
        const PixelExtent presentation_extent{
            ui_rect.rect.right - ui_rect.rect.left,
            ui_rect.rect.bottom - ui_rect.rect.top,
        };
        new_video_pixels = PrepareVideoPixels(*pending, presentation_extent);
        if (!new_video_pixels && error_count_++ < 8u) {
            PBVP_LOG_WARN(
                "Decoded video frame failed the bounded presentation scaling contract");
        }
    }

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
        upload_retry_pending_ = true;
        return;
    }
    last_surface_status_ = 0u;

    IDirect3DDevice9* device = FindDevice();
    if (!ValidateDevice(device)) {
        if (error_count_++ < 8u) {
            PBVP_LOG_WARN("Texture upload skipped after device validation failure");
        }
        upload_retry_pending_ = true;
        return;
    }
    const bool surface_changed = surface.d3d_texture != last_surface_;
    if (!surface_changed && !new_video_pixels && !upload_retry_pending_) {
        return;
    }
    if (!presentation_pixels_valid_) {
        PrepareCheckerboard();
    }

    ++upload_attempt_count_;
    if (!UploadPixels(device, surface.d3d_texture)) {
        ++upload_failure_count_;
        upload_retry_pending_ = true;
        if (error_count_++ < 8u) {
            PBVP_LOG_WARN("Presentation pixels could not be uploaded to the engine image");
        }
        return;
    }
    ++upload_success_count_;
    if (video_pixels_ready_) {
        ++uploaded_video_frame_count_;
        if (!video_upload_logged_) {
            PBVP_LOG_INFO("Decoded BGRA video reached PBVP_VideoSurface");
            video_upload_logged_ = true;
        }
    } else if (surface_changed) {
        PBVP_LOG_INFO("Generated checkerboard uploaded to PBVP_VideoSurface");
    }
    last_surface_ = surface.d3d_texture;
    upload_retry_pending_ = false;
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
        cadence_minimum_fps_ = (std::min)(cadence_minimum_fps_, sample.frames_per_second);
        cadence_maximum_fps_ = (std::max)(cadence_maximum_fps_, sample.frames_per_second);
    }
    cadence_total_fps_ += sample.frames_per_second;
    ++cadence_sample_count_;
    PBVP_LOG_INFO(
        "Visible frame cadence: frames=%u elapsed-ms=%.2f fps=%.2f",
        sample.frames, sample.elapsed_seconds * 1000.0, sample.frames_per_second);
}

void D3dRenderer::RequestShutdown() noexcept {
    shutdown_requested_ = true;
    ClearVideoFrame();
    presentation_pixels_valid_ = false;
    video_pixels_ready_ = false;
    ReleaseResources();
    LogSessionSummary();
}

D3dRendererSnapshot D3dRenderer::Snapshot() const noexcept {
    D3dRendererSnapshot result{};
    result.frame_callbacks = frame_callback_count_;
    result.visible_frames = visible_frame_count_;
    result.submitted_video_frames = submitted_video_frame_count_;
    result.replaced_mailbox_frames = mailbox_replacement_count_;
    result.cleared_mailbox_frames = mailbox_clear_count_;
    result.uploaded_video_frames = uploaded_video_frame_count_;
    result.upload_attempts = upload_attempt_count_;
    result.upload_successes = upload_success_count_;
    result.upload_failures = upload_failure_count_;
    result.last_video_generation = last_video_generation_;
    result.last_video_pts_us = last_video_pts_us_;
    result.upload_minimum_us = upload_minimum_microseconds_;
    result.upload_average_us = upload_timing_count_ > 0u
        ? upload_total_microseconds_ / static_cast<double>(upload_timing_count_)
        : 0.0;
    result.upload_maximum_us = upload_maximum_microseconds_;
    result.video_pixels_ready = video_pixels_ready_;
    try {
        std::scoped_lock lock(mailbox_mutex_);
        result.mailbox_occupied = pending_frame_.has_value();
    } catch (...) {
    }
    return result;
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
    upload_retry_pending_ = true;
    return true;
}

std::optional<DecodedVideoFrame> D3dRenderer::TakePendingFrame() noexcept {
    try {
        std::scoped_lock lock(mailbox_mutex_);
        if (!pending_frame_.has_value()) {
            return std::nullopt;
        }
        std::optional<DecodedVideoFrame> result{std::move(pending_frame_)};
        pending_frame_.reset();
        return result;
    } catch (...) {
        return std::nullopt;
    }
}

bool D3dRenderer::PrepareVideoPixels(
    const DecodedVideoFrame& frame,
    const PixelExtent presentation_extent) noexcept {
    const VideoScaleMode scale_mode =
        aspect_mode_.load(std::memory_order_acquire) ==
                static_cast<std::uint32_t>(AspectMode::fill)
            ? VideoScaleMode::fill
            : VideoScaleMode::fit;
    const VideoColorMode color_mode =
        tint_mode_.load(std::memory_order_acquire) ==
                static_cast<std::uint32_t>(TintMode::full_color)
            ? VideoColorMode::full_color
            : VideoColorMode::pipboy_luminance;
    const VideoScaleResult scaled = ScaleBgraForPresentation(
        frame.bgra, frame.width, frame.height, frame.stride,
        presentation_pixels_, kTextureWidth, kTextureHeight, kTextureWidth * 4u,
        presentation_extent, scale_mode, color_mode);
    if (scaled.status != VideoScaleStatus::ok) {
        return false;
    }
    presentation_pixels_valid_ = true;
    video_pixels_ready_ = true;
    upload_retry_pending_ = true;
    last_video_generation_ = frame.generation;
    last_video_pts_us_ = frame.pts_us;
    return true;
}

void D3dRenderer::PrepareCheckerboard() noexcept {
    for (std::uint32_t y = 0u; y < kTextureHeight; ++y) {
        auto* row = presentation_pixels_.data() +
            static_cast<std::size_t>(y) * kTextureWidth * 4u;
        for (std::uint32_t x = 0u; x < kTextureWidth; ++x) {
            const bool light = ((x / 32u) + (y / 32u)) % 2u == 0u;
            const std::uint32_t pixel = light ? 0xFF42F56Cu : 0xFF102818u;
            std::memcpy(row + static_cast<std::size_t>(x) * 4u, &pixel, sizeof(pixel));
        }
    }
    presentation_pixels_valid_ = true;
    video_pixels_ready_ = false;
}

bool D3dRenderer::UploadPixels(
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
    const TexturePixelFormat pixel_format =
        description.Format == D3DFMT_A8R8G8B8
            ? TexturePixelFormat::argb8
            : (description.Format == D3DFMT_X8R8G8B8 ? TexturePixelFormat::xrgb8
                                                       : TexturePixelFormat::unsupported);
    const TextureMemoryPool memory_pool =
        description.Pool == D3DPOOL_MANAGED ? TextureMemoryPool::managed
                                             : TextureMemoryPool::unsupported;
    if (!AcceptEngineVideoTexture(
            description.Width, description.Height, pixel_format, memory_pool)) {
        texture->Release();
        PBVP_LOG_ERROR("PBVP_VideoSurface must be a 256x256 managed ARGB or XRGB texture");
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

    auto* destination = static_cast<std::uint8_t*>(locked.pBits);
    const auto* source = presentation_pixels_.data();
    for (std::uint32_t y = 0u; y < kTextureHeight; ++y) {
        std::memcpy(destination, source, static_cast<std::size_t>(kTextureWidth) * 4u);
        destination += locked.Pitch;
        source += static_cast<std::size_t>(kTextureWidth) * 4u;
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
                                    1'000'000.0 / static_cast<double>(frequency.QuadPart);
        RecordUploadDuration(microseconds);
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
        upload_minimum_microseconds_ = (std::min)(upload_minimum_microseconds_, microseconds);
        upload_maximum_microseconds_ = (std::max)(upload_maximum_microseconds_, microseconds);
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
        ? upload_total_microseconds_ / static_cast<double>(upload_timing_count_)
        : 0.0;
    PBVP_LOG_INFO(
        "Renderer summary: callbacks=%llu visible=%llu devices=%u video-submitted=%llu video-uploaded=%llu mailbox-replaced=%llu mailbox-cleared=%llu upload-successes=%llu upload-attempts=%llu upload-failures=%llu upload-us=%.2f/%.2f/%.2f",
        static_cast<unsigned long long>(frame_callback_count_),
        static_cast<unsigned long long>(visible_frame_count_), device_validation_count_,
        static_cast<unsigned long long>(submitted_video_frame_count_),
        static_cast<unsigned long long>(uploaded_video_frame_count_),
        static_cast<unsigned long long>(mailbox_replacement_count_),
        static_cast<unsigned long long>(mailbox_clear_count_),
        static_cast<unsigned long long>(upload_success_count_),
        static_cast<unsigned long long>(upload_attempt_count_),
        static_cast<unsigned long long>(upload_failure_count_),
        upload_minimum_microseconds_, average, upload_maximum_microseconds_);
    const double cadence_average = cadence_sample_count_ > 0u
        ? cadence_total_fps_ / static_cast<double>(cadence_sample_count_)
        : 0.0;
    PBVP_LOG_INFO(
        "Visible cadence summary: samples=%u fps=%.2f/%.2f/%.2f",
        cadence_sample_count_, cadence_minimum_fps_, cadence_average,
        cadence_maximum_fps_);
}

void D3dRenderer::ReleaseResources() noexcept {
    cadence_tracker_.Reset();
    device_ = nullptr;
    last_surface_ = 0u;
    last_surface_status_ = 0u;
    upload_retry_pending_ = true;
}

} // namespace pbvp
