#include "pbvp/ffmpeg_runtime.hpp"

#include <Windows.h>

#include <cstdint>
#include <iostream>
#include <string>

namespace {

bool Require(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

} // namespace

int wmain(const int argc, wchar_t** argv) {
    if (argc != 2) {
        return 2;
    }

    pbvp::FfmpegRuntime runtime;
    pbvp::FfmpegLoadFailure failure{};
    if (!Require(
            !runtime.Load(L"relative\\bin", failure) &&
                failure.status == pbvp::FfmpegLoadStatus::path_not_absolute,
            "relative FFmpeg path was accepted")) {
        return 1;
    }

    if (!Require(runtime.Load(argv[1], failure), "verified FFmpeg runtime did not load")) {
        std::cerr << pbvp::FfmpegLoadStatusName(failure.status) << '\n';
        return 1;
    }
    const pbvp::FfmpegVersions versions = runtime.Versions();
    if (!Require(runtime.IsLoaded(), "runtime did not report loaded state") ||
        !Require(versions.avcodec == 0x3E1C66u, "avcodec version mismatch") ||
        !Require(versions.avformat == 0x3E0C66u, "avformat version mismatch") ||
        !Require(versions.avutil == 0x3C1A66u, "avutil version mismatch") ||
        !Require(versions.swresample == 0x060366u, "swresample version mismatch") ||
        !Require(versions.swscale == 0x090566u, "swscale version mismatch")) {
        return 1;
    }
    if (!Require(
            !runtime.Load(argv[1], failure) &&
                failure.status == pbvp::FfmpegLoadStatus::already_loaded,
            "second runtime load was accepted")) {
        return 1;
    }

    runtime.Unload();
    if (!Require(!runtime.IsLoaded(), "runtime remained loaded after unload")) {
        return 1;
    }

    wchar_t temporary_path[MAX_PATH]{};
    if (GetTempPathW(MAX_PATH, temporary_path) == 0u) {
        return 1;
    }
    const std::wstring missing_directory =
        std::wstring(temporary_path) + L"pbvp-ffmpeg-empty-" + std::to_wstring(GetCurrentProcessId());
    if (!CreateDirectoryW(missing_directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
        return 1;
    }
    const bool missing_refused =
        !runtime.Load(missing_directory, failure) &&
        failure.status == pbvp::FfmpegLoadStatus::module_missing;
    RemoveDirectoryW(missing_directory.c_str());
    if (!Require(missing_refused, "incomplete private runtime was accepted")) {
        return 1;
    }

    std::cout << "private FFmpeg loader checks passed\n";
    return 0;
}
