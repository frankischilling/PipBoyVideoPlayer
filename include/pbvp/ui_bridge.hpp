#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "pbvp/playback_state.hpp"
#include "pbvp/configuration.hpp"
#include "pbvp/rect_math.hpp"

namespace pbvp {

enum class UiInputAction : std::uint32_t {
    none = 0u,
    open_page = 1u << 0u,
    close_page = 1u << 1u,
    previous_item = 1u << 2u,
    next_item = 1u << 3u,
    activate = 1u << 4u,
    pause_resume = 1u << 5u,
    stop = 1u << 6u,
    seek_backward = 1u << 7u,
    seek_forward = 1u << 8u,
    toggle_presentation = 1u << 9u,
};

enum class UiInputMethod : std::uint32_t {
    keyboard_mouse,
    controller,
};

constexpr std::uint32_t kUiCatalogRowShift = 16u;
constexpr std::uint32_t kUiCatalogRowMask = 0xFFu << kUiCatalogRowShift;

struct UiInputSnapshot final {
    std::uint32_t actions{};
    UiInputMethod method{UiInputMethod::keyboard_mouse};
    bool map_menu_visible{};
    bool menu_hook_available{};
    bool controller_connected{};
};

enum class UiVideosMode : std::uint32_t {
    data_page,
    catalog,
    playback,
};

constexpr std::size_t kUiCatalogVisibleRows = 8u;

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
    void SetGameInputState(std::uintptr_t input_state) noexcept;
    [[nodiscard]] bool SetInputBindings(const InputSettings& settings) noexcept;
    void UpdateInputOnGameThread(bool videos_page_active) noexcept;
    [[nodiscard]] UiInputSnapshot TakeInputSnapshot() noexcept;
    [[nodiscard]] bool SetLayerEnabled(bool enabled) noexcept;
    [[nodiscard]] bool SetPipBoyTintEnabled(bool enabled) noexcept;
    [[nodiscard]] bool SetVideosMode(UiVideosMode mode) noexcept;
    [[nodiscard]] bool SetCatalogRows(
        const std::array<std::wstring, kUiCatalogVisibleRows>& rows,
        std::size_t row_count,
        std::size_t selected_row,
        UiInputMethod input_method) noexcept;
    [[nodiscard]] bool SetPlaybackStatus(
        const PlaybackStateSnapshot& playback,
        UiInputMethod input_method) noexcept;
    [[nodiscard]] bool SetPlaybackDetails(
        const std::wstring& title,
        std::int64_t current_time_us,
        std::int64_t duration_us) noexcept;
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
    bool menu_input_available_{};
    bool map_menu_visible_{};
    PlaybackState last_status_state_{PlaybackState::unavailable};
    PlaybackError last_status_error_{PlaybackError::none};
    UiInputMethod last_status_input_method_{UiInputMethod::keyboard_mouse};
    bool found_logged_{};
};

} // namespace pbvp
