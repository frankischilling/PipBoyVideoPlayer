#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <array>
#include <cstdint>
#include <string>

extern "C" {
struct AVIOContext;
}

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

    void* (*av_malloc)(std::size_t){};
    void (*av_free)(void*){};
    AVIOContext* (*avio_alloc_context)(
        unsigned char*, int, int, void*, ReadPacket, WritePacket, Seek){};
    void (*avio_context_free)(AVIOContext**){};
    int (*avio_read)(AVIOContext*, unsigned char*, int){};
    std::int64_t (*avio_seek)(AVIOContext*, std::int64_t, int){};
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
