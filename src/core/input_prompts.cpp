#include "pbvp/input_prompts.hpp"

#include <cstdarg>
#include <cstdio>

namespace pbvp {
namespace {

bool Format(std::span<char> output, const char* format, ...) noexcept {
    if (output.empty() || format == nullptr) {
        return false;
    }
    output.front() = '\0';
    std::va_list arguments;
    va_start(arguments, format);
    const int length = std::vsnprintf(
        output.data(), output.size(), format, arguments);
    va_end(arguments);
    output.back() = '\0';
    return length >= 0 && static_cast<std::size_t>(length) < output.size();
}

} // namespace

bool FormatCatalogPrompt(
    const UiInputMethod input_method,
    const UiPromptLabels& labels,
    const std::span<char> output) noexcept {
    if (input_method == UiInputMethod::controller) {
        return Format(output, "A PLAY  D-PAD SELECT");
    }
    return labels.select_or_play != nullptr && labels.previous_item != nullptr &&
           labels.next_item != nullptr && Format(
        output, "%s PLAY  %s/%s SELECT",
        labels.select_or_play, labels.previous_item, labels.next_item);
}

bool FormatCatalogBackPrompt(
    const UiInputMethod input_method,
    const UiPromptLabels& labels,
    const std::span<char> output) noexcept {
    if (input_method == UiInputMethod::controller) {
        return Format(output, "B BACK");
    }
    return labels.back_or_stop != nullptr &&
           Format(output, "%s BACK", labels.back_or_stop);
}

bool FormatPlaybackPrompt(
    const PlaybackStateSnapshot& playback,
    const UiInputMethod input_method,
    const UiPromptLabels& labels,
    const std::span<char> output) noexcept {
    const bool controller = input_method == UiInputMethod::controller;
    switch (playback.state) {
        case PlaybackState::unavailable:
            return Format(output, "PLAYER UNAVAILABLE");
        case PlaybackState::idle:
            return Format(output, "VIDEOS");
        case PlaybackState::opening:
            return Format(output, "OPENING VIDEO");
        case PlaybackState::buffering:
            if (!controller && (labels.back_or_stop == nullptr ||
                                (playback.pause_after_buffering &&
                                 labels.pause_resume == nullptr))) {
                return false;
            }
            if (playback.pause_after_buffering) {
                return Format(
                    output, "BUFFERING PAUSED  %s RESUME  %s STOP",
                    controller ? "X" : labels.pause_resume,
                    controller ? "B" : labels.back_or_stop);
            }
            return Format(
                output, "BUFFERING  %s STOP",
                controller ? "B" : labels.back_or_stop);
        case PlaybackState::playing:
            if (!controller && (labels.pause_resume == nullptr ||
                                labels.back_or_stop == nullptr ||
                                labels.seek_backward == nullptr ||
                                labels.seek_forward == nullptr ||
                                labels.toggle_color == nullptr)) {
                return false;
            }
            return Format(
                output, "PLAYING  %s PAUSE  %s STOP  %s/%s SEEK  %s COLOR",
                controller ? "X" : labels.pause_resume,
                controller ? "B" : labels.back_or_stop,
                controller ? "LB" : labels.seek_backward,
                controller ? "RB" : labels.seek_forward,
                controller ? "Y" : labels.toggle_color);
        case PlaybackState::paused:
            if (!controller && (labels.pause_resume == nullptr ||
                                labels.back_or_stop == nullptr ||
                                labels.seek_backward == nullptr ||
                                labels.seek_forward == nullptr ||
                                labels.toggle_color == nullptr)) {
                return false;
            }
            return Format(
                output, "PAUSED  %s RESUME  %s STOP  %s/%s SEEK  %s COLOR",
                controller ? "X" : labels.pause_resume,
                controller ? "B" : labels.back_or_stop,
                controller ? "LB" : labels.seek_backward,
                controller ? "RB" : labels.seek_forward,
                controller ? "Y" : labels.toggle_color);
        case PlaybackState::stopping:
            return Format(output, "STOPPING");
        case PlaybackState::error:
            switch (playback.error) {
                case PlaybackError::media_open_failed:
                    return Format(output, "VIDEO COULD NOT BE OPENED");
                case PlaybackError::decoder_failed:
                    return Format(output, "VIDEO DECODE ERROR");
                case PlaybackError::decoder_memory_failed:
                    return Format(output, "VIDEO MEMORY ERROR");
                case PlaybackError::audio_initialization_failed:
                case PlaybackError::audio_device_failed:
                case PlaybackError::audio_stream_failed:
                    return Format(output, "AUDIO PLAYBACK ERROR");
                case PlaybackError::clock_unavailable:
                    return Format(output, "PLAYBACK CLOCK ERROR");
                case PlaybackError::render_failed:
                    return Format(output, "VIDEO DISPLAY ERROR");
                case PlaybackError::invalid_state:
                case PlaybackError::none:
                    return Format(output, "PLAYBACK ERROR");
            }
    }
    return Format(output, "PLAYBACK ERROR");
}

} // namespace pbvp
