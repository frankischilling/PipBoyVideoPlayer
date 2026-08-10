#pragma once

#include <atomic>
#include <cstdint>

namespace pbvp {

class AudioCallbackState final {
public:
    void RecordBufferEnd() noexcept {
        completed_buffers_.fetch_add(1u, std::memory_order_relaxed);
    }

    void RecordStreamEnd() noexcept {
        stream_ends_.fetch_add(1u, std::memory_order_relaxed);
    }

    void RecordVoiceError(const std::int32_t error) noexcept {
        voice_error_.store(error, std::memory_order_release);
    }

    void RecordEngineError(const std::int32_t error) noexcept {
        engine_error_.store(error, std::memory_order_release);
    }

    [[nodiscard]] std::uint64_t CompletedBuffers() const noexcept {
        return completed_buffers_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] std::uint64_t StreamEnds() const noexcept {
        return stream_ends_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] std::int32_t VoiceError() const noexcept {
        return voice_error_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::int32_t EngineError() const noexcept {
        return engine_error_.load(std::memory_order_acquire);
    }

    void Reset() noexcept {
        completed_buffers_.store(0u, std::memory_order_relaxed);
        stream_ends_.store(0u, std::memory_order_relaxed);
        voice_error_.store(0, std::memory_order_relaxed);
        engine_error_.store(0, std::memory_order_relaxed);
    }

private:
    std::atomic<std::uint64_t> completed_buffers_{};
    std::atomic<std::uint64_t> stream_ends_{};
    std::atomic<std::int32_t> voice_error_{};
    std::atomic<std::int32_t> engine_error_{};
};

} // namespace pbvp
