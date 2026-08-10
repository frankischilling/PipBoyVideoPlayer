#include "pbvp/media_decoder.hpp"

extern "C" {
#include <libavutil/error.h>
}

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace pbvp {
namespace {

constexpr std::int64_t kNoDiscard = (std::numeric_limits<std::int64_t>::min)();
constexpr AVRational kMicrosecondTimeBase{1, AV_TIME_BASE};

bool FormatNameContains(const char* names, const char* expected) noexcept {
    if (names == nullptr || expected == nullptr) {
        return false;
    }
    const std::size_t expected_length = std::strlen(expected);
    const char* current = names;
    while (*current != '\0') {
        const char* end = std::strchr(current, ',');
        const std::size_t length = end == nullptr
            ? std::strlen(current)
            : static_cast<std::size_t>(end - current);
        if (length == expected_length && std::memcmp(current, expected, length) == 0) {
            return true;
        }
        if (end == nullptr) {
            break;
        }
        current = end + 1;
    }
    return false;
}

bool AddInt64(
    const std::int64_t left,
    const std::int64_t right,
    std::int64_t& output) noexcept {
    output = 0;
    if ((right > 0 && left > (std::numeric_limits<std::int64_t>::max)() - right) ||
        (right < 0 && left < (std::numeric_limits<std::int64_t>::min)() - right)) {
        return false;
    }
    output = left + right;
    return true;
}

bool SubtractInt64(
    const std::int64_t left,
    const std::int64_t right,
    std::int64_t& output) noexcept {
    output = 0;
    if ((right > 0 && left < (std::numeric_limits<std::int64_t>::min)() + right) ||
        (right < 0 && left > (std::numeric_limits<std::int64_t>::max)() + right)) {
        return false;
    }
    output = left - right;
    return true;
}

} // namespace

const char* MediaDecodeStatusName(const MediaDecodeStatus status) noexcept {
    switch (status) {
        case MediaDecodeStatus::ok: return "ok";
        case MediaDecodeStatus::runtime_unavailable: return "FFmpeg runtime is unavailable";
        case MediaDecodeStatus::invalid_configuration: return "decoder configuration is invalid";
        case MediaDecodeStatus::already_started: return "decoder has already started";
        case MediaDecodeStatus::worker_start_failed: return "decoder worker failed to start";
        case MediaDecodeStatus::io_failure: return "media I/O failed";
        case MediaDecodeStatus::format_allocation_failed: return "format context allocation failed";
        case MediaDecodeStatus::demux_open_failed: return "container open failed";
        case MediaDecodeStatus::unsupported_container: return "container is unsupported";
        case MediaDecodeStatus::stream_info_failed: return "stream information is damaged";
        case MediaDecodeStatus::video_stream_missing: return "video stream is missing";
        case MediaDecodeStatus::unsupported_video_codec: return "video codec is unsupported";
        case MediaDecodeStatus::unsupported_audio_codec: return "audio codec is unsupported";
        case MediaDecodeStatus::encrypted_media: return "encrypted media is unsupported";
        case MediaDecodeStatus::unsupported_rotation: return "rotation metadata is unsupported";
        case MediaDecodeStatus::source_limit_exceeded: return "media source exceeds a limit";
        case MediaDecodeStatus::codec_allocation_failed: return "codec allocation failed";
        case MediaDecodeStatus::codec_parameters_failed: return "codec parameters are invalid";
        case MediaDecodeStatus::codec_open_failed: return "codec open failed";
        case MediaDecodeStatus::packet_allocation_failed: return "packet allocation failed";
        case MediaDecodeStatus::frame_allocation_failed: return "frame allocation failed";
        case MediaDecodeStatus::demux_read_failed: return "container read failed";
        case MediaDecodeStatus::damaged_media: return "media data is damaged";
        case MediaDecodeStatus::video_conversion_failed: return "video conversion failed";
        case MediaDecodeStatus::audio_conversion_failed: return "audio conversion failed";
        case MediaDecodeStatus::seek_failed: return "media seek failed";
        case MediaDecodeStatus::queue_failure: return "decoded queue failed";
        case MediaDecodeStatus::cancelled: return "decoding was cancelled";
        case MediaDecodeStatus::allocation_failed: return "decoder allocation failed";
        case MediaDecodeStatus::unexpected_failure: return "unexpected decoder failure";
    }
    return "unknown decoder failure";
}

MediaDecoder::MediaDecoder(
    const FfmpegRuntime& runtime,
    MediaDecoderConfig config)
    : runtime_(runtime),
      config_(config),
      video_queue_(config.video_queue, 1u),
      audio_queue_(config.audio_queue, 1u) {}

MediaDecoder::~MediaDecoder() {
    Stop();
}

bool MediaDecoder::ValidateConfiguration() const noexcept {
    if (!video_queue_.IsValid() || !audio_queue_.IsValid() ||
        config_.payload_limits.maximum_width == 0u ||
        config_.payload_limits.maximum_height == 0u ||
        config_.payload_limits.maximum_video_payload_bytes == 0u ||
        config_.payload_limits.maximum_audio_payload_bytes == 0u ||
        config_.output_audio_channels == 0u || config_.output_audio_channels > 2u ||
        config_.output_audio_rate < 8000u || config_.output_audio_rate > 192000u ||
        config_.maximum_streams == 0u || config_.maximum_streams > 64u ||
        config_.decoder_threads == 0u || config_.decoder_threads > 8u ||
        config_.probe_bytes < 32ll * 1024ll || config_.probe_bytes > 64ll * 1024ll * 1024ll ||
        config_.analyze_duration_us <= 0 || config_.analyze_duration_us > 60ll * 1000ll * 1000ll) {
        return false;
    }
    return config_.video_queue.maximum_payload_bytes >= 4u &&
           config_.audio_queue.maximum_payload_bytes >= 2u;
}

bool MediaDecoder::Start(
    const std::wstring& media_root,
    const std::wstring& relative_name,
    MediaDecodeFailure& failure) noexcept {
    failure = {};
    if (worker_.joinable()) {
        failure.status = MediaDecodeStatus::already_started;
        return false;
    }
    if (!runtime_.IsLoaded()) {
        failure.status = MediaDecodeStatus::runtime_unavailable;
        return false;
    }
    if (!ValidateConfiguration()) {
        failure.status = MediaDecodeStatus::invalid_configuration;
        return false;
    }

    try {
        media_root_ = media_root;
        relative_name_ = relative_name;
        stop_requested_.store(false, std::memory_order_release);
        requested_seek_us_.store(0, std::memory_order_relaxed);
        requested_generation_.store(1u, std::memory_order_release);
        worker_generation_ = 1u;
        {
            std::scoped_lock lock(snapshot_mutex_);
            snapshot_ = {};
            snapshot_.state = DecoderState::opening;
            snapshot_.generation = 1u;
        }
        worker_ = std::thread(&MediaDecoder::WorkerMain, this);
        return true;
    } catch (const std::bad_alloc&) {
        failure.status = MediaDecodeStatus::allocation_failed;
    } catch (...) {
        failure.status = MediaDecodeStatus::worker_start_failed;
    }
    {
        std::scoped_lock lock(snapshot_mutex_);
        snapshot_.state = DecoderState::failed;
        snapshot_.failure = failure;
    }
    return false;
}

bool MediaDecoder::RequestSeek(
    std::int64_t target_us,
    std::uint64_t& generation) noexcept {
    generation = 0u;
    std::int64_t duration_us = 0;
    {
        std::scoped_lock lock(snapshot_mutex_);
        if (snapshot_.state != DecoderState::decoding &&
            snapshot_.state != DecoderState::end_of_stream) {
            return false;
        }
        duration_us = snapshot_.info.duration_us;
    }
    target_us = (std::max)(target_us, std::int64_t{0});
    if (duration_us > 0) {
        target_us = (std::min)(target_us, duration_us);
    }

    std::uint64_t current = requested_generation_.load(std::memory_order_acquire);
    for (;;) {
        if (current == (std::numeric_limits<std::uint64_t>::max)()) {
            return false;
        }
        requested_seek_us_.store(target_us, std::memory_order_relaxed);
        if (requested_generation_.compare_exchange_weak(
                current, current + 1u,
                std::memory_order_release, std::memory_order_acquire)) {
            generation = current + 1u;
            break;
        }
    }

    video_queue_.AdvanceGeneration(generation);
    audio_queue_.AdvanceGeneration(generation);
    command_available_.notify_all();
    return true;
}

void MediaDecoder::Cancel() noexcept {
    stop_requested_.store(true, std::memory_order_release);
    avio_.Cancel();
    video_queue_.Close();
    audio_queue_.Close();
    command_available_.notify_all();
}

void MediaDecoder::Stop() noexcept {
    Cancel();
    if (worker_.joinable()) {
        try {
            worker_.join();
        } catch (...) {
            return;
        }
    }
}

DecoderSnapshot MediaDecoder::Snapshot() const noexcept {
    std::scoped_lock lock(snapshot_mutex_);
    DecoderSnapshot result = snapshot_;
    result.generation = requested_generation_.load(std::memory_order_acquire);
    return result;
}

QueuePopResult<DecodedVideoFrame> MediaDecoder::TryPopVideo() noexcept {
    return video_queue_.TryPop();
}

QueuePopResult<DecodedAudioChunk> MediaDecoder::TryPopAudio() noexcept {
    return audio_queue_.TryPop();
}

int MediaDecoder::InterruptCallback(void* opaque) noexcept {
    if (opaque == nullptr) {
        return 1;
    }
    return static_cast<MediaDecoder*>(opaque)->stop_requested_.load(
        std::memory_order_acquire) ? 1 : 0;
}

void MediaDecoder::WorkerMain() noexcept {
    try {
        if (OpenMedia()) {
            while (!stop_requested_.load(std::memory_order_acquire)) {
                if (!HandlePendingSeek()) {
                    break;
                }
                const WorkerResult result = DecodeUntilControlChange();
                if (result == WorkerResult::seek_pending) {
                    continue;
                }
                if (result == WorkerResult::stopped || result == WorkerResult::failed) {
                    break;
                }
                SetState(DecoderState::end_of_stream);
                if (!WaitAtEndOfStream()) {
                    break;
                }
            }
        }
    } catch (const std::bad_alloc&) {
        Fail(MediaDecodeStatus::allocation_failed);
    } catch (...) {
        Fail(MediaDecodeStatus::unexpected_failure);
    }

    ReleaseMedia();
    video_queue_.Close();
    audio_queue_.Close();
    if (stop_requested_.load(std::memory_order_acquire)) {
        SetState(DecoderState::stopped);
    }
}

bool MediaDecoder::OpenMedia() {
    MediaIoFailure io_failure{};
    if (!avio_.Open(media_root_, relative_name_, config_.io_limits, runtime_, io_failure)) {
        if (!stop_requested_.load(std::memory_order_acquire)) {
            FailIo(io_failure);
        }
        return false;
    }

    const FfmpegApi& api = runtime_.Api();
    format_ = api.avformat_alloc_context();
    if (format_ == nullptr) {
        Fail(MediaDecodeStatus::format_allocation_failed);
        return false;
    }
    format_->pb = avio_.Context();
    format_->flags |= AVFMT_FLAG_CUSTOM_IO | AVFMT_FLAG_GENPTS | AVFMT_FLAG_DISCARD_CORRUPT;
    format_->interrupt_callback = {&InterruptCallback, this};
    format_->probesize = config_.probe_bytes;
    format_->max_analyze_duration = config_.analyze_duration_us;
    format_->max_streams = static_cast<int>(config_.maximum_streams);
    format_->max_index_size = 4u * 1024u * 1024u;

    int result = api.avformat_open_input(&format_, nullptr, nullptr, nullptr);
    if (result < 0) {
        if (!stop_requested_.load(std::memory_order_acquire)) {
            Fail(MediaDecodeStatus::demux_open_failed, result);
        }
        return false;
    }
    format_opened_ = true;
    if (format_->iformat == nullptr ||
        !FormatNameContains(format_->iformat->name, "mov")) {
        Fail(MediaDecodeStatus::unsupported_container);
        return false;
    }

    result = api.avformat_find_stream_info(format_, nullptr);
    if (result < 0) {
        if (!stop_requested_.load(std::memory_order_acquire)) {
            Fail(MediaDecodeStatus::stream_info_failed, result);
        }
        return false;
    }

    video_stream_index_ = api.av_find_best_stream(
        format_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream_index_ < 0) {
        Fail(video_stream_index_ == AVERROR_STREAM_NOT_FOUND
                 ? MediaDecodeStatus::video_stream_missing
                 : MediaDecodeStatus::unsupported_video_codec,
             video_stream_index_);
        return false;
    }
    AVStream* video_stream = format_->streams[video_stream_index_];
    if (video_stream == nullptr || video_stream->codecpar == nullptr ||
        video_stream->codecpar->codec_id != AV_CODEC_ID_H264) {
        Fail(MediaDecodeStatus::unsupported_video_codec);
        return false;
    }
    if (video_stream->codecpar->width <= 0 || video_stream->codecpar->height <= 0) {
        Fail(MediaDecodeStatus::source_limit_exceeded);
        return false;
    }

    VideoLayout video_layout{};
    const auto source_width = static_cast<std::uint32_t>(video_stream->codecpar->width);
    const auto source_height = static_cast<std::uint32_t>(video_stream->codecpar->height);
    if (ComputeBgraLayout(
            source_width, source_height,
            config_.payload_limits, video_layout) != LayoutStatus::ok) {
        Fail(MediaDecodeStatus::source_limit_exceeded);
        return false;
    }
    if (!ReadRotation(*video_stream, rotation_degrees_)) {
        return false;
    }

    audio_stream_index_ = api.av_find_best_stream(
        format_, AVMEDIA_TYPE_AUDIO, -1, video_stream_index_, nullptr, 0);
    if (audio_stream_index_ < 0 && audio_stream_index_ != AVERROR_STREAM_NOT_FOUND) {
        Fail(MediaDecodeStatus::unsupported_audio_codec, audio_stream_index_);
        return false;
    }
    if (audio_stream_index_ >= 0) {
        AVStream* audio_stream = format_->streams[audio_stream_index_];
        if (audio_stream == nullptr || audio_stream->codecpar == nullptr ||
            audio_stream->codecpar->codec_id != AV_CODEC_ID_AAC) {
            Fail(MediaDecodeStatus::unsupported_audio_codec);
            return false;
        }
        if (audio_stream->codecpar->sample_rate <= 0 ||
            audio_stream->codecpar->ch_layout.nb_channels <= 0 ||
            audio_stream->codecpar->ch_layout.nb_channels > 32) {
            Fail(MediaDecodeStatus::source_limit_exceeded);
            return false;
        }
    }

    if (!OpenCodec(video_stream_index_, video_codec_, true)) {
        return false;
    }
    if (audio_stream_index_ >= 0 &&
        !OpenCodec(audio_stream_index_, audio_codec_, false)) {
        return false;
    }

    packet_ = api.av_packet_alloc();
    if (packet_ == nullptr) {
        Fail(MediaDecodeStatus::packet_allocation_failed);
        return false;
    }
    video_frame_ = api.av_frame_alloc();
    if (video_frame_ == nullptr) {
        Fail(MediaDecodeStatus::frame_allocation_failed);
        return false;
    }
    if (audio_codec_ != nullptr) {
        audio_frame_ = api.av_frame_alloc();
        if (audio_frame_ == nullptr) {
            Fail(MediaDecodeStatus::frame_allocation_failed);
            return false;
        }
    }

    if (format_->start_time != AV_NOPTS_VALUE) {
        timeline_origin_us_ = format_->start_time;
    } else {
        bool found_origin = false;
        std::int64_t earliest = 0;
        for (const int stream_index : {video_stream_index_, audio_stream_index_}) {
            if (stream_index < 0) {
                continue;
            }
            const AVStream* stream = format_->streams[stream_index];
            if (stream->start_time == AV_NOPTS_VALUE) {
                continue;
            }
            const std::int64_t start = api.av_rescale_q(
                stream->start_time, stream->time_base, kMicrosecondTimeBase);
            if (!found_origin || start < earliest) {
                earliest = start;
                found_origin = true;
            }
        }
        timeline_origin_us_ = found_origin ? earliest : 0;
    }

    if (video_stream->duration > 0) {
        const std::int64_t stream_start = video_stream->start_time == AV_NOPTS_VALUE
            ? timeline_origin_us_
            : api.av_rescale_q(
                video_stream->start_time,
                video_stream->time_base,
                kMicrosecondTimeBase);
        const std::int64_t stream_duration = api.av_rescale_q(
            video_stream->duration,
            video_stream->time_base,
            kMicrosecondTimeBase);
        std::int64_t absolute_end = 0;
        if (stream_duration > 0 && AddInt64(stream_start, stream_duration, absolute_end) &&
            SubtractInt64(absolute_end, timeline_origin_us_, expected_video_end_us_)) {
            expected_video_end_us_ = (std::max)(expected_video_end_us_, std::int64_t{0});
        }
    } else if (format_->duration != AV_NOPTS_VALUE && format_->duration > 0) {
        expected_video_end_us_ = format_->duration;
    }
    last_video_end_us_ = 0;

    MediaInfo info{};
    info.source_width = source_width;
    info.source_height = source_height;
    info.clockwise_rotation_degrees = rotation_degrees_;
    info.display_width = rotation_degrees_ == 90u || rotation_degrees_ == 270u
        ? source_height : source_width;
    info.display_height = rotation_degrees_ == 90u || rotation_degrees_ == 270u
        ? source_width : source_height;
    info.duration_us = format_->duration == AV_NOPTS_VALUE
        ? 0 : (std::max)(format_->duration, std::int64_t{0});
    info.has_audio = audio_codec_ != nullptr;
    if (audio_codec_ != nullptr) {
        info.source_audio_channels = static_cast<std::uint32_t>(
            audio_codec_->ch_layout.nb_channels);
        info.source_audio_rate = static_cast<std::uint32_t>(audio_codec_->sample_rate);
        info.output_audio_channels = config_.output_audio_channels;
        info.output_audio_rate = config_.output_audio_rate;
    }
    video_discard_before_us_ = 0;
    audio_discard_before_us_ = 0;
    SetInfo(info);
    SetState(DecoderState::decoding);
    return true;
}

bool MediaDecoder::OpenCodec(
    const int stream_index,
    AVCodecContext*& context,
    const bool video) {
    const FfmpegApi& api = runtime_.Api();
    AVCodecParameters* parameters = format_->streams[stream_index]->codecpar;
    const AVCodec* codec = api.avcodec_find_decoder(parameters->codec_id);
    if (codec == nullptr) {
        Fail(video ? MediaDecodeStatus::unsupported_video_codec
                   : MediaDecodeStatus::unsupported_audio_codec);
        return false;
    }
    context = api.avcodec_alloc_context3(codec);
    if (context == nullptr) {
        Fail(MediaDecodeStatus::codec_allocation_failed);
        return false;
    }
    int result = api.avcodec_parameters_to_context(context, parameters);
    if (result < 0) {
        Fail(MediaDecodeStatus::codec_parameters_failed, result);
        return false;
    }
    context->thread_count = static_cast<int>(config_.decoder_threads);
    context->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
    context->err_recognition = AV_EF_CRCCHECK | AV_EF_BITSTREAM | AV_EF_BUFFER;
    result = api.avcodec_open2(context, codec, nullptr);
    if (result < 0) {
        Fail(MediaDecodeStatus::codec_open_failed, result);
        return false;
    }
    return true;
}

bool MediaDecoder::ReadRotation(
    const AVStream& stream,
    std::uint32_t& clockwise_degrees) noexcept {
    clockwise_degrees = 0u;
    const AVCodecParameters* parameters = stream.codecpar;
    const AVPacketSideData* side_data = runtime_.Api().av_packet_side_data_get(
        parameters->coded_side_data,
        parameters->nb_coded_side_data,
        AV_PKT_DATA_DISPLAYMATRIX);
    if (side_data == nullptr) {
        return true;
    }
    if (side_data->data == nullptr || side_data->size < 9u * sizeof(std::int32_t)) {
        Fail(MediaDecodeStatus::damaged_media, AVERROR_INVALIDDATA);
        return false;
    }
    const double counterclockwise = runtime_.Api().av_display_rotation_get(
        reinterpret_cast<const std::int32_t*>(side_data->data));
    if (!std::isfinite(counterclockwise)) {
        Fail(MediaDecodeStatus::unsupported_rotation);
        return false;
    }
    const long rounded = std::lround(-counterclockwise);
    if (std::fabs(-counterclockwise - static_cast<double>(rounded)) > 0.5) {
        Fail(MediaDecodeStatus::unsupported_rotation);
        return false;
    }
    const long normalized = ((rounded % 360l) + 360l) % 360l;
    if (normalized != 0l && normalized != 90l &&
        normalized != 180l && normalized != 270l) {
        Fail(MediaDecodeStatus::unsupported_rotation);
        return false;
    }
    clockwise_degrees = static_cast<std::uint32_t>(normalized);
    return true;
}

MediaDecoder::WorkerResult MediaDecoder::DecodeUntilControlChange() {
    const FfmpegApi& api = runtime_.Api();
    for (;;) {
        if (stop_requested_.load(std::memory_order_acquire)) {
            return WorkerResult::stopped;
        }
        if (requested_generation_.load(std::memory_order_acquire) > worker_generation_) {
            return WorkerResult::seek_pending;
        }

        const int result = api.av_read_frame(format_, packet_);
        if (result == AVERROR_EOF) {
            return DrainEndOfStream();
        }
        if (result < 0) {
            if (stop_requested_.load(std::memory_order_acquire) || result == AVERROR_EXIT) {
                return WorkerResult::stopped;
            }
            Fail(MediaDecodeStatus::demux_read_failed, result);
            return WorkerResult::failed;
        }

        std::size_t encryption_bytes = 0u;
        const bool encrypted = api.av_packet_get_side_data(
            packet_, AV_PKT_DATA_ENCRYPTION_INFO, &encryption_bytes) != nullptr;
        if (encrypted || encryption_bytes != 0u) {
            api.av_packet_unref(packet_);
            Fail(MediaDecodeStatus::encrypted_media);
            return WorkerResult::failed;
        }
        if ((packet_->flags & AV_PKT_FLAG_CORRUPT) != 0) {
            api.av_packet_unref(packet_);
            Fail(MediaDecodeStatus::damaged_media, AVERROR_INVALIDDATA);
            return WorkerResult::failed;
        }

        WorkerResult decode_result = WorkerResult::ok;
        if (packet_->stream_index == video_stream_index_) {
            decode_result = DecodePacket(video_codec_, packet_, true);
        } else if (packet_->stream_index == audio_stream_index_) {
            decode_result = DecodePacket(audio_codec_, packet_, false);
        }
        api.av_packet_unref(packet_);
        if (decode_result != WorkerResult::ok) {
            return decode_result;
        }
    }
}

MediaDecoder::WorkerResult MediaDecoder::DecodePacket(
    AVCodecContext* context,
    AVPacket* packet,
    const bool video) {
    const FfmpegApi& api = runtime_.Api();
    for (unsigned int attempt = 0u; attempt < 4u; ++attempt) {
        const int result = api.avcodec_send_packet(context, packet);
        if (result == AVERROR_EOF && packet == nullptr) {
            return WorkerResult::ok;
        }
        if (result == AVERROR(EAGAIN)) {
            const WorkerResult drain = DrainDecoder(context, video);
            if (drain != WorkerResult::ok) {
                return drain;
            }
            continue;
        }
        if (result < 0) {
            Fail(MediaDecodeStatus::damaged_media, result);
            return WorkerResult::failed;
        }
        return DrainDecoder(context, video);
    }
    Fail(MediaDecodeStatus::damaged_media, AVERROR_INVALIDDATA);
    return WorkerResult::failed;
}

MediaDecoder::WorkerResult MediaDecoder::DrainDecoder(
    AVCodecContext* context,
    const bool video) {
    const FfmpegApi& api = runtime_.Api();
    AVFrame* frame = video ? video_frame_ : audio_frame_;
    for (;;) {
        if (stop_requested_.load(std::memory_order_acquire)) {
            return WorkerResult::stopped;
        }
        if (requested_generation_.load(std::memory_order_acquire) > worker_generation_) {
            return WorkerResult::seek_pending;
        }
        const int result = api.avcodec_receive_frame(context, frame);
        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) {
            return WorkerResult::ok;
        }
        if (result < 0) {
            Fail(MediaDecodeStatus::damaged_media, result);
            return WorkerResult::failed;
        }
        if ((frame->flags & AV_FRAME_FLAG_CORRUPT) != 0 || frame->decode_error_flags != 0) {
            api.av_frame_unref(frame);
            Fail(MediaDecodeStatus::damaged_media, AVERROR_INVALIDDATA);
            return WorkerResult::failed;
        }
        const WorkerResult converted = video
            ? ConvertVideoFrame(*frame)
            : ConvertAudioFrame(*frame);
        api.av_frame_unref(frame);
        if (converted != WorkerResult::ok) {
            return converted;
        }
    }
}

