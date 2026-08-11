#pragma once

#include "pbvp/rect_math.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace pbvp {

enum class VideoScaleMode : std::uint32_t {
    fit,
    fill,
};

enum class VideoColorMode : std::uint32_t {
    full_color,
    pipboy_luminance,
};

enum class VideoScaleStatus : std::uint32_t {
    ok,
    invalid_source,
    invalid_destination,
    invalid_presentation,
    source_buffer_too_small,
    destination_buffer_too_small,
    arithmetic_overflow,
};

struct VideoScaleResult final {
    VideoScaleStatus status{VideoScaleStatus::invalid_source};
    std::uint32_t content_x{};
    std::uint32_t content_y{};
    std::uint32_t content_width{};
    std::uint32_t content_height{};
};

[[nodiscard]] VideoScaleResult ScaleBgra(
    std::span<const std::uint8_t> source,
    std::uint32_t source_width,
    std::uint32_t source_height,
    std::uint32_t source_stride,
    std::span<std::uint8_t> destination,
    std::uint32_t destination_width,
    std::uint32_t destination_height,
    std::uint32_t destination_stride,
    VideoScaleMode scale_mode,
    VideoColorMode color_mode) noexcept;

[[nodiscard]] VideoScaleResult ScaleBgraForPresentation(
    std::span<const std::uint8_t> source,
    std::uint32_t source_width,
    std::uint32_t source_height,
    std::uint32_t source_stride,
    std::span<std::uint8_t> destination,
    std::uint32_t destination_width,
    std::uint32_t destination_height,
    std::uint32_t destination_stride,
    PixelExtent presentation_extent,
    VideoScaleMode scale_mode,
    VideoColorMode color_mode) noexcept;

[[nodiscard]] inline VideoScaleResult ScaleBgraToFit(
    const std::span<const std::uint8_t> source,
    const std::uint32_t source_width,
    const std::uint32_t source_height,
    const std::uint32_t source_stride,
    const std::span<std::uint8_t> destination,
    const std::uint32_t destination_width,
    const std::uint32_t destination_height,
    const std::uint32_t destination_stride) noexcept {
    return ScaleBgra(
        source, source_width, source_height, source_stride,
        destination, destination_width, destination_height, destination_stride,
        VideoScaleMode::fit, VideoColorMode::full_color);
}

} // namespace pbvp
