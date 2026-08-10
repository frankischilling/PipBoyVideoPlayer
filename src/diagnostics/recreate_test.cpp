#include "pbvp/recreate_test.hpp"

#include "pbvp/log.hpp"
#include "pbvp/recreate_gate.hpp"

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
bool g_attempted = false;

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
        !RegionContains(
            gate_information, kDeferredRecreateGateAddress, kDeferredRecreateGateBytes) ||
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
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        PBVP_LOG_ERROR("Deferred recreation request write failed; the test was not scheduled");
        return false;
    }

    PBVP_LOG_WARN(
        "Private Phase 1 diagnostic scheduled one engine-owned recreation request");
    return true;
#endif
}

} // namespace pbvp::diagnostics