MediaDecoder::WorkerResult MediaDecoder::DrainEndOfStream() {
    WorkerResult result = DecodePacket(video_codec_, nullptr, true);
    if (result != WorkerResult::ok) {
        return result;
    }
    if (audio_codec_ != nullptr) {
        result = DecodePacket(audio_codec_, nullptr, false);
        if (result != WorkerResult::ok) {
            return result;
        }
        result = FlushResampler();
    }
    if (result == WorkerResult::ok && expected_video_end_us_ > 0) {
        constexpr std::int64_t end_tolerance_us = 250000;
        std::int64_t accepted_end = 0;
        if (AddInt64(last_video_end_us_, end_tolerance_us, accepted_end) &&
            accepted_end < expected_video_end_us_) {
            Fail(MediaDecodeStatus::damaged_media, AVERROR_INVALIDDATA);
            return WorkerResult::failed;
        }
    }
    return result;
}

MediaDecoder::WorkerResult MediaDecoder::ConvertVideoFrame(const AVFrame& frame) {
    if (frame.width <= 0 || frame.height <= 0) {
        Fail(MediaDecodeStatus::source_limit_exceeded);
        return WorkerResult::failed;
    }
    const auto source_width = static_cast<std::uint32_t>(frame.width);
    const auto source_height = static_cast<std::uint32_t>(frame.height);
    VideoLayout source_layout{};
    if (ComputeBgraLayout(
            source_width, source_height,
            config_.payload_limits, source_layout) != LayoutStatus::ok ||
        source_layout.row_bytes > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        Fail(MediaDecodeStatus::source_limit_exceeded);
        return WorkerResult::failed;
    }

    SwsContext* updated = runtime_.Api().sws_getCachedContext(
        scaler_, frame.width, frame.height,
        static_cast<AVPixelFormat>(frame.format),
        frame.width, frame.height, AV_PIX_FMT_BGRA,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (updated == nullptr) {
        scaler_ = nullptr;
        Fail(MediaDecodeStatus::video_conversion_failed);
        return WorkerResult::failed;
    }
    scaler_ = updated;

    std::vector<std::uint8_t> source_pixels(source_layout.total_bytes);
    std::uint8_t* destination_data[4]{source_pixels.data(), nullptr, nullptr, nullptr};
    int destination_stride[4]{static_cast<int>(source_layout.row_bytes), 0, 0, 0};
    const int converted_rows = runtime_.Api().sws_scale(
        scaler_, frame.data, frame.linesize, 0, frame.height,
        destination_data, destination_stride);
    if (converted_rows != frame.height) {
        Fail(MediaDecodeStatus::video_conversion_failed, converted_rows);
        return WorkerResult::failed;
    }

    DecodedVideoFrame output{};
    output.width = rotation_degrees_ == 90u || rotation_degrees_ == 270u
        ? source_height : source_width;
    output.height = rotation_degrees_ == 90u || rotation_degrees_ == 270u
        ? source_width : source_height;
    const std::size_t output_stride = static_cast<std::size_t>(output.width) * 4u;
    if (output_stride > static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)())) {
        Fail(MediaDecodeStatus::source_limit_exceeded);
        return WorkerResult::failed;
    }
    output.stride = static_cast<std::uint32_t>(output_stride);

    if (rotation_degrees_ == 0u) {
        output.bgra = std::move(source_pixels);
    } else {
        output.bgra.resize(source_layout.total_bytes);
        for (std::uint32_t y = 0u; y < source_height; ++y) {
            for (std::uint32_t x = 0u; x < source_width; ++x) {
                std::uint32_t destination_x = 0u;
                std::uint32_t destination_y = 0u;
                if (rotation_degrees_ == 90u) {
                    destination_x = source_height - 1u - y;
                    destination_y = x;
                } else if (rotation_degrees_ == 180u) {
                    destination_x = source_width - 1u - x;
                    destination_y = source_height - 1u - y;
                } else {
                    destination_x = y;
                    destination_y = source_width - 1u - x;
                }
                const std::size_t source_offset =
                    static_cast<std::size_t>(y) * source_layout.row_bytes +
                    static_cast<std::size_t>(x) * 4u;
                const std::size_t destination_offset =
                    static_cast<std::size_t>(destination_y) * output_stride +
                    static_cast<std::size_t>(destination_x) * 4u;
                std::memcpy(
                    output.bgra.data() + destination_offset,
                    source_pixels.data() + source_offset, 4u);
            }
        }
    }

    std::int64_t pts_us = frame.best_effort_timestamp == AV_NOPTS_VALUE
        ? (video_next_pts_valid_ ? video_next_pts_us_ : 0)
        : TimestampUs(
            frame.best_effort_timestamp,
            format_->streams[video_stream_index_]->time_base);
    if (pts_us == AV_NOPTS_VALUE) {
        Fail(MediaDecodeStatus::damaged_media, AVERROR_INVALIDDATA);
        return WorkerResult::failed;
    }
    std::int64_t duration_us = 0;
    if (frame.duration > 0) {
        duration_us = runtime_.Api().av_rescale_q(
            frame.duration,
            format_->streams[video_stream_index_]->time_base,
            kMicrosecondTimeBase);
        duration_us = (std::max)(duration_us, std::int64_t{0});
    }
    output.pts_us = pts_us;
    output.duration_us = duration_us;
    output.generation = worker_generation_;
    std::int64_t next_pts = pts_us;
    if (!AddInt64(pts_us, (std::max)(duration_us, std::int64_t{1}), next_pts)) {
        next_pts = (std::numeric_limits<std::int64_t>::max)();
    }
    video_next_pts_us_ = next_pts;
    video_next_pts_valid_ = true;
    last_video_end_us_ = (std::max)(last_video_end_us_, next_pts);

    if (video_discard_before_us_ != kNoDiscard) {
        if ((duration_us > 0 && next_pts <= video_discard_before_us_) ||
            (duration_us == 0 && pts_us < video_discard_before_us_)) {
            return WorkerResult::ok;
        }
        video_discard_before_us_ = kNoDiscard;
    }

    const std::size_t payload_bytes = output.bgra.size();
    const QueuePushStatus pushed = video_queue_.WaitPush(
        std::move(output), payload_bytes, worker_generation_);
    if (pushed == QueuePushStatus::accepted) {
        std::scoped_lock lock(snapshot_mutex_);
        ++snapshot_.video_frames;
        return WorkerResult::ok;
    }
    if (pushed == QueuePushStatus::stale_generation) {
        return WorkerResult::seek_pending;
    }
    if (pushed == QueuePushStatus::closed &&
        stop_requested_.load(std::memory_order_acquire)) {
        return WorkerResult::stopped;
    }
    Fail(pushed == QueuePushStatus::allocation_failed
             ? MediaDecodeStatus::allocation_failed
             : MediaDecodeStatus::queue_failure);
    return WorkerResult::failed;
}

