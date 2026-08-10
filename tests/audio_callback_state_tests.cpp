#include "pbvp/audio_callback_state.hpp"

#include "test_support.hpp"

#include <cstdint>

void RunAudioCallbackStateTests() {
    pbvp::AudioCallbackState state;
    PBVP_CHECK(state.CompletedBuffers() == 0u);
    PBVP_CHECK(state.StreamEnds() == 0u);
    PBVP_CHECK(state.VoiceError() == 0);
    PBVP_CHECK(state.EngineError() == 0);

    state.RecordBufferEnd();
    state.RecordBufferEnd();
    state.RecordStreamEnd();
    state.RecordVoiceError(static_cast<std::int32_t>(0x88960001u));
    state.RecordEngineError(static_cast<std::int32_t>(0x88960004u));
    PBVP_CHECK(state.CompletedBuffers() == 2u);
    PBVP_CHECK(state.StreamEnds() == 1u);
    PBVP_CHECK(state.VoiceError() == static_cast<std::int32_t>(0x88960001u));
    PBVP_CHECK(state.EngineError() == static_cast<std::int32_t>(0x88960004u));

    state.Reset();
    PBVP_CHECK(state.CompletedBuffers() == 0u);
    PBVP_CHECK(state.StreamEnds() == 0u);
    PBVP_CHECK(state.VoiceError() == 0);
    PBVP_CHECK(state.EngineError() == 0);
}
