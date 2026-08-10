#pragma once

#include <cstdarg>

namespace pbvp::log {

bool Open(const char* runtime_directory, const char* relative_log_directory) noexcept;
void Close() noexcept;
void Write(const char* level, const char* format, ...) noexcept;
void WriteV(const char* level, const char* format, std::va_list arguments) noexcept;

} // namespace pbvp::log

#define PBVP_LOG_INFO(...) ::pbvp::log::Write("INFO", __VA_ARGS__)
#define PBVP_LOG_WARN(...) ::pbvp::log::Write("WARN", __VA_ARGS__)
#define PBVP_LOG_ERROR(...) ::pbvp::log::Write("ERROR", __VA_ARGS__)
