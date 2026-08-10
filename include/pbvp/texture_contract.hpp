#pragma once

#include <cstdint>

namespace pbvp {

enum class TexturePixelFormat : std::uint8_t {
    unsupported,
    argb8,
    xrgb8,
};

enum class TextureMemoryPool : std::uint8_t {
    unsupported,
    managed,
};

bool AcceptEngineVideoTexture(
    std::uint32_t width,
    std::uint32_t height,
    TexturePixelFormat pixel_format,
    TextureMemoryPool memory_pool) noexcept;

} // namespace pbvp
