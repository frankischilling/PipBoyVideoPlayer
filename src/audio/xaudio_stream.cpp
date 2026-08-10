#include "pbvp/xaudio_stream.hpp"

#include "pbvp/audio_callback_state.hpp"
#include "pbvp/checked_math.hpp"
#include "pbvp/playback_clock.hpp"

#include <Windows.h>
#include <xaudio2.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <new>

namespace pbvp {
namespace {

constexpr std::uint32_t kMinimumSampleRate = 8'000u;
constexpr std::uint32_t kMaximumSampleRate = 192'000u;
constexpr std::uint32_t kMinimumSlots = 2u;
constexpr std::uint32_t kMaximumSlots = 32u;
constexpr std::size_t kMinimumSlotBytes = 1u * 1024u;
constexpr std::size_t kMaximumSlotBytes = 64u * 1024u;
constexpr std::size_t kMaximumPoolBytes = 2u * 1024u * 1024u;
constexpr std::uint32_t kMinimumPrebufferMs = 50u;
constexpr std::uint32_t kMaximumPrebufferMs = 1'000u;

enum class SlotState : std::uint32_t {
    free,
    submitted,
    completed,
};

} // namespace

struct XAudioStream::Impl final {
    struct Slot final {
        std::atomic<SlotState> state{SlotState::free};
        std::uint8_t* data{};
        std::uint32_t payload_bytes{};
        std::uint32_t samples_per_channel{};
        std::uint64_t generation{};
    };

    class VoiceCallback final : public IXAudio2VoiceCallback {
    public:
        explicit VoiceCallback(AudioCallbackState& callback_state) noexcept
            : callback_state_(callback_state) {}

        void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) noexcept override {}
        void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() noexcept override {}

        void STDMETHODCALLTYPE OnStreamEnd() noexcept override {
            callback_state_.RecordStreamEnd();
        }

        void STDMETHODCALLTYPE OnBufferStart(void*) noexcept override {}

        void STDMETHODCALLTYPE OnBufferEnd(void* context) noexcept override {
            if (context != nullptr) {
                static_cast<Slot*>(context)->state.store(
                    SlotState::completed, std::memory_order_release);
            }
            callback_state_.RecordBufferEnd();
        }

        void STDMETHODCALLTYPE OnLoopEnd(void*) noexcept override {}

        void STDMETHODCALLTYPE OnVoiceError(void* context, HRESULT error) noexcept override {
            if (context != nullptr) {
                static_cast<Slot*>(context)->state.store(
                    SlotState::completed, std::memory_order_release);
            }
            callback_state_.RecordVoiceError(static_cast<std::int32_t>(error));
        }

    private:
        AudioCallbackState& callback_state_;
    };

    class EngineCallback final : public IXAudio2EngineCallback {
    public:
        explicit EngineCallback(AudioCallbackState& callback_state) noexcept
            : callback_state_(callback_state) {}

        void STDMETHODCALLTYPE OnProcessingPassStart() noexcept override {}
        void STDMETHODCALLTYPE OnProcessingPassEnd() noexcept override {}

        void STDMETHODCALLTYPE OnCriticalError(HRESULT error) noexcept override {
            callback_state_.RecordEngineError(static_cast<std::int32_t>(error));
        }

    private:
        AudioCallbackState& callback_state_;
    };

    Impl() noexcept
        : voice_callback(callback_state), engine_callback(callback_state) {}

    bool IsOwnerThread() const noexcept {
        return owner_thread_id != 0u && GetCurrentThreadId() == owner_thread_id;
    }

    XAudioStreamStatus SetStatus(
        const XAudioStreamStatus status,
        const HRESULT error = S_OK) noexcept {
        last_status = status;
        last_error = static_cast<std::int32_t>(error);
        return status;
    }

    std::int32_t DeviceError() const noexcept {
        const std::int32_t voice_error = callback_state.VoiceError();
        return voice_error != 0 ? voice_error : callback_state.EngineError();
    }

