#include "pbvp/win32_playback_clock.hpp"

#include <Windows.h>

namespace pbvp {

bool Win32PlaybackClock::Start(
    const std::int64_t media_origin_us,
    const bool paused) noexcept {
    Clear();
    LARGE_INTEGER frequency{};
    LARGE_INTEGER counter{};
    if (QueryPerformanceFrequency(&frequency) == FALSE ||
        QueryPerformanceCounter(&counter) == FALSE ||
        frequency.QuadPart <= 0 || counter.QuadPart < 0) {
        return false;
    }

    frequency_ = frequency.QuadPart;
    if (!clock_.Reset(frequency_, counter.QuadPart, media_origin_us, paused)) {
        Clear();
        return false;
    }
    return true;
}

void Win32PlaybackClock::Clear() noexcept {
    clock_.Clear();
    frequency_ = 0;
}

bool Win32PlaybackClock::Pause() noexcept {
    std::int64_t counter = 0;
    return ReadCounter(counter) && clock_.Pause(counter);
}

bool Win32PlaybackClock::Resume() noexcept {
    std::int64_t counter = 0;
    return ReadCounter(counter) && clock_.Resume(counter);
}

bool Win32PlaybackClock::Seek(const std::int64_t media_origin_us) noexcept {
    std::int64_t counter = 0;
    return ReadCounter(counter) && clock_.Seek(media_origin_us, counter);
}

std::optional<std::int64_t> Win32PlaybackClock::MediaTimeUs() const noexcept {
    std::int64_t counter = 0;
    if (!ReadCounter(counter)) {
        return std::nullopt;
    }
    return clock_.MediaTimeUs(counter);
}

bool Win32PlaybackClock::IsActive() const noexcept {
    return clock_.IsActive();
}

bool Win32PlaybackClock::IsPaused() const noexcept {
    return clock_.IsPaused();
}

std::int64_t Win32PlaybackClock::Frequency() const noexcept {
    return frequency_;
}

bool Win32PlaybackClock::ReadCounter(std::int64_t& counter) const noexcept {
    counter = 0;
    if (!clock_.IsActive()) {
        return false;
    }
    LARGE_INTEGER value{};
    if (QueryPerformanceCounter(&value) == FALSE || value.QuadPart < 0) {
        return false;
    }
    counter = value.QuadPart;
    return true;
}

} // namespace pbvp
