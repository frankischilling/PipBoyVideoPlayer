#include "pbvp/media_decoder.hpp"

#include "test_support.hpp"

#include <Windows.h>
#include <Psapi.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace std::chrono_literals;

struct CollectedOutput {
    pbvp::DecoderSnapshot snapshot{};
    std::vector<std::int64_t> video_pts{};
    std::vector<std::int64_t> video_durations{};
    std::vector<std::uint64_t> video_generations{};
    std::vector<std::int64_t> audio_pts{};
    std::vector<std::uint64_t> audio_generations{};
    std::uint64_t audio_samples{};
    std::uint64_t absolute_sample_sum{};
    std::uint32_t first_video_width{};
    std::uint32_t first_video_height{};
};

void DrainAvailable(pbvp::MediaDecoder& decoder, CollectedOutput& output) {
    for (;;) {
        auto video = decoder.TryPopVideo();
        if (video.status != pbvp::QueuePopStatus::item) {
            break;
        }
        PBVP_CHECK(video.value.has_value());
        PBVP_CHECK(video.payload_bytes == video.value->bgra.size());
        if (output.video_pts.empty()) {
            output.first_video_width = video.value->width;
            output.first_video_height = video.value->height;
        }
        output.video_pts.push_back(video.value->pts_us);
        output.video_durations.push_back(video.value->duration_us);
        output.video_generations.push_back(video.value->generation);
    }
    for (;;) {
        auto audio = decoder.TryPopAudio();
        if (audio.status != pbvp::QueuePopStatus::item) {
            break;
        }
        PBVP_CHECK(audio.value.has_value());
        PBVP_CHECK(audio.payload_bytes ==
                   audio.value->samples.size() * sizeof(std::int16_t));
        PBVP_CHECK(audio.value->channels == 2u);
        PBVP_CHECK(audio.value->sample_rate == 48000u);
        PBVP_CHECK(audio.value->samples.size() ==
                   static_cast<std::size_t>(audio.value->samples_per_channel) * 2u);
        output.audio_pts.push_back(audio.value->pts_us);
        output.audio_generations.push_back(audio.value->generation);
        output.audio_samples += audio.value->samples_per_channel;
        for (const std::int16_t sample : audio.value->samples) {
            output.absolute_sample_sum += static_cast<std::uint64_t>(
                std::abs(static_cast<int>(sample)));
        }
    }
}

CollectedOutput CollectUntilTerminal(
    pbvp::MediaDecoder& decoder,
    const std::chrono::milliseconds timeout = 10s) {
    CollectedOutput output{};
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    for (;;) {
        DrainAvailable(decoder, output);
        output.snapshot = decoder.Snapshot();
        if (output.snapshot.state == pbvp::DecoderState::end_of_stream ||
            output.snapshot.state == pbvp::DecoderState::failed ||
            output.snapshot.state == pbvp::DecoderState::stopped) {
            DrainAvailable(decoder, output);
            return output;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            PBVP_CHECK(false);
            decoder.Cancel();
            return output;
        }
        Sleep(1u);
    }
}

bool WaitForDecoding(pbvp::MediaDecoder& decoder) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < deadline) {
        const pbvp::DecoderState state = decoder.Snapshot().state;
        if (state == pbvp::DecoderState::decoding) {
            return true;
        }
        if (state == pbvp::DecoderState::failed || state == pbvp::DecoderState::stopped) {
            return false;
        }
        Sleep(1u);
    }
    return false;
}

bool WaitForFullAudioQueue(pbvp::MediaDecoder& decoder) {
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while (std::chrono::steady_clock::now() < deadline) {
        const pbvp::DecoderSnapshot snapshot = decoder.Snapshot();
        if (decoder.BufferUsage().audio_items == 16u) {
            return true;
        }
        if (snapshot.state == pbvp::DecoderState::failed ||
            snapshot.state == pbvp::DecoderState::stopped ||
            snapshot.state == pbvp::DecoderState::end_of_stream) {
            return false;
        }
        Sleep(1u);
    }
    return false;
}

