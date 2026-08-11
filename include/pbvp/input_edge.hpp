#pragma once

namespace pbvp {

bool ConsumeArmedPress(
    bool down,
    bool& previous_down,
    bool& armed) noexcept;

} // namespace pbvp