    bool ValidateConfig(const XAudioStreamConfig& candidate) noexcept {
        if (candidate.sample_rate < kMinimumSampleRate ||
            candidate.sample_rate > kMaximumSampleRate ||
            candidate.channels == 0u || candidate.channels > 2u ||
            candidate.prebuffer_ms < kMinimumPrebufferMs ||
            candidate.prebuffer_ms > kMaximumPrebufferMs ||
            candidate.slot_count < kMinimumSlots ||
            candidate.slot_count > kMaximumSlots ||
            candidate.slot_bytes < kMinimumSlotBytes ||
            candidate.slot_bytes > kMaximumSlotBytes) {
            return false;
        }

        const std::size_t block_align =
            static_cast<std::size_t>(candidate.channels) * sizeof(std::int16_t);
        if (candidate.slot_bytes % block_align != 0u) {
            return false;
        }

        if (!CheckedMultiplySize(
                candidate.slot_count, candidate.slot_bytes, pool_bytes) ||
            pool_bytes > kMaximumPoolBytes) {
            pool_bytes = 0u;
            return false;
        }

        const std::uint64_t prebuffer_product =
            static_cast<std::uint64_t>(candidate.sample_rate) * candidate.prebuffer_ms;
        required_prebuffer_frames =
            (prebuffer_product + 999u) / 1'000u;
        return required_prebuffer_frames != 0u;
    }

    XAudioStreamStatus AllocatePool() noexcept {
        slots.reset(new (std::nothrow) Slot[config.slot_count]);
        storage.reset(new (std::nothrow) std::uint8_t[pool_bytes]);
        if (!slots || !storage) {
            slots.reset();
            storage.reset();
            return SetStatus(XAudioStreamStatus::allocation_failed, E_OUTOFMEMORY);
        }

        for (std::uint32_t index = 0u; index < config.slot_count; ++index) {
            slots[index].data = storage.get() + index * config.slot_bytes;
        }
        return SetStatus(XAudioStreamStatus::ok);
    }

    XAudioStreamStatus InitializeCom() noexcept {
        const HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (result == S_OK || result == S_FALSE) {
            com_initialized = true;
            com_uninitialize_required = true;
            return SetStatus(XAudioStreamStatus::ok);
        }
        if (result == RPC_E_CHANGED_MODE) {
            com_initialized = true;
            com_uninitialize_required = false;
            return SetStatus(XAudioStreamStatus::ok);
        }
        return SetStatus(XAudioStreamStatus::engine_creation_failed, result);
    }

    void ReleaseCom() noexcept {
        if (com_uninitialize_required && IsOwnerThread()) {
            CoUninitialize();
        }
        com_initialized = false;
        com_uninitialize_required = false;
    }

    XAudioStreamStatus CreateSourceVoice() noexcept {
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = static_cast<WORD>(config.channels);
        format.nSamplesPerSec = config.sample_rate;
        format.wBitsPerSample = static_cast<WORD>(sizeof(std::int16_t) * 8u);
        format.nBlockAlign = static_cast<WORD>(
            config.channels * sizeof(std::int16_t));
        format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
        format.cbSize = 0u;

        const HRESULT result = engine->CreateSourceVoice(
            &source_voice,
            &format,
            0u,
            XAUDIO2_DEFAULT_FREQ_RATIO,
            &voice_callback,
            nullptr,
            nullptr);
        if (FAILED(result)) {
            source_voice = nullptr;
            return SetStatus(XAudioStreamStatus::source_voice_failed, result);
        }

        const HRESULT volume_result = source_voice->SetVolume(
            muted ? 0.0f : volume,
            XAUDIO2_COMMIT_NOW);
        if (FAILED(volume_result)) {
            source_voice->DestroyVoice();
            source_voice = nullptr;
            return SetStatus(XAudioStreamStatus::source_voice_failed, volume_result);
        }
        return SetStatus(XAudioStreamStatus::ok);
    }

    XAudioStreamStatus CreateAudioObjects() noexcept {
        HRESULT result = XAudio2Create(&engine, 0u, XAUDIO2_DEFAULT_PROCESSOR);
        if (FAILED(result)) {
            engine = nullptr;
            return SetStatus(XAudioStreamStatus::engine_creation_failed, result);
        }

        result = engine->RegisterForCallbacks(&engine_callback);
        if (FAILED(result)) {
            engine->Release();
            engine = nullptr;
            return SetStatus(XAudioStreamStatus::engine_creation_failed, result);
        }
        engine_callback_registered = true;

        result = engine->CreateMasteringVoice(&mastering_voice);
        if (FAILED(result)) {
            engine->UnregisterForCallbacks(&engine_callback);
            engine_callback_registered = false;
            engine->Release();
            engine = nullptr;
            mastering_voice = nullptr;
            return SetStatus(XAudioStreamStatus::mastering_voice_failed, result);
        }

        return CreateSourceVoice();
    }

