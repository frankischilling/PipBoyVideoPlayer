#pragma once

#include "pbvp/rect_math.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace pbvp {

enum class VideoScaleStatus : std::uint32_t {
    ok,
    invalid_source,
    invalid_destination,
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

[[nodiscard]] VideoScaleResult ScaleBgraToFit(
    std::span<const std::uint8_t> source,
    std::uint32_t source_width,
    std::uint32_t source_height,
    std::uint32_t source_stride,
    std::span<std::uint8_t> destination,
    std::uint32_t destination_width,
    std::uint32_t destination_height,
    std::uint32_t destination_stride) noexcept;

} // namespace pbvp
