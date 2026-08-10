#include "pbvp/media_limits.hpp"

#include "test_support.hpp"

#include <limits>

void RunMediaLimitTests() {
    pbvp::DecodeLimits limits{};
    pbvp::VideoLayout layout{};
    PBVP_CHECK(pbvp::ComputeBgraLayout(1920u, 1080u, limits, layout) == pbvp::LayoutStatus::ok);
    PBVP_CHECK(layout.row_bytes == 7680u);
    PBVP_CHECK(layout.total_bytes == 8294400u);

    PBVP_CHECK(pbvp::ComputeBgraLayout(0u, 1080u, limits, layout) ==
               pbvp::LayoutStatus::zero_dimension);
    PBVP_CHECK(pbvp::ComputeBgraLayout(1921u, 1080u, limits, layout) ==
               pbvp::LayoutStatus::dimension_limit);
    limits.maximum_video_payload_bytes = 1024u;
    PBVP_CHECK(pbvp::ComputeBgraLayout(640u, 360u, limits, layout) ==
               pbvp::LayoutStatus::byte_limit);

    limits = {};
    std::size_t audio_bytes = 0u;
    PBVP_CHECK(pbvp::ComputeInterleavedAudioBytes(24000u, 2u, 4u, limits, audio_bytes) ==
               pbvp::LayoutStatus::ok);
    PBVP_CHECK(audio_bytes == 192000u);
    PBVP_CHECK(pbvp::ComputeInterleavedAudioBytes(0u, 2u, 4u, limits, audio_bytes) ==
               pbvp::LayoutStatus::invalid_sample_layout);
    PBVP_CHECK(pbvp::ComputeInterleavedAudioBytes(24000u, 33u, 4u, limits, audio_bytes) ==
               pbvp::LayoutStatus::invalid_sample_layout);
    limits.maximum_audio_payload_bytes = 64u;
    PBVP_CHECK(pbvp::ComputeInterleavedAudioBytes(24000u, 2u, 4u, limits, audio_bytes) ==
               pbvp::LayoutStatus::byte_limit);
}
