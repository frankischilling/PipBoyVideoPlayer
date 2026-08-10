#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace pbvp {

struct VideoFrameTiming final {
    std::int64_t pts_us{};
    std::int64_t duration_us{};
    std::uint64_t generation{};
};

enum class VideoSelectionStatus : std::uint32_t {
    ok,
    invalid_configuration,
    invalid_timeline,
};

struct VideoSelection final {
    VideoSelectionStatus status{VideoSelectionStatus::ok};
    std::size_t consume_count{};
    std::optional<std::size_t> present_index{};
    std::uint64_t dropped_frames{};
    std::uint64_t stale_frames{};
    std::int64_t presentation_lateness_us{};
};

class VideoScheduler final {
public:
    explicit VideoScheduler(std::int64_t presentation_lead_us = 16'667) noexcept;

    [[nodiscard]] VideoSelection Select(
        std::span<const VideoFrameTiming> frames,
        std::uint64_t expected_generation,
        std::int64_t media_clock_us) const noexcept;

    [[nodiscard]] std::int64_t PresentationLeadUs() const noexcept;

private:
    std::int64_t presentation_lead_us_{};
    bool valid_{};
};

} // namespace pbvp
