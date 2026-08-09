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
};

class UiBridge final {
public:
    static UiBridge& Instance() noexcept;

    void UpdateOnGameThread() noexcept;
    UiRectSnapshot ReadForRenderThread() const noexcept;
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
    bool polling_logged_{};
    bool map_visible_logged_{};
    std::uint32_t last_failure_{};
    bool found_logged_{};
};

} // namespace pbvp