bool MediaDecoder::ConfigureResampler(const AVFrame& frame) {
    if (frame.sample_rate <= 0 || frame.sample_rate > 384000 ||
        frame.ch_layout.nb_channels <= 0 || frame.ch_layout.nb_channels > 32 ||
        frame.format < 0 || frame.nb_samples <= 0) {
        Fail(MediaDecodeStatus::source_limit_exceeded);
        return false;
    }

    AVChannelLayout temporary_input{};
    const AVChannelLayout* input_layout = &frame.ch_layout;
    if (frame.ch_layout.order == AV_CHANNEL_ORDER_UNSPEC) {
        runtime_.Api().av_channel_layout_default(
            &temporary_input, frame.ch_layout.nb_channels);
        input_layout = &temporary_input;
    }
    const AVSampleFormat input_format = static_cast<AVSampleFormat>(frame.format);
    const bool matches = resampler_ != nullptr &&
        resampler_input_rate_ == frame.sample_rate &&
        resampler_input_format_ == input_format &&
        runtime_.Api().av_channel_layout_compare(
            &resampler_input_layout_, input_layout) == 0;
    if (matches) {
        runtime_.Api().av_channel_layout_uninit(&temporary_input);
        return true;
    }

    ResetResampler();
    AVChannelLayout output_layout{};
    runtime_.Api().av_channel_layout_default(
        &output_layout, static_cast<int>(config_.output_audio_channels));
    SwrContext* candidate = nullptr;
    int result = runtime_.Api().swr_alloc_set_opts2(
        &candidate,
        &output_layout,
        AV_SAMPLE_FMT_S16,
        static_cast<int>(config_.output_audio_rate),
        input_layout,
        input_format,
        frame.sample_rate,
        0,
        nullptr);
    runtime_.Api().av_channel_layout_uninit(&output_layout);
    if (result >= 0) {
        result = runtime_.Api().swr_init(candidate);
    }
    if (result >= 0) {
        result = runtime_.Api().av_channel_layout_copy(
            &resampler_input_layout_, input_layout);
    }
    runtime_.Api().av_channel_layout_uninit(&temporary_input);
    if (result < 0) {
        if (candidate != nullptr) {
            runtime_.Api().swr_free(&candidate);
        }
        runtime_.Api().av_channel_layout_uninit(&resampler_input_layout_);
        Fail(MediaDecodeStatus::audio_conversion_failed, result);
        return false;
    }
    resampler_ = candidate;
    resampler_input_format_ = input_format;
    resampler_input_rate_ = frame.sample_rate;
    return true;
}

