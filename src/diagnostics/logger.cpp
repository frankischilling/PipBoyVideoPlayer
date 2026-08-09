#include "pbvp/log.hpp"

#include <Windows.h>

#include <array>
#include <cstdio>
#include <cstring>

namespace pbvp::log {
namespace {

HANDLE g_file = INVALID_HANDLE_VALUE;
SRWLOCK g_lock = SRWLOCK_INIT;

} // namespace

bool Open(const char* runtime_directory, const char* relative_log_directory) noexcept {
    if (runtime_directory == nullptr || relative_log_directory == nullptr) {
        return false;
    }

    std::array<char, MAX_PATH> path{};
    const int written = std::snprintf(
        path.data(), path.size(), "%s%sPipBoyVideoPlayer.log", runtime_directory, relative_log_directory);
    if (written <= 0 || static_cast<std::size_t>(written) >= path.size()) {
        return false;
    }

    g_file = CreateFileA(
        path.data(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    return g_file != INVALID_HANDLE_VALUE;
}

void Close() noexcept {
    AcquireSRWLockExclusive(&g_lock);
    if (g_file != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(g_file);
        CloseHandle(g_file);
        g_file = INVALID_HANDLE_VALUE;
    }
    ReleaseSRWLockExclusive(&g_lock);
}

void Write(const char* level, const char* format, ...) noexcept {
    std::va_list arguments;
    va_start(arguments, format);
    WriteV(level, format, arguments);
    va_end(arguments);
}

void WriteV(const char* level, const char* format, std::va_list arguments) noexcept {
    if (level == nullptr || format == nullptr) {
        return;
    }

    std::array<char, 2048> message{};
    const int body_length = vsnprintf_s(
        message.data(), message.size(), _TRUNCATE, format, arguments);
    if (body_length < 0 && message.front() == '\0') {
        return;
    }

    SYSTEMTIME time{};
    GetLocalTime(&time);
    std::array<char, 2304> line{};
    const int line_length = std::snprintf(
        line.data(), line.size(), "%02u:%02u:%02u.%03u [%s] %s\r\n",
        time.wHour, time.wMinute, time.wSecond, time.wMilliseconds, level, message.data());
    if (line_length <= 0) {
        return;
    }

    OutputDebugStringA(line.data());
    AcquireSRWLockExclusive(&g_lock);
    if (g_file != INVALID_HANDLE_VALUE) {
        DWORD bytes_written = 0;
        const DWORD bytes_to_write = static_cast<DWORD>(
            (std::min)(static_cast<std::size_t>(line_length), line.size() - 1));
        WriteFile(g_file, line.data(), bytes_to_write, &bytes_written, nullptr);
    }
    ReleaseSRWLockExclusive(&g_lock);
}

} // namespace pbvp::log
