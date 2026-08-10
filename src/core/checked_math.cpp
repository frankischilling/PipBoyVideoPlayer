#include "pbvp/checked_math.hpp"

#include <limits>

namespace pbvp {

bool CheckedAddSize(
    const std::size_t left,
    const std::size_t right,
    std::size_t& output) noexcept {
    output = 0u;
    if (left > (std::numeric_limits<std::size_t>::max)() - right) {
        return false;
    }
    output = left + right;
    return true;
}

bool CheckedMultiplySize(
    const std::size_t left,
    const std::size_t right,
    std::size_t& output) noexcept {
    output = 0u;
    if (left != 0u && right > (std::numeric_limits<std::size_t>::max)() / left) {
        return false;
    }
    output = left * right;
    return true;
}

bool CheckedAlignSize(
    const std::size_t value,
    const std::size_t alignment,
    std::size_t& output) noexcept {
    output = 0u;
    if (alignment == 0u) {
        return false;
    }
    const std::size_t remainder = value % alignment;
    if (remainder == 0u) {
        output = value;
        return true;
    }
    return CheckedAddSize(value, alignment - remainder, output);
}

bool CheckedUint64ToSize(const std::uint64_t value, std::size_t& output) noexcept {
    output = 0u;
    if (value > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())) {
        return false;
    }
    output = static_cast<std::size_t>(value);
    return true;
}

} // namespace pbvp