MediaDecoder::WorkerResult MediaDecoder::ConvertAudioFrame(const AVFrame& frame) {
    if (!ConfigureResampler(frame)) {
        return WorkerResult::failed;
    }
    const AVRational time_base = format_->streams[audio_stream_index_]->time_base;
    const std::int64_t source_timestamp = frame.best_effort_timestamp != AV_NOPTS_VALUE
        ? frame.best_effort_timestamp : frame.pts;
    const std::int64_t frame_pts_us = source_timestamp == AV_NOPTS_VALUE
        ? (audio_next_pts_valid_ ? audio_next_pts_us_ : 0)
        : TimestampUs(source_timestamp, time_base);
    if (frame_pts_us == AV_NOPTS_VALUE) {
        Fail(MediaDecodeStatus::damaged_media, AVERROR_INVALIDDATA);
        return WorkerResult::failed;
    }
    std::int64_t discontinuity_min = (std::numeric_limits<std::int64_t>::min)();
    std::int64_t discontinuity_max = (std::numeric_limits<std::int64_t>::max)();
    if (!AddInt64(audio_next_pts_us_, -250000, discontinuity_min)) {
        discontinuity_min = (std::numeric_limits<std::int64_t>::min)();
    }
    if (!AddInt64(audio_next_pts_us_, 250000, discontinuity_max)) {
        discontinuity_max = (std::numeric_limits<std::int64_t>::max)();
    }
    if (!audio_next_pts_valid_ ||
        frame_pts_us > discontinuity_max || frame_pts_us < discontinuity_min) {
        audio_next_pts_us_ = frame_pts_us;
        audio_next_pts_valid_ = true;
    }

    const std::int64_t delay = runtime_.Api().swr_get_delay(
        resampler_, frame.sample_rate);
    if (delay < 0 || delay > (std::numeric_limits<std::int64_t>::max)() - frame.nb_samples) {
        Fail(MediaDecodeStatus::audio_conversion_failed, AVERROR_INVALIDDATA);
        return WorkerResult::failed;
    }
    const std::int64_t maximum_output = runtime_.Api().av_rescale_rnd(
        delay + frame.nb_samples,
        static_cast<std::int64_t>(config_.output_audio_rate),
        frame.sample_rate,
        AV_ROUND_UP);
    if (maximum_output <= 0 ||
        maximum_output > static_cast<std::int64_t>((std::numeric_limits<int>::max)())) {
        Fail(MediaDecodeStatus::source_limit_exceeded);
        return WorkerResult::failed;
    }
    std::size_t output_bytes = 0u;
    if (ComputeInterleavedAudioBytes(
            static_cast<std::uint64_t>(maximum_output),
            config_.output_audio_channels,
            sizeof(std::int16_t),
            config_.payload_limits,
            output_bytes) != LayoutStatus::ok) {
        Fail(MediaDecodeStatus::source_limit_exceeded);
        return WorkerResult::failed;
    }

    std::vector<std::int16_t> samples(output_bytes / sizeof(std::int16_t));
    std::uint8_t* output_planes[1]{
        reinterpret_cast<std::uint8_t*>(samples.data())};
    const auto* input_planes = const_cast<const std::uint8_t* const*>(
        frame.extended_data);
    const int produced = runtime_.Api().swr_convert(
        resampler_, output_planes, static_cast<int>(maximum_output),
        input_planes, frame.nb_samples);
    if (produced < 0) {
        Fail(MediaDecodeStatus::audio_conversion_failed, produced);
        return WorkerResult::failed;
    }
    if (produced == 0) {
        return WorkerResult::ok;
    }
    samples.resize(
        static_cast<std::size_t>(produced) * config_.output_audio_channels);
    const std::int64_t chunk_pts_us = audio_next_pts_us_;
    const std::int64_t produced_duration = runtime_.Api().av_rescale_q(
        produced,
        AVRational{1, static_cast<int>(config_.output_audio_rate)},
        kMicrosecondTimeBase);
    if (!AddInt64(audio_next_pts_us_, produced_duration, audio_next_pts_us_)) {
        Fail(MediaDecodeStatus::audio_conversion_failed, AVERROR_INVALIDDATA);
        return WorkerResult::failed;
    }
    return PublishAudio(
        std::move(samples), static_cast<std::uint32_t>(produced), chunk_pts_us);
}

