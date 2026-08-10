#include "pbvp/ffmpeg_runtime.hpp"
#include "pbvp/win32_avio.hpp"

extern "C" {
#include <libavformat/avio.h>
}

#include <Windows.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool Require(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

bool WriteFixture(const std::wstring& path, const std::vector<std::uint8_t>& bytes) {
    const HANDLE file = CreateFileW(
        path.c_str(), GENERIC_WRITE, 0u, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0u;
    const bool result = WriteFile(
        file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) != FALSE &&
        written == bytes.size();
    CloseHandle(file);
    return result;
}

} // namespace

int wmain(const int argc, wchar_t** argv) {
    if (argc != 2) {
        return 2;
    }

    pbvp::FfmpegRuntime runtime;
    pbvp::FfmpegLoadFailure runtime_failure{};
    if (!Require(runtime.Load(argv[1], runtime_failure), "FFmpeg runtime did not load")) {
        return 1;
    }

    wchar_t temporary_path[MAX_PATH]{};
    if (GetTempPathW(MAX_PATH, temporary_path) == 0u) {
        return 1;
    }
    const std::wstring root = std::wstring(temporary_path) + L"pbvp-avio-" +
                              std::to_wstring(GetCurrentProcessId()) + L"-" +
                              std::to_wstring(GetTickCount64());
    if (!CreateDirectoryW(root.c_str(), nullptr)) {
        return 1;
    }

    const std::wstring media_name = L"unicode-\u89c6\u9891.mp4";
    const std::wstring media_path = root + L"\\" + media_name;
    const std::wstring empty_name = L"empty.mp4";
    const std::wstring empty_path = root + L"\\" + empty_name;
    const std::wstring directory_name = L"not-a-file.mp4";
    const std::wstring directory_path = root + L"\\" + directory_name;
    std::vector<std::uint8_t> bytes(128u * 1024u);
    for (std::size_t index = 0u; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::uint8_t>(index & 0xFFu);
    }
    if (!WriteFixture(media_path, bytes) || !WriteFixture(empty_path, {}) ||
        !CreateDirectoryW(directory_path.c_str(), nullptr)) {
        return 1;
    }

    int result = 0;
    {
        pbvp::Win32AvioFile media;
        pbvp::MediaIoFailure failure{};
        if (!Require(media.Open(root, media_name, {}, runtime, failure), "Unicode media file did not open") ||
            !Require(media.FileBytes() == bytes.size(), "media size mismatch")) {
            result = 1;
        } else {
            const pbvp::FfmpegApi& api = runtime.Api();
            std::array<unsigned char, 64u> output{};
            const int first_read = api.avio_read(media.Context(), output.data(), 32);
            if (!Require(first_read == 32, "initial AVIO read failed")) {
                result = 1;
            }
            for (int index = 0; index < first_read; ++index) {
                if (!Require(output[static_cast<std::size_t>(index)] == bytes[static_cast<std::size_t>(index)],
                             "initial AVIO data mismatch")) {
                    result = 1;
                    break;
                }
            }
            if (!Require(api.avio_seek(media.Context(), 1000, SEEK_SET) == 1000, "AVIO seek failed") ||
                !Require(api.avio_read(media.Context(), output.data(), 16) == 16, "post-seek AVIO read failed") ||
                !Require(output[0] == bytes[1000], "post-seek AVIO data mismatch") ||
                !Require(api.avio_seek(media.Context(), 0, AVSEEK_SIZE) ==
                             static_cast<std::int64_t>(bytes.size()),
                         "AVIO size query failed")) {
                result = 1;
            }
        }
    }

    {
        pbvp::Win32AvioFile media;
        pbvp::MediaIoFailure failure{};
        pbvp::MediaIoLimits small_limit{};
        small_limit.maximum_file_bytes = bytes.size() - 1u;
        pbvp::MediaIoLimits small_buffer{};
        small_buffer.avio_buffer_bytes = 1024u;
        if (!Require(
                !media.Open(root, media_name, small_limit, runtime, failure) &&
                    failure.status == pbvp::MediaIoStatus::file_too_large,
                "oversized media file was accepted") ||
            !Require(
                !media.Open(root, empty_name, {}, runtime, failure) &&
                    failure.status == pbvp::MediaIoStatus::empty_file,
                "empty media file was accepted") ||
            !Require(
                !media.Open(root, L"..\\escape.mp4", {}, runtime, failure) &&
                    failure.status == pbvp::MediaIoStatus::invalid_relative_path,
                "parent path escaped the media root") ||
            !Require(
                !media.Open(L"relative-root", media_name, {}, runtime, failure) &&
                    failure.status == pbvp::MediaIoStatus::root_not_absolute,
                "relative media root was accepted") ||
            !Require(
                !media.Open(root, L"missing.mp4", {}, runtime, failure) &&
                    failure.status == pbvp::MediaIoStatus::file_missing,
                "missing media file was accepted") ||
            !Require(
                !media.Open(root, directory_name, {}, runtime, failure) &&
                    failure.status == pbvp::MediaIoStatus::file_not_regular,
                "media directory was accepted as a file") ||
            !Require(
                !media.Open(root, media_name, small_buffer, runtime, failure) &&
                    failure.status == pbvp::MediaIoStatus::invalid_buffer_limit,
                "undersized AVIO buffer was accepted") ||
            !Require(
                !media.Open(L"\\\\server\\share", media_name, {}, runtime, failure) &&
                    failure.status == pbvp::MediaIoStatus::root_not_absolute,
                "network media root was accepted")) {
            result = 1;
        }
    }

    {
        pbvp::Win32AvioFile media;
        pbvp::MediaIoFailure failure{};
        if (!Require(media.Open(root, media_name, {}, runtime, failure), "cancellation fixture did not open")) {
            result = 1;
        } else {
            media.Cancel();
            std::array<unsigned char, 16u> output{};
            if (!Require(
                    runtime.Api().avio_read(media.Context(), output.data(), static_cast<int>(output.size())) < 0,
                    "cancelled AVIO read was accepted")) {
                result = 1;
            }
        }
    }

    DeleteFileW(media_path.c_str());
    DeleteFileW(empty_path.c_str());
    RemoveDirectoryW(directory_path.c_str());
    RemoveDirectoryW(root.c_str());
    runtime.Unload();
    if (result == 0) {
        std::cout << "Win32 custom AVIO checks passed\n";
    }
    return result;
}
