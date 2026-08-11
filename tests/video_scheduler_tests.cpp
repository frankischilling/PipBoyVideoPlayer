#include "pbvp/video_scheduler.hpp"

#include "test_support.hpp"

#include <cstdint>
#include <limits>
#include <vector>

void RunVideoSchedulerTests() {
    {
        const pbvp::VideoScheduler scheduler(0);
        const std::vector<pbvp::VideoFrameTiming> frames{
            {0, 33'333, 1u},
            {33'333, 33'333, 1u},
            {66'666, 33'333, 1u},
            {99'999, 33'333, 1u},
        };
        const pbvp::VideoSelection selected = scheduler.Select(frames, 1u, 70'000);
        PBVP_CHECK(selected.status == pbvp::VideoSelectionStatus::ok);
        PBVP_CHECK(selected.consume_count == 3u);
        PBVP_CHECK(selected.present_index == 2u);
        PBVP_CHECK(selected.dropped_frames == 2u);
        PBVP_CHECK(selected.stale_frames == 0u);
        PBVP_CHECK(selected.presentation_lateness_us == 3'334);
    }

    {
        const pbvp::VideoScheduler scheduler(16'667);
        const std::vector<pbvp::VideoFrameTiming> frames{
            {0, 33'333, 1u},
            {33'333, 33'333, 1u},
        };
        const pbvp::VideoSelection selected = scheduler.Select(frames, 1u, 16'666);
        PBVP_CHECK(selected.consume_count == 2u);
        PBVP_CHECK(selected.present_index == 1u);
        PBVP_CHECK(selected.dropped_frames == 1u);
        PBVP_CHECK(selected.presentation_lateness_us == 0);
    }

    {
        const pbvp::VideoScheduler scheduler;
        const std::vector<pbvp::VideoFrameTiming> frames{
            {10'000, 10'000, 1u},
            {20'000, 10'000, 2u},
            {30'000, 10'000, 2u},
        };
        const pbvp::VideoSelection selected = scheduler.Select(frames, 2u, 5'000);
        PBVP_CHECK(selected.consume_count == 2u);
        PBVP_CHECK(selected.present_index == 1u);
        PBVP_CHECK(selected.stale_frames == 1u);
        PBVP_CHECK(selected.dropped_frames == 1u);
    }

    {
        const pbvp::VideoScheduler scheduler;
        const std::vector<pbvp::VideoFrameTiming> invalid{
            {20'000, 10'000, 1u},
            {10'000, 10'000, 1u},
        };
        const pbvp::VideoSelection selected = scheduler.Select(invalid, 1u, 50'000);
        PBVP_CHECK(selected.status == pbvp::VideoSelectionStatus::invalid_timeline);
        PBVP_CHECK(selected.consume_count == 0u);
        PBVP_CHECK(!selected.present_index.has_value());
    }

    {
        const pbvp::VideoScheduler invalid(-1);
        PBVP_CHECK(invalid.Select({}, 1u, 0).status ==
                   pbvp::VideoSelectionStatus::invalid_configuration);
        const pbvp::VideoScheduler scheduler;
        PBVP_CHECK(scheduler.Select({}, 0u, 0).status ==
                   pbvp::VideoSelectionStatus::invalid_configuration);
        const std::vector<pbvp::VideoFrameTiming> last{{
            (std::numeric_limits<std::int64_t>::max)() - 1,
            1,
            1u,
        }};
        const auto selected = scheduler.Select(
            last, 1u, (std::numeric_limits<std::int64_t>::max)() - 5);
        PBVP_CHECK(selected.present_index == 0u);
    }

    {
        const pbvp::VideoScheduler scheduler(0);
        std::vector<pbvp::VideoFrameTiming> frames;
        frames.reserve(54'001u);
        for (std::int64_t index = 0; index <= 54'000; ++index) {
            frames.push_back({index * 100'000ll / 3ll, 33'333, 7u});
        }

        std::size_t consumed = 0u;
        std::uint64_t dropped = 0u;
        std::int64_t last_presented = 0;
        for (std::int64_t clock = 0; clock <= 1'800'000'000ll; clock += 100'000ll) {
            const auto remaining = std::span<const pbvp::VideoFrameTiming>(frames).subspan(consumed);
            const pbvp::VideoSelection selected = scheduler.Select(remaining, 7u, clock);
            PBVP_CHECK(selected.status == pbvp::VideoSelectionStatus::ok);
            if (selected.present_index.has_value()) {
                last_presented = remaining[*selected.present_index].pts_us;
                PBVP_CHECK(clock >= last_presented);
                PBVP_CHECK(clock - last_presented <= 33'334);
            }
            consumed += selected.consume_count;
            dropped += selected.dropped_frames;
        }
        PBVP_CHECK(consumed == frames.size());
        PBVP_CHECK(last_presented == 1'800'000'000ll);
        PBVP_CHECK(dropped > 30'000u);
    }
}
