#include "pbvp/menu_vtable_validation.hpp"

namespace pbvp {

MenuVtableValidation ValidateMenuVtable(const MenuVtableProfile& profile) noexcept {
    if (!profile.table_readable) {
        return {MenuVtableValidationResult::table_unreadable, kMenuVtableEntryCount};
    }

    for (std::size_t index = 0u; index < kMenuVtableEntryCount; ++index) {
        if (!profile.entry_executable[index]) {
            return {MenuVtableValidationResult::entry_not_executable, index};
        }
    }

    if (!profile.entry_in_main_image[kMenuHandleClickEntry]) {
        return {
            MenuVtableValidationResult::handle_click_occupied,
            kMenuHandleClickEntry};
    }
    if (!profile.entry_in_main_image[kMenuHandleKeyboardEntry] &&
        !profile.handle_keyboard_chain_verified) {
        return {
            MenuVtableValidationResult::handle_keyboard_occupied,
            kMenuHandleKeyboardEntry};
    }

    if (profile.handle_keyboard_chain_verified) {
        return {
            MenuVtableValidationResult::compatible_verified_keyboard_chain,
            kMenuVtableEntryCount};
    }

    return {
        profile.table_in_main_image
            ? MenuVtableValidationResult::compatible_game_table
            : MenuVtableValidationResult::compatible_private_table,
        kMenuVtableEntryCount};
}

bool IsCompatibleMenuVtable(const MenuVtableValidationResult result) noexcept {
    return result == MenuVtableValidationResult::compatible_game_table ||
        result == MenuVtableValidationResult::compatible_private_table ||
        result == MenuVtableValidationResult::compatible_verified_keyboard_chain;
}

bool AcceptPinnedStewieKeyboardChain(
    const StewieKeyboardChainProfile& profile) noexcept {
    return profile.module_name_matches &&
        profile.machine == kStewieImageMachineI386 &&
        profile.timestamp == kStewieImageTimestamp &&
        profile.image_size == kStewieImageSize &&
        profile.handler_rva == kStewieMenuSearchKeyboardRva &&
        profile.entry_bytes_match &&
        profile.forwarding_bytes_match &&
        profile.original_pointer_storage_matches &&
        profile.original_target_in_main_image &&
        profile.original_target_executable;
}

} // namespace pbvp
