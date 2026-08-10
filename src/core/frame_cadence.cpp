#include "pbvp/frame_cadence.hpp"

#include <limits>

namespace pbvp {
namespace {

constexpr double kSampleDurationSeconds = 3.0;

} // namespace

FrameCadenceSample FrameCadenceTracker::Observe(
    const std::int64_t counter,
    const std::int64_t frequency) noexcept {
    if (counter < 0 || frequency <= 0) {
        Reset();
        return {};
    }
    if (!active_ || frequency_ != frequency || counter < previous_) {
        frequency_ = frequency;
        started_ = counter;
        previous_ = counter;
        frame_count_ = 1u;
        active_ = true;
        return {};
    }
    if (frame_count_ == std::numeric_limits<std::uint32_t>::max()) {
        Reset();
        return {};
    }

    previous_ = counter;
    ++frame_count_;
    const double elapsed_seconds =
        static_cast<double>(counter - started_) / static_cast<double>(frequency_);
    if (elapsed_seconds < kSampleDurationSeconds) {
        return {};
    }

    const double frames_per_second =
        static_cast<double>(frame_count_ - 1u) / elapsed_seconds;
    const FrameCadenceSample sample{
        true,
        frame_count_,
        elapsed_seconds,
        frames_per_second};

    started_ = counter;
    frame_count_ = 1u;
    return sample;
}

void FrameCadenceTracker::Reset() noexcept {
    frequency_ = 0;
    started_ = 0;
    previous_ = 0;
    frame_count_ = 0u;
    active_ = false;
}

} // namespace pbvp
