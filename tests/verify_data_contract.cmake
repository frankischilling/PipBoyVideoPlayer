if(NOT DEFINED PBVP_SOURCE_DIR)
    message(FATAL_ERROR "PBVP_SOURCE_DIR is required")
endif()

set(registration "${PBVP_SOURCE_DIR}/data/uio/public/PipBoyVideoPlayer.txt")
set(prefab "${PBVP_SOURCE_DIR}/data/menus/prefabs/PipBoyVideoPlayer/Player.xml")
set(configuration "${PBVP_SOURCE_DIR}/data/Config/PipBoyVideoPlayer.ini")

file(READ "${registration}" registration_text)
string(REPLACE "\r\n" "\n" registration_text "${registration_text}")
string(STRIP "${registration_text}" registration_text)
set(expected_registration
    "PipBoyVideoPlayer\\Player.xml::MapMenu::MM_MainRect\ntrue")
if(NOT registration_text STREQUAL expected_registration)
    message(FATAL_ERROR "UIO registration must contain the target and unconditional condition")
endif()

file(READ "${prefab}" prefab_text)
string(REPLACE "\r\n" "\n" prefab_text "${prefab_text}")
foreach(required_name IN ITEMS
        PBVP_Root
        PBVP_OpenButton
        PBVP_CatalogPanel
        PBVP_VideoRect
        PBVP_VideoSurface
        PBVP_LayerProbeBackground
        PBVP_LayerProbe
        PBVP_BackButton
        PBVP_PauseButton
        PBVP_StopButton
        PBVP_SeekBackButton
        PBVP_SeekForwardButton
        PBVP_PresentationButton)
    string(FIND "${prefab_text}" "name=\"${required_name}\"" match_offset)
    if(match_offset EQUAL -1)
        message(FATAL_ERROR "UI prefab is missing ${required_name}")
    endif()
endforeach()

file(READ "${configuration}" configuration_text)
foreach(required_setting IN ITEMS
        "Enabled=1"
        "Volume=1.0"
        "Muted=0"
        "SeekSeconds=10"
        "AspectMode=Fit"
        "TintMode=PipBoy"
        "MaximumEntries=500"
        "MaximumDisplayCharacters=128"
        "SelectOrPlay=28"
        "PauseResume=57"
        "BackOrStop=1"
        "SeekBackward=203"
        "SeekForward=205"
        "PreviousItem=200"
        "NextItem=208"
        "ToggleColor=20"
        "MaximumSourceWidth=1920"
        "MaximumSourceHeight=1080"
        "MaximumQueuedVideoEdge=512"
        "MaximumMediaFileMiB=32768"
        "Detail=Normal")
    string(FIND "${configuration_text}" "${required_setting}" setting_offset)
    if(setting_offset EQUAL -1)
        message(FATAL_ERROR "Default configuration is missing ${required_setting}")
    endif()
endforeach()

string(FIND "${prefab_text}" "<brightness> 255 </brightness>" brightness_position)
if(brightness_position EQUAL -1)
    message(FATAL_ERROR "Video surface does not expose full brightness for system color")
endif()

set(expected_root_stack
    "<height> <copy src=\"parent()\" trait=\"height\" /> </height>\n    <depth> 10 </depth>\n    <visible> 1 </visible>\n    <target> 0 </target>")
string(FIND "${prefab_text}" "${expected_root_stack}" root_stack_offset)
if(root_stack_offset EQUAL -1)
    message(FATAL_ERROR "UI prefab root must remain above page content and below native controls")
endif()

set(expected_open_button_anchor
    "<hotrect name=\"PBVP_OpenButton\">\n        <id> 9100 </id>\n        <x>\n            <copy src=\"parent\" trait=\"width\" />\n            <sub src=\"me\" trait=\"width\" />\n            <sub> 12 </sub>\n        </x>\n        <y>\n            <copy src=\"parent\" trait=\"height\" />\n            <sub src=\"me\" trait=\"height\" />\n            <sub> 284 </sub>\n        </y>\n        <width> 112 </width>\n        <height> 34 </height>\n        <depth> 11 </depth>\n        <locus> 1 </locus>")
string(FIND "${prefab_text}" "${expected_open_button_anchor}" open_button_anchor_offset)
if(open_button_anchor_offset EQUAL -1)
    message(FATAL_ERROR "UI open button must retain the isolated right-side test anchor")
endif()

foreach(row_index RANGE 0 7)
    string(REGEX MATCH
        "name=\"PBVP_Row${row_index}\">[^\r\n]*<locus> 1 </locus>"
        catalog_row_locus
        "${prefab_text}")
    if(catalog_row_locus STREQUAL "")
        message(FATAL_ERROR "Catalog row ${row_index} must own its child label position")
    endif()
endforeach()

set(expected_catalog_back_locus
    "<hotrect name=\"PBVP_BackButton\">\n            <id> 9101 </id><x> 10 </x><y> 194 </y><width> 100 </width><height> 20 </height><depth> 12 </depth>\n            <locus> 1 </locus>")