MediaDecoder::WorkerResult MediaDecoder::PublishAudio(
    std::vector<std::int16_t> samples,
    std::uint32_t samples_per_channel,
    std::int64_t pts_us) {
    if (samples_per_channel == 0u) {
        return WorkerResult::ok;
    }
    const std::int64_t duration_us = runtime_.Api().av_rescale_q(
        samples_per_channel,
        AVRational{1, static_cast<int>(config_.output_audio_rate)},
        kMicrosecondTimeBase);
    std::int64_t end_us = pts_us;
    if (!AddInt64(pts_us, duration_us, end_us)) {
        Fail(MediaDecodeStatus::audio_conversion_failed, AVERROR_INVALIDDATA);
        return WorkerResult::failed;
    }
    if (audio_discard_before_us_ != kNoDiscard) {
        if (end_us <= audio_discard_before_us_) {
            return WorkerResult::ok;
        }
        if (pts_us < audio_discard_before_us_) {
            const std::int64_t delta_us = audio_discard_before_us_ - pts_us;
            const std::int64_t trim = runtime_.Api().av_rescale_rnd(
                delta_us,
                config_.output_audio_rate,
                AV_TIME_BASE,
                AV_ROUND_UP);
            if (trim >= samples_per_channel) {
                return WorkerResult::ok;
            }
            const std::size_t trim_values =
                static_cast<std::size_t>(trim) * config_.output_audio_channels;
            const std::size_t remaining_values = samples.size() - trim_values;
            std::memmove(
                samples.data(), samples.data() + trim_values,
                remaining_values * sizeof(std::int16_t));
            samples.resize(remaining_values);
            samples_per_channel -= static_cast<std::uint32_t>(trim);
            const std::int64_t trim_duration_us = runtime_.Api().av_rescale_q(
                trim,
                AVRational{1, static_cast<int>(config_.output_audio_rate)},
                kMicrosecondTimeBase);
            if (!AddInt64(pts_us, trim_duration_us, pts_us)) {
                Fail(MediaDecodeStatus::audio_conversion_failed, AVERROR_INVALIDDATA);
                return WorkerResult::failed;
            }
        }
        audio_discard_before_us_ = kNoDiscard;
    }

    DecodedAudioChunk chunk{};
    chunk.samples = std::move(samples);
    chunk.channels = config_.output_audio_channels;
    chunk.sample_rate = config_.output_audio_rate;
    chunk.samples_per_channel = samples_per_channel;
    chunk.pts_us = pts_us;
    chunk.generation = worker_generation_;
    const std::size_t payload_bytes = chunk.samples.size() * sizeof(std::int16_t);
    const QueuePushStatus pushed = audio_queue_.WaitPush(
        std::move(chunk), payload_bytes, worker_generation_);
    if (pushed == QueuePushStatus::accepted) {
        std::scoped_lock lock(snapshot_mutex_);
        ++snapshot_.audio_chunks;
        return WorkerResult::ok;
    }
    if (pushed == QueuePushStatus::stale_generation) {
        return WorkerResult::seek_pending;
    }
    if (pushed == QueuePushStatus::closed &&
        stop_requested_.load(std::memory_order_acquire)) {
        return WorkerResult::stopped;
    }
    Fail(pushed == QueuePushStatus::allocation_failed
             ? MediaDecodeStatus::allocation_failed
             : MediaDecodeStatus::queue_failure);
    return WorkerResult::failed;
}

