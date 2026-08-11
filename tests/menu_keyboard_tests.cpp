#include "pbvp/menu_keyboard.hpp"

#include "test_support.hpp"

void RunMenuKeyboardTests() {
    const pbvp::InputSettings defaults{};
    PBVP_CHECK(pbvp::CommandForMenuCharacter(pbvp::kMenuKeyEnter, defaults) ==
        pbvp::MenuKeyboardCommand::activate);
    PBVP_CHECK(pbvp::CommandForMenuCharacter(pbvp::kMenuKeyUp, defaults) ==
        pbvp::MenuKeyboardCommand::previous_item);
    PBVP_CHECK(pbvp::CommandForMenuCharacter(pbvp::kMenuKeyDown, defaults) ==
        pbvp::MenuKeyboardCommand::next_item);
    PBVP_CHECK(pbvp::CommandForMenuCharacter(pbvp::kMenuKeyLeft, defaults) ==
        pbvp::MenuKeyboardCommand::seek_backward);
    PBVP_CHECK(pbvp::CommandForMenuCharacter(pbvp::kMenuKeyRight, defaults) ==
        pbvp::MenuKeyboardCommand::seek_forward);
    PBVP_CHECK(pbvp::CommandForMenuCharacter(pbvp::kMenuKeyBackspace, defaults) ==
        pbvp::MenuKeyboardCommand::close_page);
    PBVP_CHECK(pbvp::CommandForMenuCharacter(27u, defaults) ==
        pbvp::MenuKeyboardCommand::close_page);
    PBVP_CHECK(pbvp::CommandForMenuCharacter(32u, defaults) ==
        pbvp::MenuKeyboardCommand::pause_resume);
    PBVP_CHECK(pbvp::CommandForMenuCharacter(
        static_cast<std::uint8_t>('t'), defaults) ==
        pbvp::MenuKeyboardCommand::toggle_presentation);
    PBVP_CHECK(pbvp::CommandForMenuCharacter(
        static_cast<std::uint8_t>('T'), defaults) ==
        pbvp::MenuKeyboardCommand::toggle_presentation);
    PBVP_CHECK(pbvp::CommandForMenuCharacter(
        static_cast<std::uint8_t>('?'), defaults) ==
        pbvp::MenuKeyboardCommand::none);

    pbvp::InputSettings custom{};
    custom.select_or_play = 0x12u;
    custom.pause_resume = 0x19u;
    custom.back_or_stop = 0x10u;
    custom.seek_backward = 0x1Eu;
    custom.seek_forward = 0x20u;
    custom.previous_item = 0x11u;
    custom.next_item = 0x1Fu;
    custom.toggle_color = 0x2Eu;
    PBVP_CHECK(pbvp::CommandForMenuCharacter(
        static_cast<std::uint8_t>('e'), custom) ==
        pbvp::MenuKeyboardCommand::activate);
    PBVP_CHECK(pbvp::CommandForMenuCharacter(
        static_cast<std::uint8_t>('W'), custom) ==
        pbvp::MenuKeyboardCommand::previous_item);
    PBVP_CHECK(pbvp::CommandForMenuCharacter(
        static_cast<std::uint8_t>('s'), custom) ==
        pbvp::MenuKeyboardCommand::next_item);
    PBVP_CHECK(pbvp::CommandForMenuCharacter(pbvp::kMenuKeyBackspace, custom) ==
        pbvp::MenuKeyboardCommand::close_page);

    PBVP_CHECK(pbvp::CommandForMenuCharacter(0u, defaults) ==
        pbvp::MenuKeyboardCommand::none);
    PBVP_CHECK(pbvp::CommandForMenuCharacter(1u, defaults) ==
        pbvp::MenuKeyboardCommand::none);
    PBVP_CHECK(pbvp::CommandForMenuCharacter(8u, defaults) ==
        pbvp::MenuKeyboardCommand::none);
}
