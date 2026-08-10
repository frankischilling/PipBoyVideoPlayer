#pragma once

#include <cstdint>

namespace pbvp {

enum class PlaybackState : std::uint32_t {
    unavailable,
    idle,
    opening,
    buffering,
    playing,
    paused,
    stopping,
    error,
};

enum class PlaybackError : std::uint32_t {
    none,
    media_open_failed,
    decoder_failed,
    audio_initialization_failed,
    audio_device_failed,
    audio_stream_failed,
    clock_unavailable,
    render_failed,
    invalid_state,
};

const char* PlaybackStateName(PlaybackState state) noexcept;
const char* PlaybackErrorName(PlaybackError error) noexcept;

struct PlaybackStateSnapshot final {
    PlaybackState state{PlaybackState::idle};
    PlaybackError error{PlaybackError::none};
    bool pause_after_buffering{};
};

class PlaybackStateMachine final {
public:
    [[nodiscard]] bool SetAvailable() noexcept;
    void SetUnavailable() noexcept;

    [[nodiscard]] bool BeginOpen() noexcept;
    [[nodiscard]] bool MediaOpened() noexcept;
    [[nodiscard]] bool BufferReady() noexcept;
    [[nodiscard]] bool Pause() noexcept;
    [[nodiscard]] bool Resume() noexcept;
    [[nodiscard]] bool BeginSeek() noexcept;
    [[nodiscard]] bool BeginStop() noexcept;
    [[nodiscard]] bool FinishStop() noexcept;
    [[nodiscard]] bool AcknowledgeError() noexcept;
    void Fail(PlaybackError error) noexcept;

    [[nodiscard]] PlaybackStateSnapshot Snapshot() const noexcept;

private:
    PlaybackState state_{PlaybackState::idle};
    PlaybackError error_{PlaybackError::none};
    bool pause_after_buffering_{};
};

} // namespace pbvp
