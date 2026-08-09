#pragma once

#include <atomic>
#include <cstdint>

#include "pbvp/rect_math.hpp"

namespace pbvp {

struct UiRectSnapshot {
    FloatRect rect{};
    PixelExtent ui_extent{};
    bool visible{};
    std::uint32_t generation{};
    std::uint32_t game_thread_id{};
};

enum class UiSurfaceStatus : std::uint32_t {
    available,
    wrong_thread,
    map_hidden,
    menu_unavailable,
    image_unavailable,
    wrong_tile_type,
    texture_unavailable,
    wrong_texture_type,
    renderer_data_unavailable,
    wrong_renderer_data_type,
    d3d_texture_unavailable,
    access_violation,
};

struct UiSurfaceSnapshot {
    std::uintptr_t d3d_texture{};
    std::uintptr_t surface_texture_member{};
    std::uintptr_t surface_shader_member{};
    std::uintptr_t reference_texture_member{};
    std::uintptr_t reference_shader_member{};
    UiSurfaceStatus status{UiSurfaceStatus::image_unavailable};
};

const char* UiSurfaceStatusName(UiSurfaceStatus status) noexcept;

class UiBridge final {
public:
    static UiBridge& Instance() noexcept;

    void UpdateOnGameThread() noexcept;
    UiRectSnapshot ReadForRenderThread() const noexcept;
    UiSurfaceSnapshot ResolveSurfaceOnSharedThread(std::uint32_t game_thread_id) const noexcept;
    void Clear() noexcept;

private:
    UiBridge() = default;
    void RefreshSurfaceTextureOnGameThread() noexcept;
    void Publish(const UiRectSnapshot& snapshot) noexcept;

    std::atomic<std::uint32_t> sequence_{0};
    std::atomic<float> left_{0.0f};
    std::atomic<float> top_{0.0f};
    std::atomic<float> right_{0.0f};
    std::atomic<float> bottom_{0.0f};
    std::atomic<float> ui_width_{0.0f};
    std::atomic<float> ui_height_{0.0f};
    std::atomic<bool> visible_{false};
    std::atomic<std::uint32_t> generation_{0};
    std::atomic<std::uint32_t> game_thread_id_{0};
    bool polling_logged_{};
    bool map_visible_logged_{};
    std::uint32_t last_failure_{};
    bool found_logged_{};
    std::uintptr_t last_refreshed_surface_{};
    std::uint32_t surface_refresh_count_{};
    bool surface_refresh_limit_logged_{};
};

} // namespace pbvp
