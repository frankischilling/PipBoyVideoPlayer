#include "pbvp/win32_avio.hpp"

extern "C" {
#include <libavformat/avio.h>
#include <libavutil/error.h>
}

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <limits>
#include <new>
#include <vector>

namespace pbvp {
namespace {

bool IsAbsoluteWindowsPath(const std::wstring& path) noexcept {
    return path.size() >= 3u &&
           ((path[0] >= L'A' && path[0] <= L'Z') || (path[0] >= L'a' && path[0] <= L'z')) &&
           path[1] == L':' && (path[2] == L'\\' || path[2] == L'/');
}

bool NormalizeAbsolutePath(const std::wstring& path, std::wstring& output) {
    const DWORD required = GetFullPathNameW(path.c_str(), 0u, nullptr, nullptr);
    if (required == 0u) {
        return false;
    }
    std::vector<wchar_t> buffer(static_cast<std::size_t>(required) + 1u);
    const DWORD written = GetFullPathNameW(
        path.c_str(), static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
    if (written == 0u || written >= buffer.size()) {
        return false;
    }
    output.assign(buffer.data(), written);
    std::replace(output.begin(), output.end(), L'/', L'\\');
    while (output.size() > 3u && output.back() == L'\\') {
        output.pop_back();
    }
    return true;
}

bool IsDirectChildName(const std::wstring& name) noexcept {
    if (name.empty() || name == L"." || name == L"..") {
        return false;
    }
    return std::none_of(name.begin(), name.end(), [](const wchar_t value) {
        return value == L'\\' || value == L'/' || value == L':';
    });
}

} // namespace

const char* MediaIoStatusName(const MediaIoStatus status) noexcept {
    switch (status) {
        case MediaIoStatus::ok: return "ok";
        case MediaIoStatus::runtime_unavailable: return "FFmpeg runtime is unavailable";
        case MediaIoStatus::root_not_absolute: return "media root is not absolute";
        case MediaIoStatus::root_missing: return "media root is missing";
        case MediaIoStatus::root_reparse_point: return "media root is a reparse point";
        case MediaIoStatus::invalid_relative_path: return "media name is not a direct child";
        case MediaIoStatus::file_missing: return "media file is missing";
        case MediaIoStatus::file_not_regular: return "media path is not a regular file";
        case MediaIoStatus::file_reparse_point: return "media file is a reparse point";
        case MediaIoStatus::file_open_failed: return "media file could not be opened";
        case MediaIoStatus::file_size_failed: return "media file size is unavailable";
        case MediaIoStatus::empty_file: return "media file is empty";
        case MediaIoStatus::file_too_large: return "media file exceeds the limit";
        case MediaIoStatus::invalid_buffer_limit: return "AVIO buffer limit is invalid";
        case MediaIoStatus::event_creation_failed: return "I/O event creation failed";
        case MediaIoStatus::buffer_allocation_failed: return "AVIO buffer allocation failed";
        case MediaIoStatus::avio_allocation_failed: return "AVIO context allocation failed";
        case MediaIoStatus::unexpected_failure: return "unexpected media I/O failure";
    }
    return "unknown media I/O failure";
}

Win32AvioFile::~Win32AvioFile() {
    Close();
}

bool Win32AvioFile::Open(
    const std::wstring& media_root,
    const std::wstring& relative_name,
    const MediaIoLimits& limits,
    const FfmpegRuntime& runtime,
    MediaIoFailure& failure) noexcept {
    failure = {};
    Close();
    if (!runtime.IsLoaded()) {
        failure.status = MediaIoStatus::runtime_unavailable;
        return false;
    }
    if (!IsAbsoluteWindowsPath(media_root)) {
        failure.status = MediaIoStatus::root_not_absolute;
        return false;
    }
    if (!IsDirectChildName(relative_name)) {
        failure.status = MediaIoStatus::invalid_relative_path;
        return false;
    }
    if (limits.avio_buffer_bytes < 4096u || limits.avio_buffer_bytes > 1024u * 1024u ||
        limits.avio_buffer_bytes > static_cast<std::uint32_t>((std::numeric_limits<int>::max)()) ||
        limits.maximum_file_bytes == 0u ||
        limits.maximum_file_bytes > static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)())) {
        failure.status = MediaIoStatus::invalid_buffer_limit;
        return false;
    }