string(FIND "${prefab_text}" "${expected_catalog_back_locus}" catalog_back_locus_offset)
if(catalog_back_locus_offset EQUAL -1)
    message(FATAL_ERROR "Catalog Back control must own its child label position")
endif()

set(expected_video_anchor
    "<rect name=\"PBVP_VideoRect\">\n        <x> 12 </x>\n        <y>\n            <copy src=\"parent\" trait=\"height\" />\n            <sub src=\"me\" trait=\"height\" />\n            <sub> 64 </sub>\n        </y>")
string(FIND "${prefab_text}" "${expected_video_anchor}" video_anchor_offset)
if(video_anchor_offset EQUAL -1)
    message(FATAL_ERROR "UI video rectangle must retain the raised lower-left anchor")
endif()

string(FIND "${prefab_text}"
    "<width> 384 </width>\n        <height> 216 </height>\n        <depth> 10 </depth>\n        <locus> 1 </locus>"
    diagnostic_viewport_size_offset)
if(diagnostic_viewport_size_offset EQUAL -1)
    message(FATAL_ERROR "UI diagnostic viewport must retain the reviewed compact size and child locus")
endif()

function(require_drawable_depth drawable_name drawable_depth)
    string(REGEX MATCH
        "name=\"${drawable_name}\">[\r\n\t ]*<visible>[^\r\n]*</visible>[\r\n\t ]*<alpha> [0-9]+ </alpha>[\r\n\t ]*<depth> ${drawable_depth} </depth>"
        drawable_depth_match
        "${prefab_text}")
    if(drawable_depth_match STREQUAL "")
        message(FATAL_ERROR "${drawable_name} must retain its reviewed draw depth")
    endif()
endfunction()

require_drawable_depth(PBVP_VideoSurface 10)
require_drawable_depth(PBVP_LayerProbeBackground 11)
require_drawable_depth(PBVP_LayerProbe 12)

string(FIND "${prefab_text}"
    "<copy src=\"parent\" trait=\"height\" />\n                <sub src=\"me\" trait=\"height\" />\n                <sub> 12 </sub>"
    lower_left_inset_offset)
if(lower_left_inset_offset EQUAL -1)
    message(FATAL_ERROR "UI status background must stay anchored to the lower-left inset")
endif()

string(FIND "${prefab_text}"
    "<copy src=\"sibling(PBVP_LayerProbeBackground)\" trait=\"y\" />"
    probe_text_anchor_offset)
if(probe_text_anchor_offset EQUAL -1)
    message(FATAL_ERROR "UI status text must stay anchored to its background")
endif()

string(FIND "${prefab_text}" "Interface\\PipBoyVideoPlayer\\Surface.dds" surface_path_offset)
if(surface_path_offset EQUAL -1)
    message(FATAL_ERROR "UI prefab does not use the private generated surface texture")
endif()

file(READ "${PBVP_SOURCE_DIR}/scripts/package.ps1" package_text)
string(FIND "${package_text}" "Write-PbvpSurfaceDds" generator_offset)
if(generator_offset EQUAL -1)
    message(FATAL_ERROR "Package script does not generate the private surface texture")
endif()

string(FIND "${package_text}" "sanitize-public-pdb.ps1" symbol_sanitizer_offset)
if(symbol_sanitizer_offset EQUAL -1)
    message(FATAL_ERROR "Package script does not prepare path-neutral public symbols")
endif()

foreach(required_package_text IN ITEMS
        "audit-ffmpeg-runtime.ps1"
        "PipBoyVideoPlayer\\bin"
        "licenseFiles"
        "build-manifest.json"
        "winpthreads")
    string(FIND "${package_text}" "${required_package_text}" package_text_offset)
    if(package_text_offset EQUAL -1)
        message(FATAL_ERROR "Package script is missing ${required_package_text}")
    endif()
endforeach()

file(READ "${PBVP_SOURCE_DIR}/CMakeLists.txt" cmake_text)
string(REGEX MATCH "MinHook|minhook" removed_hook_dependency "${cmake_text}")
if(NOT removed_hook_dependency STREQUAL "")
    message(FATAL_ERROR "The managed texture build must not link the retired hook dependency")
endif()

file(READ "${PBVP_SOURCE_DIR}/src/plugin/plugin_main.cpp" plugin_text)
foreach(required_configuration_path IN ITEMS
        "LoadConfiguration"
        "ReloadConfiguration"
        "SetLayerEnabled")
    string(FIND "${plugin_text}" "${required_configuration_path}" configuration_path_offset)
    if(configuration_path_offset EQUAL -1)
        message(FATAL_ERROR "Plugin lifecycle is missing ${required_configuration_path}")
    endif()
endforeach()
string(FIND "${plugin_text}"
    "xNVSE frame-present presentation path enabled without executable hooks"
    hook_free_log_offset)
if(hook_free_log_offset EQUAL -1)
    message(FATAL_ERROR "Plugin lifecycle does not declare the hook-free presentation path")
endif()