    void DestroySourceVoice() noexcept {
        if (source_voice != nullptr) {
            source_voice->DestroyVoice();
            source_voice = nullptr;
        }
    }

    void DestroyAudioObjects() noexcept {
        DestroySourceVoice();
        if (mastering_voice != nullptr) {
            mastering_voice->DestroyVoice();
            mastering_voice = nullptr;
        }
        if (engine != nullptr) {
            if (engine_callback_registered) {
                engine->UnregisterForCallbacks(&engine_callback);
                engine_callback_registered = false;
            }
            engine->Release();
            engine = nullptr;
        }
    }

    void ResetStreamState() noexcept {
        if (slots) {
            for (std::uint32_t index = 0u; index < config.slot_count; ++index) {
                slots[index].payload_bytes = 0u;
                slots[index].samples_per_channel = 0u;
                slots[index].generation = 0u;
                slots[index].state.store(SlotState::free, std::memory_order_relaxed);
            }
        }
        callback_state.Reset();
        sample_clock.Clear();
        queued_bytes = 0u;
        queued_frames = 0u;
        submitted_frames = 0u;
        end_media_time_us.reset();
        generation = 0u;
        generation_active = false;
        started = false;
        paused = false;
        end_of_stream_submitted = false;
        empty_observed = false;
    }

    void ReclaimCompleted() noexcept {
        if (!slots) {
            return;
        }
        for (std::uint32_t index = 0u; index < config.slot_count; ++index) {
            Slot& slot = slots[index];
            if (slot.state.load(std::memory_order_acquire) != SlotState::completed) {
                continue;
            }

            queued_bytes = slot.payload_bytes > queued_bytes
                ? 0u
                : queued_bytes - slot.payload_bytes;
            queued_frames = slot.samples_per_channel > queued_frames
                ? 0u
                : queued_frames - slot.samples_per_channel;
            slot.payload_bytes = 0u;
            slot.samples_per_channel = 0u;
            slot.generation = 0u;
            slot.state.store(SlotState::free, std::memory_order_release);
        }
    }

    bool ReadyToStart() const noexcept {
        return end_of_stream_submitted || queued_frames >= required_prebuffer_frames;
    }

    XAudioStreamSnapshot ReadSnapshot() noexcept {
        XAudioStreamSnapshot snapshot{};
        snapshot.status = last_status;
        snapshot.error_code = last_error;
        snapshot.pool_bytes = pool_bytes;
        snapshot.submitted_samples = submitted_frames;
        snapshot.completed_buffers = callback_state.CompletedBuffers();
        snapshot.stream_end_callbacks = callback_state.StreamEnds();
        snapshot.underruns = underruns;
        snapshot.generation = generation;
        snapshot.initialized = initialized;
        snapshot.started = started;
        snapshot.paused = paused;
        snapshot.muted = muted;
        snapshot.ready_to_start = ReadyToStart();
        snapshot.end_of_stream_submitted = end_of_stream_submitted;
        snapshot.end_of_stream_reached = callback_state.StreamEnds() != 0u;

        if (!initialized || source_voice == nullptr) {
            return snapshot;
        }

        ReclaimCompleted();
        XAUDIO2_VOICE_STATE state{};
        source_voice->GetState(&state, 0u);
        snapshot.queued_buffers = state.BuffersQueued;
        snapshot.samples_played = state.SamplesPlayed;
        snapshot.queued_bytes = queued_bytes;

        const std::int32_t device_error = DeviceError();
        if (device_error != 0) {
            snapshot.status = XAudioStreamStatus::audio_device_failed;
            snapshot.error_code = device_error;
            last_status = snapshot.status;
            last_error = device_error;
        }

        const bool empty_now =
            started && !paused && !end_of_stream_submitted &&
            state.BuffersQueued == 0u;
        if (empty_now && !empty_observed) {
            ++underruns;
            snapshot.underruns = underruns;
        }
        empty_observed = empty_now;
        return snapshot;
    }

