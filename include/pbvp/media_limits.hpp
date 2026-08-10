#pragma once

#include <cstddef>
#include <cstdint>

namespace pbvp {

struct DecodeLimits {
    std::uint32_t maximum_width{1920u};
    std::uint32_t maximum_height{1080u};
    std::size_t maximum_video_payload_bytes{32u * 1024u * 1024u};
    std::size_t maximum_audio_payload_bytes{2u * 1024u * 1024u};
};

enum class LayoutStatus : std::uint32_t {
    ok,
    zero_dimension,
    dimension_limit,
    invalid_sample_layout,
    arithmetic_overflow,
    byte_limit,
};

struct VideoLayout {
    std::size_t row_bytes{};
    std::size_t total_bytes{};
};

const char* LayoutStatusName(LayoutStatus status) noexcept;
LayoutStatus ComputeBgraLayout(
    std::uint32_t width,
    std::uint32_t height,
    const DecodeLimits& limits,
    VideoLayout& output) noexcept;
LayoutStatus ComputeInterleavedAudioBytes(
    std::uint64_t samples_per_channel,
    std::uint32_t channels,
    std::uint32_t bytes_per_sample,
    const DecodeLimits& limits,
    std::size_t& output) noexcept;

} // namespace pbvp
