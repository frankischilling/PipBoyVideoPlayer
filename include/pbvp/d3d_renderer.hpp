#pragma once

#include <cstdint>

struct IDirect3DDevice9;

namespace pbvp {

struct UiRectSnapshot;

class D3dRenderer final {
public:
    static D3dRenderer& Instance() noexcept;

    void OnFrame(const UiRectSnapshot& ui_rect) noexcept;
    void BeforeDeviceRecreate(void* renderer) noexcept;
    void AfterDeviceRecreate(void* renderer, std::uint32_t result) noexcept;
    void RequestShutdown() noexcept;

private:
    D3dRenderer() = default;

    IDirect3DDevice9* FindDevice() noexcept;
    bool ValidateDevice(IDirect3DDevice9* device) noexcept;
    bool UploadCheckerboard(IDirect3DDevice9* device, std::uintptr_t surface) noexcept;
    void RecordUploadDuration(double microseconds) noexcept;
    void LogDeviceProfile(IDirect3DDevice9* device) noexcept;
    void LogSessionSummary() noexcept;
    void ReleaseResources() noexcept;

    IDirect3DDevice9* device_{};
    std::uintptr_t last_surface_{};
    std::uint64_t frame_callback_count_{};
    std::uint64_t visible_frame_count_{};
    std::uint64_t upload_attempt_count_{};
    std::uint64_t upload_success_count_{};
    std::uint64_t upload_failure_count_{};
    std::uint64_t upload_timing_count_{};
    double upload_total_microseconds_{};
    double upload_minimum_microseconds_{};
    double upload_maximum_microseconds_{};
    std::uint32_t error_count_{};
    std::uint32_t device_validation_count_{};
    std::uint32_t reset_count_{};
    std::uint32_t recreate_success_count_{};
    std::uint32_t recreate_failure_count_{};
    std::uint32_t last_surface_status_{};
    bool shutdown_requested_{};
    bool device_lost_{};
    bool frame_callback_logged_{};
    bool thread_identity_logged_{};
    bool summary_logged_{};
};

} // namespace pbvp