    XAudioStreamConfig config{};
    std::unique_ptr<Slot[]> slots{};
    std::unique_ptr<std::uint8_t[]> storage{};
    AudioCallbackState callback_state{};
    VoiceCallback voice_callback;
    EngineCallback engine_callback;
    AudioSampleClock sample_clock{};
    IXAudio2* engine{};
    IXAudio2MasteringVoice* mastering_voice{};
    IXAudio2SourceVoice* source_voice{};
    DWORD owner_thread_id{};
    std::size_t pool_bytes{};
    std::size_t queued_bytes{};
    std::uint64_t required_prebuffer_frames{};
    std::uint64_t queued_frames{};
    std::uint64_t submitted_frames{};
    std::optional<std::int64_t> end_media_time_us{};
    std::uint64_t underruns{};
    std::uint64_t generation{};
    XAudioStreamStatus last_status{XAudioStreamStatus::not_initialized};
    std::int32_t last_error{};
    float volume{1.0f};
    bool initialized{};
    bool com_initialized{};
    bool com_uninitialize_required{};
    bool engine_callback_registered{};
    bool generation_active{};
    bool started{};
    bool paused{};
    bool muted{};
    bool end_of_stream_submitted{};
    bool empty_observed{};
};

const char* XAudioStreamStatusName(const XAudioStreamStatus status) noexcept {
    switch (status) {
    case XAudioStreamStatus::ok: return "ok";
    case XAudioStreamStatus::not_initialized: return "not_initialized";
    case XAudioStreamStatus::already_initialized: return "already_initialized";
    case XAudioStreamStatus::wrong_thread: return "wrong_thread";
    case XAudioStreamStatus::invalid_config: return "invalid_config";
    case XAudioStreamStatus::allocation_failed: return "allocation_failed";
    case XAudioStreamStatus::engine_creation_failed: return "engine_creation_failed";
    case XAudioStreamStatus::mastering_voice_failed: return "mastering_voice_failed";
    case XAudioStreamStatus::source_voice_failed: return "source_voice_failed";
    case XAudioStreamStatus::invalid_state: return "invalid_state";
    case XAudioStreamStatus::invalid_pcm: return "invalid_pcm";
    case XAudioStreamStatus::buffer_too_large: return "buffer_too_large";
    case XAudioStreamStatus::queue_full: return "queue_full";
    case XAudioStreamStatus::stale_generation: return "stale_generation";
    case XAudioStreamStatus::insufficient_prebuffer: return "insufficient_prebuffer";
    case XAudioStreamStatus::operation_failed: return "operation_failed";
    case XAudioStreamStatus::audio_device_failed: return "audio_device_failed";
    }
    return "unknown";
}

XAudioStream::~XAudioStream() {
    Shutdown();
}

XAudioStreamStatus XAudioStream::Initialize(
    const XAudioStreamConfig& config) noexcept {
    if (impl_ != nullptr) {
        return XAudioStreamStatus::already_initialized;
    }

    std::unique_ptr<Impl> candidate(new (std::nothrow) Impl{});
    if (!candidate) {
        return XAudioStreamStatus::allocation_failed;
    }
    if (!candidate->ValidateConfig(config)) {
        return XAudioStreamStatus::invalid_config;
    }

    candidate->config = config;
    candidate->owner_thread_id = GetCurrentThreadId();
    XAudioStreamStatus status = candidate->InitializeCom();
    if (status != XAudioStreamStatus::ok) {
        return status;
    }
    status = candidate->AllocatePool();
    if (status != XAudioStreamStatus::ok) {
        candidate->ReleaseCom();
        return status;
    }
    status = candidate->CreateAudioObjects();
    if (status != XAudioStreamStatus::ok) {
        candidate->DestroyAudioObjects();
        candidate->ReleaseCom();
        return status;
    }

    candidate->initialized = true;
    candidate->SetStatus(XAudioStreamStatus::ok);
    impl_ = candidate.release();
    return XAudioStreamStatus::ok;
}

void XAudioStream::Shutdown() noexcept {
    if (impl_ == nullptr) {
        return;
    }
    impl_->DestroyAudioObjects();
    impl_->ResetStreamState();
    impl_->initialized = false;
    impl_->ReleaseCom();
    delete impl_;
    impl_ = nullptr;
}

