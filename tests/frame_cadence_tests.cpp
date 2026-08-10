#include "pbvp/frame_cadence.hpp"

#include "test_support.hpp"

#include <cmath>

namespace {

bool Near(const double left, const double right, const double tolerance = 0.001) {
    return std::abs(left - right) <= tolerance;
}

} // namespace

void RunFrameCadenceTests() {
    {
        pbvp::FrameCadenceTracker tracker;
        constexpr std::int64_t frequency = 60000;
        pbvp::FrameCadenceSample sample{};
        for (std::int64_t frame = 0; frame <= 180; ++frame) {
            sample = tracker.Observe(frame * 1000, frequency);
        }

        PBVP_CHECK(sample.ready);
        PBVP_CHECK(sample.frames == 181u);
        PBVP_CHECK(Near(sample.elapsed_seconds, 3.0));
        PBVP_CHECK(Near(sample.frames_per_second, 60.0));
    }

    {
        pbvp::FrameCadenceTracker tracker;
        constexpr std::int64_t frequency = 60000;
        pbvp::FrameCadenceSample sample{};
        for (std::int64_t frame = 0; frame < 180; ++frame) {
            sample = tracker.Observe(frame * 1000, frequency);
        }

        PBVP_CHECK(!sample.ready);
    }

    {
        pbvp::FrameCadenceTracker tracker;
        constexpr std::int64_t frequency = 60000;
        for (std::int64_t frame = 0; frame < 120; ++frame) {
            PBVP_CHECK(!tracker.Observe(frame * 1000, frequency).ready);
        }
        tracker.Reset();

        pbvp::FrameCadenceSample sample{};
        for (std::int64_t frame = 0; frame <= 180; ++frame) {
            sample = tracker.Observe(500000 + frame * 1000, frequency);
        }
        PBVP_CHECK(sample.ready);
        PBVP_CHECK(Near(sample.frames_per_second, 60.0));
    }

    {
        pbvp::FrameCadenceTracker tracker;
        constexpr std::int64_t frequency = 1000;
        PBVP_CHECK(!tracker.Observe(1000, frequency).ready);
        PBVP_CHECK(!tracker.Observe(2000, frequency).ready);
        PBVP_CHECK(!tracker.Observe(500, frequency).ready);
        PBVP_CHECK(!tracker.Observe(2500, frequency).ready);
        const auto sample = tracker.Observe(3500, frequency);

        PBVP_CHECK(sample.ready);
        PBVP_CHECK(sample.frames == 3u);
        PBVP_CHECK(Near(sample.elapsed_seconds, 3.0));
    }

    {
        pbvp::FrameCadenceTracker tracker;
        PBVP_CHECK(!tracker.Observe(0, 0).ready);
        PBVP_CHECK(!tracker.Observe(3000, 1000).ready);
    }
}
