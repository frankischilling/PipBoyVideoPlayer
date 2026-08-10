#include "pbvp/playback_clock.hpp"

#include <limits>

namespace pbvp {
namespace {

constexpr std::uint64_t kMicrosecondsPerSecond = 1'000'000u;

bool ScaleToMicroseconds(
    const std::uint64_t elapsed,
    const std::uint64_t frequency,
    std::int64_t& output) noexcept {
    output = 0;
    if (frequency == 0u ||
        frequency > (std::numeric_limits<std::uint64_t>::max)() / kMicrosecondsPerSecond) {
        return false;
    }

    const std::uint64_t whole_seconds = elapsed / frequency;
    const std::uint64_t remainder = elapsed % frequency;
    const std::uint64_t maximum =
        static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)());
    if (whole_seconds > maximum / kMicrosecondsPerSecond) {
        return false;
    }

    const std::uint64_t whole_microseconds = whole_seconds * kMicrosecondsPerSecond;
    const std::uint64_t fractional_microseconds =
        (remainder * kMicrosecondsPerSecond) / frequency;
    if (fractional_microseconds > maximum - whole_microseconds) {
        return false;
    }

    output = static_cast<std::int64_t>(whole_microseconds + fractional_microseconds);
    return true;
}

bool AddElapsed(
    const std::int64_t media_origin_us,
    const std::int64_t elapsed_us,
    std::int64_t& output) noexcept {
    output = 0;
    if (elapsed_us < 0 ||
        media_origin_us > (std::numeric_limits<std::int64_t>::max)() - elapsed_us) {
        return false;
    }
    output = media_origin_us + elapsed_us;
    return true;
}

} // namespace

bool AudioSampleClock::Reset(
    const std::uint32_t sample_rate,
    const std::uint64_t sample_origin,
    const std::int64_t media_origin_us) noexcept {
    Clear();
    if (sample_rate == 0u) {
        return false;
    }

    sample_rate_ = sample_rate;
    sample_origin_ = sample_origin;
    media_origin_us_ = media_origin_us;
    active_ = true;
    return true;
}

void AudioSampleClock::Clear() noexcept {
    sample_rate_ = 0u;
    sample_origin_ = 0u;
    media_origin_us_ = 0;
    active_ = false;
}

std::optional<std::int64_t> AudioSampleClock::MediaTimeUs(
    const std::uint64_t samples_played) const noexcept {
    if (!active_ || samples_played < sample_origin_) {
        return std::nullopt;
    }

    std::int64_t elapsed_us = 0;
    if (!ScaleToMicroseconds(samples_played - sample_origin_, sample_rate_, elapsed_us)) {
        return std::nullopt;
    }

    std::int64_t media_time_us = 0;
    if (!AddElapsed(media_origin_us_, elapsed_us, media_time_us)) {
        return std::nullopt;
    }
    return media_time_us;
}

bool AudioSampleClock::IsActive() const noexcept {
    return active_;
}

std::uint32_t AudioSampleClock::SampleRate() const noexcept {
    return sample_rate_;
}

bool QpcPlaybackClock::Reset(
    const std::int64_t frequency,
    const std::int64_t counter,
    const std::int64_t media_origin_us,
    const bool paused) noexcept {
    Clear();
    if (frequency <= 0 || counter < 0 ||
        static_cast<std::uint64_t>(frequency) >
            (std::numeric_limits<std::uint64_t>::max)() / kMicrosecondsPerSecond) {
        return false;
    }

    frequency_ = frequency;
    counter_origin_ = counter;
    media_origin_us_ = media_origin_us;
    active_ = true;
    paused_ = paused;
    return true;
}

void QpcPlaybackClock::Clear() noexcept {
    frequency_ = 0;
    counter_origin_ = 0;
    media_origin_us_ = 0;
    active_ = false;
    paused_ = false;
}

bool QpcPlaybackClock::Pause(const std::int64_t counter) noexcept {
    if (!active_ || counter < 0) {
        return false;
    }
    if (paused_) {
        return true;
    }

    const auto media_time = MediaTimeUs(counter);
    if (!media_time.has_value()) {
        return false;
    }

    media_origin_us_ = *media_time;
    counter_origin_ = counter;
    paused_ = true;
    return true;
}

bool QpcPlaybackClock::Resume(const std::int64_t counter) noexcept {
    if (!active_ || counter < 0) {
        return false;
    }
    if (!paused_) {
        return true;
    }

    counter_origin_ = counter;
    paused_ = false;
    return true;
}

bool QpcPlaybackClock::Seek(
    const std::int64_t media_origin_us,
    const std::int64_t counter) noexcept {
    if (!active_ || counter < 0) {
        return false;
    }

    media_origin_us_ = media_origin_us;
    counter_origin_ = counter;
    return true;
}

std::optional<std::int64_t> QpcPlaybackClock::MediaTimeUs(
    const std::int64_t counter) const noexcept {
    if (!active_ || counter < 0) {
        return std::nullopt;
    }
    if (paused_) {
        return media_origin_us_;
    }
    if (counter < counter_origin_) {
        return std::nullopt;
    }

    const std::uint64_t elapsed_ticks =
        static_cast<std::uint64_t>(counter) -
        static_cast<std::uint64_t>(counter_origin_);
    std::int64_t elapsed_us = 0;
    if (!ScaleToMicroseconds(
            elapsed_ticks,
            static_cast<std::uint64_t>(frequency_),
            elapsed_us)) {
        return std::nullopt;
    }

    std::int64_t media_time_us = 0;
    if (!AddElapsed(media_origin_us_, elapsed_us, media_time_us)) {
        return std::nullopt;
    }
    return media_time_us;
}

bool QpcPlaybackClock::IsActive() const noexcept {
    return active_;
}

bool QpcPlaybackClock::IsPaused() const noexcept {
    return paused_;
}

} // namespace pbvp
