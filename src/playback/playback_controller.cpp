#include "pbvp/playback_controller.hpp"

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <utility>

namespace pbvp {
namespace {

constexpr std::size_t kMaximumWorkItemsPerUpdate = 128u;
constexpr std::size_t kMaximumStagedFrames = 8u;
constexpr std::size_t kMaximumAudioLookahead = 4u;

bool IsActiveState(const PlaybackState state) noexcept {
    return state == PlaybackState::opening || state == PlaybackState::buffering ||
           state == PlaybackState::playing || state == PlaybackState::paused;
}

} // namespace

const char* PlaybackFailureSiteName(const PlaybackFailureSite site) noexcept {
    switch (site) {
        case PlaybackFailureSite::none: return "none";
        case PlaybackFailureSite::decoder_state_before_drain:
            return "decoder_state_before_drain";
        case PlaybackFailureSite::decoder_state_after_drain:
            return "decoder_state_after_drain";
        case PlaybackFailureSite::video_queue_contract: return "video_queue_contract";
        case PlaybackFailureSite::video_staging_capacity: return "video_staging_capacity";
        case PlaybackFailureSite::audio_queue_contract: return "audio_queue_contract";
        case PlaybackFailureSite::video_timeline: return "video_timeline";
    }
    return "unknown";
}

PlaybackController::PlaybackController(
    const FfmpegRuntime& runtime,
    PlaybackControllerConfig config) noexcept
    : runtime_(runtime),
      config_(std::move(config)),
      scheduler_(config_.presentation_lead_us) {
    if (!ValidateConfig() || !runtime_.IsLoaded()) {
        state_.SetUnavailable();
    }
    snapshot_.playback = state_.Snapshot();
}

PlaybackController::~PlaybackController() {
    Shutdown();
}

bool PlaybackController::Open(
    const std::wstring& media_root,
    const std::wstring& relative_name) noexcept {
    if (!ValidateConfig() || !runtime_.IsLoaded() || media_root.empty() ||
        relative_name.empty() || !state_.BeginOpen()) {
        return false;
    }

    owner_thread_id_ = GetCurrentThreadId();
    ResetSessionData();
    try {
        decoder_ = std::make_unique<MediaDecoder>(runtime_, config_.decoder);
    } catch (...) {
        Fail(PlaybackError::media_open_failed);
        return false;
    }

    MediaDecodeFailure failure{};
    if (!decoder_->Start(media_root, relative_name, failure)) {
        snapshot_.decoder.failure = failure;
        Fail(PlaybackError::media_open_failed);
        return false;
    }
    snapshot_.playback = state_.Snapshot();
    return true;
}

bool PlaybackController::Update(const bool presentation_visible) noexcept {
    ++snapshot_.metrics.update_calls;
    if (!IsOwnerThread()) {
        return false;
    }

    const PlaybackState current_state = state_.Snapshot().state;
    if (!presentation_visible && IsActiveState(current_state)) {
        Stop(PlaybackTerminalReason::presentation_hidden);
        return true;
    }
    if (!IsActiveState(current_state)) {
        snapshot_.playback = state_.Snapshot();
        return current_state == PlaybackState::idle || current_state == PlaybackState::error;
    }
    if (decoder_ == nullptr) {
        Fail(PlaybackError::invalid_state);
        return false;
    }

    DecoderSnapshot decoder_snapshot = decoder_->Snapshot();
    snapshot_.decoder = decoder_snapshot;
    if (decoder_snapshot.state == DecoderState::failed ||
        decoder_snapshot.state == DecoderState::stopped) {
        Fail(
            PlaybackError::decoder_failed,
            PlaybackFailureSite::decoder_state_before_drain);
        return false;
    }
    if (!media_configured_ &&
        (decoder_snapshot.state == DecoderState::decoding ||
         decoder_snapshot.state == DecoderState::end_of_stream)) {
        if (!ConfigureOpenedMedia(decoder_snapshot)) {
            return false;
        }
    }
    if (!media_configured_) {
        RecordBufferUsage();
        snapshot_.playback = state_.Snapshot();
        return true;
    }

    const PlaybackState before_feed = state_.Snapshot().state;
    const bool discard_video = before_feed == PlaybackState::buffering && !audio_started_;
    if (!DrainVideo(discard_video)) {
        return false;
    }
    decoder_snapshot = decoder_->Snapshot();
    snapshot_.decoder = decoder_snapshot;
    if (decoder_snapshot.state == DecoderState::failed ||
        decoder_snapshot.state == DecoderState::stopped) {
        Fail(
            PlaybackError::decoder_failed,
            PlaybackFailureSite::decoder_state_after_drain);
        return false;
    }
    if (!FeedAudio(decoder_snapshot)) {
        return false;
    }
    if (state_.Snapshot().state == PlaybackState::buffering &&
        !StartBufferedPlayback()) {
        if (state_.Snapshot().state == PlaybackState::error) {
            return false;
        }
    }

    const PlaybackState after_buffering = state_.Snapshot().state;
    if ((after_buffering == PlaybackState::playing ||
         after_buffering == PlaybackState::paused) &&
        !SelectVideoForCurrentClock()) {
        return false;
    }

    if (audio_ != nullptr) {
        snapshot_.audio = audio_->Snapshot();
        if (snapshot_.audio.status == XAudioStreamStatus::audio_device_failed) {
            Fail(PlaybackError::audio_device_failed);
            return false;
        }
        if (audio_started_ && snapshot_.audio.underruns > observed_underruns_) {
            observed_underruns_ = snapshot_.audio.underruns;
            if (observed_underruns_ > config_.maximum_underruns) {
                Fail(PlaybackError::audio_stream_failed);
                return false;
            }
            if (state_.Snapshot().state == PlaybackState::playing &&
                state_.BeginRebuffer()) {
                ++snapshot_.metrics.buffering_events;
            }
        }
    }

    RecordBufferUsage();
    decoder_snapshot = decoder_->Snapshot();
    snapshot_.decoder = decoder_snapshot;
    if (!CheckForCompletion(decoder_snapshot)) {
        return false;
    }
    snapshot_.playback = state_.Snapshot();
    snapshot_.generation = generation_;
    snapshot_.staged_video_frames = staged_video_.size();
    snapshot_.staged_video_bytes = staged_video_bytes_;
    snapshot_.audio_lookahead_chunks = audio_lookahead_.size();
    snapshot_.has_audio = has_audio_;
    snapshot_.audio_started = audio_started_;
    snapshot_.end_of_stream_submitted = end_of_stream_submitted_;
    snapshot_.frame_ready = ready_frame_.has_value();
    return true;
}

bool PlaybackController::Pause() noexcept {
    if (!IsOwnerThread()) {
        return false;
    }
    const PlaybackState current = state_.Snapshot().state;
    if (current == PlaybackState::buffering) {
        if (state_.Pause()) {
            ++snapshot_.metrics.pause_count;
            snapshot_.playback = state_.Snapshot();
            return true;
        }
        return false;
    }
    if (current != PlaybackState::playing) {
        return false;
    }

    const bool paused = has_audio_
        ? audio_ != nullptr && audio_->Pause() == XAudioStreamStatus::ok
        : silent_clock_.Pause();
    if (!paused || !state_.Pause()) {
        Fail(has_audio_ ? PlaybackError::audio_stream_failed
                        : PlaybackError::clock_unavailable);
        return false;
    }
    ++snapshot_.metrics.pause_count;
    snapshot_.playback = state_.Snapshot();
    return true;
}

bool PlaybackController::Resume() noexcept {
    if (!IsOwnerThread()) {
        return false;
    }
    const PlaybackState current = state_.Snapshot().state;
    if (current == PlaybackState::buffering) {
        if (state_.Resume()) {
            ++snapshot_.metrics.resume_count;
            snapshot_.playback = state_.Snapshot();
            return true;
        }
        return false;
    }
    if (current != PlaybackState::paused) {
        return false;
    }

    const bool resumed = has_audio_
        ? audio_ != nullptr && audio_->Resume() == XAudioStreamStatus::ok
        : silent_clock_.Resume();
    if (!resumed || !state_.Resume()) {
        Fail(has_audio_ ? PlaybackError::audio_stream_failed
                        : PlaybackError::clock_unavailable);
        return false;
    }
    ++snapshot_.metrics.resume_count;
    snapshot_.playback = state_.Snapshot();
    return true;
}

bool PlaybackController::Seek(std::int64_t target_us) noexcept {
    if (!IsOwnerThread() || decoder_ == nullptr) {
        return false;
    }
    const PlaybackState current = state_.Snapshot().state;
    if (current != PlaybackState::playing && current != PlaybackState::paused &&
        current != PlaybackState::buffering) {
        return false;
    }

    std::uint64_t requested_generation = 0u;
    if (!decoder_->RequestSeek(target_us, requested_generation) ||
        !state_.BeginSeek()) {
        return false;
    }
    if (audio_ != nullptr) {
        const XAudioStreamStatus stopped = audio_->StopAndFlush();
        if (stopped != XAudioStreamStatus::ok) {
            Fail(stopped == XAudioStreamStatus::audio_device_failed
                     ? PlaybackError::audio_device_failed
                     : PlaybackError::audio_stream_failed);
            return false;
        }
    }

    staged_video_.clear();
    staged_video_bytes_ = 0u;
    audio_lookahead_.clear();
    ready_frame_.reset();
    silent_clock_.Clear();
    generation_ = requested_generation;
    observed_underruns_ = 0u;
    audio_started_ = false;
    end_of_stream_submitted_ = false;
    completion_pending_ = false;
    ++snapshot_.metrics.seek_count;
    ++snapshot_.metrics.buffering_events;
    snapshot_.playback = state_.Snapshot();
    snapshot_.generation = generation_;
    return true;
}

void PlaybackController::NotifyRenderFailure() noexcept {
    if (!IsOwnerThread() || !IsActiveState(state_.Snapshot().state)) {
        return;
    }
    Fail(PlaybackError::render_failed);
}

void PlaybackController::Stop(const PlaybackTerminalReason reason) noexcept {
    if (owner_thread_id_ != 0u && !IsOwnerThread()) {
        return;
    }
    const PlaybackState current = state_.Snapshot().state;
    if (current == PlaybackState::idle || current == PlaybackState::unavailable) {
        return;
    }
    if (current == PlaybackState::error) {
        if (!state_.AcknowledgeError()) {
            return;
        }
    } else if (!state_.BeginStop()) {
        return;
    }
    ReleaseSessionResources();
    if (!state_.FinishStop()) {
        state_.Fail(PlaybackError::invalid_state);
        snapshot_.terminal_reason = PlaybackTerminalReason::failed;
    }
    snapshot_.terminal_reason = reason;
    snapshot_.playback = state_.Snapshot();
}

void PlaybackController::Shutdown() noexcept {
    if (owner_thread_id_ != 0u && !IsOwnerThread()) {
        return;
    }
    if (state_.Snapshot().state != PlaybackState::unavailable) {
        Stop(PlaybackTerminalReason::shutdown);
    }
    ReleaseSessionResources();
}

bool PlaybackController::AcknowledgeError() noexcept {
    if (!IsOwnerThread() || !state_.AcknowledgeError()) {
        return false;
    }
    ReleaseSessionResources();
    const bool finished = state_.FinishStop();
    snapshot_.playback = state_.Snapshot();
    return finished;
}

std::optional<DecodedVideoFrame> PlaybackController::TakeVideoFrame() noexcept {
    if (!IsOwnerThread() || !ready_frame_.has_value()) {
        return std::nullopt;
    }
    std::optional<DecodedVideoFrame> result{std::move(ready_frame_)};
    ready_frame_.reset();
    snapshot_.frame_ready = false;
    return result;
}

PlaybackControllerSnapshot PlaybackController::Snapshot() noexcept {
    snapshot_.playback = state_.Snapshot();
    snapshot_.generation = generation_;
    snapshot_.staged_video_frames = staged_video_.size();
    snapshot_.staged_video_bytes = staged_video_bytes_;
    snapshot_.audio_lookahead_chunks = audio_lookahead_.size();
    snapshot_.has_audio = has_audio_;
    snapshot_.audio_started = audio_started_;
    snapshot_.end_of_stream_submitted = end_of_stream_submitted_;
    snapshot_.frame_ready = ready_frame_.has_value();
    if (audio_ != nullptr && IsOwnerThread()) {
        snapshot_.audio = audio_->Snapshot();
    }
    return snapshot_;
}

bool PlaybackController::ValidateConfig() const noexcept {
    return config_.staged_video_frames > 0u &&
           config_.staged_video_frames <= kMaximumStagedFrames &&
           config_.staged_video_bytes >= 4u &&
           config_.staged_video_bytes <= 32u * 1024u * 1024u &&
           config_.audio_lookahead_chunks >= 2u &&
           config_.audio_lookahead_chunks <= kMaximumAudioLookahead &&
           config_.presentation_lead_us >= 0 &&
           config_.presentation_lead_us <= 1'000'000 &&
           std::isfinite(config_.volume) && config_.volume >= 0.0f &&
           config_.volume <= 1.0f;
}

bool PlaybackController::IsOwnerThread() const noexcept {
    return owner_thread_id_ != 0u && owner_thread_id_ == GetCurrentThreadId();
}

bool PlaybackController::ConfigureOpenedMedia(
    const DecoderSnapshot& decoder_snapshot) noexcept {
    has_audio_ = decoder_snapshot.info.has_audio;
    generation_ = decoder_snapshot.generation;
    if (has_audio_) {
        try {
            audio_ = std::make_unique<XAudioStream>();
        } catch (...) {
            Fail(PlaybackError::audio_initialization_failed);
            return false;
        }
        XAudioStreamConfig audio_config = config_.audio;
        audio_config.channels = decoder_snapshot.info.output_audio_channels;
        audio_config.sample_rate = decoder_snapshot.info.output_audio_rate;
        XAudioStreamStatus status = audio_->Initialize(audio_config);
        if (status == XAudioStreamStatus::ok) {
            status = audio_->SetVolume(config_.volume);
        }
        if (status == XAudioStreamStatus::ok) {
            status = audio_->SetMuted(config_.muted);
        }
        if (status != XAudioStreamStatus::ok) {
            Fail(status == XAudioStreamStatus::audio_device_failed
                     ? PlaybackError::audio_device_failed
                     : PlaybackError::audio_initialization_failed);
            return false;
        }
    }
    media_configured_ = true;
    if (!state_.MediaOpened()) {
        Fail(PlaybackError::invalid_state);
        return false;
    }
    ++snapshot_.metrics.buffering_events;
    snapshot_.has_audio = has_audio_;
    return true;
}

bool PlaybackController::DrainVideo(const bool discard_when_full) noexcept {
    if (decoder_ == nullptr) {
        return false;
    }
    for (std::size_t work = 0u; work < kMaximumWorkItemsPerUpdate; ++work) {
        if (!discard_when_full &&
            (staged_video_.size() >= config_.staged_video_frames ||
             staged_video_bytes_ >= config_.staged_video_bytes)) {
            break;
        }
        auto popped = decoder_->TryPopVideo();
        if (popped.status != QueuePopStatus::item) {
            break;
        }
        if (!popped.value.has_value()) {
            Fail(
                PlaybackError::decoder_failed,
                PlaybackFailureSite::video_queue_contract);
            return false;
        }

        DecodedVideoFrame frame = std::move(*popped.value);
        ++snapshot_.metrics.decoded_video_frames;
        if (frame.generation != generation_) {
            ++snapshot_.metrics.stale_video_frames;
            ++snapshot_.metrics.dropped_video_frames;
            continue;
        }
        const std::size_t bytes = frame.bgra.size();
        const bool has_capacity = staged_video_.size() < config_.staged_video_frames &&
            bytes <= config_.staged_video_bytes - staged_video_bytes_;
        if (!has_capacity) {
            if (discard_when_full) {
                ++snapshot_.metrics.dropped_video_frames;
                continue;
            }
            Fail(
                PlaybackError::decoder_failed,
                PlaybackFailureSite::video_staging_capacity);
            return false;
        }
        staged_video_bytes_ += bytes;
        snapshot_.metrics.peak_staged_video_bytes = (std::max)(
            snapshot_.metrics.peak_staged_video_bytes, staged_video_bytes_);
        staged_video_.push_back(std::move(frame));
    }
    return true;
}

bool PlaybackController::FeedAudio(const DecoderSnapshot& decoder_snapshot) noexcept {
    if (!has_audio_) {
        return true;
    }
    if (decoder_ == nullptr || audio_ == nullptr) {
        Fail(PlaybackError::invalid_state);
        return false;
    }

    for (std::size_t work = 0u; work < kMaximumWorkItemsPerUpdate; ++work) {
        while (audio_lookahead_.size() < config_.audio_lookahead_chunks) {
            auto popped = decoder_->TryPopAudio();
            if (popped.status != QueuePopStatus::item) {
                break;
            }
            if (!popped.value.has_value()) {
                Fail(
                    PlaybackError::decoder_failed,
                    PlaybackFailureSite::audio_queue_contract);
                return false;
            }
            if (popped.value->generation != generation_) {
                ++snapshot_.metrics.stale_audio_chunks;
                continue;
            }
            audio_lookahead_.push_back(std::move(*popped.value));
        }

        const bool final_pending =
            decoder_snapshot.state == DecoderState::end_of_stream &&
            decoder_->BufferUsage().audio_items == 0u &&
            audio_lookahead_.size() == 1u;
        if (audio_lookahead_.size() < 2u && !final_pending) {
            break;
        }

        DecodedAudioChunk& chunk = audio_lookahead_.front();
        const XAudioStreamStatus status = audio_->SubmitPcm(
            chunk.samples, chunk.pts_us, chunk.generation, final_pending);
        if (status == XAudioStreamStatus::queue_full) {
            break;
        }
        if (status != XAudioStreamStatus::ok) {
            Fail(status == XAudioStreamStatus::audio_device_failed
                     ? PlaybackError::audio_device_failed
                     : PlaybackError::audio_stream_failed);
            return false;
        }
        ++snapshot_.metrics.submitted_audio_chunks;
        snapshot_.metrics.submitted_audio_samples += chunk.samples_per_channel;
        end_of_stream_submitted_ = end_of_stream_submitted_ || final_pending;
        audio_lookahead_.pop_front();
    }
    return true;
}

bool PlaybackController::StartBufferedPlayback() noexcept {
    if (staged_video_.empty()) {
        return false;
    }
    const bool pause_after_start = state_.Snapshot().pause_after_buffering;
    if (has_audio_) {
        if (audio_ == nullptr) {
            Fail(PlaybackError::invalid_state);
            return false;
        }
        const XAudioStreamSnapshot audio_snapshot = audio_->Snapshot();
        snapshot_.audio = audio_snapshot;
        if (!audio_snapshot.ready_to_start) {
            return false;
        }
        if (!audio_started_) {
            const XAudioStreamStatus status = audio_->Start();
            if (status != XAudioStreamStatus::ok) {
                Fail(status == XAudioStreamStatus::audio_device_failed
                         ? PlaybackError::audio_device_failed
                         : PlaybackError::audio_stream_failed);
                return false;
            }
            audio_started_ = true;
            if (pause_after_start && audio_->Pause() != XAudioStreamStatus::ok) {
                Fail(PlaybackError::audio_stream_failed);
                return false;
            }
        }
    } else if (!silent_clock_.IsActive()) {
        if (!silent_clock_.Start(staged_video_.front().pts_us, pause_after_start)) {
            Fail(PlaybackError::clock_unavailable);
            return false;
        }
    }
    if (!state_.BufferReady()) {
        Fail(PlaybackError::invalid_state);
        return false;
    }
    return true;
}

bool PlaybackController::SelectVideoForCurrentClock() noexcept {
    const std::optional<std::int64_t> media_time = MediaTimeUs();
    if (!media_time.has_value() || *media_time < 0) {
        Fail(PlaybackError::clock_unavailable);
        return false;
    }
    snapshot_.metrics.last_media_time_us = *media_time;

    for (std::size_t work = 0u; work < kMaximumWorkItemsPerUpdate; ++work) {
        if (staged_video_.empty() && !DrainVideo(false)) {
            return false;
        }
        if (staged_video_.empty()) {
            break;
        }

        const DecodedVideoFrame& frame = staged_video_.front();
        const VideoFrameTiming timing{frame.pts_us, frame.duration_us, frame.generation};
        const VideoSelection selection = scheduler_.Select(
            std::span<const VideoFrameTiming>(&timing, 1u), generation_, *media_time);
        if (selection.status != VideoSelectionStatus::ok) {
            Fail(PlaybackError::decoder_failed, PlaybackFailureSite::video_timeline);
            return false;
        }
        if (selection.consume_count == 0u) {
            break;
        }

        if (selection.present_index.has_value()) {
            const DecodedVideoFrame& selected = staged_video_.front();
            const std::size_t selected_bytes = selected.bgra.size();
            const std::int64_t maximum = (std::numeric_limits<std::int64_t>::max)();
            if (selected.duration_us > maximum - selected.pts_us) {
                Fail(PlaybackError::decoder_failed, PlaybackFailureSite::video_timeline);
                return false;
            }
            if (ready_frame_.has_value()) {
                ++snapshot_.metrics.dropped_video_frames;
            }
            snapshot_.metrics.last_presented_video_pts_us = selected.pts_us;
            snapshot_.metrics.last_presented_video_end_us =
                selected.pts_us + selected.duration_us;
            ready_frame_ = std::move(staged_video_.front());
            ++snapshot_.metrics.presented_video_frames;
            snapshot_.metrics.maximum_video_lateness_us = (std::max)(
                snapshot_.metrics.maximum_video_lateness_us,
                selection.presentation_lateness_us);
            staged_video_bytes_ -= selected_bytes;
        } else {
            snapshot_.metrics.dropped_video_frames += selection.dropped_frames;
            snapshot_.metrics.stale_video_frames += selection.stale_frames;
            staged_video_bytes_ -= staged_video_.front().bgra.size();
        }
        staged_video_.pop_front();
    }
    return true;
}

std::optional<std::int64_t> PlaybackController::MediaTimeUs() noexcept {
    if (has_audio_) {
        return audio_ != nullptr ? audio_->MediaTimeUs() : std::nullopt;
    }
    return silent_clock_.MediaTimeUs();
}

bool PlaybackController::CheckForCompletion(
    const DecoderSnapshot& decoder_snapshot) noexcept {
    if (completion_pending_) {
        if (ready_frame_.has_value()) {
            return true;
        }
        if (!state_.BeginStop()) {
            Fail(PlaybackError::invalid_state);
            return false;
        }
        ReleaseSessionResources();
        if (!state_.FinishStop()) {
            state_.Fail(PlaybackError::invalid_state);
            snapshot_.terminal_reason = PlaybackTerminalReason::failed;
            return false;
        }
        snapshot_.terminal_reason = PlaybackTerminalReason::completed;
        completion_pending_ = false;
        return true;
    }
    if (decoder_snapshot.state != DecoderState::end_of_stream ||
        !staged_video_.empty()) {
        return true;
    }

    bool completed = false;
    if (has_audio_) {
        if (audio_ != nullptr) {
            snapshot_.audio = audio_->Snapshot();
            completed = end_of_stream_submitted_ &&
                snapshot_.audio.end_of_stream_reached;
        }
    } else {
        const auto media_time = silent_clock_.MediaTimeUs();
        completed = media_time.has_value() && decoder_snapshot.info.duration_us > 0 &&
            *media_time >= decoder_snapshot.info.duration_us;
    }
    if (!completed) {
        return true;
    }
    if (ready_frame_.has_value()) {
        completion_pending_ = true;
        return true;
    }
    if (!state_.BeginStop()) {
        Fail(PlaybackError::invalid_state);
        return false;
    }
    ReleaseSessionResources();
    if (!state_.FinishStop()) {
        state_.Fail(PlaybackError::invalid_state);
        snapshot_.terminal_reason = PlaybackTerminalReason::failed;
        return false;
    }
    snapshot_.terminal_reason = PlaybackTerminalReason::completed;
    return true;
}

void PlaybackController::RecordBufferUsage() noexcept {
    if (decoder_ == nullptr) {
        return;
    }
    snapshot_.decoder_buffers = decoder_->BufferUsage();
    snapshot_.metrics.peak_decoder_video_bytes = (std::max)(
        snapshot_.metrics.peak_decoder_video_bytes,
        snapshot_.decoder_buffers.video_bytes);
    snapshot_.metrics.peak_decoder_audio_bytes = (std::max)(
        snapshot_.metrics.peak_decoder_audio_bytes,
        snapshot_.decoder_buffers.audio_bytes);
}

void PlaybackController::Fail(
    const PlaybackError error,
    const PlaybackFailureSite site) noexcept {
    state_.Fail(error);
    snapshot_.terminal_reason = PlaybackTerminalReason::failed;
    snapshot_.failure_site = site;
    snapshot_.playback = state_.Snapshot();
    ReleaseSessionResources();
}

void PlaybackController::ReleaseSessionResources(const bool keep_ready_frame) noexcept {
    if (audio_ != nullptr) {
        snapshot_.audio = audio_->Snapshot();
        audio_->StopAndFlush();
    }
    if (decoder_ != nullptr) {
        const DecoderSnapshot before_stop = decoder_->Snapshot();
        decoder_->Stop();
        snapshot_.decoder = before_stop.state == DecoderState::end_of_stream ||
                before_stop.state == DecoderState::failed
            ? before_stop
            : decoder_->Snapshot();
        snapshot_.decoder_buffers = decoder_->BufferUsage();
        decoder_.reset();
    }
    if (audio_ != nullptr) {
        audio_->Shutdown();
        audio_.reset();
    }
    silent_clock_.Clear();
    staged_video_.clear();
    audio_lookahead_.clear();
    staged_video_bytes_ = 0u;
    if (!keep_ready_frame) {
        ready_frame_.reset();
    }
    media_configured_ = false;
    has_audio_ = false;
    audio_started_ = false;
    end_of_stream_submitted_ = false;
    observed_underruns_ = 0u;
}

void PlaybackController::ResetSessionData() noexcept {
    snapshot_ = {};
    snapshot_.playback = state_.Snapshot();
    snapshot_.generation = 1u;
    staged_video_.clear();
    audio_lookahead_.clear();
    ready_frame_.reset();
    staged_video_bytes_ = 0u;
    generation_ = 1u;
    observed_underruns_ = 0u;
    media_configured_ = false;
    has_audio_ = false;
    audio_started_ = false;
    end_of_stream_submitted_ = false;
    completion_pending_ = false;
    silent_clock_.Clear();
}

} // namespace pbvp
