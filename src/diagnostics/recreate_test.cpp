#include "pbvp/recreate_test.hpp"

#include "pbvp/log.hpp"
#include "pbvp/recreate_context.hpp"
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
constexpr std::uintptr_t kRequestedWidthAddress = 0x011C70E0u;
constexpr std::uintptr_t kRequestedHeightAddress = 0x011C70E4u;
constexpr std::uintptr_t kActiveWidthAddress = 0x0118947Cu;
constexpr std::uintptr_t kActiveHeightAddress = 0x01189480u;
bool g_attempted = false;
DeferredRequestObserver g_observer;
LARGE_INTEGER g_scheduled_counter{};
LONGLONG g_counter_frequency = 0;
std::uint32_t g_original_requested_width = 0u;
std::uint32_t g_original_requested_height = 0u;
std::uint32_t g_staged_requested_width = 0u;
std::uint32_t g_staged_requested_height = 0u;
bool g_requested_dimensions_staged = false;

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

template <typename Value>
bool CanWriteValue(const std::uintptr_t address) noexcept {
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(reinterpret_cast<const void*>(address), &information, sizeof(information)) !=
            sizeof(information) ||
        !RegionContains(information, address, sizeof(Value)) || !IsWritable(information.Protect)) {
        return false;
    }
    return true;
}

