#include "pbvp/win32_playback_clock.hpp"
#include "pbvp/xaudio_stream.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <span>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

int failures = 0;

void Check(const bool condition, const char* expression, const int line) {
    if (!condition) {
        std::fprintf(stderr, "line %d: check failed: %s\n", line, expression);
        ++failures;
    }
}

#define CHECK(expression) Check((expression), #expression, __LINE__)

bool CheckStatus(
    const pbvp::XAudioStreamStatus actual,
    const pbvp::XAudioStreamStatus expected,
    const int line) {
    if (actual == expected) {
        return true;
    }
    std::fprintf(
        stderr,
        "line %d: expected status %s, got %s\n",
        line,
        pbvp::XAudioStreamStatusName(expected),
        pbvp::XAudioStreamStatusName(actual));
    ++failures;
    return false;
}

#define CHECK_STATUS(expression, expected) CheckStatus((expression), (expected), __LINE__)

std::vector<std::int16_t> SilentPcm(
    const std::uint32_t sample_rate,
    const std::uint32_t channels,
    const std::uint32_t duration_ms) {
    const std::size_t frames =
        static_cast<std::size_t>(sample_rate) * duration_ms / 1'000u;
    return std::vector<std::int16_t>(frames * channels, 0);
}

bool WaitForEnd(pbvp::XAudioStream& stream, const DWORD timeout_ms) {
    const ULONGLONG deadline = GetTickCount64() + timeout_ms;
    while (GetTickCount64() < deadline) {
        const auto snapshot = stream.Snapshot();
        if (snapshot.end_of_stream_reached) {
            return true;
        }
        if (snapshot.status == pbvp::XAudioStreamStatus::audio_device_failed) {
            return false;
        }
        Sleep(5u);
    }
    return false;
}

bool WaitForMediaTime(
    pbvp::XAudioStream& stream,
    const std::int64_t target_us,
    const DWORD timeout_ms) {
    const ULONGLONG deadline = GetTickCount64() + timeout_ms;
    while (GetTickCount64() < deadline) {
        const auto current = stream.MediaTimeUs();
        if (current.has_value() && *current >= target_us) {
            return true;
        }
        Sleep(5u);
    }
    return false;
}

