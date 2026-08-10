#include "pbvp/recreate_test.hpp"

#include "pbvp/log.hpp"
#include "pbvp/recreate_gate.hpp"
#include "pbvp/recreate_observer.hpp"

#if defined(PBVP_ENABLE_RECREATE_TEST)
#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#endif

namespace pbvp::diagnostics {

#if defined(PBVP_ENABLE_RECREATE_TEST)
namespace {

constexpr std::uintptr_t kDeferredRecreateGateAddress = 0x0086EDF5u;
constexpr std::uintptr_t kDeferredRecreateRequestAddress = 0x011C6FBBu;
constexpr std::uintptr_t kRendererSingletonPointerAddress = 0x011C73B4u;
constexpr std::uintptr_t kConfiguredWidthAddress = 0x011C70E0u;
constexpr std::uintptr_t kConfiguredHeightAddress = 0x011C70E4u;
bool g_attempted = false;
DeferredRequestObserver g_observer;
LARGE_INTEGER g_scheduled_counter{};
LONGLONG g_counter_frequency = 0;

bool IsReadable(const DWORD protection) noexcept {
    const DWORD access = protection & 0xFFu;
    return access == PAGE_READONLY || access == PAGE_READWRITE ||
           access == PAGE_WRITECOPY || access == PAGE_EXECUTE_READ ||
           access == PAGE_EXECUTE_READWRITE || access == PAGE_EXECUTE_WRITECOPY;
}

bool IsWritable(const DWORD protection) noexcept {
    const DWORD access = protection & 0xFFu;
    return access == PAGE_READWRITE || access == PAGE_WRITECOPY ||
           access == PAGE_EXECUTE_READWRITE || access == PAGE_EXECUTE_WRITECOPY;
}

bool RegionContains(
    const MEMORY_BASIC_INFORMATION& information,
    const std::uintptr_t address,
    const std::size_t size) noexcept {
    const auto base = reinterpret_cast<std::uintptr_t>(information.BaseAddress);
    if (information.RegionSize >
        (std::numeric_limits<std::uintptr_t>::max)() - base) {
        return false;
    }
    const auto end = base + information.RegionSize;
    return information.State == MEM_COMMIT &&
           (information.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0u &&
           address >= base && address + size >= address &&
           address + size <= end;
}

template <typename Value>
bool ReadValue(const std::uintptr_t address, Value& value) noexcept {
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(reinterpret_cast<const void*>(address), &information, sizeof(information)) !=
            sizeof(information) ||
        !RegionContains(information, address, sizeof(Value)) || !IsReadable(information.Protect)) {
        return false;
    }
    __try {
        value = *reinterpret_cast<const volatile Value*>(address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

std::uint64_t ElapsedObservationMilliseconds() noexcept {
    if (g_counter_frequency <= 0 || g_scheduled_counter.QuadPart <= 0) {
        return 0u;
    }
    LARGE_INTEGER current{};
    if (!QueryPerformanceCounter(&current) || current.QuadPart < g_scheduled_counter.QuadPart) {
        return 0u;
    }
    const auto elapsed =
        static_cast<std::uint64_t>(current.QuadPart - g_scheduled_counter.QuadPart);
    const auto frequency = static_cast<std::uint64_t>(g_counter_frequency);
    const auto seconds = elapsed / frequency;
    const auto remainder = elapsed % frequency;
    return seconds * 1000u + remainder * 1000u / frequency;
}

} // namespace
#endif

bool ScheduleEngineRecreateTest() noexcept {
#if !defined(PBVP_ENABLE_RECREATE_TEST)
    return false;
#else
    if (g_attempted) {
        return false;
    }
    g_attempted = true;

    MEMORY_BASIC_INFORMATION gate_information{};
    const auto* gate = reinterpret_cast<const std::uint8_t*>(kDeferredRecreateGateAddress);
    if (VirtualQuery(gate, &gate_information, sizeof(gate_information)) !=
            sizeof(gate_information) ||
        !RegionContains(gate_information, kDeferredRecreateGateAddress,
                        kDeferredRecreateGateBytes) ||
        !IsReadable(gate_information.Protect)) {
        PBVP_LOG_ERROR("Deferred recreation gate is unreadable; the test request was not written");
        return false;
    }

    DeferredRecreateGateBytes live_gate{};
    __try {
        for (std::size_t index = 0; index < live_gate.size(); ++index) {
            live_gate[index] = gate[index];
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        PBVP_LOG_ERROR("Deferred recreation gate read failed; the test request was not written");
        return false;
    }
    if (!MatchesDeferredRecreateGate(live_gate)) {
        PBVP_LOG_ERROR(
            "Deferred recreation gate has no reviewed signature; the test request was not written");
        return false;
    }

    std::uintptr_t renderer = 0u;
    std::uint32_t width = 0u;
    std::uint32_t height = 0u;
    if (!ReadValue(kRendererSingletonPointerAddress, renderer) || renderer == 0u ||
        !ReadValue(kConfiguredWidthAddress, width) || width == 0u ||
        !ReadValue(kConfiguredHeightAddress, height) || height == 0u) {
        PBVP_LOG_ERROR("Deferred recreation helper preconditions are unavailable; "
                       "the test request was not written");
        return false;
    }

    LARGE_INTEGER frequency{};
    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0 ||
        !QueryPerformanceCounter(&g_scheduled_counter)) {
        PBVP_LOG_ERROR(
            "Deferred recreation observation timer is unavailable; the test request was not written");
        return false;
    }
    g_counter_frequency = frequency.QuadPart;

    MEMORY_BASIC_INFORMATION request_information{};
    auto* request = reinterpret_cast<volatile std::uint8_t*>(
        kDeferredRecreateRequestAddress);
    if (VirtualQuery(
            reinterpret_cast<const void*>(kDeferredRecreateRequestAddress),
            &request_information,
            sizeof(request_information)) != sizeof(request_information) ||
        !RegionContains(
            request_information, kDeferredRecreateRequestAddress, sizeof(*request)) ||
        !IsWritable(request_information.Protect)) {
        PBVP_LOG_ERROR("Deferred recreation request is not writable; the test was not scheduled");
        return false;
    }

    __try {
        if (*request != 0u) {
            PBVP_LOG_WARN(
                "Deferred recreation request was already pending; the one-shot test was not scheduled");
            return false;
        }
        *request = 1u;
        if (*request != 1u) {
            PBVP_LOG_ERROR(
                "Deferred recreation request readback failed; the test was not scheduled");
            return false;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        PBVP_LOG_ERROR("Deferred recreation request write failed; the test was not scheduled");
        return false;
    }

    g_observer.Arm();

    PBVP_LOG_INFO(
        "Private Phase 1 request context: byte=1 renderer=0x%08X width=%u height=%u",
        static_cast<unsigned>(renderer), width, height);

    PBVP_LOG_WARN(
        "Private Phase 1 diagnostic scheduled one engine-owned recreation request");
    return true;
#endif
}

void ObserveEngineRecreateTest() noexcept {
#if !defined(PBVP_ENABLE_RECREATE_TEST)
    return;
#else
    if (!g_observer.IsActive()) {
        return;
    }

    std::uint8_t value = 0u;
    if (!ReadValue(kDeferredRecreateRequestAddress, value)) {
        g_observer.Cancel();
        PBVP_LOG_ERROR("Deferred recreation request became unreadable during observation");
        return;
    }

    switch (g_observer.Observe(value, ElapsedObservationMilliseconds())) {
        case DeferredRequestObservation::pending:
            PBVP_LOG_INFO(
                "Private Phase 1 diagnostic observed the deferred request still pending");
            break;
        case DeferredRequestObservation::consumed:
            PBVP_LOG_INFO("Private Phase 1 diagnostic observed deferred request consumption");
            break;
        case DeferredRequestObservation::unexpected:
            PBVP_LOG_ERROR(
                "Deferred recreation request changed to unexpected value %u",
                static_cast<unsigned>(value));
            break;
        case DeferredRequestObservation::timed_out:
            PBVP_LOG_ERROR("Deferred recreation request remained pending for five seconds");
            break;
        case DeferredRequestObservation::none:
            break;
    }
#endif
}

} // namespace pbvp::diagnostics
