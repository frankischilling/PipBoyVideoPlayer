if(NOT DEFINED PBVP_SOURCE_DIR)
    message(FATAL_ERROR "PBVP_SOURCE_DIR is required")
endif()

set(registration "${PBVP_SOURCE_DIR}/data/uio/public/PipBoyVideoPlayer.txt")
set(prefab "${PBVP_SOURCE_DIR}/data/menus/prefabs/PipBoyVideoPlayer/Player.xml")

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
        PBVP_VideoRect
        PBVP_VideoSurface
        PBVP_LayerProbeBackground
        PBVP_LayerProbe)
    string(FIND "${prefab_text}" "name=\"${required_name}\"" match_offset)
    if(match_offset EQUAL -1)
        message(FATAL_ERROR "UI prefab is missing ${required_name}")
    endif()
endforeach()

set(expected_root_stack
    "<height> <copy src=\"parent()\" trait=\"height\" /> </height>\n    <depth> 10 </depth>\n    <visible> 1 </visible>\n    <target> 0 </target>")
string(FIND "${prefab_text}" "${expected_root_stack}" root_stack_offset)
if(root_stack_offset EQUAL -1)
    message(FATAL_ERROR "UI prefab root must remain above page content and below native controls")
endif()

set(expected_video_anchor
    "<rect name=\"PBVP_VideoRect\">\n        <x> 12 </x>\n        <y>\n            <copy src=\"parent\" trait=\"height\" />\n            <sub src=\"me\" trait=\"height\" />\n            <sub> 12 </sub>\n        </y>")
string(FIND "${prefab_text}" "${expected_video_anchor}" video_anchor_offset)
if(video_anchor_offset EQUAL -1)
    message(FATAL_ERROR "UI video rectangle must stay anchored to the lower-left inset")
endif()

string(FIND "${prefab_text}"
    "<width> 320 </width>\n        <height> 180 </height>"
    diagnostic_viewport_size_offset)
if(diagnostic_viewport_size_offset EQUAL -1)
    message(FATAL_ERROR "UI diagnostic viewport must retain the reviewed compact size")
endif()

function(require_drawable_depth drawable_name drawable_depth)
    string(REGEX MATCH
        "name=\"${drawable_name}\">[\r\n\t ]*<visible> 1 </visible>[\r\n\t ]*<alpha> [0-9]+ </alpha>[\r\n\t ]*<depth> ${drawable_depth} </depth>"
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

string(FIND "${prefab_text}" "PBVP UI LAYER" layer_probe_offset)
if(layer_probe_offset EQUAL -1)
    message(FATAL_ERROR "UI prefab is missing the visible layer-probe text")
endif()
