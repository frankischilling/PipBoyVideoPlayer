#include "pbvp/video_scaler.hpp"

#include "pbvp/checked_math.hpp"

#include <algorithm>
#include <cstring>

namespace pbvp {
namespace {

bool RequiredBytes(
    const std::uint32_t width,
    const std::uint32_t height,
    const std::uint32_t stride,
    std::size_t& required) noexcept {
    required = 0u;
    std::size_t row_bytes = 0u;
    if (width == 0u || height == 0u ||
        !CheckedMultiplySize(static_cast<std::size_t>(width), 4u, row_bytes) ||
        stride < row_bytes) {
        return false;
    }
    std::size_t preceding_rows = 0u;
    return CheckedMultiplySize(
               static_cast<std::size_t>(height - 1u),
               static_cast<std::size_t>(stride), preceding_rows) &&
           CheckedAddSize(preceding_rows, row_bytes, required);
}

} // namespace

VideoScaleResult ScaleBgra(
    const std::span<const std::uint8_t> source,
    const std::uint32_t source_width,
    const std::uint32_t source_height,
    const std::uint32_t source_stride,
    const std::span<std::uint8_t> destination,
    const std::uint32_t destination_width,
    const std::uint32_t destination_height,
    const std::uint32_t destination_stride,
    const VideoScaleMode scale_mode,
    const VideoColorMode color_mode) noexcept {
    VideoScaleResult result{};
    std::size_t required_source = 0u;
    if (!RequiredBytes(
            source_width, source_height, source_stride, required_source)) {
        result.status = VideoScaleStatus::invalid_source;
        return result;
    }
    if (source.size() < required_source) {
        result.status = VideoScaleStatus::source_buffer_too_small;
        return result;
    }

    std::size_t required_destination = 0u;
    if (!RequiredBytes(
            destination_width, destination_height, destination_stride,
            required_destination)) {
        result.status = VideoScaleStatus::invalid_destination;
        return result;
    }
    if (destination.size() < required_destination) {
        result.status = VideoScaleStatus::destination_buffer_too_small;
        return result;
    }

    const std::uint64_t source_aspect =
        static_cast<std::uint64_t>(source_width) * destination_height;
    const std::uint64_t destination_aspect =
        static_cast<std::uint64_t>(destination_width) * source_height;
    std::uint32_t source_x = 0u;
    std::uint32_t source_y = 0u;
    std::uint32_t sampled_width = source_width;
    std::uint32_t sampled_height = source_height;
    if (scale_mode == VideoScaleMode::fill) {
        result.content_width = destination_width;
        result.content_height = destination_height;
        if (source_aspect > destination_aspect) {
            sampled_width = static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(source_height) * destination_width /
                destination_height);
            sampled_width = std::clamp(sampled_width, 1u, source_width);
            source_x = (source_width - sampled_width) / 2u;
        } else if (source_aspect < destination_aspect) {
            sampled_height = static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(source_width) * destination_height /
                destination_width);
            sampled_height = std::clamp(sampled_height, 1u, source_height);
            source_y = (source_height - sampled_height) / 2u;
        }
    } else {
        if (source_aspect > destination_aspect) {
            result.content_width = destination_width;
            result.content_height = static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(source_height) * destination_width /
                source_width);
        } else {
            result.content_height = destination_height;
            result.content_width = static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(source_width) * destination_height /
                source_height);
        }
        result.content_width = (std::max)(result.content_width, 1u);
        result.content_height = (std::max)(result.content_height, 1u);
        result.content_x = (destination_width - result.content_width) / 2u;
        result.content_y = (destination_height - result.content_height) / 2u;
    }

    for (std::uint32_t y = 0u; y < destination_height; ++y) {
        auto* row = destination.data() + static_cast<std::size_t>(y) * destination_stride;
        for (std::uint32_t x = 0u; x < destination_width; ++x) {
            const std::size_t pixel = static_cast<std::size_t>(x) * 4u;
            row[pixel] = 0u;
            row[pixel + 1u] = 0u;
            row[pixel + 2u] = 0u;
            row[pixel + 3u] = 255u;
        }
    }

    for (std::uint32_t y = 0u; y < result.content_height; ++y) {
        const std::uint32_t sampled_y = source_y + static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(y) * sampled_height / result.content_height);
        const auto* source_row = source.data() +
            static_cast<std::size_t>(sampled_y) * source_stride;
        auto* destination_row = destination.data() +
            static_cast<std::size_t>(result.content_y + y) * destination_stride;
        for (std::uint32_t x = 0u; x < result.content_width; ++x) {
            const std::uint32_t sampled_x = source_x + static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(x) * sampled_width / result.content_width);
            const auto* source_pixel = source_row + static_cast<std::size_t>(sampled_x) * 4u;
            auto* destination_pixel = destination_row +
                static_cast<std::size_t>(result.content_x + x) * 4u;
            if (color_mode == VideoColorMode::pipboy_luminance) {
                const std::uint32_t luminance =
                    (29u * source_pixel[0] + 150u * source_pixel[1] +
                     77u * source_pixel[2] + 128u) >> 8u;
                destination_pixel[0] = static_cast<std::uint8_t>(luminance);
                destination_pixel[1] = static_cast<std::uint8_t>(luminance);
                destination_pixel[2] = static_cast<std::uint8_t>(luminance);
            } else {
                std::memcpy(destination_pixel, source_pixel, 3u);
            }
            destination_pixel[3] = 255u;
        }
    }

    result.status = VideoScaleStatus::ok;
    return result;
}

} // namespace pbvp
