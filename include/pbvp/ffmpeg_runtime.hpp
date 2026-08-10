#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/display.h>
#include <libavutil/frame.h>
#include <libavutil/log.h>
#include <libavutil/mathematics.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <array>
#include <cstdint>
#include <string>

namespace pbvp {

enum class FfmpegLoadStatus : std::uint32_t {
    ok,
    already_loaded,
    path_not_absolute,
    directory_missing,
    module_missing,
    module_load_failed,
    module_path_mismatch,
    symbol_missing,
    version_mismatch,
    configuration_mismatch,
    allocation_failed,
    unexpected_failure,
};

struct FfmpegLoadFailure {
    FfmpegLoadStatus status{FfmpegLoadStatus::ok};
    const wchar_t* module{};
    const char* symbol{};
    DWORD windows_error{};
};

struct FfmpegVersions {
    std::uint32_t avcodec{};
    std::uint32_t avformat{};
    std::uint32_t avutil{};
    std::uint32_t swresample{};
    std::uint32_t swscale{};
};

struct FfmpegApi {
    using ReadPacket = int (*)(void*, std::uint8_t*, int);
    using WritePacket = int (*)(void*, const std::uint8_t*, int);
    using Seek = std::int64_t (*)(void*, std::int64_t, int);

    decltype(&::av_malloc) av_malloc{};
    decltype(&::av_free) av_free{};
    decltype(&::av_frame_alloc) av_frame_alloc{};
    decltype(&::av_frame_free) av_frame_free{};
    decltype(&::av_frame_unref) av_frame_unref{};
    decltype(&::av_log_set_level) av_log_set_level{};
    decltype(&::av_rescale_q) av_rescale_q{};
    decltype(&::av_rescale_rnd) av_rescale_rnd{};
    decltype(&::av_display_rotation_get) av_display_rotation_get{};
    decltype(&::av_channel_layout_default) av_channel_layout_default{};
    decltype(&::av_channel_layout_copy) av_channel_layout_copy{};
    decltype(&::av_channel_layout_uninit) av_channel_layout_uninit{};
    decltype(&::av_channel_layout_compare) av_channel_layout_compare{};

    decltype(&::avio_alloc_context) avio_alloc_context{};
    decltype(&::avio_context_free) avio_context_free{};
    decltype(&::avio_read) avio_read{};
    decltype(&::avio_seek) avio_seek{};
    decltype(&::avformat_alloc_context) avformat_alloc_context{};
    decltype(&::avformat_open_input) avformat_open_input{};
    decltype(&::avformat_find_stream_info) avformat_find_stream_info{};
    decltype(&::av_find_best_stream) av_find_best_stream{};
    decltype(&::av_read_frame) av_read_frame{};
    decltype(&::avformat_seek_file) avformat_seek_file{};
    decltype(&::avformat_flush) avformat_flush{};
    decltype(&::avformat_close_input) avformat_close_input{};
    decltype(&::avformat_free_context) avformat_free_context{};

    decltype(&::avcodec_find_decoder) avcodec_find_decoder{};
    decltype(&::avcodec_alloc_context3) avcodec_alloc_context3{};
    decltype(&::avcodec_parameters_to_context) avcodec_parameters_to_context{};
    decltype(&::avcodec_open2) avcodec_open2{};
    decltype(&::avcodec_send_packet) avcodec_send_packet{};
    decltype(&::avcodec_receive_frame) avcodec_receive_frame{};
    decltype(&::avcodec_flush_buffers) avcodec_flush_buffers{};
    decltype(&::avcodec_free_context) avcodec_free_context{};
    decltype(&::av_packet_alloc) av_packet_alloc{};
    decltype(&::av_packet_free) av_packet_free{};
    decltype(&::av_packet_unref) av_packet_unref{};
    decltype(&::av_packet_get_side_data) av_packet_get_side_data{};
    decltype(&::av_packet_side_data_get) av_packet_side_data_get{};

    decltype(&::sws_getCachedContext) sws_getCachedContext{};
    decltype(&::sws_scale) sws_scale{};
    decltype(&::sws_freeContext) sws_freeContext{};

    decltype(&::swr_alloc_set_opts2) swr_alloc_set_opts2{};
    decltype(&::swr_init) swr_init{};
    decltype(&::swr_get_delay) swr_get_delay{};
    decltype(&::swr_convert) swr_convert{};
    decltype(&::swr_free) swr_free{};
};

const char* FfmpegLoadStatusName(FfmpegLoadStatus status) noexcept;

class FfmpegRuntime final {
public:
    FfmpegRuntime() = default;
    FfmpegRuntime(const FfmpegRuntime&) = delete;
    FfmpegRuntime& operator=(const FfmpegRuntime&) = delete;

    bool Load(const std::wstring& directory, FfmpegLoadFailure& failure) noexcept;
    void Unload() noexcept;

    bool IsLoaded() const noexcept { return loaded_; }
    FfmpegVersions Versions() const noexcept { return versions_; }
    const FfmpegApi& Api() const noexcept { return api_; }

private:
    using VersionFunction = unsigned int (*)();
    using TextFunction = const char* (*)();

    enum ModuleIndex : std::size_t {
        avutil,
        swresample,
        swscale,
        avcodec,
        avformat,
        module_count,
    };

    std::array<HMODULE, module_count> modules_{};
    FfmpegApi api_{};
    FfmpegVersions versions_{};
    bool loaded_{};
};

} // namespace pbvp
