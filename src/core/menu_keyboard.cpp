#include "pbvp/menu_keyboard.hpp"

#include <array>
#include <cstddef>

namespace pbvp {
namespace {

constexpr std::uint32_t kNoMenuCharacter = 0xFFFFFFFFu;

struct ScanCharacter final {
    std::uint8_t scan_code;
    std::uint32_t menu_character;
};

constexpr std::array<ScanCharacter, 51u> kScanCharacters{{
    {0x01u, 27u},
    {0x02u, static_cast<std::uint8_t>('1')},
    {0x03u, static_cast<std::uint8_t>('2')},
    {0x04u, static_cast<std::uint8_t>('3')},
    {0x05u, static_cast<std::uint8_t>('4')},
    {0x06u, static_cast<std::uint8_t>('5')},
    {0x07u, static_cast<std::uint8_t>('6')},
    {0x08u, static_cast<std::uint8_t>('7')},
    {0x09u, static_cast<std::uint8_t>('8')},
    {0x0Au, static_cast<std::uint8_t>('9')},
    {0x0Bu, static_cast<std::uint8_t>('0')},
    {0x0Cu, static_cast<std::uint8_t>('-')},
    {0x0Du, static_cast<std::uint8_t>('=')},
    {0x0Eu, kMenuKeyBackspace},
    {0x0Fu, 9u},
    {0x10u, static_cast<std::uint8_t>('q')},
    {0x11u, static_cast<std::uint8_t>('w')},
    {0x12u, static_cast<std::uint8_t>('e')},
    {0x13u, static_cast<std::uint8_t>('r')},
    {0x14u, static_cast<std::uint8_t>('t')},
    {0x15u, static_cast<std::uint8_t>('y')},
    {0x16u, static_cast<std::uint8_t>('u')},
    {0x17u, static_cast<std::uint8_t>('i')},
    {0x18u, static_cast<std::uint8_t>('o')},
    {0x19u, static_cast<std::uint8_t>('p')},
    {0x1Au, static_cast<std::uint8_t>('[')},
    {0x1Bu, static_cast<std::uint8_t>(']')},
    {0x1Cu, kMenuKeyEnter},
    {0x1Eu, static_cast<std::uint8_t>('a')},
    {0x1Fu, static_cast<std::uint8_t>('s')},
    {0x20u, static_cast<std::uint8_t>('d')},
    {0x21u, static_cast<std::uint8_t>('f')},
    {0x22u, static_cast<std::uint8_t>('g')},
    {0x23u, static_cast<std::uint8_t>('h')},
    {0x24u, static_cast<std::uint8_t>('j')},
    {0x25u, static_cast<std::uint8_t>('k')},
    {0x26u, static_cast<std::uint8_t>('l')},
    {0x27u, static_cast<std::uint8_t>(';')},
    {0x28u, static_cast<std::uint8_t>('\'')},
    {0x29u, static_cast<std::uint8_t>('`')},
    {0x2Bu, static_cast<std::uint8_t>('\\')},
    {0x2Cu, static_cast<std::uint8_t>('z')},
    {0x2Du, static_cast<std::uint8_t>('x')},
    {0x2Eu, static_cast<std::uint8_t>('c')},
    {0x2Fu, static_cast<std::uint8_t>('v')},
    {0x30u, static_cast<std::uint8_t>('b')},
    {0x31u, static_cast<std::uint8_t>('n')},
    {0x32u, static_cast<std::uint8_t>('m')},
    {0x33u, static_cast<std::uint8_t>(',')},
    {0x34u, static_cast<std::uint8_t>('.')},
    {0x35u, static_cast<std::uint8_t>('/')},
}};

std::uint32_t MenuCharacterForScanCode(const std::uint32_t scan_code) noexcept {
    switch (scan_code) {
        case 0x39u: return 32u;
        case 0x9Cu: return kMenuKeyEnter;
        case 0xC8u: return kMenuKeyUp;
        case 0xD0u: return kMenuKeyDown;
        case 0xCBu: return kMenuKeyLeft;
        case 0xCDu: return kMenuKeyRight;
        default: break;
    }
    for (const ScanCharacter& entry : kScanCharacters) {
        if (entry.scan_code == scan_code) {
            return entry.menu_character;
        }
    }
    return kNoMenuCharacter;
}

bool CharacterMatches(
    const std::uint32_t input_character,
    const std::uint32_t scan_code) noexcept {
    const std::uint32_t expected = MenuCharacterForScanCode(scan_code);
    if (expected == input_character) {
        return true;
    }
    return input_character >= static_cast<std::uint8_t>('A') &&
        input_character <= static_cast<std::uint8_t>('Z') &&
        expected == input_character + static_cast<std::uint32_t>('a' - 'A');
}

} // namespace

MenuKeyboardCommand CommandForMenuCharacter(
    const std::uint32_t input_character,
    const InputSettings& settings) noexcept {
    if (input_character == kMenuKeyBackspace) {
        return MenuKeyboardCommand::close_page;
    }

    struct Binding final {
        std::uint32_t scan_code;
        MenuKeyboardCommand command;
    };
    const std::array<Binding, 8u> bindings{{
        {settings.select_or_play, MenuKeyboardCommand::activate},
        {settings.pause_resume, MenuKeyboardCommand::pause_resume},
        {settings.back_or_stop, MenuKeyboardCommand::close_page},
        {settings.seek_backward, MenuKeyboardCommand::seek_backward},
        {settings.seek_forward, MenuKeyboardCommand::seek_forward},
        {settings.previous_item, MenuKeyboardCommand::previous_item},
        {settings.next_item, MenuKeyboardCommand::next_item},
        {settings.toggle_color, MenuKeyboardCommand::toggle_presentation},
    }};
    for (const Binding& binding : bindings) {
        if (CharacterMatches(input_character, binding.scan_code)) {
            return binding.command;
        }
    }
    return MenuKeyboardCommand::none;
}

} // namespace pbvp
