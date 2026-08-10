#pragma once

#include <cstdint>

namespace pbvp {

inline constexpr std::uint64_t kDeferredRequestTimeoutMilliseconds = 5000u;

enum class DeferredRequestObservation {
    none,
    pending,
    consumed,
    unexpected,
    timed_out,
};

class DeferredRequestObserver final {
public:
    void Arm() noexcept;
    DeferredRequestObservation Observe(
        std::uint8_t request_value,
        std::uint64_t elapsed_milliseconds,
        std::uint64_t timeout_milliseconds =
            kDeferredRequestTimeoutMilliseconds) noexcept;
    bool IsActive() const noexcept;
    void Cancel() noexcept;

private:
    bool active_{};
    bool pending_reported_{};
};

} // namespace pbvp