XAudioStreamStatus XAudioStream::RecoverDevice() noexcept {
    if (impl_ == nullptr || !impl_->initialized) {
        return XAudioStreamStatus::not_initialized;
    }
    if (!impl_->IsOwnerThread()) {
        return impl_->SetStatus(XAudioStreamStatus::wrong_thread);
    }

    impl_->DestroyAudioObjects();
    impl_->ResetStreamState();
    const XAudioStreamStatus status = impl_->CreateAudioObjects();
    if (status != XAudioStreamStatus::ok) {
        impl_->DestroyAudioObjects();
        return status;
    }
    return impl_->SetStatus(XAudioStreamStatus::ok);
}

XAudioStreamStatus XAudioStream::SubmitPcm(
    const std::span<const std::int16_t> interleaved_samples,
    const std::int64_t pts_us,
    const std::uint64_t generation,
    const bool end_of_stream) noexcept {
    if (impl_ == nullptr || !impl_->initialized || impl_->source_voice == nullptr) {
        return XAudioStreamStatus::not_initialized;
    }
    if (!impl_->IsOwnerThread()) {
        return impl_->SetStatus(XAudioStreamStatus::wrong_thread);
    }
    if (impl_->DeviceError() != 0) {
        return impl_->SetStatus(
            XAudioStreamStatus::audio_device_failed,
            static_cast<HRESULT>(impl_->DeviceError()));
    }
    if (impl_->end_of_stream_submitted) {
        return impl_->SetStatus(XAudioStreamStatus::invalid_state);
    }
    if (interleaved_samples.empty() ||
        interleaved_samples.size() % impl_->config.channels != 0u) {
        return impl_->SetStatus(XAudioStreamStatus::invalid_pcm);
    }

    const std::size_t payload_bytes = interleaved_samples.size_bytes();
    if (payload_bytes > impl_->pool_bytes) {
        return impl_->SetStatus(XAudioStreamStatus::buffer_too_large);
    }
    const std::size_t required_slots =
        payload_bytes / impl_->config.slot_bytes +
        (payload_bytes % impl_->config.slot_bytes == 0u ? 0u : 1u);
    if (required_slots == 0u || required_slots > impl_->config.slot_count) {
        return impl_->SetStatus(XAudioStreamStatus::buffer_too_large);
    }
    const std::uint64_t chunk_frames =
        interleaved_samples.size() / impl_->config.channels;
    if (impl_->submitted_frames >
        (std::numeric_limits<std::uint64_t>::max)() - chunk_frames) {
        return impl_->SetStatus(XAudioStreamStatus::buffer_too_large);
    }

    std::optional<std::int64_t> end_media_time{};
    if (end_of_stream) {
        AudioSampleClock end_clock;
        if (!end_clock.Reset(impl_->config.sample_rate, 0u, pts_us)) {
            return impl_->SetStatus(XAudioStreamStatus::invalid_pcm);
        }
        end_media_time = end_clock.MediaTimeUs(chunk_frames);
        if (!end_media_time.has_value()) {
            return impl_->SetStatus(XAudioStreamStatus::invalid_pcm);
        }
    }

    impl_->ReclaimCompleted();
    if (impl_->generation_active && impl_->generation != generation) {
        return impl_->SetStatus(XAudioStreamStatus::stale_generation);
    }

    std::array<std::uint32_t, kMaximumSlots> available{};
    std::size_t available_count = 0u;
    for (std::uint32_t index = 0u; index < impl_->config.slot_count; ++index) {
        if (impl_->slots[index].state.load(std::memory_order_acquire) == SlotState::free) {
            available[available_count++] = index;
        }
    }
    if (available_count < required_slots) {
        return impl_->SetStatus(XAudioStreamStatus::queue_full);
    }

    XAUDIO2_VOICE_STATE initial_state{};
    if (!impl_->sample_clock.IsActive()) {
        impl_->source_voice->GetState(&initial_state, 0u);
        if (!impl_->sample_clock.Reset(
                impl_->config.sample_rate,
                initial_state.SamplesPlayed,
                pts_us)) {
            return impl_->SetStatus(XAudioStreamStatus::invalid_pcm);
        }
    }

    const auto* source_bytes = reinterpret_cast<const std::uint8_t*>(
        interleaved_samples.data());
    const std::uint32_t block_align =
        impl_->config.channels * sizeof(std::int16_t);
    std::size_t offset = 0u;
    for (std::size_t part = 0u; part < required_slots; ++part) {
        Impl::Slot& slot = impl_->slots[available[part]];
        const std::size_t part_bytes = (std::min)(
            impl_->config.slot_bytes, payload_bytes - offset);
        const std::uint32_t part_frames = static_cast<std::uint32_t>(
            part_bytes / block_align);
        std::memcpy(slot.data, source_bytes + offset, part_bytes);
        slot.payload_bytes = static_cast<std::uint32_t>(part_bytes);
        slot.samples_per_channel = part_frames;
        slot.generation = generation;
        slot.state.store(SlotState::submitted, std::memory_order_release);

        XAUDIO2_BUFFER buffer{};
        buffer.Flags = end_of_stream && part + 1u == required_slots
            ? XAUDIO2_END_OF_STREAM
            : 0u;
        buffer.AudioBytes = slot.payload_bytes;
        buffer.pAudioData = slot.data;
        buffer.pContext = &slot;
        const HRESULT result = impl_->source_voice->SubmitSourceBuffer(&buffer);
        if (FAILED(result)) {
            slot.payload_bytes = 0u;
            slot.samples_per_channel = 0u;
            slot.generation = 0u;
            slot.state.store(SlotState::free, std::memory_order_release);
            return impl_->SetStatus(XAudioStreamStatus::operation_failed, result);
        }

        impl_->queued_bytes += part_bytes;
        impl_->queued_frames += part_frames;
        impl_->submitted_frames += part_frames;
        offset += part_bytes;
    }

    impl_->generation = generation;
    impl_->generation_active = true;
    if (end_of_stream) {
        impl_->end_of_stream_submitted = true;
        impl_->end_media_time_us = end_media_time;
    }
    return impl_->SetStatus(XAudioStreamStatus::ok);
}

