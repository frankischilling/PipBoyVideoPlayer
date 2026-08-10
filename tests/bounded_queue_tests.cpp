#include "pbvp/bounded_queue.hpp"

#include "test_support.hpp"

#include <chrono>
#include <cstdint>
#include <future>
#include <memory>

namespace {

struct QueueItem {
    std::unique_ptr<int> value{};

    explicit QueueItem(const int input) : value(std::make_unique<int>(input)) {}
    QueueItem(QueueItem&&) noexcept = default;
    QueueItem& operator=(QueueItem&&) noexcept = default;
};

} // namespace

void RunBoundedQueueTests() {
    using namespace std::chrono_literals;
    using pbvp::QueuePopStatus;
    using pbvp::QueuePushStatus;

    pbvp::BoundedQueue<QueueItem> queue({2u, 100u}, 7u);
    PBVP_CHECK(queue.IsValid());
    PBVP_CHECK(queue.TryPush(QueueItem(1), 40u, 7u) == QueuePushStatus::accepted);
    PBVP_CHECK(queue.TryPush(QueueItem(2), 60u, 7u) == QueuePushStatus::accepted);
    PBVP_CHECK(queue.Size() == 2u);
    PBVP_CHECK(queue.PayloadBytes() == 100u);
    PBVP_CHECK(queue.TryPush(QueueItem(3), 1u, 7u) == QueuePushStatus::full);
    PBVP_CHECK(queue.TryPush(QueueItem(3), 101u, 7u) == QueuePushStatus::item_too_large);
    PBVP_CHECK(queue.TryPush(QueueItem(3), 1u, 6u) == QueuePushStatus::stale_generation);

    auto first = queue.TryPop();
    PBVP_CHECK(first.status == QueuePopStatus::item);
    PBVP_CHECK(first.value.has_value());
    PBVP_CHECK(*first.value->value == 1);
    PBVP_CHECK(first.payload_bytes == 40u);
    PBVP_CHECK(first.generation == 7u);
    PBVP_CHECK(queue.PayloadBytes() == 60u);

    PBVP_CHECK(queue.AdvanceGeneration(8u));
    PBVP_CHECK(!queue.AdvanceGeneration(8u));
    PBVP_CHECK(queue.Size() == 0u);
    PBVP_CHECK(queue.PayloadBytes() == 0u);
    PBVP_CHECK(queue.TryPush(QueueItem(4), 100u, 7u) == QueuePushStatus::stale_generation);
    PBVP_CHECK(queue.TryPush(QueueItem(5), 100u, 8u) == QueuePushStatus::accepted);

    auto stale_waiter = std::async(std::launch::async, [&queue] {
        return queue.WaitPush(QueueItem(6), 1u, 8u);
    });
    PBVP_CHECK(stale_waiter.wait_for(50ms) == std::future_status::timeout);
    PBVP_CHECK(queue.AdvanceGeneration(9u));
    PBVP_CHECK(stale_waiter.wait_for(1s) == std::future_status::ready);
    PBVP_CHECK(stale_waiter.get() == QueuePushStatus::stale_generation);
    PBVP_CHECK(queue.Size() == 0u);
    PBVP_CHECK(queue.Generation() == 9u);
    PBVP_CHECK(queue.TryPush(QueueItem(7), 100u, 9u) == QueuePushStatus::accepted);

    auto waiter = std::async(std::launch::async, [&queue] {
        return queue.WaitPush(QueueItem(8), 1u, 9u);
    });
    PBVP_CHECK(waiter.wait_for(50ms) == std::future_status::timeout);
    queue.Close();
    PBVP_CHECK(queue.IsClosed());
    PBVP_CHECK(waiter.wait_for(1s) == std::future_status::ready);
    PBVP_CHECK(waiter.get() == QueuePushStatus::closed);

    auto last = queue.WaitPop();
    PBVP_CHECK(last.status == QueuePopStatus::item);
    PBVP_CHECK(last.value.has_value());
    PBVP_CHECK(*last.value->value == 7);
    PBVP_CHECK(queue.WaitPop().status == QueuePopStatus::closed);
    PBVP_CHECK(queue.TryPush(QueueItem(9), 1u, 9u) == QueuePushStatus::closed);

    pbvp::BoundedQueue<QueueItem> invalid({0u, 0u});
    PBVP_CHECK(!invalid.IsValid());
    PBVP_CHECK(invalid.TryPush(QueueItem(1), 1u, 1u) == QueuePushStatus::item_too_large);
}
