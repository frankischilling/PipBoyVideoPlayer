#include "pbvp/controller_input.hpp"

#include "test_support.hpp"

namespace {

bool HasCommand(
    const std::uint32_t commands,
    const pbvp::ControllerCommand command) {
    return (commands & static_cast<std::uint32_t>(command)) != 0u;
}

} // namespace

void RunControllerInputTests() {
    PBVP_CHECK(pbvp::ControllerCommandsForButtonEdges(0u, 0u) == 0u);

    constexpr std::uint16_t dpad_up = 0x0001u;
    constexpr std::uint16_t dpad_down = 0x0002u;
    constexpr std::uint16_t left_shoulder = 0x0100u;
    constexpr std::uint16_t right_shoulder = 0x0200u;
    constexpr std::uint16_t a = 0x1000u;
    constexpr std::uint16_t b = 0x2000u;
    constexpr std::uint16_t x = 0x4000u;
    constexpr std::uint16_t y = 0x8000u;

    PBVP_CHECK(HasCommand(
        pbvp::ControllerCommandsForButtonEdges(dpad_up, 0u),
        pbvp::ControllerCommand::previous_item));
    PBVP_CHECK(HasCommand(
        pbvp::ControllerCommandsForButtonEdges(dpad_down, 0u),
        pbvp::ControllerCommand::next_item));
    PBVP_CHECK(HasCommand(
        pbvp::ControllerCommandsForButtonEdges(a, 0u),
        pbvp::ControllerCommand::activate));
    PBVP_CHECK(HasCommand(
        pbvp::ControllerCommandsForButtonEdges(x, 0u),
        pbvp::ControllerCommand::pause_resume));
    PBVP_CHECK(HasCommand(
        pbvp::ControllerCommandsForButtonEdges(b, 0u),
        pbvp::ControllerCommand::close_page));
    PBVP_CHECK(HasCommand(
        pbvp::ControllerCommandsForButtonEdges(left_shoulder, 0u),
        pbvp::ControllerCommand::seek_backward));
    PBVP_CHECK(HasCommand(
        pbvp::ControllerCommandsForButtonEdges(right_shoulder, 0u),
        pbvp::ControllerCommand::seek_forward));
    PBVP_CHECK(HasCommand(
        pbvp::ControllerCommandsForButtonEdges(y, 0u),
        pbvp::ControllerCommand::toggle_presentation));

    const std::uint16_t simultaneous = static_cast<std::uint16_t>(a | dpad_down | y);
    const std::uint32_t simultaneous_commands =
        pbvp::ControllerCommandsForButtonEdges(simultaneous, 0u);
    PBVP_CHECK(HasCommand(simultaneous_commands, pbvp::ControllerCommand::activate));
    PBVP_CHECK(HasCommand(simultaneous_commands, pbvp::ControllerCommand::next_item));
    PBVP_CHECK(HasCommand(
        simultaneous_commands, pbvp::ControllerCommand::toggle_presentation));

    PBVP_CHECK(pbvp::ControllerCommandsForButtonEdges(a, a) == 0u);
    PBVP_CHECK(pbvp::ControllerCommandsForButtonEdges(0x0010u, 0u) == 0u);
}