struct ProcessUsage {
    std::uint64_t working_set_bytes{};
    std::uint64_t private_bytes{};
    std::uint64_t address_space_bytes{};
};

ProcessUsage MeasureProcessUsage() {
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = static_cast<DWORD>(sizeof(counters));
    PBVP_CHECK(GetProcessMemoryInfo(
        GetCurrentProcess(),
        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
        static_cast<DWORD>(sizeof(counters))) != FALSE);

    SYSTEM_INFO system_info{};
    GetSystemInfo(&system_info);
    const std::uintptr_t maximum_address = reinterpret_cast<std::uintptr_t>(
        system_info.lpMaximumApplicationAddress);
    std::uintptr_t address = 0u;
    std::uint64_t used_address_space = 0u;
    while (address < maximum_address) {
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(
                reinterpret_cast<const void*>(address),
                &region, sizeof(region)) == 0u) {
            break;
        }
        if (region.State != MEM_FREE) {
            used_address_space += static_cast<std::uint64_t>(region.RegionSize);
        }
        const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(
            region.BaseAddress);
        if (region.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - base) {
            break;
        }
        const std::uintptr_t next = base + region.RegionSize;
        if (next <= address) {
            break;
        }
        address = next;
    }
    return {
        static_cast<std::uint64_t>(counters.WorkingSetSize),
        static_cast<std::uint64_t>(counters.PrivateUsage),
        used_address_space,
    };
}

std::uint64_t PositiveDelta(
    const std::uint64_t value,
    const std::uint64_t baseline) {
    return value > baseline ? value - baseline : 0u;
}

std::uint64_t FileTimeValue(const FILETIME& value) {
    ULARGE_INTEGER combined{};
    combined.LowPart = value.dwLowDateTime;
    combined.HighPart = value.dwHighDateTime;
    return combined.QuadPart;
}

std::uint64_t ProcessCpuTime100ns() {
    FILETIME creation{};
    FILETIME exit{};
    FILETIME kernel{};
    FILETIME user{};
    PBVP_CHECK(GetProcessTimes(
        GetCurrentProcess(), &creation, &exit, &kernel, &user) != FALSE);
    return FileTimeValue(kernel) + FileTimeValue(user);
}

void CheckAllGeneration(
    const std::vector<std::uint64_t>& generations,
    const std::uint64_t expected) {
    PBVP_CHECK(!generations.empty());
    PBVP_CHECK(std::all_of(
        generations.begin(), generations.end(),
        [expected](const std::uint64_t value) { return value == expected; }));
}

void TestVideoPayloadAllocator() {
    constexpr std::size_t payload_bytes = 512u * 288u * 4u;
    constexpr std::uint64_t retained_limit = 8u * 1024u * 1024u;
    const ProcessUsage baseline = MeasureProcessUsage();
    for (std::size_t iteration = 0u; iteration < 512u; ++iteration) {
        pbvp::VideoPixelBuffer pixels(payload_bytes);
        PBVP_CHECK(pixels.size() == payload_bytes);
        MEMORY_BASIC_INFORMATION region{};
        PBVP_CHECK(VirtualQuery(
            pixels.data(), &region, sizeof(region)) == sizeof(region));
        PBVP_CHECK(region.AllocationBase == pixels.data());
        PBVP_CHECK(region.State == MEM_COMMIT);
        PBVP_CHECK(region.Type == MEM_PRIVATE);

        pbvp::VideoPixelBuffer moved(std::move(pixels));
        PBVP_CHECK(pixels.empty());
        PBVP_CHECK(moved.size() == payload_bytes);
    }
    const ProcessUsage completed = MeasureProcessUsage();
    PBVP_CHECK(PositiveDelta(
        completed.private_bytes, baseline.private_bytes) < retained_limit);
    PBVP_CHECK(PositiveDelta(
        completed.address_space_bytes, baseline.address_space_bytes) < retained_limit);
}

std::wstring CreateTemporaryDirectory() {
    wchar_t root[MAX_PATH]{};
    PBVP_CHECK(GetTempPathW(MAX_PATH, root) != 0u);
    wchar_t candidate[MAX_PATH]{};
    PBVP_CHECK(GetTempFileNameW(root, L"pbv", 0u, candidate) != 0u);
    PBVP_CHECK(DeleteFileW(candidate) != FALSE);
    PBVP_CHECK(CreateDirectoryW(candidate, nullptr) != FALSE);
    return candidate;
}

