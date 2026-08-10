#pragma once

#include "pbvp/playback_clock.hpp"

#include <cstdint>
#include <optional>

namespace pbvp {

class Win32PlaybackClock final {
public:
    bool Start(std::int64_t media_origin_us, bool paused = false) noexcept;
    void Clear() noexcept;

    bool Pause() noexcept;
    bool Resume() noexcept;
    bool Seek(std::int64_t media_origin_us) noexcept;

    [[nodiscard]] std::optional<std::int64_t> MediaTimeUs() const noexcept;
    [[nodiscard]] bool IsActive() const noexcept;
    [[nodiscard]] bool IsPaused() const noexcept;
    [[nodiscard]] std::int64_t Frequency() const noexcept;

private:
    bool ReadCounter(std::int64_t& counter) const noexcept;

    QpcPlaybackClock clock_{};
    std::int64_t frequency_{};
};

} // namespace pbvp
