#include "pbvp/video_scheduler.hpp"

#include <limits>

namespace pbvp {

VideoScheduler::VideoScheduler(const std::int64_t presentation_lead_us) noexcept
    : presentation_lead_us_(presentation_lead_us),
      valid_(presentation_lead_us >= 0 && presentation_lead_us <= 1'000'000) {}

VideoSelection VideoScheduler::Select(
    const std::span<const VideoFrameTiming> frames,
    const std::uint64_t expected_generation,
    const std::int64_t media_clock_us) const noexcept {
    VideoSelection result{};
    if (!valid_ || expected_generation == 0u || media_clock_us < 0) {
        result.status = VideoSelectionStatus::invalid_configuration;
        return result;
    }

    const std::int64_t maximum = (std::numeric_limits<std::int64_t>::max)();
    const std::int64_t deadline = media_clock_us > maximum - presentation_lead_us_
        ? maximum
        : media_clock_us + presentation_lead_us_;
    std::optional<std::int64_t> previous_pts{};
    std::size_t eligible_count = 0u;

    for (std::size_t index = 0u; index < frames.size(); ++index) {
        const VideoFrameTiming& frame = frames[index];
        if (frame.generation != expected_generation) {
            ++result.consume_count;
            ++result.stale_frames;
            continue;
        }
        if (frame.pts_us < 0 || frame.duration_us < 0 ||
            (previous_pts.has_value() && frame.pts_us < *previous_pts)) {
            result.status = VideoSelectionStatus::invalid_timeline;
            result.consume_count = 0u;
            result.present_index.reset();
            result.dropped_frames = 0u;
            result.stale_frames = 0u;
            result.presentation_lateness_us = 0;
            return result;
        }
        previous_pts = frame.pts_us;
        if (frame.pts_us > deadline) {
            break;
        }

        ++result.consume_count;
        ++eligible_count;
        result.present_index = index;
    }

    result.dropped_frames = result.stale_frames;
    if (eligible_count > 1u) {
        result.dropped_frames += static_cast<std::uint64_t>(eligible_count - 1u);
    }
    if (result.present_index.has_value()) {
        const std::int64_t pts = frames[*result.present_index].pts_us;
        result.presentation_lateness_us = media_clock_us > pts ? media_clock_us - pts : 0;
    }
    return result;
}

std::int64_t VideoScheduler::PresentationLeadUs() const noexcept {
    return presentation_lead_us_;
}

} // namespace pbvp
