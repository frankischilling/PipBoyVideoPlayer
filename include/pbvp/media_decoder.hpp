#pragma once

#include "pbvp/bounded_queue.hpp"
#include "pbvp/ffmpeg_runtime.hpp"
#include "pbvp/media_limits.hpp"
#include "pbvp/win32_avio.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace pbvp {

enum class MediaDecodeStatus : std::uint32_t {
    ok,
    runtime_unavailable,
    invalid_configuration,
    already_started,
    worker_start_failed,
    io_failure,
    format_allocation_failed,
    demux_open_failed,
    unsupported_container,
    stream_info_failed,
    video_stream_missing,
    unsupported_video_codec,
    unsupported_audio_codec,
    encrypted_media,
    unsupported_rotation,
    source_limit_exceeded,
    codec_allocation_failed,
    codec_parameters_failed,
    codec_open_failed,
    packet_allocation_failed,
    frame_allocation_failed,
    demux_read_failed,
    damaged_media,
    video_conversion_failed,
    audio_conversion_failed,
    seek_failed,
    queue_failure,
    cancelled,
    allocation_failed,
    unexpected_failure,
};

const char* MediaDecodeStatusName(MediaDecodeStatus status) noexcept;

enum class MediaDecodeFailureSite : std::uint32_t {
    none,
    media_open,
    video_pixel_buffer,
    video_rotation_buffer,
    video_queue,
    audio_sample_buffer,
    audio_queue,
    resampler_flush_buffer,
};

const char* MediaDecodeFailureSiteName(MediaDecodeFailureSite site) noexcept;

struct MediaDecodeFailure {
    MediaDecodeStatus status{MediaDecodeStatus::ok};
    int ffmpeg_error{};
    MediaIoFailure io{};
    MediaDecodeFailureSite site{MediaDecodeFailureSite::none};
};

enum class DecoderState : std::uint32_t {
    idle,
    opening,
    decoding,
    end_of_stream,
    failed,
    stopped,
};

struct MediaInfo {
    std::uint32_t source_width{};
    std::uint32_t source_height{};
    std::uint32_t display_width{};
    std::uint32_t display_height{};
    std::uint32_t clockwise_rotation_degrees{};
    std::int64_t duration_us{};
    bool has_audio{};
    std::uint32_t source_audio_channels{};
    std::uint32_t source_audio_rate{};
    std::uint32_t output_audio_channels{};
    std::uint32_t output_audio_rate{};
};

struct DecodedVideoFrame {
    std::vector<std::uint8_t> bgra{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint32_t stride{};
    std::int64_t pts_us{};
    std::int64_t duration_us{};
    std::uint64_t generation{};
};

struct DecodedAudioChunk {
    std::vector<std::int16_t> samples{};
    std::uint32_t channels{};
    std::uint32_t sample_rate{};
    std::uint32_t samples_per_channel{};
    std::int64_t pts_us{};
    std::uint64_t generation{};
};

struct MediaDecoderConfig {
    DecodeLimits payload_limits{};
    MediaIoLimits io_limits{};
    QueueLimits video_queue{3u, 32u * 1024u * 1024u};
    QueueLimits audio_queue{16u, 4u * 1024u * 1024u};
    std::uint32_t output_audio_channels{2u};
    std::uint32_t output_audio_rate{48000u};
    std::uint32_t output_video_edge_limit{512u};
    std::uint32_t maximum_streams{16u};
    std::uint32_t decoder_threads{2u};
    std::int64_t probe_bytes{8ll * 1024ll * 1024ll};
    std::int64_t analyze_duration_us{10ll * 1000ll * 1000ll};
};

struct DecoderSnapshot {
    DecoderState state{DecoderState::idle};
    MediaInfo info{};
    MediaDecodeFailure failure{};
    std::uint64_t generation{1u};
    std::uint64_t video_frames{};
    std::uint64_t audio_chunks{};
};

struct DecoderBufferUsage {
    std::size_t video_items{};
    std::size_t video_bytes{};
    std::size_t audio_items{};
    std::size_t audio_bytes{};
};

class MediaDecoder final {
public:
    explicit MediaDecoder(
        const FfmpegRuntime& runtime,
        MediaDecoderConfig config = {});
    ~MediaDecoder();
    MediaDecoder(const MediaDecoder&) = delete;
    MediaDecoder& operator=(const MediaDecoder&) = delete;

