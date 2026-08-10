#include "pbvp/media_limits.hpp"

#include "pbvp/checked_math.hpp"

namespace pbvp {

const char* LayoutStatusName(const LayoutStatus status) noexcept {
    switch (status) {
        case LayoutStatus::ok: return "ok";
        case LayoutStatus::zero_dimension: return "zero dimension";
        case LayoutStatus::dimension_limit: return "dimension limit exceeded";
        case LayoutStatus::invalid_sample_layout: return "invalid sample layout";
        case LayoutStatus::arithmetic_overflow: return "allocation arithmetic overflow";
        case LayoutStatus::byte_limit: return "allocation byte limit exceeded";
    }
    return "unknown layout failure";
}

LayoutStatus ComputeBgraLayout(
    const std::uint32_t width,
    const std::uint32_t height,
    const DecodeLimits& limits,
    VideoLayout& output) noexcept {
    output = {};
    if (width == 0u || height == 0u) {
        return LayoutStatus::zero_dimension;
    }
    if (width > limits.maximum_width || height > limits.maximum_height) {
        return LayoutStatus::dimension_limit;
    }
    if (!CheckedMultiplySize(static_cast<std::size_t>(width), 4u, output.row_bytes) ||
        !CheckedMultiplySize(output.row_bytes, static_cast<std::size_t>(height), output.total_bytes)) {
        output = {};
        return LayoutStatus::arithmetic_overflow;
    }
    if (output.total_bytes > limits.maximum_video_payload_bytes) {
        output = {};
        return LayoutStatus::byte_limit;
    }
    return LayoutStatus::ok;
}

LayoutStatus ComputeInterleavedAudioBytes(
    const std::uint64_t samples_per_channel,
    const std::uint32_t channels,
    const std::uint32_t bytes_per_sample,
    const DecodeLimits& limits,
    std::size_t& output) noexcept {
    output = 0u;
    if (samples_per_channel == 0u || channels == 0u || channels > 32u ||
        bytes_per_sample == 0u || bytes_per_sample > 8u) {
        return LayoutStatus::invalid_sample_layout;
    }
    std::size_t samples = 0u;
    std::size_t values = 0u;
    if (!CheckedUint64ToSize(samples_per_channel, samples) ||
        !CheckedMultiplySize(samples, static_cast<std::size_t>(channels), values) ||
        !CheckedMultiplySize(values, static_cast<std::size_t>(bytes_per_sample), output)) {
        output = 0u;
        return LayoutStatus::arithmetic_overflow;
    }
    if (output > limits.maximum_audio_payload_bytes) {
        output = 0u;
        return LayoutStatus::byte_limit;
    }
    return LayoutStatus::ok;
}

} // namespace pbvp
