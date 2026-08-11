#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace pbvp {

inline constexpr std::size_t kMenuVtableEntryCount = 15u;
inline constexpr std::size_t kMenuHandleClickEntry = 3u;
inline constexpr std::size_t kMenuHandleKeyboardEntry = 12u;
inline constexpr std::uintptr_t kStewieMenuSearchKeyboardRva = 0x0003C370u;
inline constexpr std::uintptr_t kStewieOriginalKeyboardPointerRva = 0x000F3728u;
inline constexpr std::uint32_t kStewieImageTimestamp = 0x6949B18Du;
inline constexpr std::uint32_t kStewieImageSize = 0x00106000u;
inline constexpr std::uint16_t kStewieImageMachineI386 = 0x014Cu;

enum class MenuVtableValidationResult {
    compatible_game_table,
    compatible_private_table,
    compatible_verified_keyboard_chain,
    table_unreadable,
    entry_not_executable,
    handle_click_occupied,
    handle_keyboard_occupied,
};

struct MenuVtableProfile final {
    bool table_readable{};
    bool table_in_main_image{};
    bool handle_keyboard_chain_verified{};
    std::array<bool, kMenuVtableEntryCount> entry_executable{};
    std::array<bool, kMenuVtableEntryCount> entry_in_main_image{};
};

struct MenuVtableValidation final {
    MenuVtableValidationResult result{MenuVtableValidationResult::table_unreadable};
    std::size_t rejected_entry{kMenuVtableEntryCount};
};

struct StewieKeyboardChainProfile final {
    bool module_name_matches{};
    std::uint16_t machine{};
    std::uint32_t timestamp{};
    std::uint32_t image_size{};
    std::uintptr_t handler_rva{};
    bool entry_bytes_match{};
    bool forwarding_bytes_match{};
    bool original_pointer_storage_matches{};
    bool original_target_in_main_image{};
    bool original_target_executable{};
};

[[nodiscard]] MenuVtableValidation ValidateMenuVtable(
    const MenuVtableProfile& profile) noexcept;

[[nodiscard]] bool IsCompatibleMenuVtable(
    MenuVtableValidationResult result) noexcept;

[[nodiscard]] bool AcceptPinnedStewieKeyboardChain(
    const StewieKeyboardChainProfile& profile) noexcept;

} // namespace pbvp
