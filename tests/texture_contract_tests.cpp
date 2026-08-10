#include "pbvp/texture_contract.hpp"

#include "test_support.hpp"

void RunTextureContractTests() {
    using namespace pbvp;
    PBVP_CHECK(AcceptEngineVideoTexture(
        256u, 256u, TexturePixelFormat::argb8, TextureMemoryPool::managed));
    PBVP_CHECK(AcceptEngineVideoTexture(
        256u, 256u, TexturePixelFormat::xrgb8, TextureMemoryPool::managed));
    PBVP_CHECK(!AcceptEngineVideoTexture(
        255u, 256u, TexturePixelFormat::argb8, TextureMemoryPool::managed));
    PBVP_CHECK(!AcceptEngineVideoTexture(
        256u, 257u, TexturePixelFormat::argb8, TextureMemoryPool::managed));
    PBVP_CHECK(!AcceptEngineVideoTexture(
        256u, 256u, TexturePixelFormat::unsupported, TextureMemoryPool::managed));
    PBVP_CHECK(!AcceptEngineVideoTexture(
        256u, 256u, TexturePixelFormat::argb8, TextureMemoryPool::unsupported));
}
