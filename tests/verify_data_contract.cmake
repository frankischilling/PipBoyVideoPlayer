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
foreach(required_name IN ITEMS PBVP_Root PBVP_VideoRect)
    string(FIND "${prefab_text}" "name=\"${required_name}\"" match_offset)
    if(match_offset EQUAL -1)
        message(FATAL_ERROR "UI prefab is missing ${required_name}")
    endif()
endforeach()
