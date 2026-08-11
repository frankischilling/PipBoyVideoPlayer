#include "pbvp/input_prompts.hpp"

#include "test_support.hpp"

#include <array>
#include <string>

namespace {

pbvp::UiPromptLabels KeyboardLabels() noexcept {
    return {
        "ENTER", "SPACE", "ESC", "LEFT", "RIGHT", "UP", "DOWN", "T",
    };
}

std::string Text(const std::array<char, 192u>& output) {
    return output.data();
}

} // namespace

void RunInputPromptTests() {
    std::array<char, 192u> output{};
    const pbvp::UiPromptLabels labels = KeyboardLabels();

    PBVP_CHECK(std::string(pbvp::UiInputMethodName(
        pbvp::UiInputMethod::keyboard_mouse)) == "keyboard-mouse");
    PBVP_CHECK(std::string(pbvp::UiInputMethodName(
        pbvp::UiInputMethod::controller)) == "controller");
    PBVP_CHECK(std::string(pbvp::UiInputMethodName(
        static_cast<pbvp::UiInputMethod>(99u))) == "unknown");

    PBVP_CHECK(pbvp::FormatCatalogPrompt(
        pbvp::UiInputMethod::keyboard_mouse, labels, output));
    PBVP_CHECK(Text(output) == "ENTER PLAY  UP/DOWN SELECT");
    PBVP_CHECK(pbvp::FormatCatalogPrompt(
        pbvp::UiInputMethod::controller, {}, output));
    PBVP_CHECK(Text(output) == "A PLAY  D-PAD SELECT");

    PBVP_CHECK(pbvp::FormatCatalogBackPrompt(
        pbvp::UiInputMethod::keyboard_mouse, labels, output));
    PBVP_CHECK(Text(output) == "ESC BACK");
    PBVP_CHECK(pbvp::FormatCatalogBackPrompt(
        pbvp::UiInputMethod::controller, {}, output));
    PBVP_CHECK(Text(output) == "B BACK");

    pbvp::PlaybackStateSnapshot playback{};
    playback.state = pbvp::PlaybackState::playing;
    PBVP_CHECK(pbvp::FormatPlaybackPrompt(
        playback, pbvp::UiInputMethod::keyboard_mouse, labels, output));
    PBVP_CHECK(Text(output) ==
               "PLAYING  SPACE PAUSE  ESC STOP  LEFT/RIGHT SEEK  T COLOR");
    PBVP_CHECK(pbvp::FormatPlaybackPrompt(
        playback, pbvp::UiInputMethod::controller, {}, output));
    PBVP_CHECK(Text(output) ==
               "PLAYING  X PAUSE  B STOP  LB/RB SEEK  Y COLOR");

    playback.state = pbvp::PlaybackState::paused;
    PBVP_CHECK(pbvp::FormatPlaybackPrompt(
        playback, pbvp::UiInputMethod::controller, {}, output));
    PBVP_CHECK(Text(output) ==
               "PAUSED  X RESUME  B STOP  LB/RB SEEK  Y COLOR");

    playback.state = pbvp::PlaybackState::buffering;
    playback.pause_after_buffering = false;
    PBVP_CHECK(pbvp::FormatPlaybackPrompt(
        playback, pbvp::UiInputMethod::controller, {}, output));
    PBVP_CHECK(Text(output) == "BUFFERING  B STOP");
    playback.pause_after_buffering = true;
    PBVP_CHECK(pbvp::FormatPlaybackPrompt(
        playback, pbvp::UiInputMethod::keyboard_mouse, labels, output));
    PBVP_CHECK(Text(output) == "BUFFERING PAUSED  SPACE RESUME  ESC STOP");

    playback.state = pbvp::PlaybackState::error;
    playback.error = pbvp::PlaybackError::decoder_memory_failed;
    PBVP_CHECK(pbvp::FormatPlaybackPrompt(
        playback, pbvp::UiInputMethod::keyboard_mouse, labels, output));
    PBVP_CHECK(Text(output) == "VIDEO MEMORY ERROR");
    playback.error = pbvp::PlaybackError::audio_device_failed;
    PBVP_CHECK(pbvp::FormatPlaybackPrompt(
        playback, pbvp::UiInputMethod::controller, {}, output));
    PBVP_CHECK(Text(output) == "AUDIO PLAYBACK ERROR");

    struct FixedPromptCase final {
        pbvp::PlaybackState state;
        pbvp::PlaybackError error;
        const char* expected;
    };
    constexpr std::array<FixedPromptCase, 13u> fixed_cases{{
        {pbvp::PlaybackState::unavailable, pbvp::PlaybackError::none,
         "PLAYER UNAVAILABLE"},
        {pbvp::PlaybackState::idle, pbvp::PlaybackError::none, "VIDEOS"},
        {pbvp::PlaybackState::opening, pbvp::PlaybackError::none, "OPENING VIDEO"},
        {pbvp::PlaybackState::stopping, pbvp::PlaybackError::none, "STOPPING"},
        {pbvp::PlaybackState::error, pbvp::PlaybackError::media_open_failed,
         "VIDEO COULD NOT BE OPENED"},
        {pbvp::PlaybackState::error, pbvp::PlaybackError::decoder_failed,
         "VIDEO DECODE ERROR"},
        {pbvp::PlaybackState::error, pbvp::PlaybackError::decoder_memory_failed,
         "VIDEO MEMORY ERROR"},
        {pbvp::PlaybackState::error, pbvp::PlaybackError::audio_initialization_failed,
         "AUDIO PLAYBACK ERROR"},
        {pbvp::PlaybackState::error, pbvp::PlaybackError::audio_stream_failed,
         "AUDIO PLAYBACK ERROR"},
        {pbvp::PlaybackState::error, pbvp::PlaybackError::clock_unavailable,
         "PLAYBACK CLOCK ERROR"},
        {pbvp::PlaybackState::error, pbvp::PlaybackError::render_failed,
         "VIDEO DISPLAY ERROR"},
        {pbvp::PlaybackState::error, pbvp::PlaybackError::invalid_state,
         "PLAYBACK ERROR"},
        {pbvp::PlaybackState::error, pbvp::PlaybackError::none,
         "PLAYBACK ERROR"},
    }};
    for (const FixedPromptCase& test_case : fixed_cases) {
        playback.state = test_case.state;
        playback.error = test_case.error;
        playback.pause_after_buffering = false;
        PBVP_CHECK(pbvp::FormatPlaybackPrompt(
            playback, pbvp::UiInputMethod::keyboard_mouse, {}, output));
        PBVP_CHECK(Text(output) == test_case.expected);
    }

    std::array<char, 5u> short_output{};
    PBVP_CHECK(!pbvp::FormatCatalogPrompt(
        pbvp::UiInputMethod::controller, {}, short_output));
    PBVP_CHECK(short_output.back() == '\0');
    PBVP_CHECK(!pbvp::FormatCatalogPrompt(
        pbvp::UiInputMethod::keyboard_mouse, {}, output));
    PBVP_CHECK(!pbvp::FormatPlaybackPrompt(
        {pbvp::PlaybackState::playing, pbvp::PlaybackError::none, false},
        pbvp::UiInputMethod::keyboard_mouse, {}, output));
    PBVP_CHECK(!pbvp::FormatPlaybackPrompt(
        {}, pbvp::UiInputMethod::controller, {}, std::span<char>{}));
}
