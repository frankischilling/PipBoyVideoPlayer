#include "pbvp/playback_state.hpp"

#include "test_support.hpp"

void RunPlaybackStateTests() {
    pbvp::PlaybackStateMachine state;
    PBVP_CHECK(state.Snapshot().state == pbvp::PlaybackState::idle);
    PBVP_CHECK(state.BeginOpen());
    PBVP_CHECK(!state.BeginOpen());
    PBVP_CHECK(state.MediaOpened());
    PBVP_CHECK(state.BufferReady());
    PBVP_CHECK(state.Snapshot().state == pbvp::PlaybackState::playing);
    PBVP_CHECK(state.Pause());
    PBVP_CHECK(state.Snapshot().state == pbvp::PlaybackState::paused);
    PBVP_CHECK(state.BeginSeek());
    PBVP_CHECK(state.Snapshot().state == pbvp::PlaybackState::buffering);
    PBVP_CHECK(state.Snapshot().pause_after_buffering);
    PBVP_CHECK(state.BufferReady());
    PBVP_CHECK(state.Snapshot().state == pbvp::PlaybackState::paused);
    PBVP_CHECK(state.Resume());
    PBVP_CHECK(state.BeginSeek());
    PBVP_CHECK(state.BufferReady());
    PBVP_CHECK(state.Snapshot().state == pbvp::PlaybackState::playing);
    PBVP_CHECK(state.BeginStop());
    PBVP_CHECK(state.BeginStop());
    PBVP_CHECK(state.FinishStop());
    PBVP_CHECK(state.Snapshot().state == pbvp::PlaybackState::idle);

    PBVP_CHECK(state.BeginOpen());
    PBVP_CHECK(state.MediaOpened());
    PBVP_CHECK(state.Pause());
    PBVP_CHECK(state.BufferReady());
    PBVP_CHECK(state.Snapshot().state == pbvp::PlaybackState::paused);
    PBVP_CHECK(state.BeginStop());
    PBVP_CHECK(state.FinishStop());

    PBVP_CHECK(state.BeginOpen());
    state.Fail(pbvp::PlaybackError::decoder_failed);
    PBVP_CHECK(state.Snapshot().state == pbvp::PlaybackState::error);
    PBVP_CHECK(state.Snapshot().error == pbvp::PlaybackError::decoder_failed);
    PBVP_CHECK(state.AcknowledgeError());
    PBVP_CHECK(state.FinishStop());
    PBVP_CHECK(state.Snapshot().error == pbvp::PlaybackError::none);

    state.SetUnavailable();
    PBVP_CHECK(state.Snapshot().state == pbvp::PlaybackState::unavailable);
    PBVP_CHECK(!state.BeginOpen());
    state.Fail(pbvp::PlaybackError::decoder_failed);
    PBVP_CHECK(state.Snapshot().state == pbvp::PlaybackState::unavailable);
    PBVP_CHECK(state.SetAvailable());
    PBVP_CHECK(!state.SetAvailable());
    PBVP_CHECK(state.Snapshot().state == pbvp::PlaybackState::idle);
}