file(READ "${PBVP_SOURCE_DIR}/src/render/d3d_renderer.cpp" renderer_text)
foreach(required_presentation_fragment IN ITEMS
        "ConfigurePresentation"
        "SetPipBoyTintEnabled")
    string(FIND "${plugin_text}${renderer_text}" "${required_presentation_fragment}" fragment_position)
    if(fragment_position EQUAL -1)
        message(FATAL_ERROR "Presentation setting is not connected: ${required_presentation_fragment}")
    endif()
endforeach()
string(FIND "${renderer_text}" "ScaleBgra(" scaler_position)
if(scaler_position EQUAL -1)
    message(FATAL_ERROR "Renderer does not use the configurable BGRA scaler")
endif()
string(FIND "${renderer_text}" "description.Pool == D3DPOOL_MANAGED" managed_pool_offset)
string(FIND "${renderer_text}" "AcceptEngineVideoTexture" texture_contract_offset)
if(managed_pool_offset EQUAL -1 OR texture_contract_offset EQUAL -1)
    message(FATAL_ERROR "Renderer does not enforce the managed engine texture contract")
endif()

foreach(required_button_id IN ITEMS
        9100 9101 9110 9111 9112 9113 9114 9115 9116 9117
        9120 9121 9122 9123 9124)
    string(FIND "${prefab_text}" "<id> ${required_button_id} </id>" button_id_offset)
    if(button_id_offset EQUAL -1)
        message(FATAL_ERROR "UI prefab is missing button ID ${required_button_id}")
    endif()
endforeach()

foreach(required_input_fragment IN ITEMS
        "AttachMapMenuInput"
        "AddressInsideMainImage"
        "VerifyStewieMenuSearchKeyboardChain"
        "ActionForClickedTile"
        "ActionForCursorPosition"
        "ReadEngineCursorPosition"
        "offsetof(InterfaceManagerLayout, cursor_x) == 0x38"
        "offsetof(InterfaceManagerLayout, cursor_y) == 0x40"
        "kTileGetLocusAdjustedPosXAddress = 0x00A013D0u"
        "kTileGetLocusAdjustedPosYAddress = 0x00A01440u"
        "TileGetLocusAdjustedPosition"
        "PollOpenButtonMouse"
        "singleton_vtable"
        "offsetof(NvseInputStateLayout, keys) == 4u"
        "Filtered Videos entry polling active"
        "Scoped MapMenu keyboard actions active"
        "CommandForMenuCharacter"
        "ControllerCommandsForButtonEdges"
        "kMenuKeyBackspace"
        "using MenuHandleKeyboardInput = bool(__thiscall*)(void*, std::uint32_t)"
        "ReadInputState"
        "UiRectContainsPoint"
        "g_videos_page_active"
        "kNVSEData_DIHookControl"
        "SetInputBindings"
        "GetKeyNameTextA"
        "CatalogBackPromptText"
        "LOAD_LIBRARY_SEARCH_SYSTEM32")
    string(FIND "${plugin_text}${renderer_text}" "${required_input_fragment}" input_fragment_offset)
    if(input_fragment_offset EQUAL -1)
        file(READ "${PBVP_SOURCE_DIR}/src/ui/ui_bridge.cpp" ui_bridge_text)
        string(FIND "${ui_bridge_text}" "${required_input_fragment}" input_fragment_offset)
    endif()
    if(input_fragment_offset EQUAL -1)
        file(READ "${PBVP_SOURCE_DIR}/src/core/menu_keyboard.cpp" menu_keyboard_text)
        string(FIND "${menu_keyboard_text}" "${required_input_fragment}" input_fragment_offset)
    endif()
    if(input_fragment_offset EQUAL -1)
        message(FATAL_ERROR "Scoped input bridge is missing ${required_input_fragment}")
    endif()
endforeach()

file(READ "${PBVP_SOURCE_DIR}/src/ui/ui_bridge.cpp" ui_bridge_text)
string(FIND "${ui_bridge_text}" "keyboard probe" keyboard_probe_offset)
if(NOT keyboard_probe_offset EQUAL -1)
    message(FATAL_ERROR "Normal input code must not log menu characters or keyboard state")
endif()
string(FIND "${ui_bridge_text}" "GetAsyncKeyState" global_key_poll_offset)
if(NOT global_key_poll_offset EQUAL -1)
    message(FATAL_ERROR "Videos controls must use xNVSE filtered input instead of global key polling")
endif()

string(FIND "${plugin_text}" "case NVSEMessagingInterface::kMessage_ReloadConfig:" reload_case_offset)
string(FIND "${plugin_text}" "g_videos_page_state = VideosPageState::data_page;\n            break;" reload_state_leak_offset)
if(reload_case_offset EQUAL -1 OR NOT reload_state_leak_offset EQUAL -1)
    message(FATAL_ERROR "Rejected configuration reloads must not change the Videos page state")
endif()

string(FIND "${prefab_text}" "<string> PLAYING </string>" layer_probe_offset)
if(layer_probe_offset EQUAL -1)
    message(FATAL_ERROR "UI prefab is missing the playback status text")
endif()
