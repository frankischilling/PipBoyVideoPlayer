#include "pbvp/menu_vtable_validation.hpp"

#include "test_support.hpp"

namespace {

pbvp::MenuVtableProfile CompatibleProfile(const bool table_in_main_image) {
    pbvp::MenuVtableProfile profile{};
    profile.table_readable = true;
    profile.table_in_main_image = table_in_main_image;
    profile.entry_executable.fill(true);
    profile.entry_in_main_image.fill(true);
    return profile;
}

pbvp::StewieKeyboardChainProfile CompatibleStewieProfile() {
    return {
        true,
        pbvp::kStewieImageMachineI386,
        pbvp::kStewieImageTimestamp,
        pbvp::kStewieImageSize,
        pbvp::kStewieMenuSearchKeyboardRva,
        true,
        true,
        true,
        true,
        true};
}

} // namespace

void RunMenuVtableValidationTests() {
    {
        const auto validation = pbvp::ValidateMenuVtable(CompatibleProfile(true));
        PBVP_CHECK(validation.result ==
            pbvp::MenuVtableValidationResult::compatible_game_table);
        PBVP_CHECK(pbvp::IsCompatibleMenuVtable(validation.result));
    }
    {
        auto profile = CompatibleProfile(false);
        profile.entry_in_main_image[5u] = false;
        const auto validation = pbvp::ValidateMenuVtable(profile);
        PBVP_CHECK(validation.result ==
            pbvp::MenuVtableValidationResult::compatible_private_table);
        PBVP_CHECK(pbvp::IsCompatibleMenuVtable(validation.result));
    }
    {
        auto profile = CompatibleProfile(false);
        profile.table_readable = false;
        const auto validation = pbvp::ValidateMenuVtable(profile);
        PBVP_CHECK(validation.result ==
            pbvp::MenuVtableValidationResult::table_unreadable);
        PBVP_CHECK(!pbvp::IsCompatibleMenuVtable(validation.result));
    }
    {
        auto profile = CompatibleProfile(false);
        profile.entry_executable[7u] = false;
        const auto validation = pbvp::ValidateMenuVtable(profile);
        PBVP_CHECK(validation.result ==
            pbvp::MenuVtableValidationResult::entry_not_executable);
        PBVP_CHECK(validation.rejected_entry == 7u);
    }
    {
        auto profile = CompatibleProfile(false);
        profile.entry_in_main_image[pbvp::kMenuHandleClickEntry] = false;
        const auto validation = pbvp::ValidateMenuVtable(profile);
        PBVP_CHECK(validation.result ==
            pbvp::MenuVtableValidationResult::handle_click_occupied);
        PBVP_CHECK(validation.rejected_entry == pbvp::kMenuHandleClickEntry);
    }
    {
        PBVP_CHECK(pbvp::AcceptPinnedStewieKeyboardChain(
            CompatibleStewieProfile()));
    }
    {
        auto profile = CompatibleStewieProfile();
        profile.timestamp += 1u;
        PBVP_CHECK(!pbvp::AcceptPinnedStewieKeyboardChain(profile));
        profile = CompatibleStewieProfile();
        profile.handler_rva += 1u;
        PBVP_CHECK(!pbvp::AcceptPinnedStewieKeyboardChain(profile));
        profile = CompatibleStewieProfile();
        profile.entry_bytes_match = false;
        PBVP_CHECK(!pbvp::AcceptPinnedStewieKeyboardChain(profile));
        profile = CompatibleStewieProfile();
        profile.forwarding_bytes_match = false;
        PBVP_CHECK(!pbvp::AcceptPinnedStewieKeyboardChain(profile));
        profile = CompatibleStewieProfile();
        profile.original_pointer_storage_matches = false;
        PBVP_CHECK(!pbvp::AcceptPinnedStewieKeyboardChain(profile));
        profile = CompatibleStewieProfile();
        profile.original_target_in_main_image = false;
        PBVP_CHECK(!pbvp::AcceptPinnedStewieKeyboardChain(profile));
        profile = CompatibleStewieProfile();
        profile.original_target_executable = false;
        PBVP_CHECK(!pbvp::AcceptPinnedStewieKeyboardChain(profile));
    }
    {
        auto profile = CompatibleProfile(false);
        profile.entry_in_main_image[pbvp::kMenuHandleKeyboardEntry] = false;
        const auto validation = pbvp::ValidateMenuVtable(profile);
        PBVP_CHECK(validation.result ==
            pbvp::MenuVtableValidationResult::handle_keyboard_occupied);
        PBVP_CHECK(validation.rejected_entry == pbvp::kMenuHandleKeyboardEntry);
    }
    {
        auto profile = CompatibleProfile(false);
        profile.entry_in_main_image[pbvp::kMenuHandleKeyboardEntry] = false;
        profile.handle_keyboard_chain_verified = true;
        const auto validation = pbvp::ValidateMenuVtable(profile);
        PBVP_CHECK(validation.result ==
            pbvp::MenuVtableValidationResult::compatible_verified_keyboard_chain);
        PBVP_CHECK(pbvp::IsCompatibleMenuVtable(validation.result));
    }
    {
        auto profile = CompatibleProfile(false);
        profile.entry_in_main_image[pbvp::kMenuHandleClickEntry] = false;
        profile.entry_in_main_image[pbvp::kMenuHandleKeyboardEntry] = false;
        profile.handle_keyboard_chain_verified = true;
        const auto validation = pbvp::ValidateMenuVtable(profile);
        PBVP_CHECK(validation.result ==
            pbvp::MenuVtableValidationResult::handle_click_occupied);
    }
    {
        auto profile = CompatibleProfile(false);
        profile.entry_executable[pbvp::kMenuHandleClickEntry] = false;
        profile.entry_in_main_image[pbvp::kMenuHandleClickEntry] = false;
        const auto validation = pbvp::ValidateMenuVtable(profile);
        PBVP_CHECK(validation.result ==
            pbvp::MenuVtableValidationResult::entry_not_executable);
        PBVP_CHECK(validation.rejected_entry == pbvp::kMenuHandleClickEntry);
    }
}