    bool Start(
        const std::wstring& media_root,
        const std::wstring& relative_name,
        MediaDecodeFailure& failure) noexcept;
    bool RequestSeek(std::int64_t target_us, std::uint64_t& generation) noexcept;
    void Cancel() noexcept;
    void Stop() noexcept;

    DecoderSnapshot Snapshot() const noexcept;
    DecoderBufferUsage BufferUsage() const noexcept;
    QueuePopResult<DecodedVideoFrame> TryPopVideo() noexcept;
    QueuePopResult<DecodedAudioChunk> TryPopAudio() noexcept;

private:
    enum class WorkerResult : std::uint32_t {
        ok,
        seek_pending,
        stopped,
        failed,
    };

    static int InterruptCallback(void* opaque) noexcept;

    void WorkerMain() noexcept;
    bool OpenMedia();
    bool OpenCodec(int stream_index, AVCodecContext*& context, bool video);
    WorkerResult DecodeUntilControlChange();
    WorkerResult DecodePacket(AVCodecContext* context, AVPacket* packet, bool video);
    WorkerResult DrainDecoder(AVCodecContext* context, bool video);
    WorkerResult DrainEndOfStream();
    WorkerResult ConvertVideoFrame(const AVFrame& frame);
    WorkerResult ConvertAudioFrame(const AVFrame& frame);
    WorkerResult FlushResampler();
    WorkerResult PublishAudio(
        std::vector<std::int16_t> samples,
        std::uint32_t samples_per_channel,
        std::int64_t pts_us);
    bool ConfigureResampler(const AVFrame& frame);
    bool HandlePendingSeek();
    bool PerformSeek(std::int64_t target_us, std::uint64_t generation);
    bool WaitAtEndOfStream();
    bool ReadRotation(const AVStream& stream, std::uint32_t& clockwise_degrees) noexcept;
    bool ValidateConfiguration() const noexcept;
    std::int64_t TimestampUs(std::int64_t timestamp, AVRational time_base) const noexcept;
    void ResetResampler() noexcept;
    void ReleaseMedia() noexcept;
    void SetState(DecoderState state) noexcept;
    void SetInfo(const MediaInfo& info) noexcept;
    void Fail(
        MediaDecodeStatus status,
        int ffmpeg_error = 0,
        MediaDecodeFailureSite site = MediaDecodeFailureSite::none) noexcept;
    void FailIo(const MediaIoFailure& failure) noexcept;

    const FfmpegRuntime& runtime_;
    MediaDecoderConfig config_{};
    BoundedQueue<DecodedVideoFrame> video_queue_;
    BoundedQueue<DecodedAudioChunk> audio_queue_;

    mutable std::mutex snapshot_mutex_{};
    DecoderSnapshot snapshot_{};
    std::condition_variable command_available_{};
    std::mutex command_mutex_{};
    std::thread worker_{};
    std::wstring media_root_{};
    std::wstring relative_name_{};

    std::atomic<bool> stop_requested_{false};
    std::atomic<std::uint64_t> requested_generation_{1u};
    std::atomic<std::int64_t> requested_seek_us_{0};

    Win32AvioFile avio_{};
    AVFormatContext* format_{};
    AVCodecContext* video_codec_{};
    AVCodecContext* audio_codec_{};
    AVPacket* packet_{};
    AVFrame* video_frame_{};
    AVFrame* audio_frame_{};
    SwsContext* scaler_{};
    SwrContext* resampler_{};
    AVChannelLayout resampler_input_layout_{};
    AVSampleFormat resampler_input_format_{AV_SAMPLE_FMT_NONE};
    int resampler_input_rate_{};
    int video_stream_index_{-1};
    int audio_stream_index_{-1};
    std::uint64_t worker_generation_{1u};
    std::int64_t timeline_origin_us_{};
    std::int64_t video_discard_before_us_{};
    std::int64_t audio_discard_before_us_{};
    std::int64_t expected_video_end_us_{};
    std::int64_t last_video_end_us_{};
    std::int64_t video_next_pts_us_{};
    std::int64_t audio_next_pts_us_{};
    bool video_next_pts_valid_{};
    bool audio_next_pts_valid_{};
    std::uint32_t rotation_degrees_{};
    bool format_opened_{};
    MediaDecodeFailureSite allocation_site_{MediaDecodeFailureSite::none};
};

} // namespace pbvp
