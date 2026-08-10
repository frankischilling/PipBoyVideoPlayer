#pragma once

#include <cstdint>
#include <optional>

namespace pbvp {

class AudioSampleClock final {
public:
    bool Reset(
        std::uint32_t sample_rate,
        std::uint64_t sample_origin,
        std::int64_t media_origin_us) noexcept;
    void Clear() noexcept;

    [[nodiscard]] std::optional<std::int64_t> MediaTimeUs(
        std::uint64_t samples_played) const noexcept;
    [[nodiscard]] bool IsActive() const noexcept;
    [[nodiscard]] std::uint32_t SampleRate() const noexcept;

private:
    std::uint32_t sample_rate_{};
    std::uint64_t sample_origin_{};
    std::int64_t media_origin_us_{};
    bool active_{};
};

class QpcPlaybackClock final {
public:
    bool Reset(
        std::int64_t frequency,
        std::int64_t counter,
        std::int64_t media_origin_us,
        bool paused = false) noexcept;
    void Clear() noexcept;

    bool Pause(std::int64_t counter) noexcept;
    bool Resume(std::int64_t counter) noexcept;
    bool Seek(std::int64_t media_origin_us, std::int64_t counter) noexcept;

    [[nodiscard]] std::optional<std::int64_t> MediaTimeUs(
        std::int64_t counter) const noexcept;
    [[nodiscard]] bool IsActive() const noexcept;
    [[nodiscard]] bool IsPaused() const noexcept;

private:
    std::int64_t frequency_{};
    std::int64_t counter_origin_{};
    std::int64_t media_origin_us_{};
    bool active_{};
    bool paused_{};
};

} // namespace pbvp