MediaDecoder::WorkerResult MediaDecoder::FlushResampler() {
    if (resampler_ == nullptr || !audio_next_pts_valid_) {
        return WorkerResult::ok;
    }
    for (unsigned int iteration = 0u; iteration < 32u; ++iteration) {
        const std::int64_t delay = runtime_.Api().swr_get_delay(
            resampler_, resampler_input_rate_);
        if (delay <= 0) {
            return WorkerResult::ok;
        }
        const std::int64_t maximum_output = runtime_.Api().av_rescale_rnd(
            delay,
            config_.output_audio_rate,
            resampler_input_rate_,
            AV_ROUND_UP);
        if (maximum_output <= 0 ||
            maximum_output > static_cast<std::int64_t>((std::numeric_limits<int>::max)())) {
            Fail(MediaDecodeStatus::audio_conversion_failed, AVERROR_INVALIDDATA);
            return WorkerResult::failed;
        }
        std::size_t output_bytes = 0u;
        if (ComputeInterleavedAudioBytes(
                static_cast<std::uint64_t>(maximum_output),
                config_.output_audio_channels,
                sizeof(std::int16_t),
                config_.payload_limits,
                output_bytes) != LayoutStatus::ok) {
            Fail(MediaDecodeStatus::source_limit_exceeded);
            return WorkerResult::failed;
        }
        std::vector<std::int16_t> samples(output_bytes / sizeof(std::int16_t));
        std::uint8_t* output_planes[1]{
            reinterpret_cast<std::uint8_t*>(samples.data())};
        const int produced = runtime_.Api().swr_convert(
            resampler_, output_planes, static_cast<int>(maximum_output),
            nullptr, 0);
        if (produced < 0) {
            Fail(MediaDecodeStatus::audio_conversion_failed, produced);
            return WorkerResult::failed;
        }
        if (produced == 0) {
            return WorkerResult::ok;
        }
        samples.resize(
            static_cast<std::size_t>(produced) * config_.output_audio_channels);
        const std::int64_t pts_us = audio_next_pts_us_;
        const std::int64_t duration_us = runtime_.Api().av_rescale_q(
            produced,
            AVRational{1, static_cast<int>(config_.output_audio_rate)},
            kMicrosecondTimeBase);
        if (!AddInt64(audio_next_pts_us_, duration_us, audio_next_pts_us_)) {
            Fail(MediaDecodeStatus::audio_conversion_failed, AVERROR_INVALIDDATA);
            return WorkerResult::failed;
        }
        const WorkerResult published = PublishAudio(
            std::move(samples), static_cast<std::uint32_t>(produced), pts_us);
        if (published != WorkerResult::ok) {
            return published;
        }
    }
    Fail(MediaDecodeStatus::audio_conversion_failed, AVERROR_INVALIDDATA);
    return WorkerResult::failed;
}

