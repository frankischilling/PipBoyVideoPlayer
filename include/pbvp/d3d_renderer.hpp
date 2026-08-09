#pragma once

#include <cstdint>

struct IDirect3DDevice9;
struct IDirect3DTexture9;

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
    bool EnsureResources(IDirect3DDevice9* device) noexcept;
    bool UploadCheckerboard() noexcept;
    bool Draw(IDirect3DDevice9* device, const UiRectSnapshot& ui_rect) noexcept;
    void ReleaseResources() noexcept;

    IDirect3DDevice9* device_{};
    IDirect3DTexture9* texture_{};
    std::uint32_t frame_count_{};
    std::uint32_t error_count_{};
    bool shutdown_requested_{};
    bool device_lost_{};
};

} // namespace pbvp