XAudioStreamStatus XAudioStream::Start() noexcept {
    if (impl_ == nullptr || !impl_->initialized || impl_->source_voice == nullptr) {
        return XAudioStreamStatus::not_initialized;
    }
    if (!impl_->IsOwnerThread()) {
        return impl_->SetStatus(XAudioStreamStatus::wrong_thread);
    }
    if (impl_->DeviceError() != 0) {
        return impl_->SetStatus(
            XAudioStreamStatus::audio_device_failed,
            static_cast<HRESULT>(impl_->DeviceError()));
    }
    if (impl_->started) {
        return impl_->SetStatus(XAudioStreamStatus::invalid_state);
    }
    if (!impl_->ReadyToStart()) {
        return impl_->SetStatus(XAudioStreamStatus::insufficient_prebuffer);
    }

    const HRESULT result = impl_->source_voice->Start(0u, XAUDIO2_COMMIT_NOW);
    if (FAILED(result)) {
        return impl_->SetStatus(XAudioStreamStatus::operation_failed, result);
    }
    impl_->started = true;
    impl_->paused = false;
    impl_->empty_observed = false;
    return impl_->SetStatus(XAudioStreamStatus::ok);
}

XAudioStreamStatus XAudioStream::Pause() noexcept {
    if (impl_ == nullptr || !impl_->initialized || impl_->source_voice == nullptr) {
        return XAudioStreamStatus::not_initialized;
    }
    if (!impl_->IsOwnerThread()) {
        return impl_->SetStatus(XAudioStreamStatus::wrong_thread);
    }
    if (!impl_->started || impl_->paused) {
        return impl_->SetStatus(XAudioStreamStatus::invalid_state);
    }

    const HRESULT result = impl_->source_voice->Stop(0u, XAUDIO2_COMMIT_NOW);
    if (FAILED(result)) {
        return impl_->SetStatus(XAudioStreamStatus::operation_failed, result);
    }
    impl_->paused = true;
    return impl_->SetStatus(XAudioStreamStatus::ok);
}

XAudioStreamStatus XAudioStream::Resume() noexcept {
    if (impl_ == nullptr || !impl_->initialized || impl_->source_voice == nullptr) {
        return XAudioStreamStatus::not_initialized;
    }
    if (!impl_->IsOwnerThread()) {
        return impl_->SetStatus(XAudioStreamStatus::wrong_thread);
    }
    if (!impl_->started || !impl_->paused) {
        return impl_->SetStatus(XAudioStreamStatus::invalid_state);
    }

    const HRESULT result = impl_->source_voice->Start(0u, XAUDIO2_COMMIT_NOW);
    if (FAILED(result)) {
        return impl_->SetStatus(XAudioStreamStatus::operation_failed, result);
    }
    impl_->paused = false;
    impl_->empty_observed = false;
    return impl_->SetStatus(XAudioStreamStatus::ok);
}

