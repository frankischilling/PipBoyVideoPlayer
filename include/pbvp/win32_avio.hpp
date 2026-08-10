#pragma once

#include "pbvp/ffmpeg_runtime.hpp"

#include <Windows.h>

#include <atomic>
#include <cstdint>
#include <string>

namespace pbvp {

struct MediaIoLimits {
    std::uint64_t maximum_file_bytes{32ull * 1024ull * 1024ull * 1024ull};
    std::uint32_t avio_buffer_bytes{32u * 1024u};
};

enum class MediaIoStatus : std::uint32_t {
    ok,
    runtime_unavailable,
    root_not_absolute,
    root_missing,
    root_reparse_point,
    invalid_relative_path,
    file_missing,
    file_not_regular,
    file_reparse_point,
    file_open_failed,
    file_size_failed,
    empty_file,
    file_too_large,
    invalid_buffer_limit,
    event_creation_failed,
    buffer_allocation_failed,
    avio_allocation_failed,
    unexpected_failure,
};

struct MediaIoFailure {
    MediaIoStatus status{MediaIoStatus::ok};
    DWORD windows_error{};
    std::uint64_t file_bytes{};
};

const char* MediaIoStatusName(MediaIoStatus status) noexcept;

class Win32AvioFile final {
public:
    Win32AvioFile() = default;
    ~Win32AvioFile();
    Win32AvioFile(const Win32AvioFile&) = delete;
    Win32AvioFile& operator=(const Win32AvioFile&) = delete;

    bool Open(
        const std::wstring& media_root,
        const std::wstring& relative_name,
        const MediaIoLimits& limits,
        const FfmpegRuntime& runtime,
        MediaIoFailure& failure) noexcept;
    void Cancel() noexcept;
    void Close() noexcept;

    bool IsOpen() const noexcept { return context_ != nullptr; }
    AVIOContext* Context() const noexcept { return context_; }
    std::uint64_t FileBytes() const noexcept { return file_bytes_; }

private:
    static int ReadCallback(void* opaque, std::uint8_t* buffer, int buffer_size) noexcept;
    static std::int64_t SeekCallback(void* opaque, std::int64_t offset, int whence) noexcept;

    int Read(std::uint8_t* buffer, int buffer_size) noexcept;
    std::int64_t Seek(std::int64_t offset, int whence) noexcept;

    const FfmpegApi* api_{};
    HANDLE file_{INVALID_HANDLE_VALUE};
    HANDLE read_event_{};
    HANDLE cancel_event_{};
    AVIOContext* context_{};
    unsigned char* initial_buffer_{};
    std::uint64_t file_bytes_{};
    std::uint64_t position_{};
    std::atomic<bool> cancelled_{false};
};

} // namespace pbvp
