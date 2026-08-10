#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace pbvp {

enum class XAudioStreamStatus : std::uint32_t {
    ok,
    not_initialized,
    already_initialized,
    wrong_thread,
    invalid_config,
    allocation_failed,
    engine_creation_failed,
    mastering_voice_failed,
    source_voice_failed,
    invalid_state,
    invalid_pcm,
    buffer_too_large,
    queue_full,
    stale_generation,
    insufficient_prebuffer,
    operation_failed,
    audio_device_failed,
};

const char* XAudioStreamStatusName(XAudioStreamStatus status) noexcept;

struct XAudioStreamConfig final {
    std::uint32_t sample_rate{48'000u};
    std::uint32_t channels{2u};
    std::uint32_t prebuffer_ms{200u};
    std::uint32_t slot_count{16u};
    std::size_t slot_bytes{16u * 1024u};
};

struct XAudioStreamSnapshot final {
    XAudioStreamStatus status{XAudioStreamStatus::not_initialized};
    std::int32_t error_code{};
    std::uint32_t queued_buffers{};
    std::size_t queued_bytes{};
    std::size_t pool_bytes{};
    std::uint64_t samples_played{};
    std::uint64_t submitted_samples{};
    std::uint64_t completed_buffers{};
    std::uint64_t stream_end_callbacks{};
    std::uint64_t underruns{};
    std::uint64_t generation{};
    bool initialized{};
    bool started{};
    bool paused{};
    bool muted{};
    bool ready_to_start{};
    bool end_of_stream_submitted{};
    bool end_of_stream_reached{};
};

class XAudioStream final {
public:
    XAudioStream() noexcept = default;
    ~XAudioStream();

    XAudioStream(const XAudioStream&) = delete;
    XAudioStream& operator=(const XAudioStream&) = delete;

    XAudioStreamStatus Initialize(const XAudioStreamConfig& config) noexcept;
    void Shutdown() noexcept;
    XAudioStreamStatus RecoverDevice() noexcept;

    XAudioStreamStatus SubmitPcm(
        std::span<const std::int16_t> interleaved_samples,
        std::int64_t pts_us,
        std::uint64_t generation,
        bool end_of_stream) noexcept;
    XAudioStreamStatus Start() noexcept;
    XAudioStreamStatus Pause() noexcept;
    XAudioStreamStatus Resume() noexcept;
    XAudioStreamStatus StopAndFlush() noexcept;
    XAudioStreamStatus SetVolume(float volume) noexcept;
    XAudioStreamStatus SetMuted(bool muted) noexcept;

    [[nodiscard]] std::optional<std::int64_t> MediaTimeUs() noexcept;
    [[nodiscard]] XAudioStreamSnapshot Snapshot() noexcept;

private:
    struct Impl;
    Impl* impl_{};
};

} // namespace pbvp