XAudioStreamStatus XAudioStream::StopAndFlush() noexcept {
    if (impl_ == nullptr || !impl_->initialized || impl_->source_voice == nullptr) {
        return XAudioStreamStatus::not_initialized;
    }
    if (!impl_->IsOwnerThread()) {
        return impl_->SetStatus(XAudioStreamStatus::wrong_thread);
    }

    const std::int32_t device_error = impl_->DeviceError();
    HRESULT first_error = impl_->source_voice->Stop(0u, XAUDIO2_COMMIT_NOW);
    const HRESULT flush_result = impl_->source_voice->FlushSourceBuffers();
    if (SUCCEEDED(first_error) && FAILED(flush_result)) {
        first_error = flush_result;
    }

    impl_->DestroySourceVoice();
    impl_->ResetStreamState();
    if (device_error != 0) {
        return impl_->SetStatus(
            XAudioStreamStatus::audio_device_failed,
            static_cast<HRESULT>(device_error));
    }
    if (FAILED(first_error)) {
        return impl_->SetStatus(XAudioStreamStatus::operation_failed, first_error);
    }
    return impl_->CreateSourceVoice();
}

XAudioStreamStatus XAudioStream::SetVolume(const float volume) noexcept {
    if (impl_ == nullptr || !impl_->initialized || impl_->source_voice == nullptr) {
        return XAudioStreamStatus::not_initialized;
    }
    if (!impl_->IsOwnerThread()) {
        return impl_->SetStatus(XAudioStreamStatus::wrong_thread);
    }
    if (!std::isfinite(volume) || volume < 0.0f || volume > 1.0f) {
        return impl_->SetStatus(XAudioStreamStatus::invalid_config);
    }

    const HRESULT result = impl_->source_voice->SetVolume(
        impl_->muted ? 0.0f : volume,
        XAUDIO2_COMMIT_NOW);
    if (FAILED(result)) {
        return impl_->SetStatus(XAudioStreamStatus::operation_failed, result);
    }
    impl_->volume = volume;
    return impl_->SetStatus(XAudioStreamStatus::ok);
}

XAudioStreamStatus XAudioStream::SetMuted(const bool muted) noexcept {
    if (impl_ == nullptr || !impl_->initialized || impl_->source_voice == nullptr) {
        return XAudioStreamStatus::not_initialized;
    }
    if (!impl_->IsOwnerThread()) {
        return impl_->SetStatus(XAudioStreamStatus::wrong_thread);
    }

    const HRESULT result = impl_->source_voice->SetVolume(
        muted ? 0.0f : impl_->volume,
        XAUDIO2_COMMIT_NOW);
    if (FAILED(result)) {
        return impl_->SetStatus(XAudioStreamStatus::operation_failed, result);
    }
    impl_->muted = muted;
    return impl_->SetStatus(XAudioStreamStatus::ok);
}

std::optional<std::int64_t> XAudioStream::MediaTimeUs() noexcept {
    if (impl_ == nullptr || !impl_->initialized || impl_->source_voice == nullptr ||
        !impl_->IsOwnerThread()) {
        return std::nullopt;
    }
    if (impl_->callback_state.StreamEnds() != 0u &&
        impl_->end_media_time_us.has_value()) {
        return impl_->end_media_time_us;
    }

    XAUDIO2_VOICE_STATE state{};
    impl_->source_voice->GetState(&state, 0u);
    return impl_->sample_clock.MediaTimeUs(state.SamplesPlayed);
}

XAudioStreamSnapshot XAudioStream::Snapshot() noexcept {
    if (impl_ == nullptr) {
        return {};
    }
    if (!impl_->IsOwnerThread()) {
        XAudioStreamSnapshot snapshot{};
        snapshot.status = XAudioStreamStatus::wrong_thread;
        snapshot.initialized = impl_->initialized;
        return snapshot;
    }
    return impl_->ReadSnapshot();
}

} // namespace pbvp
