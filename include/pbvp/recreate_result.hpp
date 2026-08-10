#pragma once

#include <cstdint>

namespace pbvp {

enum class RecreateResult {
    failed,
    recovered,
    requested,
    unknown,
};

RecreateResult ClassifyRecreateResult(std::uint32_t value) noexcept;

} // namespace pbvp
