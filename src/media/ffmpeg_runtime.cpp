#include "pbvp/ffmpeg_runtime.hpp"

extern "C" {
#include <libavcodec/version.h>
#include <libavformat/version.h>
#include <libavutil/avutil.h>
#include <libavutil/version.h>
#include <libswresample/version.h>
#include <libswscale/version.h>
}

#include <algorithm>
#include <array>
#include <cstring>
#include <cwctype>
#include <new>
#include <string_view>
#include <vector>

namespace pbvp {
namespace {

constexpr std::array<const wchar_t*, 5u> kModuleNames{
    L"avutil-60.dll",
    L"swresample-6.dll",
    L"swscale-9.dll",
    L"avcodec-62.dll",
    L"avformat-62.dll",
};

bool IsAbsoluteWindowsPath(const std::wstring& path) noexcept {
    if (path.size() >= 3u && std::iswalpha(path[0]) != 0 && path[1] == L':' &&
        (path[2] == L'\\' || path[2] == L'/')) {
        return true;
    }
    return path.size() >= 3u && path[0] == L'\\' && path[1] == L'\\' &&
           path[2] != L'\\';
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
    while (output.size() > 3u && (output.back() == L'\\' || output.back() == L'/')) {
        output.pop_back();
    }
    std::replace(output.begin(), output.end(), L'/', L'\\');
    return true;
}

bool PathsEqual(const std::wstring& left, const std::wstring& right) noexcept {
    return _wcsicmp(left.c_str(), right.c_str()) == 0;
}

bool ReadModulePath(const HMODULE module, std::wstring& output) {
    std::vector<wchar_t> buffer(512u);
    for (;;) {
        const DWORD written = GetModuleFileNameW(
            module, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (written == 0u) {
            return false;
        }
        if (written < buffer.size() - 1u) {
            output.assign(buffer.data(), written);
            return true;
        }
        if (buffer.size() >= 32768u) {
            return false;
        }
        buffer.resize((std::min)(buffer.size() * 2u, static_cast<std::size_t>(32768u)));
    }
}

template <typename Function>
bool ResolveSymbol(
    const HMODULE module,
    const wchar_t* module_name,
    const char* symbol_name,
    Function& output,
    FfmpegLoadFailure& failure) noexcept {
    const FARPROC address = GetProcAddress(module, symbol_name);
    if (address == nullptr) {
        failure = {FfmpegLoadStatus::symbol_missing, module_name, symbol_name, GetLastError()};
        return false;
    }
    output = reinterpret_cast<Function>(address);
    return true;
}

bool Contains(const char* text, const char* expected) noexcept {
    return text != nullptr && expected != nullptr && std::strstr(text, expected) != nullptr;
}

} // namespace

const char* FfmpegLoadStatusName(const FfmpegLoadStatus status) noexcept {
    switch (status) {
        case FfmpegLoadStatus::ok: return "ok";
        case FfmpegLoadStatus::already_loaded: return "already loaded";
        case FfmpegLoadStatus::path_not_absolute: return "path is not absolute";
        case FfmpegLoadStatus::directory_missing: return "runtime directory is missing";
        case FfmpegLoadStatus::module_missing: return "runtime module is missing";
        case FfmpegLoadStatus::module_load_failed: return "runtime module failed to load";
        case FfmpegLoadStatus::module_path_mismatch: return "loaded module path does not match";
        case FfmpegLoadStatus::symbol_missing: return "required symbol is missing";
        case FfmpegLoadStatus::version_mismatch: return "runtime version does not match";
        case FfmpegLoadStatus::configuration_mismatch: return "runtime configuration does not match";
        case FfmpegLoadStatus::allocation_failed: return "allocation failed";
        case FfmpegLoadStatus::unexpected_failure: return "unexpected loader failure";
    }
    return "unknown failure";
}

bool FfmpegRuntime::Load(
    const std::wstring& directory,
    FfmpegLoadFailure& failure) noexcept {
    failure = {};
    if (loaded_) {
        failure.status = FfmpegLoadStatus::already_loaded;
        return false;
    }
    if (!IsAbsoluteWindowsPath(directory)) {
        failure.status = FfmpegLoadStatus::path_not_absolute;
        return false;
    }

    try {
        std::wstring normalized_directory;
        if (!NormalizeAbsolutePath(directory, normalized_directory)) {
            failure = {FfmpegLoadStatus::directory_missing, nullptr, nullptr, GetLastError()};
            return false;
        }
        const DWORD attributes = GetFileAttributesW(normalized_directory.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES ||
            (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0u) {
            failure = {FfmpegLoadStatus::directory_missing, nullptr, nullptr, GetLastError()};
            return false;
        }

        for (std::size_t index = 0u; index < modules_.size(); ++index) {
            const wchar_t* module_name = kModuleNames[index];
            std::wstring expected_path = normalized_directory;
            expected_path.push_back(L'\\');
            expected_path.append(module_name);
            const DWORD file_attributes = GetFileAttributesW(expected_path.c_str());
            if (file_attributes == INVALID_FILE_ATTRIBUTES ||
                (file_attributes & FILE_ATTRIBUTE_DIRECTORY) != 0u) {
                failure = {
                    FfmpegLoadStatus::module_missing, module_name, nullptr, GetLastError()};
                Unload();
                return false;
            }

            modules_[index] = LoadLibraryExW(
                expected_path.c_str(), nullptr,
                LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (modules_[index] == nullptr) {
                failure = {
                    FfmpegLoadStatus::module_load_failed, module_name, nullptr, GetLastError()};
                Unload();
                return false;
            }

            std::wstring loaded_path;
            std::wstring normalized_loaded_path;
            std::wstring normalized_expected_path;
            if (!ReadModulePath(modules_[index], loaded_path) ||
                !NormalizeAbsolutePath(loaded_path, normalized_loaded_path) ||
                !NormalizeAbsolutePath(expected_path, normalized_expected_path) ||
                !PathsEqual(normalized_loaded_path, normalized_expected_path)) {
                failure = {
                    FfmpegLoadStatus::module_path_mismatch, module_name, nullptr, GetLastError()};
                Unload();
                return false;
            }
        }

        VersionFunction avutil_version = nullptr;
        VersionFunction avcodec_version = nullptr;
        VersionFunction avformat_version = nullptr;
        VersionFunction swresample_version = nullptr;
        VersionFunction swscale_version = nullptr;
        TextFunction version_info = nullptr;
        TextFunction configuration = nullptr;
        if (!ResolveSymbol(modules_[avutil], kModuleNames[avutil], "av_malloc", api_.av_malloc, failure) ||
            !ResolveSymbol(modules_[avutil], kModuleNames[avutil], "av_free", api_.av_free, failure) ||
            !ResolveSymbol(modules_[avutil], kModuleNames[avutil], "av_frame_alloc", api_.av_frame_alloc, failure) ||
            !ResolveSymbol(modules_[avutil], kModuleNames[avutil], "av_frame_free", api_.av_frame_free, failure) ||
            !ResolveSymbol(modules_[avutil], kModuleNames[avutil], "av_frame_unref", api_.av_frame_unref, failure) ||
            !ResolveSymbol(modules_[avutil], kModuleNames[avutil], "av_log_set_level", api_.av_log_set_level, failure) ||
            !ResolveSymbol(modules_[avutil], kModuleNames[avutil], "av_rescale_q", api_.av_rescale_q, failure) ||
            !ResolveSymbol(modules_[avutil], kModuleNames[avutil], "av_rescale_rnd", api_.av_rescale_rnd, failure) ||
            !ResolveSymbol(modules_[avutil], kModuleNames[avutil], "av_display_rotation_get", api_.av_display_rotation_get, failure) ||
            !ResolveSymbol(modules_[avutil], kModuleNames[avutil], "av_dict_get", api_.av_dict_get, failure) ||
            !ResolveSymbol(modules_[avutil], kModuleNames[avutil], "av_channel_layout_default", api_.av_channel_layout_default, failure) ||
            !ResolveSymbol(modules_[avutil], kModuleNames[avutil], "av_channel_layout_copy", api_.av_channel_layout_copy, failure) ||
            !ResolveSymbol(modules_[avutil], kModuleNames[avutil], "av_channel_layout_uninit", api_.av_channel_layout_uninit, failure) ||
            !ResolveSymbol(modules_[avutil], kModuleNames[avutil], "av_channel_layout_compare", api_.av_channel_layout_compare, failure) ||
            !ResolveSymbol(modules_[avformat], kModuleNames[avformat], "avio_alloc_context", api_.avio_alloc_context, failure) ||
            !ResolveSymbol(modules_[avformat], kModuleNames[avformat], "avio_context_free", api_.avio_context_free, failure) ||
            !ResolveSymbol(modules_[avformat], kModuleNames[avformat], "avio_read", api_.avio_read, failure) ||
            !ResolveSymbol(modules_[avformat], kModuleNames[avformat], "avio_seek", api_.avio_seek, failure) ||
            !ResolveSymbol(modules_[avformat], kModuleNames[avformat], "avformat_alloc_context", api_.avformat_alloc_context, failure) ||
            !ResolveSymbol(modules_[avformat], kModuleNames[avformat], "avformat_open_input", api_.avformat_open_input, failure) ||
            !ResolveSymbol(modules_[avformat], kModuleNames[avformat], "avformat_find_stream_info", api_.avformat_find_stream_info, failure) ||
            !ResolveSymbol(modules_[avformat], kModuleNames[avformat], "av_find_best_stream", api_.av_find_best_stream, failure) ||
            !ResolveSymbol(modules_[avformat], kModuleNames[avformat], "av_read_frame", api_.av_read_frame, failure) ||
            !ResolveSymbol(modules_[avformat], kModuleNames[avformat], "avformat_seek_file", api_.avformat_seek_file, failure) ||
            !ResolveSymbol(modules_[avformat], kModuleNames[avformat], "avformat_flush", api_.avformat_flush, failure) ||
            !ResolveSymbol(modules_[avformat], kModuleNames[avformat], "avformat_close_input", api_.avformat_close_input, failure) ||
            !ResolveSymbol(modules_[avformat], kModuleNames[avformat], "avformat_free_context", api_.avformat_free_context, failure) ||
            !ResolveSymbol(modules_[avcodec], kModuleNames[avcodec], "avcodec_find_decoder", api_.avcodec_find_decoder, failure) ||
            !ResolveSymbol(modules_[avcodec], kModuleNames[avcodec], "avcodec_alloc_context3", api_.avcodec_alloc_context3, failure) ||
            !ResolveSymbol(modules_[avcodec], kModuleNames[avcodec], "avcodec_parameters_to_context", api_.avcodec_parameters_to_context, failure) ||
            !ResolveSymbol(modules_[avcodec], kModuleNames[avcodec], "avcodec_open2", api_.avcodec_open2, failure) ||
            !ResolveSymbol(modules_[avcodec], kModuleNames[avcodec], "avcodec_send_packet", api_.avcodec_send_packet, failure) ||
            !ResolveSymbol(modules_[avcodec], kModuleNames[avcodec], "avcodec_receive_frame", api_.avcodec_receive_frame, failure) ||
            !ResolveSymbol(modules_[avcodec], kModuleNames[avcodec], "avcodec_flush_buffers", api_.avcodec_flush_buffers, failure) ||
            !ResolveSymbol(modules_[avcodec], kModuleNames[avcodec], "avcodec_free_context", api_.avcodec_free_context, failure) ||
            !ResolveSymbol(modules_[avcodec], kModuleNames[avcodec], "av_packet_alloc", api_.av_packet_alloc, failure) ||
            !ResolveSymbol(modules_[avcodec], kModuleNames[avcodec], "av_packet_free", api_.av_packet_free, failure) ||
            !ResolveSymbol(modules_[avcodec], kModuleNames[avcodec], "av_packet_unref", api_.av_packet_unref, failure) ||
            !ResolveSymbol(modules_[avcodec], kModuleNames[avcodec], "av_packet_get_side_data", api_.av_packet_get_side_data, failure) ||
            !ResolveSymbol(modules_[avcodec], kModuleNames[avcodec], "av_packet_side_data_get", api_.av_packet_side_data_get, failure) ||
            !ResolveSymbol(modules_[swscale], kModuleNames[swscale], "sws_getCachedContext", api_.sws_getCachedContext, failure) ||
            !ResolveSymbol(modules_[swscale], kModuleNames[swscale], "sws_scale", api_.sws_scale, failure) ||
            !ResolveSymbol(modules_[swscale], kModuleNames[swscale], "sws_freeContext", api_.sws_freeContext, failure) ||
            !ResolveSymbol(modules_[swresample], kModuleNames[swresample], "swr_alloc_set_opts2", api_.swr_alloc_set_opts2, failure) ||
            !ResolveSymbol(modules_[swresample], kModuleNames[swresample], "swr_init", api_.swr_init, failure) ||
            !ResolveSymbol(modules_[swresample], kModuleNames[swresample], "swr_get_delay", api_.swr_get_delay, failure) ||
            !ResolveSymbol(modules_[swresample], kModuleNames[swresample], "swr_convert", api_.swr_convert, failure) ||
            !ResolveSymbol(modules_[swresample], kModuleNames[swresample], "swr_free", api_.swr_free, failure) ||
            !ResolveSymbol(modules_[avutil], kModuleNames[avutil], "avutil_version", avutil_version, failure) ||
            !ResolveSymbol(modules_[avutil], kModuleNames[avutil], "av_version_info", version_info, failure) ||
            !ResolveSymbol(modules_[avutil], kModuleNames[avutil], "avutil_configuration", configuration, failure) ||
            !ResolveSymbol(modules_[avcodec], kModuleNames[avcodec], "avcodec_version", avcodec_version, failure) ||
            !ResolveSymbol(modules_[avformat], kModuleNames[avformat], "avformat_version", avformat_version, failure) ||
            !ResolveSymbol(modules_[swresample], kModuleNames[swresample], "swresample_version", swresample_version, failure) ||
            !ResolveSymbol(modules_[swscale], kModuleNames[swscale], "swscale_version", swscale_version, failure)) {
            Unload();
            return false;
        }

        versions_ = {
            avcodec_version(),
            avformat_version(),
            avutil_version(),
            swresample_version(),
            swscale_version(),
        };
        if (versions_.avcodec != LIBAVCODEC_VERSION_INT ||
            versions_.avformat != LIBAVFORMAT_VERSION_INT ||
            versions_.avutil != LIBAVUTIL_VERSION_INT ||
            versions_.swresample != LIBSWRESAMPLE_VERSION_INT ||
            versions_.swscale != LIBSWSCALE_VERSION_INT ||
            version_info() == nullptr || std::strcmp(version_info(), "8.1.2") != 0) {
            failure.status = FfmpegLoadStatus::version_mismatch;
            Unload();
            return false;
        }

        const char* config = configuration();
        constexpr std::array<const char*, 7u> required_configuration{
            "--disable-network",
            "--disable-everything",
            "--enable-avcodec",
            "--enable-avformat",
            "--enable-avutil",
            "--enable-swscale",
            "--enable-swresample",
        };
        const bool missing_required = std::any_of(
            required_configuration.begin(), required_configuration.end(),
            [config](const char* required) { return !Contains(config, required); });
        const bool prohibited = Contains(config, "--enable-gpl") ||
                                Contains(config, "--enable-nonfree") ||
                                Contains(config, "--enable-version3");
        if (missing_required || prohibited || !Contains(config, "h264,aac") ||
            !Contains(config, "--enable-demuxer=mov")) {
            failure.status = FfmpegLoadStatus::configuration_mismatch;
            Unload();
            return false;
        }

        api_.av_log_set_level(AV_LOG_QUIET);
        loaded_ = true;
        return true;
    } catch (const std::bad_alloc&) {
        failure.status = FfmpegLoadStatus::allocation_failed;
        Unload();
        return false;
    } catch (...) {
        failure.status = FfmpegLoadStatus::unexpected_failure;
        Unload();
        return false;
    }
}

void FfmpegRuntime::Unload() noexcept {
    for (std::size_t index = modules_.size(); index > 0u; --index) {
        HMODULE& module = modules_[index - 1u];
        if (module != nullptr) {
            FreeLibrary(module);
            module = nullptr;
        }
    }
    versions_ = {};
    api_ = {};
    loaded_ = false;
}

} // namespace pbvp
