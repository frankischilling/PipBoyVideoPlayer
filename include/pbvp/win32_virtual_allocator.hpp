#pragma once

#include <cstddef>
#include <limits>
#include <new>
#include <type_traits>

namespace pbvp {

[[nodiscard]] void* AllocateVirtualPages(std::size_t bytes) noexcept;
void FreeVirtualPages(void* allocation) noexcept;

template <typename T>
class Win32VirtualAllocator final {
public:
    using value_type = T;
    using is_always_equal = std::true_type;

    Win32VirtualAllocator() noexcept = default;

    template <typename U>
    Win32VirtualAllocator(const Win32VirtualAllocator<U>&) noexcept {}

    [[nodiscard]] T* allocate(const std::size_t count) {
        if (count > (std::numeric_limits<std::size_t>::max)() / sizeof(T)) {
            throw std::bad_array_new_length{};
        }
        if (count == 0u) {
            return nullptr;
        }
        void* allocation = AllocateVirtualPages(count * sizeof(T));
        if (allocation == nullptr) {
            throw std::bad_alloc{};
        }
        return static_cast<T*>(allocation);
    }

    void deallocate(T* allocation, std::size_t) noexcept {
        FreeVirtualPages(allocation);
    }
};

template <typename T, typename U>
bool operator==(
    const Win32VirtualAllocator<T>&,
    const Win32VirtualAllocator<U>&) noexcept {
    return true;
}

template <typename T, typename U>
bool operator!=(
    const Win32VirtualAllocator<T>&,
    const Win32VirtualAllocator<U>&) noexcept {
    return false;
}

} // namespace pbvp
