#pragma once

#include <cstdint>

namespace pbvp::diagnostics {

bool ScheduleEngineRecreateTest(
    std::uint32_t backbuffer_width,
    std::uint32_t backbuffer_height) noexcept;
void ObserveEngineRecreateTest() noexcept;
void CancelEngineRecreateTest() noexcept;

} // namespace pbvp::diagnostics
