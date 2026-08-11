#pragma once

#include "pbvp/playback_state.hpp"

#include <span>

namespace pbvp {

struct UiPromptLabels final {
    const char* select_or_play{};
    const char* pause_resume{};
    const char* back_or_stop{};
    const char* seek_backward{};
    const char* seek_forward{};
    const char* previous_item{};
    const char* next_item{};
    const char* toggle_color{};
};

[[nodiscard]] bool FormatCatalogPrompt(
    const UiPromptLabels& labels,
    std::span<char> output) noexcept;

[[nodiscard]] bool FormatCatalogBackPrompt(
    const UiPromptLabels& labels,
    std::span<char> output) noexcept;

[[nodiscard]] bool FormatPlaybackPrompt(
    const PlaybackStateSnapshot& playback,
    const UiPromptLabels& labels,
    std::span<char> output) noexcept;

} // namespace pbvp
