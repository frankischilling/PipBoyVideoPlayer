#pragma once

#include "pbvp/configuration.hpp"

#include <cstdint>

namespace pbvp {

enum class MenuKeyboardCommand : std::uint32_t {
    none,
    activate,
    pause_resume,
    close_page,
    seek_backward,
    seek_forward,
    previous_item,
    next_item,
    toggle_presentation,
};

constexpr std::uint32_t kMenuKeyBackspace = 0x80000000u;
constexpr std::uint32_t kMenuKeyLeft = 0x80000001u;
constexpr std::uint32_t kMenuKeyRight = 0x80000002u;
constexpr std::uint32_t kMenuKeyUp = 0x80000003u;
constexpr std::uint32_t kMenuKeyDown = 0x80000004u;
constexpr std::uint32_t kMenuKeyEnter = 0x80000008u;

[[nodiscard]] MenuKeyboardCommand CommandForMenuCharacter(
    std::uint32_t input_character,
    const InputSettings& settings) noexcept;

} // namespace pbvp
