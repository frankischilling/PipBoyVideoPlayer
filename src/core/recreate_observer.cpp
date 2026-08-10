#include "pbvp/recreate_observer.hpp"

namespace pbvp {

void DeferredRequestObserver::Arm() noexcept {
    active_ = true;
    pending_reported_ = false;
}

DeferredRequestObservation DeferredRequestObserver::Observe(
    const std::uint8_t request_value,
    const std::uint64_t elapsed_milliseconds,
    const std::uint64_t timeout_milliseconds) noexcept {
    if (!active_) {
        return DeferredRequestObservation::none;
    }
    if (request_value == 0u) {
        Cancel();
        return DeferredRequestObservation::consumed;
    }
    if (request_value != 1u) {
        Cancel();
        return DeferredRequestObservation::unexpected;
    }
    if (elapsed_milliseconds >= timeout_milliseconds) {
        Cancel();
        return DeferredRequestObservation::timed_out;
    }
    if (!pending_reported_) {
        pending_reported_ = true;
        return DeferredRequestObservation::pending;
    }
    return DeferredRequestObservation::none;
}

bool DeferredRequestObserver::IsActive() const noexcept {
    return active_;
}

void DeferredRequestObserver::Cancel() noexcept {
    active_ = false;
    pending_reported_ = false;
}

} // namespace pbvp
