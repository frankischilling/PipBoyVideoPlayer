#pragma once

namespace pbvp {

struct FloatRect {
    float left;
    float top;
    float right;
    float bottom;
};

struct PixelExtent {
    float width;
    float height;
};

bool ConvertUiRectToPixels(
    const FloatRect& ui_rect,
    const PixelExtent& ui_extent,
    const PixelExtent& backbuffer_extent,
    FloatRect& output) noexcept;

bool UiRectContainsPoint(
    const FloatRect& ui_rect,
    float x,
    float y) noexcept;

} // namespace pbvp
