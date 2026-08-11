#pragma once

#include <atomic>
#include <cstdint>

#include "pbvp/playback_state.hpp"
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
    source_texture_unavailable,
    wrong_shader_property_type,
    wrong_source_texture_type,
    renderer_data_unavailable,
    wrong_renderer_data_type,
    d3d_texture_unavailable,
    access_violation,
};

struct UiSurfaceSnapshot {
    std::uintptr_t d3d_texture{};
    std::uintptr_t direct_texture{};
    std::uintptr_t direct_texture_vtable{};
    std::uintptr_t shader_property{};
    std::uintptr_t shader_property_vtable{};
    std::uintptr_t shader_source_texture{};
    std::uintptr_t shader_source_texture_vtable{};
    UiSurfaceStatus status{UiSurfaceStatus::image_unavailable};
};

const char* UiSurfaceStatusName(UiSurfaceStatus status) noexcept;

class UiBridge final {
public:
    static UiBridge& Instance() noexcept;

    void UpdateOnGameThread() noexcept;
    [[nodiscard]] bool SetLayerEnabled(bool enabled) noexcept;
    [[nodiscard]] bool SetPipBoyTintEnabled(bool enabled) noexcept;
    [[nodiscard]] bool SetPlaybackStatus(
        const PlaybackStateSnapshot& playback) noexcept;
    UiRectSnapshot ReadForRenderThread() const noexcept;
    UiSurfaceSnapshot ResolveSurfaceOnSharedThread(std::uint32_t game_thread_id) const noexcept;
    void Clear() noexcept;

private:
    UiBridge() = default;
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
    std::uintptr_t last_status_tile_{};
    std::uintptr_t last_root_tile_{};
    std::uintptr_t last_surface_tile_{};
    bool last_layer_enabled_{true};
    bool last_pipboy_tint_enabled_{true};
    PlaybackState last_status_state_{PlaybackState::unavailable};
    PlaybackError last_status_error_{PlaybackError::none};
    bool found_logged_{};
};

} // namespace pbvp
