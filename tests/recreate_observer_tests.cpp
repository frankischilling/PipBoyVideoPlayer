#include "pbvp/recreate_observer.hpp"

#include "test_support.hpp"

void RunRecreateObserverTests() {
    using namespace pbvp;

    DeferredRequestObserver observer;
    PBVP_CHECK(!observer.IsActive());
    PBVP_CHECK(observer.Observe(0u, 0u) == DeferredRequestObservation::none);

    observer.Arm();
    PBVP_CHECK(observer.IsActive());
    PBVP_CHECK(observer.Observe(1u, 10u) == DeferredRequestObservation::pending);
    PBVP_CHECK(observer.Observe(1u, 20u) == DeferredRequestObservation::none);
    PBVP_CHECK(observer.Observe(0u, 30u) == DeferredRequestObservation::consumed);
    PBVP_CHECK(!observer.IsActive());
    PBVP_CHECK(observer.Observe(0u, 40u) == DeferredRequestObservation::none);

    observer.Arm();
    PBVP_CHECK(observer.Observe(2u, 10u) == DeferredRequestObservation::unexpected);
    PBVP_CHECK(!observer.IsActive());

    observer.Arm();
    PBVP_CHECK(observer.Observe(1u, kDeferredRequestTimeoutMilliseconds) ==
               DeferredRequestObservation::timed_out);
    PBVP_CHECK(!observer.IsActive());

    observer.Arm();
    observer.Cancel();
    PBVP_CHECK(!observer.IsActive());
    PBVP_CHECK(observer.Observe(1u, 0u) == DeferredRequestObservation::none);
}
