#include "pbvp/win32_virtual_allocator.hpp"

#include <Windows.h>

namespace pbvp {

void* AllocateVirtualPages(const std::size_t bytes) noexcept {
    if (bytes == 0u) {
        return nullptr;
    }
    return VirtualAlloc(
        nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void FreeVirtualPages(void* allocation) noexcept {
    if (allocation != nullptr) {
        VirtualFree(allocation, 0u, MEM_RELEASE);
    }
}

} // namespace pbvp