template <typename Value>
bool WriteValue(const std::uintptr_t address, const Value value) noexcept {
    if (!CanWriteValue<Value>(address)) {
        return false;
    }
    __try {
        *reinterpret_cast<volatile Value*>(address) = value;
        return *reinterpret_cast<const volatile Value*>(address) == value;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool RestoreRequestedDimensions() noexcept {
    if (!g_requested_dimensions_staged) {
        return true;
    }

    std::uint32_t live_width = 0u;
    std::uint32_t live_height = 0u;
    if (!ReadValue(kRequestedWidthAddress, live_width) ||
        !ReadValue(kRequestedHeightAddress, live_height)) {
        PBVP_LOG_ERROR("Deferred recreation requested-size restoration could not read the live values");
        return false;
    }
    const bool width_owned = live_width == g_staged_requested_width ||
                             live_width == g_original_requested_width;
    const bool height_owned = live_height == g_staged_requested_height ||
                              live_height == g_original_requested_height;
    if (!width_owned || !height_owned) {
        PBVP_LOG_ERROR(
            "Deferred recreation requested-size restoration refused changed values: width=%u height=%u",
            live_width, live_height);
        g_requested_dimensions_staged = false;
        return false;
    }

    const bool width_restored = live_width == g_original_requested_width ||
                                WriteValue(kRequestedWidthAddress, g_original_requested_width);
    const bool height_restored = live_height == g_original_requested_height ||
                                 WriteValue(kRequestedHeightAddress, g_original_requested_height);
    if (!width_restored || !height_restored) {
        PBVP_LOG_ERROR(
            "Deferred recreation requested-size restoration failed: width=%u height=%u",
            width_restored ? 1u : 0u, height_restored ? 1u : 0u);
        return false;
    }

    g_requested_dimensions_staged = false;
    PBVP_LOG_INFO("Private Phase 1 diagnostic restored the original requested-size values");
    return true;
}

bool CancelOwnedRequest() noexcept {
    std::uint8_t request = 0u;
    if (!ReadValue(kDeferredRecreateRequestAddress, request)) {
        return false;
    }
    if (request == 0u) {
        return true;
    }
    if (request != 1u) {
        return false;
    }
    return WriteValue(kDeferredRecreateRequestAddress, static_cast<std::uint8_t>(0u));
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

bool ScheduleEngineRecreateTest(
    const std::uint32_t backbuffer_width,
    const std::uint32_t backbuffer_height) noexcept {
#if !defined(PBVP_ENABLE_RECREATE_TEST)
    static_cast<void>(backbuffer_width);
    static_cast<void>(backbuffer_height);
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
    std::uint32_t requested_width = 0u;
    std::uint32_t requested_height = 0u;
    std::uint32_t active_width = 0u;
    std::uint32_t active_height = 0u;
    const bool renderer_read = ReadValue(kRendererSingletonPointerAddress, renderer);
    const bool requested_width_read =
        ReadValue(kRequestedWidthAddress, requested_width);
    const bool requested_height_read =
        ReadValue(kRequestedHeightAddress, requested_height);
    const bool active_width_read = ReadValue(kActiveWidthAddress, active_width);
    const bool active_height_read = ReadValue(kActiveHeightAddress, active_height);
    if (!renderer_read || !requested_width_read || !requested_height_read ||
        !active_width_read || !active_height_read) {
        PBVP_LOG_ERROR(
            "Deferred recreation context read failed: renderer-read=%u renderer=0x%08X requested-width-read=%u requested-width=%u requested-height-read=%u requested-height=%u active-width-read=%u active-width=%u active-height-read=%u active-height=%u; the test request was not written",
            renderer_read ? 1u : 0u, static_cast<unsigned>(renderer),
            requested_width_read ? 1u : 0u, requested_width,
            requested_height_read ? 1u : 0u, requested_height,
            active_width_read ? 1u : 0u, active_width,
            active_height_read ? 1u : 0u, active_height);
        return false;
    }

    const RecreateContext context{
        renderer,
        requested_width,
        requested_height,
        active_width,
        active_height,
        backbuffer_width,
        backbuffer_height,
    };
    const RecreateContextResult context_result = ValidateRecreateContext(context);
    if (context_result != RecreateContextResult::ready) {
        PBVP_LOG_ERROR(
            "Deferred recreation context rejected: reason=%s renderer=0x%08X requested=%ux%u active=%ux%u backbuffer=%ux%u; the test request was not written",
            RecreateContextResultName(context_result), static_cast<unsigned>(renderer),
            requested_width, requested_height, active_width, active_height,
            backbuffer_width, backbuffer_height);
        return false;
    }
    if (!CanWriteValue<std::uint32_t>(kRequestedWidthAddress) ||
        !CanWriteValue<std::uint32_t>(kRequestedHeightAddress)) {
        PBVP_LOG_ERROR(
            "Deferred recreation requested-size values are not writable; the test request was not written");
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

    std::uint8_t request = 0u;
    if (!ReadValue(kDeferredRecreateRequestAddress, request) ||
        !CanWriteValue<std::uint8_t>(kDeferredRecreateRequestAddress)) {
        PBVP_LOG_ERROR("Deferred recreation request is not writable; the test was not scheduled");
        return false;
    }
    if (request != 0u) {
        PBVP_LOG_WARN(
            "Deferred recreation request was already pending; the one-shot test was not scheduled");
        return false;
    }

    g_original_requested_width = requested_width;
    g_original_requested_height = requested_height;
    g_staged_requested_width = active_width;
    g_staged_requested_height = active_height;
    g_requested_dimensions_staged = true;
    const bool width_staged = WriteValue(kRequestedWidthAddress, active_width);
    const bool height_staged = WriteValue(kRequestedHeightAddress, active_height);
    if (!width_staged || !height_staged) {
        const bool rolled_back = RestoreRequestedDimensions();
        PBVP_LOG_ERROR(
            "Deferred recreation requested-size staging failed: width=%u height=%u rollback=%u; the test request was not written",
            width_staged ? 1u : 0u, height_staged ? 1u : 0u,
            rolled_back ? 1u : 0u);
        return false;
    }

    if (!WriteValue(kDeferredRecreateRequestAddress, static_cast<std::uint8_t>(1u))) {
        RestoreRequestedDimensions();
        PBVP_LOG_ERROR("Deferred recreation request write failed; the test was not scheduled");
        return false;
    }

    g_observer.Arm();

    PBVP_LOG_INFO(
        "Private Phase 1 request context: byte=1 renderer=0x%08X requested=%ux%u active=%ux%u backbuffer=%ux%u",
        static_cast<unsigned>(renderer), active_width, active_height,
        active_width, active_height, backbuffer_width, backbuffer_height);

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
        RestoreRequestedDimensions();
        PBVP_LOG_ERROR("Deferred recreation request became unreadable during observation");
        return;
    }

    switch (g_observer.Observe(value, ElapsedObservationMilliseconds())) {
        case DeferredRequestObservation::pending:
            PBVP_LOG_INFO(
                "Private Phase 1 diagnostic observed the deferred request still pending");
            break;
        case DeferredRequestObservation::consumed:
            RestoreRequestedDimensions();
            PBVP_LOG_INFO("Private Phase 1 diagnostic observed deferred request consumption");
            break;
        case DeferredRequestObservation::unexpected:
            RestoreRequestedDimensions();
            PBVP_LOG_ERROR(
                "Deferred recreation request changed to unexpected value %u",
                static_cast<unsigned>(value));
            break;
        case DeferredRequestObservation::timed_out:
            if (!CancelOwnedRequest()) {
                PBVP_LOG_ERROR("Deferred recreation timed-out request could not be cancelled");
            }
            RestoreRequestedDimensions();
            PBVP_LOG_ERROR("Deferred recreation request remained pending for five seconds");
            break;
        case DeferredRequestObservation::none:
            break;
    }
#endif
}

void CancelEngineRecreateTest() noexcept {
#if !defined(PBVP_ENABLE_RECREATE_TEST)
    return;
#else
    if (!g_observer.IsActive() && !g_requested_dimensions_staged) {
        return;
    }
    const bool request_cancelled = CancelOwnedRequest();
    g_observer.Cancel();
    const bool dimensions_restored = RestoreRequestedDimensions();
    if (request_cancelled && dimensions_restored) {
        PBVP_LOG_INFO(
            "Private Phase 1 diagnostic cancelled its pending request during shutdown");
    } else {
        PBVP_LOG_ERROR(
            "Private Phase 1 diagnostic shutdown cleanup failed: request=%u dimensions=%u",
            request_cancelled ? 1u : 0u, dimensions_restored ? 1u : 0u);
    }
#endif
}

} // namespace pbvp::diagnostics