void TestInvalidConfiguration() {
    pbvp::XAudioStream stream;
    pbvp::XAudioStreamConfig config{};
    config.channels = 0u;
    CHECK_STATUS(stream.Initialize(config), pbvp::XAudioStreamStatus::invalid_config);
    CHECK_STATUS(stream.Start(), pbvp::XAudioStreamStatus::not_initialized);
    CHECK(!stream.MediaTimeUs().has_value());
    CHECK(std::string_view(pbvp::XAudioStreamStatusName(
        static_cast<pbvp::XAudioStreamStatus>(999u))) == "unknown");

    config.channels = 2u;
    config.prebuffer_ms = 50u;
    config.slot_count = 2u;
    config.slot_bytes = 1u * 1024u;
    CHECK_STATUS(stream.Initialize(config), pbvp::XAudioStreamStatus::ok);
    const auto one_slot = SilentPcm(config.sample_rate, config.channels, 5u);
    CHECK_STATUS(
        stream.SubmitPcm(one_slot, 0, 1u, false),
        pbvp::XAudioStreamStatus::ok);
    CHECK_STATUS(
        stream.SubmitPcm(one_slot, 5'000, 1u, false),
        pbvp::XAudioStreamStatus::ok);
    CHECK_STATUS(
        stream.SubmitPcm(one_slot, 10'000, 1u, false),
        pbvp::XAudioStreamStatus::queue_full);
    CHECK(stream.Snapshot().pool_bytes == 2u * 1024u);
    CHECK_STATUS(stream.StopAndFlush(), pbvp::XAudioStreamStatus::ok);
}

void TestPrebufferDepth(const std::uint32_t prebuffer_ms) {
    pbvp::XAudioStream stream;
    pbvp::XAudioStreamConfig config{};
    config.prebuffer_ms = prebuffer_ms;
    config.slot_count = 32u;
    config.slot_bytes = 4u * 1024u;
    if (!CHECK_STATUS(stream.Initialize(config), pbvp::XAudioStreamStatus::ok)) {
        return;
    }

    const auto first = SilentPcm(config.sample_rate, config.channels, prebuffer_ms - 20u);
    CHECK_STATUS(
        stream.SubmitPcm(first, 0, 1u, false),
        pbvp::XAudioStreamStatus::ok);
    CHECK_STATUS(
        stream.Start(),
        pbvp::XAudioStreamStatus::insufficient_prebuffer);

    const auto second = SilentPcm(config.sample_rate, config.channels, 20u);
    CHECK_STATUS(
        stream.SubmitPcm(
            second,
            static_cast<std::int64_t>(prebuffer_ms - 20u) * 1'000,
            1u,
            false),
        pbvp::XAudioStreamStatus::ok);
    CHECK(stream.Snapshot().ready_to_start);
    CHECK_STATUS(stream.Start(), pbvp::XAudioStreamStatus::ok);

    const auto tail = SilentPcm(config.sample_rate, config.channels, 50u);
    CHECK_STATUS(
        stream.SubmitPcm(
            tail,
            static_cast<std::int64_t>(prebuffer_ms) * 1'000,
            1u,
            true),
        pbvp::XAudioStreamStatus::ok);
    CHECK(WaitForEnd(stream, 2'000u));

    const auto snapshot = stream.Snapshot();
    const auto media_time = stream.MediaTimeUs();
    CHECK(snapshot.underruns == 0u);
    CHECK(snapshot.end_of_stream_reached);
    CHECK(media_time.has_value());
    CHECK(media_time == static_cast<std::int64_t>(prebuffer_ms + 50u) * 1'000);
    CHECK(snapshot.pool_bytes == 32u * 4u * 1024u);
    std::printf(
        "prebuffer=%u ms completed=%llu underruns=%llu final=%lld us\n",
        prebuffer_ms,
        static_cast<unsigned long long>(snapshot.completed_buffers),
        static_cast<unsigned long long>(snapshot.underruns),
        static_cast<long long>(media_time.value_or(-1)));
}

void TestReadinessExcludesCompletedBuffers() {
    pbvp::XAudioStream stream;
    pbvp::XAudioStreamConfig config{};
    config.prebuffer_ms = 200u;
    config.slot_count = 16u;
    config.slot_bytes = 4u * 1024u;
    if (!CHECK_STATUS(stream.Initialize(config), pbvp::XAudioStreamStatus::ok)) {
        return;
    }

    const auto pcm = SilentPcm(config.sample_rate, config.channels, 200u);
    CHECK_STATUS(
        stream.SubmitPcm(pcm, 0, 1u, false),
        pbvp::XAudioStreamStatus::ok);
    CHECK(stream.Snapshot().ready_to_start);
    CHECK_STATUS(stream.Start(), pbvp::XAudioStreamStatus::ok);
    CHECK(WaitForMediaTime(stream, 190'000, 1'000u));
    Sleep(75u);
    CHECK_STATUS(stream.Pause(), pbvp::XAudioStreamStatus::ok);

    const auto snapshot = stream.Snapshot();
    CHECK(snapshot.queued_buffers == 0u);
    CHECK(snapshot.queued_bytes == 0u);
    CHECK(!snapshot.ready_to_start);
    CHECK_STATUS(stream.StopAndFlush(), pbvp::XAudioStreamStatus::ok);
}

void TestPauseResumeVolumeAndSeek() {
    pbvp::XAudioStream stream;
    pbvp::XAudioStreamConfig config{};
    config.prebuffer_ms = 100u;
    config.slot_count = 32u;
    config.slot_bytes = 4u * 1024u;
    if (!CHECK_STATUS(stream.Initialize(config), pbvp::XAudioStreamStatus::ok)) {
        return;
    }

    CHECK_STATUS(stream.SetVolume(0.35f), pbvp::XAudioStreamStatus::ok);
    CHECK_STATUS(stream.SetMuted(true), pbvp::XAudioStreamStatus::ok);
    CHECK(stream.Snapshot().muted);
    CHECK_STATUS(stream.SetMuted(false), pbvp::XAudioStreamStatus::ok);
    CHECK(!stream.Snapshot().muted);
    CHECK_STATUS(stream.SetVolume(-0.1f), pbvp::XAudioStreamStatus::invalid_config);
    CHECK_STATUS(
        stream.SetVolume((std::numeric_limits<float>::quiet_NaN)()),
        pbvp::XAudioStreamStatus::invalid_config);

    const auto long_clip = SilentPcm(config.sample_rate, config.channels, 500u);
    CHECK_STATUS(
        stream.SubmitPcm(long_clip, 0, 10u, true),
        pbvp::XAudioStreamStatus::ok);
    CHECK_STATUS(stream.Start(), pbvp::XAudioStreamStatus::ok);
    CHECK(WaitForMediaTime(stream, 50'000, 1'000u));
    CHECK_STATUS(stream.Pause(), pbvp::XAudioStreamStatus::ok);
    const auto paused_at = stream.MediaTimeUs();
    Sleep(120u);
    const auto still_paused = stream.MediaTimeUs();
    CHECK(paused_at.has_value());
    CHECK(still_paused.has_value());
    if (paused_at.has_value() && still_paused.has_value()) {
        CHECK(std::llabs(*still_paused - *paused_at) <= 50'000);
    }
    CHECK_STATUS(stream.Resume(), pbvp::XAudioStreamStatus::ok);
    CHECK(WaitForEnd(stream, 2'000u));
    CHECK(stream.MediaTimeUs() == 500'000);

    CHECK_STATUS(stream.StopAndFlush(), pbvp::XAudioStreamStatus::ok);
    auto snapshot = stream.Snapshot();
    CHECK(snapshot.queued_buffers == 0u);
    CHECK(snapshot.queued_bytes == 0u);
    CHECK(!snapshot.end_of_stream_submitted);

    const auto first_generation = SilentPcm(config.sample_rate, config.channels, 100u);
    CHECK_STATUS(
        stream.SubmitPcm(first_generation, 0, 20u, false),
        pbvp::XAudioStreamStatus::ok);
    CHECK_STATUS(
        stream.SubmitPcm(first_generation, 0, 21u, false),
        pbvp::XAudioStreamStatus::stale_generation);
    CHECK_STATUS(stream.Start(), pbvp::XAudioStreamStatus::ok);
    Sleep(30u);
    CHECK_STATUS(stream.StopAndFlush(), pbvp::XAudioStreamStatus::ok);

    const auto seek_clip = SilentPcm(config.sample_rate, config.channels, 100u);
    CHECK_STATUS(
        stream.SubmitPcm(seek_clip, 5'000'000, 21u, true),
        pbvp::XAudioStreamStatus::ok);
    CHECK_STATUS(stream.Start(), pbvp::XAudioStreamStatus::ok);
    CHECK(WaitForEnd(stream, 1'000u));
    CHECK(stream.MediaTimeUs() == 5'100'000);

    CHECK_STATUS(stream.RecoverDevice(), pbvp::XAudioStreamStatus::ok);
    snapshot = stream.Snapshot();
    CHECK(snapshot.initialized);
    CHECK(snapshot.queued_buffers == 0u);

    pbvp::XAudioStreamSnapshot foreign_snapshot{};
    std::thread foreign([&stream, &foreign_snapshot] {
        foreign_snapshot = stream.Snapshot();
    });
    foreign.join();
    CHECK(foreign_snapshot.status == pbvp::XAudioStreamStatus::wrong_thread);
}

void TestPcmFormats() {
    constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 4u> formats{{
        {44'100u, 1u},
        {44'100u, 2u},
        {48'000u, 1u},
        {48'000u, 2u},
    }};

    for (const auto [sample_rate, channels] : formats) {
        pbvp::XAudioStream stream;
        pbvp::XAudioStreamConfig config{};
        config.sample_rate = sample_rate;
        config.channels = channels;
        config.prebuffer_ms = 50u;
        config.slot_count = 8u;
        config.slot_bytes = 8u * 1024u;
        if (!CHECK_STATUS(stream.Initialize(config), pbvp::XAudioStreamStatus::ok)) {
            continue;
        }
        const auto pcm = SilentPcm(sample_rate, channels, 80u);
        CHECK_STATUS(
            stream.SubmitPcm(pcm, 0, 1u, true),
            pbvp::XAudioStreamStatus::ok);
        CHECK_STATUS(stream.Start(), pbvp::XAudioStreamStatus::ok);
        CHECK(WaitForEnd(stream, 1'000u));
        CHECK(stream.MediaTimeUs() == 80'000);
    }
}

void TestSilentClock() {
    pbvp::Win32PlaybackClock clock;
    CHECK(clock.Start(1'000'000));
    CHECK(clock.Frequency() > 0);
    Sleep(30u);
    const auto before_pause = clock.MediaTimeUs();
    CHECK(before_pause.has_value());
    CHECK(before_pause.value_or(0) >= 1'015'000);

    CHECK(clock.Pause());
    const auto paused = clock.MediaTimeUs();
    Sleep(50u);
    CHECK(clock.MediaTimeUs() == paused);
    CHECK(clock.Resume());
    Sleep(30u);
    CHECK(clock.MediaTimeUs().value_or(0) >= paused.value_or(0) + 15'000);

    CHECK(clock.Seek(250'000));
    const auto sought = clock.MediaTimeUs();
    CHECK(sought.has_value());
    CHECK(sought.value_or(0) >= 250'000);
    CHECK(sought.value_or(0) < 275'000);
    CHECK(clock.Pause());
    CHECK(clock.Seek(5'000'000));
    CHECK(clock.MediaTimeUs() == 5'000'000);
    clock.Clear();
    CHECK(!clock.IsActive());
    CHECK(!clock.MediaTimeUs().has_value());
}

void TestRepeatedLifetime() {
    for (std::uint32_t cycle = 0u; cycle < 25u; ++cycle) {
        pbvp::XAudioStream stream;
        pbvp::XAudioStreamConfig config{};
        config.prebuffer_ms = 50u;
        config.slot_count = 4u;
        config.slot_bytes = 4u * 1024u;
        if (!CHECK_STATUS(stream.Initialize(config), pbvp::XAudioStreamStatus::ok)) {
            return;
        }
        const auto pcm = SilentPcm(config.sample_rate, config.channels, 50u);
        CHECK_STATUS(
            stream.SubmitPcm(pcm, 0, cycle + 1u, true),
            pbvp::XAudioStreamStatus::ok);
        CHECK_STATUS(stream.Start(), pbvp::XAudioStreamStatus::ok);
        CHECK(WaitForEnd(stream, 1'000u));
    }
}

} // namespace

int main() {
    TestInvalidConfiguration();
    TestPrebufferDepth(100u);
    TestPrebufferDepth(200u);
    TestPrebufferDepth(300u);
    TestReadinessExcludesCompletedBuffers();
    TestPauseResumeVolumeAndSeek();
    TestPcmFormats();
    TestSilentClock();
    TestRepeatedLifetime();

    if (failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    std::puts("XAudio2 stream checks passed");
    return 0;
}
