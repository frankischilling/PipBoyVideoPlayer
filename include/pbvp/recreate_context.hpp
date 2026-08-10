#pragma once

#include <cstdint>

namespace pbvp {

inline constexpr std::uint32_t kMinimumRecreateWidth = 320u;
inline constexpr std::uint32_t kMinimumRecreateHeight = 200u;
inline constexpr std::uint32_t kMaximumRecreateExtent = 32768u;

struct RecreateContext final {
    std::uintptr_t renderer{};
    std::uint32_t requested_width{};
    std::uint32_t requested_height{};
    std::uint32_t active_width{};
    std::uint32_t active_height{};
    std::uint32_t backbuffer_width{};
    std::uint32_t backbuffer_height{};
};

enum class RecreateContextResult {
    ready,
    renderer_unavailable,
    request_in_progress,
    active_size_out_of_range,
    backbuffer_size_out_of_range,
    size_mismatch,
};

RecreateContextResult ValidateRecreateContext(const RecreateContext& context) noexcept;
const char* RecreateContextResultName(RecreateContextResult result) noexcept;

} // namespace pbvp
