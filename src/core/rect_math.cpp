#include "pbvp/rect_math.hpp"

#include <algorithm>
#include <cmath>

namespace pbvp {

bool ConvertUiRectToPixels(
    const FloatRect& ui_rect,
    const PixelExtent& ui_extent,
    const PixelExtent& backbuffer_extent,
    FloatRect& output) noexcept {
    output = {};
    const bool finite = std::isfinite(ui_rect.left) && std::isfinite(ui_rect.top) &&
                        std::isfinite(ui_rect.right) && std::isfinite(ui_rect.bottom) &&
                        std::isfinite(ui_extent.width) && std::isfinite(ui_extent.height) &&
                        std::isfinite(backbuffer_extent.width) && std::isfinite(backbuffer_extent.height);
    if (!finite || ui_extent.width <= 0.0f || ui_extent.height <= 0.0f ||
        backbuffer_extent.width <= 0.0f || backbuffer_extent.height <= 0.0f ||
        ui_rect.right <= ui_rect.left || ui_rect.bottom <= ui_rect.top) {
        return false;
    }

    const float scale_x = backbuffer_extent.width / ui_extent.width;
    const float scale_y = backbuffer_extent.height / ui_extent.height;
    output.left = std::clamp(ui_rect.left * scale_x, 0.0f, backbuffer_extent.width);
    output.top = std::clamp(ui_rect.top * scale_y, 0.0f, backbuffer_extent.height);
    output.right = std::clamp(ui_rect.right * scale_x, 0.0f, backbuffer_extent.width);
    output.bottom = std::clamp(ui_rect.bottom * scale_y, 0.0f, backbuffer_extent.height);
    return output.right > output.left && output.bottom > output.top;
}

bool UiRectContainsPoint(
    const FloatRect& ui_rect,
    const float x,
    const float y) noexcept {
    const bool finite = std::isfinite(ui_rect.left) && std::isfinite(ui_rect.top) &&
                        std::isfinite(ui_rect.right) && std::isfinite(ui_rect.bottom) &&
                        std::isfinite(x) && std::isfinite(y);
    return finite && ui_rect.right > ui_rect.left && ui_rect.bottom > ui_rect.top &&
           x >= ui_rect.left && x < ui_rect.right &&
           y >= ui_rect.top && y < ui_rect.bottom;
}

} // namespace pbvp