bool MediaDecoder::HandlePendingSeek() {
    const std::uint64_t requested = requested_generation_.load(
        std::memory_order_acquire);
    if (requested <= worker_generation_) {
        return true;
    }
    return PerformSeek(
        requested_seek_us_.load(std::memory_order_relaxed), requested);
}

bool MediaDecoder::PerformSeek(
    const std::int64_t target_us,
    const std::uint64_t generation) {
    std::int64_t absolute_us = 0;
    if (!AddInt64(target_us, timeline_origin_us_, absolute_us)) {
        Fail(MediaDecodeStatus::seek_failed, AVERROR_INVALIDDATA);
        return false;
    }
    const AVStream* stream = format_->streams[video_stream_index_];
    const std::int64_t target_timestamp = runtime_.Api().av_rescale_q(
        absolute_us, kMicrosecondTimeBase, stream->time_base);
    int result = runtime_.Api().avformat_seek_file(
        format_, video_stream_index_,
        (std::numeric_limits<std::int64_t>::min)(),
        target_timestamp, target_timestamp,
        AVSEEK_FLAG_BACKWARD);
    if (result >= 0) {
        result = runtime_.Api().avformat_flush(format_);
    }
    if (result < 0) {
        Fail(MediaDecodeStatus::seek_failed, result);
        return false;
    }
    runtime_.Api().avcodec_flush_buffers(video_codec_);
    if (audio_codec_ != nullptr) {
        runtime_.Api().avcodec_flush_buffers(audio_codec_);
    }
    ResetResampler();
    worker_generation_ = generation;
    video_discard_before_us_ = target_us;
    audio_discard_before_us_ = target_us;
    video_next_pts_us_ = 0;
    audio_next_pts_us_ = 0;
    video_next_pts_valid_ = false;
    audio_next_pts_valid_ = false;
    last_video_end_us_ = target_us;
    SetState(DecoderState::decoding);
    return true;
}

