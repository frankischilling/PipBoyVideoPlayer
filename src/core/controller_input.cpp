#include "pbvp/controller_input.hpp"

#include <array>

namespace pbvp {

std::uint32_t ControllerCommandsForButtonEdges(
    const std::uint16_t current_buttons,
    const std::uint16_t previous_buttons) noexcept {
    constexpr std::uint16_t kDpadUp = 0x0001u;
    constexpr std::uint16_t kDpadDown = 0x0002u;
    constexpr std::uint16_t kLeftShoulder = 0x0100u;
    constexpr std::uint16_t kRightShoulder = 0x0200u;
    constexpr std::uint16_t kA = 0x1000u;
    constexpr std::uint16_t kB = 0x2000u;
    constexpr std::uint16_t kX = 0x4000u;
    constexpr std::uint16_t kY = 0x8000u;

    struct ButtonCommand final {
        std::uint16_t button;
        ControllerCommand command;
    };
    constexpr std::array<ButtonCommand, 8u> mappings{{
        {kDpadUp, ControllerCommand::previous_item},
        {kDpadDown, ControllerCommand::next_item},
        {kA, ControllerCommand::activate},
        {kX, ControllerCommand::pause_resume},
        {kB, ControllerCommand::close_page},
        {kLeftShoulder, ControllerCommand::seek_backward},
        {kRightShoulder, ControllerCommand::seek_forward},
        {kY, ControllerCommand::toggle_presentation},
    }};

    const std::uint16_t pressed = static_cast<std::uint16_t>(
        current_buttons & static_cast<std::uint16_t>(~previous_buttons));
    std::uint32_t commands = 0u;
    for (const ButtonCommand& mapping : mappings) {
        if ((pressed & mapping.button) != 0u) {
            commands |= static_cast<std::uint32_t>(mapping.command);
        }
    }
    return commands;
}

} // namespace pbvp
