#include "pbvp/media_decoder.hpp"

#include "test_support.hpp"

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
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

void CheckAllGeneration(
    const std::vector<std::uint64_t>& generations,
    const std::uint64_t expected) {
    PBVP_CHECK(!generations.empty());
    PBVP_CHECK(std::all_of(
        generations.begin(), generations.end(),
        [expected](const std::uint64_t value) { return value == expected; }));
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
    pbvp::DecoderSnapshot unsupported = DecodeFailure(
        runtime, fixture_root, L"unsupported-mpeg4-mp3.mp4");
    PBVP_CHECK(unsupported.failure.status ==
               pbvp::MediaDecodeStatus::unsupported_video_codec);

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
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) {
        std::fputs("usage: pbvp_media_decoder_test <runtime-bin> <fixture-root>\n", stderr);
        return 2;
    }

    pbvp::FfmpegRuntime runtime;
    pbvp::FfmpegLoadFailure load_failure{};
    PBVP_CHECK(runtime.Load(argv[1], load_failure));
    if (!runtime.IsLoaded()) {
        return 1;
    }

    const std::wstring temporary_root = CreateTemporaryDirectory();
    TestBaseDecode(runtime, argv[2]);
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
