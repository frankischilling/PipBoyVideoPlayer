#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>

namespace pbvp {

struct QueueLimits {
    std::size_t maximum_items{};
    std::size_t maximum_payload_bytes{};
};

enum class QueuePushStatus : std::uint32_t {
    accepted,
    full,
    item_too_large,
    stale_generation,
    closed,
    allocation_failed,
};

enum class QueuePopStatus : std::uint32_t {
    item,
    empty,
    closed,
};

template <typename Item>
struct QueuePopResult {
    QueuePopStatus status{QueuePopStatus::empty};
    std::optional<Item> value{};
    std::size_t payload_bytes{};
    std::uint64_t generation{};
};

template <typename Item>
class BoundedQueue final {
    static_assert(std::is_nothrow_move_constructible_v<Item>);

public:
    explicit BoundedQueue(const QueueLimits limits, const std::uint64_t generation = 1u) noexcept
        : limits_(limits), generation_(generation),
          valid_(limits.maximum_items > 0u && limits.maximum_payload_bytes > 0u) {}

    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;

    bool IsValid() const noexcept { return valid_; }

    QueuePushStatus TryPush(
        Item item,
        const std::size_t payload_bytes,
        const std::uint64_t generation) noexcept {
        std::unique_lock lock(mutex_);
        const QueuePushStatus readiness = PushReadiness(payload_bytes, generation);
        if (readiness != QueuePushStatus::accepted) {
            return readiness;
        }
        return CommitPush(std::move(item), payload_bytes, generation);
    }

    QueuePushStatus WaitPush(
        Item item,
        const std::size_t payload_bytes,
        const std::uint64_t generation) noexcept {
        std::unique_lock lock(mutex_);
        if (!valid_ || payload_bytes > limits_.maximum_payload_bytes) {
            return QueuePushStatus::item_too_large;
        }
        try {
            space_available_.wait(lock, [this, payload_bytes, generation] {
                return closed_ || generation != generation_ || HasCapacity(payload_bytes);
            });
        } catch (...) {
            return QueuePushStatus::allocation_failed;
        }
        const QueuePushStatus readiness = PushReadiness(payload_bytes, generation);
        if (readiness != QueuePushStatus::accepted) {
            return readiness;
        }
        return CommitPush(std::move(item), payload_bytes, generation);
    }

    QueuePopResult<Item> TryPop() noexcept {
        std::unique_lock lock(mutex_);
        return PopLocked();
    }

    QueuePopResult<Item> WaitPop() noexcept {
        std::unique_lock lock(mutex_);
        try {
            item_available_.wait(lock, [this] { return closed_ || !items_.empty(); });
        } catch (...) {
            return {QueuePopStatus::closed, std::nullopt, 0u, generation_};
        }
        return PopLocked();
    }

    bool AdvanceGeneration(const std::uint64_t generation) noexcept {
        std::unique_lock lock(mutex_);
        if (closed_ || generation <= generation_) {
            return false;
        }
        items_.clear();
        payload_bytes_ = 0u;
        generation_ = generation;
        lock.unlock();
        space_available_.notify_all();
        item_available_.notify_all();
        return true;
    }

    void Close() noexcept {
        {
            std::scoped_lock lock(mutex_);
            closed_ = true;
        }
        space_available_.notify_all();
        item_available_.notify_all();
    }

    bool IsClosed() const noexcept {
        std::scoped_lock lock(mutex_);
        return closed_;
    }

    std::size_t Size() const noexcept {
        std::scoped_lock lock(mutex_);
        return items_.size();
    }

    std::size_t PayloadBytes() const noexcept {
        std::scoped_lock lock(mutex_);
        return payload_bytes_;
    }

    std::uint64_t Generation() const noexcept {
        std::scoped_lock lock(mutex_);
        return generation_;
    }

private:
    struct Entry {
        Item item;
        std::size_t payload_bytes{};
        std::uint64_t generation{};
    };

    bool HasCapacity(const std::size_t payload_bytes) const noexcept {
        return items_.size() < limits_.maximum_items &&
               payload_bytes <= limits_.maximum_payload_bytes - payload_bytes_;
    }

    QueuePushStatus PushReadiness(
        const std::size_t payload_bytes,
        const std::uint64_t generation) const noexcept {
        if (!valid_ || payload_bytes > limits_.maximum_payload_bytes) {
            return QueuePushStatus::item_too_large;
        }
        if (closed_) {
            return QueuePushStatus::closed;
        }
        if (generation != generation_) {
            return QueuePushStatus::stale_generation;
        }
        return HasCapacity(payload_bytes) ? QueuePushStatus::accepted : QueuePushStatus::full;
    }

    QueuePushStatus CommitPush(
        Item item,
        const std::size_t payload_bytes,
        const std::uint64_t generation) noexcept {
        try {
            items_.push_back({std::move(item), payload_bytes, generation});
        } catch (...) {
            return QueuePushStatus::allocation_failed;
        }
        payload_bytes_ += payload_bytes;
        item_available_.notify_one();
        return QueuePushStatus::accepted;
    }

    QueuePopResult<Item> PopLocked() noexcept {
        if (items_.empty()) {
            return {closed_ ? QueuePopStatus::closed : QueuePopStatus::empty,
                    std::nullopt, 0u, generation_};
        }
        Entry entry = std::move(items_.front());
        items_.pop_front();
        payload_bytes_ -= entry.payload_bytes;
        space_available_.notify_one();
        return {QueuePopStatus::item, std::move(entry.item),
                entry.payload_bytes, entry.generation};
    }

    QueueLimits limits_{};
    mutable std::mutex mutex_{};
    std::condition_variable space_available_{};
    std::condition_variable item_available_{};
    std::deque<Entry> items_{};
    std::size_t payload_bytes_{};
    std::uint64_t generation_{};
    bool valid_{};
    bool closed_{};
};

} // namespace pbvp
