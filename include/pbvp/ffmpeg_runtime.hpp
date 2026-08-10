#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

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
    FfmpegVersions versions_{};
    bool loaded_{};
};

} // namespace pbvp
