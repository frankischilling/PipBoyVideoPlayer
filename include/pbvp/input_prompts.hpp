#pragma once

#include "pbvp/playback_state.hpp"

#include <cstdint>
#include <span>

namespace pbvp {

enum class UiInputMethod : std::uint32_t {
    keyboard_mouse,
    controller,
};

[[nodiscard]] const char* UiInputMethodName(UiInputMethod input_method) noexcept;

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
    UiInputMethod input_method,
    const UiPromptLabels& labels,
    std::span<char> output) noexcept;

[[nodiscard]] bool FormatCatalogBackPrompt(
    UiInputMethod input_method,
    const UiPromptLabels& labels,
    std::span<char> output) noexcept;

[[nodiscard]] bool FormatPlaybackPrompt(
    const PlaybackStateSnapshot& playback,
    UiInputMethod input_method,
    const UiPromptLabels& labels,
    std::span<char> output) noexcept;

} // namespace pbvp
