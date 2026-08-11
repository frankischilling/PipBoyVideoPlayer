#include "pbvp/input_edge.hpp"

namespace pbvp {

bool ConsumeArmedPress(
    const bool down,
    bool& previous_down,
    bool& armed) noexcept {
    const bool pressed = down && !previous_down;
    previous_down = down;
    if (!down) {
        armed = true;
        return false;
    }
    if (!pressed || !armed) {
        return false;
    }
    armed = false;
    return true;
}

} // namespace pbvp
