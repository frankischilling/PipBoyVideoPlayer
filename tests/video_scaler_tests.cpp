#include "pbvp/video_scaler.hpp"

#include "test_support.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace {

std::uint32_t Pixel(
    const std::vector<std::uint8_t>& pixels,
    const std::uint32_t stride,
    const std::uint32_t x,
    const std::uint32_t y) {
    const std::size_t offset = static_cast<std::size_t>(y) * stride +
        static_cast<std::size_t>(x) * 4u;
    return static_cast<std::uint32_t>(pixels[offset]) |
           (static_cast<std::uint32_t>(pixels[offset + 1u]) << 8u) |
           (static_cast<std::uint32_t>(pixels[offset + 2u]) << 16u) |
           (static_cast<std::uint32_t>(pixels[offset + 3u]) << 24u);
}

} // namespace

void RunVideoScalerTests() {
    {
        const std::vector<std::uint8_t> source{
            1u, 2u, 3u, 4u,
            5u, 6u, 7u, 8u,
            9u, 10u, 11u, 12u,
            13u, 14u, 15u, 16u,
        };
        std::vector<std::uint8_t> destination(4u * 4u * 4u, 0xCCu);
        const auto result = pbvp::ScaleBgraToFit(
            source, 2u, 2u, 8u, destination, 4u, 4u, 16u);
        PBVP_CHECK(result.status == pbvp::VideoScaleStatus::ok);
        PBVP_CHECK(result.content_x == 0u);
        PBVP_CHECK(result.content_y == 0u);
        PBVP_CHECK(result.content_width == 4u);
        PBVP_CHECK(result.content_height == 4u);
        PBVP_CHECK(Pixel(destination, 16u, 0u, 0u) == 0xFF030201u);
        PBVP_CHECK(Pixel(destination, 16u, 3u, 0u) == 0xFF070605u);
        PBVP_CHECK(Pixel(destination, 16u, 0u, 3u) == 0xFF0B0A09u);
        PBVP_CHECK(Pixel(destination, 16u, 3u, 3u) == 0xFF0F0E0Du);
    }

    {
        std::vector<std::uint8_t> wide(16u * 9u * 4u, 0x7Fu);
        std::vector<std::uint8_t> square(16u * 16u * 4u, 0xCCu);
        const auto result = pbvp::ScaleBgraToFit(
            wide, 16u, 9u, 64u, square, 16u, 16u, 64u);
        PBVP_CHECK(result.status == pbvp::VideoScaleStatus::ok);
        PBVP_CHECK(result.content_width == 16u);
        PBVP_CHECK(result.content_height == 9u);
        PBVP_CHECK(result.content_y == 3u);
        PBVP_CHECK(Pixel(square, 64u, 8u, 0u) == 0xFF000000u);
        PBVP_CHECK(Pixel(square, 64u, 8u, 3u) == 0xFF7F7F7Fu);
        PBVP_CHECK(Pixel(square, 64u, 8u, 12u) == 0xFF000000u);
    }

    {
        std::vector<std::uint8_t> tall(9u * 16u * 4u, 0x55u);
        std::vector<std::uint8_t> square(16u * 16u * 4u, 0xCCu);
        const auto result = pbvp::ScaleBgraToFit(
            tall, 9u, 16u, 36u, square, 16u, 16u, 64u);
        PBVP_CHECK(result.status == pbvp::VideoScaleStatus::ok);
        PBVP_CHECK(result.content_width == 9u);
        PBVP_CHECK(result.content_height == 16u);
        PBVP_CHECK(result.content_x == 3u);
        PBVP_CHECK(Pixel(square, 64u, 0u, 8u) == 0xFF000000u);
        PBVP_CHECK(Pixel(square, 64u, 3u, 8u) == 0xFF555555u);
        PBVP_CHECK(Pixel(square, 64u, 12u, 8u) == 0xFF000000u);
    }

    {
        std::vector<std::uint8_t> bytes(64u, 0u);
        PBVP_CHECK(pbvp::ScaleBgraToFit(
            bytes, 0u, 1u, 4u, bytes, 1u, 1u, 4u).status ==
                   pbvp::VideoScaleStatus::invalid_source);
        PBVP_CHECK(pbvp::ScaleBgraToFit(
            bytes, 2u, 2u, 7u, bytes, 1u, 1u, 4u).status ==
                   pbvp::VideoScaleStatus::invalid_source);
        PBVP_CHECK(pbvp::ScaleBgraToFit(
            std::span<const std::uint8_t>(bytes.data(), 8u),
            2u, 2u, 8u, bytes, 1u, 1u, 4u).status ==
                   pbvp::VideoScaleStatus::source_buffer_too_small);
        PBVP_CHECK(pbvp::ScaleBgraToFit(
            bytes, 1u, 1u, 4u,
            std::span<std::uint8_t>(bytes.data(), 3u),
            1u, 1u, 4u).status ==
                   pbvp::VideoScaleStatus::destination_buffer_too_small);
        PBVP_CHECK(pbvp::ScaleBgraToFit(
            bytes, 1u, 1u, 4u, bytes,
            (std::numeric_limits<std::uint32_t>::max)(), 2u, 4u).status ==
                   pbvp::VideoScaleStatus::invalid_destination);
    }
}
