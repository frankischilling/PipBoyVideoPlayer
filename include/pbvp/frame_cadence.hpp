#pragma once

#include <cstdint>

namespace pbvp {

struct FrameCadenceSample final {
    bool ready{};
    std::uint32_t frames{};
    double elapsed_seconds{};
    double frames_per_second{};
};

class FrameCadenceTracker final {
public:
    FrameCadenceSample Observe(
        std::int64_t counter,
        std::int64_t frequency) noexcept;
    void Reset() noexcept;

private:
    std::int64_t frequency_{};
    std::int64_t started_{};
    std::int64_t previous_{};
    std::uint32_t frame_count_{};
    bool active_{};
};

} // namespace pbvp
