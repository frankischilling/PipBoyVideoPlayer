#pragma once

#include <cstdint>

namespace pbvp {

enum class ControllerCommand : std::uint32_t {
    none = 0u,
    previous_item = 1u << 0u,
    next_item = 1u << 1u,
    activate = 1u << 2u,
    pause_resume = 1u << 3u,
    close_page = 1u << 4u,
    seek_backward = 1u << 5u,
    seek_forward = 1u << 6u,
    toggle_presentation = 1u << 7u,
};

[[nodiscard]] std::uint32_t ControllerCommandsForButtonEdges(
    std::uint16_t current_buttons,
    std::uint16_t previous_buttons) noexcept;

} // namespace pbvp