bool CopyAndTruncate(
    const std::wstring& source,
    const std::wstring& destination) {
    if (CopyFileW(source.c_str(), destination.c_str(), FALSE) == FALSE) {
        return false;
    }
    HANDLE file = CreateFileW(
        destination.c_str(), GENERIC_READ | GENERIC_WRITE, 0u,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER size{};
    bool ok = GetFileSizeEx(file, &size) != FALSE;
    if (ok) {
        size.QuadPart /= 2;
        ok = SetFilePointerEx(file, size, nullptr, FILE_BEGIN) != FALSE &&
             SetEndOfFile(file) != FALSE;
    }
    CloseHandle(file);
    return ok;
}

void WriteRandomFixture(const std::wstring& path) {
    HANDLE file = CreateFileW(
        path.c_str(), GENERIC_WRITE, 0u, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    PBVP_CHECK(file != INVALID_HANDLE_VALUE);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    std::vector<std::uint8_t> bytes(4096u);
    for (std::size_t index = 0u; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::uint8_t>((index * 73u + 19u) & 0xFFu);
    }
    DWORD written = 0u;
    PBVP_CHECK(WriteFile(
        file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) != FALSE);
    PBVP_CHECK(written == bytes.size());
    CloseHandle(file);
}

pbvp::DecoderSnapshot DecodeFailure(
    const pbvp::FfmpegRuntime& runtime,
    const std::wstring& root,
    const std::wstring& name,
    pbvp::MediaDecoderConfig config = {}) {
    pbvp::MediaDecoder decoder(runtime, config);
    pbvp::MediaDecodeFailure start_failure{};
    PBVP_CHECK(decoder.Start(root, name, start_failure));
    PBVP_CHECK(start_failure.status == pbvp::MediaDecodeStatus::ok);
    CollectedOutput output = CollectUntilTerminal(decoder);
    decoder.Stop();
    PBVP_CHECK(output.snapshot.state == pbvp::DecoderState::failed);
    return output.snapshot;
}

void TestBaseDecode(
    const pbvp::FfmpegRuntime& runtime,
    const std::wstring& fixture_root) {
    pbvp::MediaDecoder decoder(runtime);
    pbvp::MediaDecodeFailure failure{};
    PBVP_CHECK(decoder.Start(
        fixture_root, L"h264-aac-44100-stereo.mp4", failure));
    CollectedOutput output = CollectUntilTerminal(decoder);
    decoder.Stop();

    PBVP_CHECK(output.snapshot.state == pbvp::DecoderState::end_of_stream);
    PBVP_CHECK(output.snapshot.failure.status == pbvp::MediaDecodeStatus::ok);
    PBVP_CHECK(output.snapshot.info.source_width == 160u);
    PBVP_CHECK(output.snapshot.info.source_height == 90u);
    PBVP_CHECK(output.snapshot.info.display_width == 160u);
    PBVP_CHECK(output.snapshot.info.display_height == 90u);
    PBVP_CHECK(output.snapshot.info.clockwise_rotation_degrees == 0u);
    PBVP_CHECK(output.snapshot.info.has_audio);
    PBVP_CHECK(output.snapshot.info.source_audio_channels == 2u);
    PBVP_CHECK(output.snapshot.info.source_audio_rate == 44100u);
    PBVP_CHECK(output.video_pts.size() == 20u);
    PBVP_CHECK(output.first_video_width == 160u);
    PBVP_CHECK(output.first_video_height == 90u);
    PBVP_CHECK(std::is_sorted(output.video_pts.begin(), output.video_pts.end()));
    PBVP_CHECK(!output.audio_pts.empty());
    PBVP_CHECK(output.audio_samples >= 95000u && output.audio_samples <= 97000u);
    CheckAllGeneration(output.video_generations, 1u);
    CheckAllGeneration(output.audio_generations, 1u);
}

void TestAudioLayouts(
    const pbvp::FfmpegRuntime& runtime,
    const std::wstring& fixture_root) {
    struct AudioCase {
        const wchar_t* name;
        std::uint32_t channels;
    };
    constexpr AudioCase cases[]{
        {L"h264-aac-48000-mono.mp4", 1u},
        {L"h264-aac-48000-51.mp4", 6u},
    };
    for (const AudioCase& test : cases) {
        pbvp::MediaDecoder decoder(runtime);
        pbvp::MediaDecodeFailure failure{};
        PBVP_CHECK(decoder.Start(fixture_root, test.name, failure));
        CollectedOutput output = CollectUntilTerminal(decoder);
        decoder.Stop();
        PBVP_CHECK(output.snapshot.state == pbvp::DecoderState::end_of_stream);
        PBVP_CHECK(output.snapshot.info.source_audio_channels == test.channels);
        PBVP_CHECK(output.snapshot.info.source_audio_rate == 48000u);
        PBVP_CHECK(output.snapshot.info.output_audio_channels == 2u);
        PBVP_CHECK(output.snapshot.info.output_audio_rate == 48000u);
        PBVP_CHECK(output.audio_samples >= 47000u && output.audio_samples <= 49000u);
        PBVP_CHECK(output.absolute_sample_sum > 0u);
    }
}

void TestFullHdMemory(
    const pbvp::FfmpegRuntime& runtime,
    const std::wstring& fixture_root) {
    constexpr std::uint64_t mebibyte = 1024u * 1024u;
    const ProcessUsage baseline = MeasureProcessUsage();
    pbvp::MediaDecoder decoder(runtime);
    pbvp::MediaDecodeFailure failure{};
    PBVP_CHECK(decoder.Start(fixture_root, L"h264-aac-1080p.mp4", failure));
    PBVP_CHECK(WaitForFullAudioQueue(decoder));
    const pbvp::DecoderSnapshot snapshot = decoder.Snapshot();
    const pbvp::DecoderBufferUsage buffers = decoder.BufferUsage();
    const ProcessUsage filled = MeasureProcessUsage();

    PBVP_CHECK(snapshot.state == pbvp::DecoderState::decoding);
    PBVP_CHECK(snapshot.info.source_width == 1920u);
    PBVP_CHECK(snapshot.info.source_height == 1080u);
    PBVP_CHECK(buffers.video_items > 0u);
    PBVP_CHECK(buffers.video_items <= 12u);
    PBVP_CHECK(buffers.video_bytes ==
               buffers.video_items * 512u * 288u * 4u);
    PBVP_CHECK(buffers.video_bytes <= 32u * mebibyte);
    PBVP_CHECK(buffers.audio_items == 16u);
    PBVP_CHECK(buffers.audio_bytes == 16u * 4u * 1024u);

    const std::uint64_t working_delta = PositiveDelta(
        filled.working_set_bytes, baseline.working_set_bytes);
    const std::uint64_t private_delta = PositiveDelta(
        filled.private_bytes, baseline.private_bytes);
    const std::uint64_t address_delta = PositiveDelta(
        filled.address_space_bytes, baseline.address_space_bytes);
    std::printf(
        "1080p buffered usage: working=%llu private=%llu address=%llu video_queue=%zu audio_queue=%zu\n",
        static_cast<unsigned long long>(working_delta),
        static_cast<unsigned long long>(private_delta),
        static_cast<unsigned long long>(address_delta),
        buffers.video_bytes,
        buffers.audio_bytes);
    PBVP_CHECK(private_delta < 128u * mebibyte);
    PBVP_CHECK(address_delta < 192u * mebibyte);

    const auto stop_start = std::chrono::steady_clock::now();
    decoder.Stop();
    PBVP_CHECK(std::chrono::steady_clock::now() - stop_start < 2s);
    PBVP_CHECK(decoder.Snapshot().state == pbvp::DecoderState::stopped);
}

void TestFullHdDecodePerformance(
    const pbvp::FfmpegRuntime& runtime,
    const std::wstring& fixture_root) {
    pbvp::MediaDecoder decoder(runtime);
    pbvp::MediaDecodeFailure failure{};
    const std::uint64_t cpu_start = ProcessCpuTime100ns();
    const auto wall_start = std::chrono::steady_clock::now();
    PBVP_CHECK(decoder.Start(fixture_root, L"h264-aac-1080p.mp4", failure));
    CollectedOutput output = CollectUntilTerminal(decoder);
    const auto wall_end = std::chrono::steady_clock::now();
    const std::uint64_t cpu_end = ProcessCpuTime100ns();
    decoder.Stop();

    const auto wall_us = std::chrono::duration_cast<std::chrono::microseconds>(
        wall_end - wall_start).count();
    const std::uint64_t cpu_us = cpu_end >= cpu_start
        ? (cpu_end - cpu_start) / 10u
        : 0u;
    std::printf(
        "1080p decode performance: wall_us=%lld process_cpu_us=%llu video=%zu audio_samples=%llu\n",
        static_cast<long long>(wall_us),
        static_cast<unsigned long long>(cpu_us),
        output.video_pts.size(),
        static_cast<unsigned long long>(output.audio_samples));
    PBVP_CHECK(output.snapshot.state == pbvp::DecoderState::end_of_stream);
    PBVP_CHECK(output.snapshot.failure.status == pbvp::MediaDecodeStatus::ok);
    PBVP_CHECK(output.first_video_width == 512u);
    PBVP_CHECK(output.first_video_height == 288u);
    PBVP_CHECK(output.video_pts.size() == 30u);
    PBVP_CHECK(output.audio_samples >= 47000u && output.audio_samples <= 49000u);
    PBVP_CHECK(wall_us > 0 && wall_us < 5'000'000);
    PBVP_CHECK(cpu_us > 0u && cpu_us < 5'000'000u);
}

void TestRotation(
    const pbvp::FfmpegRuntime& runtime,
    const std::wstring& fixture_root) {
    pbvp::MediaDecoder decoder(runtime);
    pbvp::MediaDecodeFailure failure{};
    PBVP_CHECK(decoder.Start(
        fixture_root, L"h264-aac-rotate90.mp4", failure));
    CollectedOutput output = CollectUntilTerminal(decoder);
    decoder.Stop();
    PBVP_CHECK(output.snapshot.state == pbvp::DecoderState::end_of_stream);
    PBVP_CHECK(output.snapshot.info.clockwise_rotation_degrees == 270u);
    PBVP_CHECK(output.snapshot.info.display_width == 90u);
    PBVP_CHECK(output.snapshot.info.display_height == 160u);
    PBVP_CHECK(output.first_video_width == 90u);
    PBVP_CHECK(output.first_video_height == 160u);
}

void TestVariableFrameRate(
    const pbvp::FfmpegRuntime& runtime,
    const std::wstring& fixture_root) {
    pbvp::MediaDecoder decoder(runtime);
    pbvp::MediaDecodeFailure failure{};
    PBVP_CHECK(decoder.Start(
        fixture_root, L"h264-vfr-silent.mp4", failure));
    CollectedOutput output = CollectUntilTerminal(decoder);
    decoder.Stop();
    PBVP_CHECK(output.snapshot.state == pbvp::DecoderState::end_of_stream);
    PBVP_CHECK(!output.snapshot.info.has_audio);
    PBVP_CHECK(output.video_pts.size() == 15u);
    bool found_short = false;
    bool found_long = false;
    for (std::size_t index = 1u; index < output.video_pts.size(); ++index) {
        const std::int64_t delta = output.video_pts[index] - output.video_pts[index - 1u];
        found_short = found_short || delta == 100000;
        found_long = found_long || delta == 200000;
    }
    PBVP_CHECK(found_short);
    PBVP_CHECK(found_long);
}

void TestSeeking(
    const pbvp::FfmpegRuntime& runtime,
    const std::wstring& fixture_root) {
    pbvp::MediaDecoder decoder(runtime);
    pbvp::MediaDecodeFailure failure{};
    PBVP_CHECK(decoder.Start(
        fixture_root, L"h264-aac-44100-stereo.mp4", failure));
    PBVP_CHECK(WaitForDecoding(decoder));
    std::uint64_t generation = 0u;
    PBVP_CHECK(decoder.RequestSeek(1000000, generation));
    PBVP_CHECK(generation == 2u);
    PBVP_CHECK(WaitForDecoding(decoder));
    CollectedOutput forward = CollectUntilTerminal(decoder);
    PBVP_CHECK(forward.snapshot.state == pbvp::DecoderState::end_of_stream);
    CheckAllGeneration(forward.video_generations, 2u);
    CheckAllGeneration(forward.audio_generations, 2u);
    PBVP_CHECK(forward.video_pts.front() >= 900000);
    PBVP_CHECK(forward.audio_pts.front() >= 1000000);

    PBVP_CHECK(decoder.RequestSeek(200000, generation));
    PBVP_CHECK(generation == 3u);
    PBVP_CHECK(WaitForDecoding(decoder));
    CollectedOutput backward = CollectUntilTerminal(decoder);
    decoder.Stop();
    PBVP_CHECK(backward.snapshot.state == pbvp::DecoderState::end_of_stream);
    CheckAllGeneration(backward.video_generations, 3u);
    CheckAllGeneration(backward.audio_generations, 3u);
    PBVP_CHECK(backward.video_pts.front() >= 100000);
    PBVP_CHECK(backward.audio_pts.front() >= 200000);
}

void TestCancellation(
    const pbvp::FfmpegRuntime& runtime,
    const std::wstring& fixture_root) {
    pbvp::MediaDecoderConfig config{};
    config.video_queue = {1u, 1024u * 1024u};
    config.audio_queue = {1u, 256u * 1024u};
    pbvp::MediaDecoder decoder(runtime, config);
    pbvp::MediaDecodeFailure failure{};
    PBVP_CHECK(decoder.Start(
        fixture_root, L"h264-aac-44100-stereo.mp4", failure));
    PBVP_CHECK(WaitForDecoding(decoder));
    const auto start = std::chrono::steady_clock::now();
    decoder.Stop();
    const auto elapsed = std::chrono::steady_clock::now() - start;
    PBVP_CHECK(elapsed < 2s);
    PBVP_CHECK(decoder.Snapshot().state == pbvp::DecoderState::stopped);
}

void TestFailures(
    const pbvp::FfmpegRuntime& runtime,
    const std::wstring& fixture_root,
    const std::wstring& temporary_root) {
    pbvp::MediaDecoderConfig invalid_output{};
    invalid_output.output_video_edge_limit = 513u;
    pbvp::MediaDecoder invalid_decoder(runtime, invalid_output);
    pbvp::MediaDecodeFailure invalid_failure{};
    PBVP_CHECK(!invalid_decoder.Start(
        fixture_root, L"h264-aac-44100-stereo.mp4", invalid_failure));
    PBVP_CHECK(invalid_failure.status == pbvp::MediaDecodeStatus::invalid_configuration);

    pbvp::DecoderSnapshot unsupported = DecodeFailure(
        runtime, fixture_root, L"unsupported-mpeg4-mp3.mp4");
    PBVP_CHECK(unsupported.failure.status ==
               pbvp::MediaDecodeStatus::unsupported_video_codec);

    pbvp::DecoderSnapshot unsupported_audio = DecodeFailure(
        runtime, fixture_root, L"h264-unsupported-mp3.mp4");
    PBVP_CHECK(unsupported_audio.failure.status ==
               pbvp::MediaDecodeStatus::unsupported_audio_codec);

    pbvp::DecoderSnapshot encrypted = DecodeFailure(
        runtime, fixture_root, L"encrypted-cenc.mp4");
    PBVP_CHECK(encrypted.failure.status == pbvp::MediaDecodeStatus::encrypted_media);

    pbvp::MediaDecoderConfig limited{};
    limited.payload_limits.maximum_width = 100u;
    pbvp::DecoderSnapshot oversized = DecodeFailure(
        runtime, fixture_root, L"h264-aac-44100-stereo.mp4", limited);
    PBVP_CHECK(oversized.failure.status ==
               pbvp::MediaDecodeStatus::source_limit_exceeded);

    pbvp::DecoderSnapshot missing = DecodeFailure(
        runtime, fixture_root, L"missing.mp4");
    PBVP_CHECK(missing.failure.status == pbvp::MediaDecodeStatus::io_failure);
    PBVP_CHECK(missing.failure.io.status == pbvp::MediaIoStatus::file_missing);

    const std::wstring empty_path = temporary_root + L"\\empty.mp4";
    HANDLE empty = CreateFileW(
        empty_path.c_str(), GENERIC_WRITE, 0u, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    PBVP_CHECK(empty != INVALID_HANDLE_VALUE);
    if (empty != INVALID_HANDLE_VALUE) {
        CloseHandle(empty);
    }
    pbvp::DecoderSnapshot empty_result = DecodeFailure(
        runtime, temporary_root, L"empty.mp4");
    PBVP_CHECK(empty_result.failure.status == pbvp::MediaDecodeStatus::io_failure);
    PBVP_CHECK(empty_result.failure.io.status == pbvp::MediaIoStatus::empty_file);

    const std::wstring random_path = temporary_root + L"\\random.mp4";
    WriteRandomFixture(random_path);
    pbvp::DecoderSnapshot random = DecodeFailure(
        runtime, temporary_root, L"random.mp4");
    PBVP_CHECK(random.failure.status == pbvp::MediaDecodeStatus::demux_open_failed);

    const std::wstring source = fixture_root + L"\\h264-aac-44100-stereo.mp4";
    const std::wstring truncated_path = temporary_root + L"\\truncated.mp4";
    PBVP_CHECK(CopyAndTruncate(source, truncated_path));
    pbvp::DecoderSnapshot truncated = DecodeFailure(
        runtime, temporary_root, L"truncated.mp4");
    PBVP_CHECK(
        truncated.failure.status == pbvp::MediaDecodeStatus::demux_open_failed ||
        truncated.failure.status == pbvp::MediaDecodeStatus::stream_info_failed ||
        truncated.failure.status == pbvp::MediaDecodeStatus::demux_read_failed ||
        truncated.failure.status == pbvp::MediaDecodeStatus::damaged_media);

    DeleteFileW(random_path.c_str());
    DeleteFileW(truncated_path.c_str());
    DeleteFileW(empty_path.c_str());
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) {
        std::fputs("usage: pbvp_media_decoder_test <runtime-bin> <fixture-root>\n", stderr);
        return 2;
    }

    pbvp::FfmpegRuntime runtime;
    PBVP_CHECK(std::string(pbvp::MediaDecodeFailureSiteName(
                   pbvp::MediaDecodeFailureSite::video_pixel_buffer)) ==
               "video_pixel_buffer");
    PBVP_CHECK(std::string(pbvp::MediaDecodeFailureSiteName(
                   pbvp::MediaDecodeFailureSite::resampler_flush_buffer)) ==
               "resampler_flush_buffer");
    pbvp::FfmpegLoadFailure load_failure{};
    PBVP_CHECK(runtime.Load(argv[1], load_failure));
    if (!runtime.IsLoaded()) {
        return 1;
    }

    const std::wstring temporary_root = CreateTemporaryDirectory();
    TestVideoPayloadAllocator();
    TestFullHdMemory(runtime, argv[2]);
    TestFullHdDecodePerformance(runtime, argv[2]);
    TestBaseDecode(runtime, argv[2]);
    TestAudioLayouts(runtime, argv[2]);
    TestRotation(runtime, argv[2]);
    TestVariableFrameRate(runtime, argv[2]);
    TestSeeking(runtime, argv[2]);
    TestCancellation(runtime, argv[2]);
    TestFailures(runtime, argv[2], temporary_root);
    PBVP_CHECK(RemoveDirectoryW(temporary_root.c_str()) != FALSE);

    runtime.Unload();
    if (pbvp::test::failures != 0) {
        std::fprintf(stderr, "%d test check(s) failed\n", pbvp::test::failures);
        return 1;
    }
    std::puts("media decoder checks passed");
    return 0;
}