bool MediaDecoder::WaitAtEndOfStream() {
    std::unique_lock lock(command_mutex_);
    command_available_.wait(lock, [this] {
        return stop_requested_.load(std::memory_order_acquire) ||
               requested_generation_.load(std::memory_order_acquire) > worker_generation_;
    });
    return !stop_requested_.load(std::memory_order_acquire);
}

std::int64_t MediaDecoder::TimestampUs(
    const std::int64_t timestamp,
    const AVRational time_base) const noexcept {
    if (timestamp == AV_NOPTS_VALUE || time_base.num <= 0 || time_base.den <= 0) {
        return AV_NOPTS_VALUE;
    }
    const std::int64_t absolute = runtime_.Api().av_rescale_q(
        timestamp, time_base, kMicrosecondTimeBase);
    std::int64_t normalized = 0;
    if (!SubtractInt64(absolute, timeline_origin_us_, normalized)) {
        return AV_NOPTS_VALUE;
    }
    return normalized;
}

void MediaDecoder::ResetResampler() noexcept {
    if (resampler_ != nullptr) {
        runtime_.Api().swr_free(&resampler_);
    }
    runtime_.Api().av_channel_layout_uninit(&resampler_input_layout_);
    resampler_input_format_ = AV_SAMPLE_FMT_NONE;
    resampler_input_rate_ = 0;
}

void MediaDecoder::ReleaseMedia() noexcept {
    const FfmpegApi& api = runtime_.Api();
    if (scaler_ != nullptr) {
        api.sws_freeContext(scaler_);
        scaler_ = nullptr;
    }
    ResetResampler();
    if (video_frame_ != nullptr) {
        api.av_frame_free(&video_frame_);
    }
    if (audio_frame_ != nullptr) {
        api.av_frame_free(&audio_frame_);
    }
    if (packet_ != nullptr) {
        api.av_packet_free(&packet_);
    }
    if (video_codec_ != nullptr) {
        api.avcodec_free_context(&video_codec_);
    }
    if (audio_codec_ != nullptr) {
        api.avcodec_free_context(&audio_codec_);
    }
    if (format_ != nullptr) {
        if (format_opened_) {
            api.avformat_close_input(&format_);
        } else {
            api.avformat_free_context(format_);
            format_ = nullptr;
        }
    }
    format_opened_ = false;
    avio_.Close();
    video_stream_index_ = -1;
    audio_stream_index_ = -1;
}

void MediaDecoder::SetState(const DecoderState state) noexcept {
    std::scoped_lock lock(snapshot_mutex_);
    if (snapshot_.state != DecoderState::failed) {
        snapshot_.state = state;
    }
}

void MediaDecoder::SetInfo(const MediaInfo& info) noexcept {
    std::scoped_lock lock(snapshot_mutex_);
    snapshot_.info = info;
}

void MediaDecoder::Fail(
    const MediaDecodeStatus status,
    const int ffmpeg_error) noexcept {
    std::scoped_lock lock(snapshot_mutex_);
    if (snapshot_.state != DecoderState::failed) {
        snapshot_.state = DecoderState::failed;
        snapshot_.failure.status = status;
        snapshot_.failure.ffmpeg_error = ffmpeg_error;
    }
}

void MediaDecoder::FailIo(const MediaIoFailure& failure) noexcept {
    std::scoped_lock lock(snapshot_mutex_);
    if (snapshot_.state != DecoderState::failed) {
        snapshot_.state = DecoderState::failed;
        snapshot_.failure.status = MediaDecodeStatus::io_failure;
        snapshot_.failure.io = failure;
    }
}

} // namespace pbvp
