#pragma once

#include "pbvp/media_decoder.hpp"
#include "pbvp/playback_state.hpp"
#include "pbvp/video_scheduler.hpp"
#include "pbvp/win32_playback_clock.hpp"
#include "pbvp/xaudio_stream.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>

namespace pbvp {

struct PlaybackControllerConfig final {
    MediaDecoderConfig decoder{};
    XAudioStreamConfig audio{};
    std::size_t staged_video_frames{1u};
    std::size_t staged_video_bytes{8u * 1024u * 1024u};
    std::size_t audio_lookahead_chunks{2u};
    std::int64_t presentation_lead_us{16'667};
    std::uint64_t maximum_underruns{3u};
    float volume{1.0f};
    bool muted{};
};

enum class PlaybackTerminalReason : std::uint32_t {
    none,
    completed,
    stopped,
    presentation_hidden,
    lifecycle_transition,
    failed,
    shutdown,
};

enum class PlaybackFailureSite : std::uint32_t {
    none,
    decoder_state_before_drain,
    decoder_state_after_drain,
    video_queue_contract,
    video_staging_capacity,
    audio_queue_contract,
    video_timeline,
};

[[nodiscard]] const char* PlaybackFailureSiteName(PlaybackFailureSite site) noexcept;

struct PlaybackMetrics final {
    std::uint64_t update_calls{};
    std::uint64_t decoded_video_frames{};
    std::uint64_t presented_video_frames{};
    std::uint64_t dropped_video_frames{};
    std::uint64_t stale_video_frames{};
    std::uint64_t submitted_audio_chunks{};
    std::uint64_t submitted_audio_samples{};
    std::uint64_t stale_audio_chunks{};
    std::uint64_t buffering_events{};
    std::uint64_t seek_count{};
    std::uint64_t pause_count{};
    std::uint64_t resume_count{};
    std::uint64_t maximum_update_gap_ms{};
    std::size_t peak_staged_video_bytes{};
    std::size_t peak_decoder_video_bytes{};
    std::size_t peak_decoder_audio_bytes{};
    std::int64_t maximum_video_lateness_us{};
    std::int64_t last_media_time_us{};
    std::int64_t last_presented_video_pts_us{};
    std::int64_t last_presented_video_end_us{};
};

struct PlaybackControllerSnapshot final {
    PlaybackStateSnapshot playback{};
    PlaybackTerminalReason terminal_reason{PlaybackTerminalReason::none};
    PlaybackFailureSite failure_site{PlaybackFailureSite::none};
    DecoderSnapshot decoder{};
    DecoderBufferUsage decoder_buffers{};
    XAudioStreamSnapshot audio{};
    PlaybackMetrics metrics{};
    std::uint64_t generation{1u};
    std::size_t staged_video_frames{};
    std::size_t staged_video_bytes{};
    std::size_t audio_lookahead_chunks{};
    bool has_audio{};
    bool audio_started{};
    bool end_of_stream_submitted{};
    bool frame_ready{};
};

class PlaybackController final {
public:
    explicit PlaybackController(
        const FfmpegRuntime& runtime,
        PlaybackControllerConfig config = {}) noexcept;
    ~PlaybackController();

    PlaybackController(const PlaybackController&) = delete;
    PlaybackController& operator=(const PlaybackController&) = delete;

    [[nodiscard]] bool Open(
        const std::wstring& media_root,
        const std::wstring& relative_name) noexcept;
    [[nodiscard]] bool Update(bool presentation_visible) noexcept;
    [[nodiscard]] bool Pause() noexcept;
    [[nodiscard]] bool Resume() noexcept;
    [[nodiscard]] bool Seek(std::int64_t target_us) noexcept;
    void NotifyRenderFailure() noexcept;
    void Stop(PlaybackTerminalReason reason = PlaybackTerminalReason::stopped) noexcept;
    void Shutdown() noexcept;
    [[nodiscard]] bool AcknowledgeError() noexcept;

    [[nodiscard]] std::optional<DecodedVideoFrame> TakeVideoFrame() noexcept;
    [[nodiscard]] PlaybackControllerSnapshot Snapshot() noexcept;

private:
    friend struct PlaybackControllerTestAccess;

    [[nodiscard]] bool ValidateConfig() const noexcept;
    [[nodiscard]] bool IsOwnerThread() const noexcept;
    [[nodiscard]] bool ConfigureOpenedMedia(const DecoderSnapshot& snapshot) noexcept;
    [[nodiscard]] bool DrainVideo(bool discard_when_full) noexcept;
    [[nodiscard]] bool FeedAudio(const DecoderSnapshot& snapshot) noexcept;
    [[nodiscard]] bool BeginAudioRebuffer() noexcept;
    [[nodiscard]] bool StartBufferedPlayback() noexcept;
    [[nodiscard]] bool SelectVideoForCurrentClock() noexcept;
    [[nodiscard]] std::optional<std::int64_t> MediaTimeUs() noexcept;
    [[nodiscard]] bool CheckForCompletion(const DecoderSnapshot& snapshot) noexcept;
    void RecordBufferUsage() noexcept;
    void Fail(
        PlaybackError error,
        PlaybackFailureSite site = PlaybackFailureSite::none) noexcept;
    void ReleaseSessionResources(bool keep_ready_frame = false) noexcept;
    void ResetSessionData() noexcept;

    const FfmpegRuntime& runtime_;
    PlaybackControllerConfig config_{};
    PlaybackStateMachine state_{};
    VideoScheduler scheduler_;
    std::unique_ptr<MediaDecoder> decoder_{};
    std::unique_ptr<XAudioStream> audio_{};
    Win32PlaybackClock silent_clock_{};
    std::deque<DecodedVideoFrame> staged_video_{};
    std::deque<DecodedAudioChunk> audio_lookahead_{};
    std::optional<DecodedVideoFrame> ready_frame_{};
    PlaybackControllerSnapshot snapshot_{};
    std::uint32_t owner_thread_id_{};
    std::size_t staged_video_bytes_{};
    std::uint64_t generation_{1u};
    std::uint64_t observed_underruns_{};
    std::uint64_t last_update_tick_ms_{};
    bool media_configured_{};
    bool has_audio_{};
    bool audio_started_{};
    bool end_of_stream_submitted_{};
    bool completion_pending_{};
};

} // namespace pbvp
