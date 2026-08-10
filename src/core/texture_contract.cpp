#include "pbvp/texture_contract.hpp"

namespace pbvp {

bool AcceptEngineVideoTexture(
    const std::uint32_t width,
    const std::uint32_t height,
    const TexturePixelFormat pixel_format,
    const TextureMemoryPool memory_pool) noexcept {
    const bool format_supported =
        pixel_format == TexturePixelFormat::argb8 || pixel_format == TexturePixelFormat::xrgb8;
    return width == 256u && height == 256u && format_supported &&
           memory_pool == TextureMemoryPool::managed;
}

} // namespace pbvp
