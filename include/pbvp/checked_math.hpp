#pragma once

#include <cstddef>
#include <cstdint>

namespace pbvp {

bool CheckedAddSize(std::size_t left, std::size_t right, std::size_t& output) noexcept;
bool CheckedMultiplySize(std::size_t left, std::size_t right, std::size_t& output) noexcept;
bool CheckedAlignSize(std::size_t value, std::size_t alignment, std::size_t& output) noexcept;
bool CheckedUint64ToSize(std::uint64_t value, std::size_t& output) noexcept;

} // namespace pbvp