    try {
        std::wstring normalized_root;
        if (!NormalizeAbsolutePath(media_root, normalized_root)) {
            failure = {MediaIoStatus::root_missing, GetLastError(), 0u};
            return false;
        }
        const DWORD root_attributes = GetFileAttributesW(normalized_root.c_str());
        if (root_attributes == INVALID_FILE_ATTRIBUTES ||
            (root_attributes & FILE_ATTRIBUTE_DIRECTORY) == 0u) {
            failure = {MediaIoStatus::root_missing, GetLastError(), 0u};
            return false;
        }
        if ((root_attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u) {
            failure.status = MediaIoStatus::root_reparse_point;
            return false;
        }

        std::wstring path = normalized_root;
        path.push_back(L'\\');
        path.append(relative_name);
        const DWORD file_attributes = GetFileAttributesW(path.c_str());
        if (file_attributes == INVALID_FILE_ATTRIBUTES) {
            failure = {MediaIoStatus::file_missing, GetLastError(), 0u};
            return false;
        }
        if ((file_attributes & FILE_ATTRIBUTE_DIRECTORY) != 0u) {
            failure.status = MediaIoStatus::file_not_regular;
            return false;
        }
        if ((file_attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u) {
            failure.status = MediaIoStatus::file_reparse_point;
            return false;
        }

        file_ = CreateFileW(
            path.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED | FILE_FLAG_RANDOM_ACCESS,
            nullptr);
        if (file_ == INVALID_HANDLE_VALUE) {
            failure = {MediaIoStatus::file_open_failed, GetLastError(), 0u};
            return false;
        }
        if (GetFileType(file_) != FILE_TYPE_DISK) {
            failure.status = MediaIoStatus::file_not_regular;
            Close();
            return false;
        }

        LARGE_INTEGER size{};
        if (!GetFileSizeEx(file_, &size) || size.QuadPart < 0) {
            failure = {MediaIoStatus::file_size_failed, GetLastError(), 0u};
            Close();
            return false;
        }
        file_bytes_ = static_cast<std::uint64_t>(size.QuadPart);
        failure.file_bytes = file_bytes_;
        if (file_bytes_ == 0u) {
            failure.status = MediaIoStatus::empty_file;
            Close();
            return false;
        }
        if (file_bytes_ > limits.maximum_file_bytes) {
            failure.status = MediaIoStatus::file_too_large;
            Close();
            return false;
        }

        read_event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        cancel_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (read_event_ == nullptr || cancel_event_ == nullptr) {
            failure = {MediaIoStatus::event_creation_failed, GetLastError(), file_bytes_};
            Close();
            return false;
        }

        api_ = &runtime.Api();
        initial_buffer_ = static_cast<unsigned char*>(api_->av_malloc(limits.avio_buffer_bytes));
        if (initial_buffer_ == nullptr) {
            failure = {MediaIoStatus::buffer_allocation_failed, 0u, file_bytes_};
            Close();
            return false;
        }
        context_ = api_->avio_alloc_context(
            initial_buffer_, static_cast<int>(limits.avio_buffer_bytes), 0, this,
            &ReadCallback, nullptr, &SeekCallback);
        if (context_ == nullptr) {
            failure = {MediaIoStatus::avio_allocation_failed, 0u, file_bytes_};
            Close();
            return false;
        }
        initial_buffer_ = nullptr;
        position_ = 0u;
        cancelled_.store(false, std::memory_order_release);
        return true;
    } catch (const std::bad_alloc&) {
        failure.status = MediaIoStatus::buffer_allocation_failed;
        Close();
        return false;
    } catch (...) {
        failure.status = MediaIoStatus::unexpected_failure;
        Close();
        return false;
    }
}

void Win32AvioFile::Cancel() noexcept {
    cancelled_.store(true, std::memory_order_release);
    if (cancel_event_ != nullptr) {
        SetEvent(cancel_event_);
    }
    if (file_ != INVALID_HANDLE_VALUE) {
        CancelIoEx(file_, nullptr);
    }
}

void Win32AvioFile::Close() noexcept {
    Cancel();
    if (context_ != nullptr && api_ != nullptr) {
        api_->av_free(context_->buffer);
        context_->buffer = nullptr;
        api_->avio_context_free(&context_);
    } else if (initial_buffer_ != nullptr && api_ != nullptr) {
        api_->av_free(initial_buffer_);
    }
    context_ = nullptr;
    initial_buffer_ = nullptr;
    if (file_ != INVALID_HANDLE_VALUE) {
        CloseHandle(file_);
        file_ = INVALID_HANDLE_VALUE;
    }
    if (read_event_ != nullptr) {
        CloseHandle(read_event_);
        read_event_ = nullptr;
    }
    if (cancel_event_ != nullptr) {
        CloseHandle(cancel_event_);
        cancel_event_ = nullptr;
    }
    api_ = nullptr;
    file_bytes_ = 0u;
    position_ = 0u;
}

int Win32AvioFile::ReadCallback(
    void* opaque,
    std::uint8_t* buffer,
    const int buffer_size) noexcept {
    if (opaque == nullptr) {
        return AVERROR(EINVAL);
    }
    return static_cast<Win32AvioFile*>(opaque)->Read(buffer, buffer_size);
}

std::int64_t Win32AvioFile::SeekCallback(
    void* opaque,
    const std::int64_t offset,
    const int whence) noexcept {
    if (opaque == nullptr) {
        return AVERROR(EINVAL);
    }
    return static_cast<Win32AvioFile*>(opaque)->Seek(offset, whence);
}

int Win32AvioFile::Read(std::uint8_t* buffer, const int buffer_size) noexcept {
    if (buffer == nullptr || buffer_size <= 0) {
        return AVERROR(EINVAL);
    }
    if (cancelled_.load(std::memory_order_acquire)) {
        return AVERROR_EXIT;
    }
    if (position_ >= file_bytes_) {
        return AVERROR_EOF;
    }

    const std::uint64_t remaining = file_bytes_ - position_;
    const DWORD requested = static_cast<DWORD>((std::min)(
        remaining, static_cast<std::uint64_t>(buffer_size)));
    OVERLAPPED operation{};
    operation.Offset = static_cast<DWORD>(position_ & 0xFFFFFFFFull);
    operation.OffsetHigh = static_cast<DWORD>(position_ >> 32u);
    operation.hEvent = read_event_;

    DWORD read = 0u;
    if (!ReadFile(file_, buffer, requested, &read, &operation)) {
        const DWORD error = GetLastError();
        if (error != ERROR_IO_PENDING) {
            return error == ERROR_HANDLE_EOF ? AVERROR_EOF : AVERROR(EIO);
        }
        const std::array<HANDLE, 2u> waits{read_event_, cancel_event_};
        const DWORD wait = WaitForMultipleObjects(
            static_cast<DWORD>(waits.size()), waits.data(), FALSE, INFINITE);
        if (wait == WAIT_OBJECT_0 + 1u) {
            CancelIoEx(file_, &operation);
            GetOverlappedResult(file_, &operation, &read, TRUE);
            return AVERROR_EXIT;
        }
        if (wait != WAIT_OBJECT_0 || !GetOverlappedResult(file_, &operation, &read, FALSE)) {
            return cancelled_.load(std::memory_order_acquire) ? AVERROR_EXIT : AVERROR(EIO);
        }
    }
    if (read == 0u) {
        return AVERROR_EOF;
    }
    position_ += read;
    return static_cast<int>(read);
}

std::int64_t Win32AvioFile::Seek(const std::int64_t offset, const int whence) noexcept {
    if (cancelled_.load(std::memory_order_acquire)) {
        return AVERROR_EXIT;
    }
    if ((whence & AVSEEK_SIZE) != 0) {
        return static_cast<std::int64_t>(file_bytes_);
    }

    const int origin = whence & ~AVSEEK_FORCE;
    std::int64_t base = 0;
    if (origin == SEEK_SET) {
        base = 0;
    } else if (origin == SEEK_CUR) {
        base = static_cast<std::int64_t>(position_);
    } else if (origin == SEEK_END) {
        base = static_cast<std::int64_t>(file_bytes_);
    } else {
        return AVERROR(EINVAL);
    }

    if ((offset > 0 && base > (std::numeric_limits<std::int64_t>::max)() - offset) ||
        (offset < 0 && base < (std::numeric_limits<std::int64_t>::min)() - offset)) {
        return AVERROR(EINVAL);
    }
    const std::int64_t target = base + offset;
    if (target < 0 || static_cast<std::uint64_t>(target) > file_bytes_) {
        return AVERROR(EINVAL);
    }
    position_ = static_cast<std::uint64_t>(target);
    return target;
}

} // namespace pbvp
