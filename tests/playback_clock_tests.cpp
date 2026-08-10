#include "pbvp/playback_clock.hpp"

#include "test_support.hpp"

#include <cstdint>
#include <limits>

void RunPlaybackClockTests() {
    {
        pbvp::AudioSampleClock clock;
        PBVP_CHECK(!clock.IsActive());
        PBVP_CHECK(!clock.Reset(0u, 0u, 0));
        PBVP_CHECK(!clock.MediaTimeUs(0u).has_value());

        PBVP_CHECK(clock.Reset(48'000u, 100u, 2'000'000));
        PBVP_CHECK(clock.IsActive());
        PBVP_CHECK(clock.SampleRate() == 48'000u);
        PBVP_CHECK(!clock.MediaTimeUs(99u).has_value());
        PBVP_CHECK(clock.MediaTimeUs(100u) == 2'000'000);
        PBVP_CHECK(clock.MediaTimeUs(24'100u) == 2'500'000);
        PBVP_CHECK(clock.MediaTimeUs(48'100u) == 3'000'000);
        PBVP_CHECK(clock.MediaTimeUs(86'400'100u) == 1'802'000'000);

        PBVP_CHECK(clock.Reset(44'100u, 0u, 10'000));
        PBVP_CHECK(clock.MediaTimeUs(4'410u) == 110'000);
        PBVP_CHECK(clock.MediaTimeUs(44'100u) == 1'010'000);

        PBVP_CHECK(clock.Reset(48'000u, 0u, -250'000));
        PBVP_CHECK(clock.MediaTimeUs(24'000u) == 250'000);

        PBVP_CHECK(clock.Reset(
            1u,
            0u,
            (std::numeric_limits<std::int64_t>::max)() - 10));
        PBVP_CHECK(!clock.MediaTimeUs(1u).has_value());

        clock.Clear();
        PBVP_CHECK(!clock.IsActive());
        PBVP_CHECK(clock.SampleRate() == 0u);
    }

    {
        pbvp::QpcPlaybackClock clock;
        PBVP_CHECK(!clock.IsActive());
        PBVP_CHECK(!clock.Reset(0, 0, 0));
        PBVP_CHECK(!clock.Reset(1'000, -1, 0));
        PBVP_CHECK(!clock.MediaTimeUs(0).has_value());

        PBVP_CHECK(clock.Reset(1'000, 1'000, 2'000'000));
        PBVP_CHECK(clock.IsActive());
        PBVP_CHECK(!clock.IsPaused());
        PBVP_CHECK(clock.MediaTimeUs(1'000) == 2'000'000);
        PBVP_CHECK(clock.MediaTimeUs(1'250) == 2'250'000);
        PBVP_CHECK(clock.MediaTimeUs(1'801'000) == 1'802'000'000);
        PBVP_CHECK(!clock.MediaTimeUs(999).has_value());

        PBVP_CHECK(clock.Pause(1'300));
        PBVP_CHECK(clock.IsPaused());
        PBVP_CHECK(clock.MediaTimeUs(1'300) == 2'300'000);
        PBVP_CHECK(clock.MediaTimeUs(5'000) == 2'300'000);
        PBVP_CHECK(clock.Pause(100));

        PBVP_CHECK(clock.Resume(5'000));
        PBVP_CHECK(!clock.IsPaused());
        PBVP_CHECK(clock.MediaTimeUs(5'100) == 2'400'000);
        PBVP_CHECK(clock.Resume(1));

        PBVP_CHECK(clock.Seek(10'000'000, 6'000));
        PBVP_CHECK(clock.MediaTimeUs(6'050) == 10'050'000);
        PBVP_CHECK(clock.Pause(6'100));
        PBVP_CHECK(clock.Seek(500'000, 7'000));
        PBVP_CHECK(clock.MediaTimeUs(9'000) == 500'000);
        PBVP_CHECK(clock.Resume(9'000));
        PBVP_CHECK(clock.MediaTimeUs(9'250) == 750'000);

        clock.Clear();
        PBVP_CHECK(!clock.IsActive());
        PBVP_CHECK(!clock.IsPaused());
        PBVP_CHECK(!clock.Pause(0));
        PBVP_CHECK(!clock.Resume(0));
        PBVP_CHECK(!clock.Seek(0, 0));
    }

    {
        pbvp::QpcPlaybackClock clock;
        PBVP_CHECK(clock.Reset(10'000'000, 0, 0, true));
        PBVP_CHECK(clock.IsPaused());
        PBVP_CHECK(clock.MediaTimeUs(100'000'000) == 0);
        PBVP_CHECK(clock.Resume(100'000'000));
        PBVP_CHECK(clock.MediaTimeUs(105'000'000) == 500'000);

        PBVP_CHECK(clock.Seek(
            (std::numeric_limits<std::int64_t>::max)() - 10,
            200'000'000));
        PBVP_CHECK(!clock.MediaTimeUs(210'000'000).has_value());
    }
}
