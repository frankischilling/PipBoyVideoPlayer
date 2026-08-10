#include "pbvp/playback_state.hpp"

namespace pbvp {

const char* PlaybackStateName(const PlaybackState state) noexcept {
    switch (state) {
        case PlaybackState::unavailable: return "unavailable";
        case PlaybackState::idle: return "idle";
        case PlaybackState::opening: return "opening";
        case PlaybackState::buffering: return "buffering";
        case PlaybackState::playing: return "playing";
        case PlaybackState::paused: return "paused";
        case PlaybackState::stopping: return "stopping";
        case PlaybackState::error: return "error";
    }
    return "unknown";
}

const char* PlaybackErrorName(const PlaybackError error) noexcept {
    switch (error) {
        case PlaybackError::none: return "none";
        case PlaybackError::media_open_failed: return "media_open_failed";
        case PlaybackError::decoder_failed: return "decoder_failed";
        case PlaybackError::decoder_memory_failed: return "decoder_memory_failed";
        case PlaybackError::audio_initialization_failed:
            return "audio_initialization_failed";
        case PlaybackError::audio_device_failed: return "audio_device_failed";
        case PlaybackError::audio_stream_failed: return "audio_stream_failed";
        case PlaybackError::clock_unavailable: return "clock_unavailable";
        case PlaybackError::render_failed: return "render_failed";
        case PlaybackError::invalid_state: return "invalid_state";
    }
    return "unknown";
}

bool PlaybackStateMachine::SetAvailable() noexcept {
    if (state_ != PlaybackState::unavailable) {
        return false;
    }
    state_ = PlaybackState::idle;
    error_ = PlaybackError::none;
    pause_after_buffering_ = false;
    return true;
}

void PlaybackStateMachine::SetUnavailable() noexcept {
    state_ = PlaybackState::unavailable;
    error_ = PlaybackError::none;
    pause_after_buffering_ = false;
}

bool PlaybackStateMachine::BeginOpen() noexcept {
    if (state_ != PlaybackState::idle) {
        return false;
    }
    state_ = PlaybackState::opening;
    error_ = PlaybackError::none;
    pause_after_buffering_ = false;
    return true;
}

bool PlaybackStateMachine::MediaOpened() noexcept {
    if (state_ != PlaybackState::opening) {
        return false;
    }
    state_ = PlaybackState::buffering;
    return true;
}

bool PlaybackStateMachine::BufferReady() noexcept {
    if (state_ != PlaybackState::buffering) {
        return false;
    }
    state_ = pause_after_buffering_ ? PlaybackState::paused : PlaybackState::playing;
    return true;
}

bool PlaybackStateMachine::Pause() noexcept {
    if (state_ == PlaybackState::playing) {
        state_ = PlaybackState::paused;
        return true;
    }
    if (state_ == PlaybackState::buffering) {
        pause_after_buffering_ = true;
        return true;
    }
    return false;
}

bool PlaybackStateMachine::Resume() noexcept {
    if (state_ == PlaybackState::paused) {
        state_ = PlaybackState::playing;
        pause_after_buffering_ = false;
        return true;
    }
    if (state_ == PlaybackState::buffering) {
        pause_after_buffering_ = false;
        return true;
    }
    return false;
}

bool PlaybackStateMachine::BeginSeek() noexcept {
    if (state_ == PlaybackState::paused) {
        pause_after_buffering_ = true;
    } else if (state_ == PlaybackState::playing) {
        pause_after_buffering_ = false;
    } else if (state_ != PlaybackState::buffering) {
        return false;
    }
    state_ = PlaybackState::buffering;
    return true;
}

bool PlaybackStateMachine::BeginRebuffer() noexcept {
    if (state_ != PlaybackState::playing) {
        return false;
    }
    pause_after_buffering_ = false;
    state_ = PlaybackState::buffering;
    return true;
}

bool PlaybackStateMachine::BeginStop() noexcept {
    switch (state_) {
        case PlaybackState::opening:
        case PlaybackState::buffering:
        case PlaybackState::playing:
        case PlaybackState::paused:
        case PlaybackState::error:
            state_ = PlaybackState::stopping;
            return true;
        case PlaybackState::stopping:
            return true;
        default:
            return false;
    }
}

bool PlaybackStateMachine::FinishStop() noexcept {
    if (state_ != PlaybackState::stopping) {
        return false;
    }
    state_ = PlaybackState::idle;
    error_ = PlaybackError::none;
    pause_after_buffering_ = false;
    return true;
}

bool PlaybackStateMachine::AcknowledgeError() noexcept {
    if (state_ != PlaybackState::error) {
        return false;
    }
    state_ = PlaybackState::stopping;
    return true;
}

void PlaybackStateMachine::Fail(const PlaybackError error) noexcept {
    if (state_ == PlaybackState::unavailable) {
        return;
    }
    state_ = PlaybackState::error;
    error_ = error == PlaybackError::none ? PlaybackError::invalid_state : error;
}

PlaybackStateSnapshot PlaybackStateMachine::Snapshot() const noexcept {
    return {state_, error_, pause_after_buffering_};
}

} // namespace pbvp
