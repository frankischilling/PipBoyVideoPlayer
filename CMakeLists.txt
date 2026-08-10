cmake_minimum_required(VERSION 3.25)
set(CMAKE_VS_GLOBALS "VcpkgEnabled=false")
project(PipBoyVideoPlayer VERSION 0.1.0 LANGUAGES C CXX RC)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(PBVP_BUILD_PLUGIN "Build the Win32 xNVSE plugin" ON)
option(PBVP_BUILD_TESTS "Build automated tests" ON)
option(PBVP_ENABLE_RECREATE_TEST "Enable the private one-shot engine recreation test" OFF)
set(PBVP_XNVSE_ROOT "${CMAKE_SOURCE_DIR}/external/NVSE-6.4.5" CACHE PATH "xNVSE 6.4.5 release source root")
set(PBVP_MINHOOK_ROOT "${CMAKE_SOURCE_DIR}/external/minhook-1.3.4" CACHE PATH "MinHook 1.3.4 source root")

set(PBVP_RECREATE_TEST_MARKER "${CMAKE_BINARY_DIR}/pbvp-recreate-test-enabled.txt")
if(PBVP_ENABLE_RECREATE_TEST AND PBVP_BUILD_PLUGIN)
    file(WRITE "${PBVP_RECREATE_TEST_MARKER}" "Private one-shot recreation test build.\n")
else()
    file(REMOVE "${PBVP_RECREATE_TEST_MARKER}")
endif()

include(CTest)

add_library(pbvp_core STATIC
    src/core/frame_cadence.cpp
    src/core/recreate_context.cpp
    src/core/recreate_gate.cpp
    src/core/recreate_observer.cpp
    src/core/recreate_result.cpp
    src/core/rect_math.cpp
    src/render/hook_probe.cpp)
target_include_directories(pbvp_core PUBLIC include)
target_compile_features(pbvp_core PUBLIC cxx_std_20)
target_compile_definitions(pbvp_core PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
set_target_properties(pbvp_core PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(MSVC)
    target_compile_options(pbvp_core PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus)
endif()

if(PBVP_BUILD_TESTS)
    find_program(PBVP_POWERSHELL_EXECUTABLE NAMES pwsh powershell REQUIRED)
endif()

if(PBVP_BUILD_PLUGIN)
    if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR "The xNVSE plugin requires a Microsoft Win32/x86 generator. Configure with -A Win32.")
    endif()
    if(NOT EXISTS "${PBVP_XNVSE_ROOT}/nvse/nvse/PluginAPI.h")
        message(FATAL_ERROR "PBVP_XNVSE_ROOT must point to the official xNVSE 6.4.5 release source.")
    endif()

    if(NOT EXISTS "${PBVP_MINHOOK_ROOT}/CMakeLists.txt")
        message(FATAL_ERROR "PBVP_MINHOOK_ROOT must point to the verified MinHook 1.3.4 source.")
    endif()
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(MH_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${PBVP_MINHOOK_ROOT}" "${CMAKE_BINARY_DIR}/minhook" EXCLUDE_FROM_ALL)
    set_target_properties(minhook PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    add_library(PipBoyVideoPlayer SHARED
        src/plugin/plugin_main.cpp
        src/diagnostics/recreate_test.cpp
        src/render/d3d_renderer.cpp
        src/render/hook_manager.cpp
        src/ui/ui_bridge.cpp
        src/diagnostics/logger.cpp
        src/plugin/version.rc
        src/plugin/exports.def)
    target_link_libraries(PipBoyVideoPlayer PRIVATE pbvp_core minhook d3d9)
    target_include_directories(PipBoyVideoPlayer PRIVATE
        include
        "${PBVP_XNVSE_ROOT}"
        "${PBVP_XNVSE_ROOT}/nvse")
    target_compile_features(PipBoyVideoPlayer PRIVATE cxx_std_20)
    target_compile_definitions(PipBoyVideoPlayer PRIVATE
        WIN32 _WINDOWS RUNTIME RUNTIME_VERSION=0x040020D0
        WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS
        PBVP_VERSION_STRING="${PROJECT_VERSION}")
    if(PBVP_ENABLE_RECREATE_TEST)
        message(WARNING "Building the private one-shot engine recreation test")
        target_compile_definitions(PipBoyVideoPlayer PRIVATE PBVP_ENABLE_RECREATE_TEST=1)
    endif()
    target_compile_options(PipBoyVideoPlayer PRIVATE
        /W4 /permissive- /utf-8 /Zc:__cplusplus /guard:cf
        "/FI${CMAKE_SOURCE_DIR}/include/pbvp/xnvse_prefix.hpp")
    target_link_options(PipBoyVideoPlayer PRIVATE
        /DEF:${CMAKE_SOURCE_DIR}/src/plugin/exports.def
        /MACHINE:X86 /DYNAMICBASE /NXCOMPAT /GUARD:CF /DEBUG:FULL
        "/PDBALTPATH:PipBoyVideoPlayer.pdb"
        "$<$<CONFIG:Release>:/PDBSTRIPPED:$<TARGET_PDB_FILE_DIR:PipBoyVideoPlayer>/PipBoyVideoPlayer-public.pdb>")
    set_target_properties(PipBoyVideoPlayer PROPERTIES
        OUTPUT_NAME PipBoyVideoPlayer
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        PDB_NAME PipBoyVideoPlayer)

    if(PBVP_BUILD_TESTS)
        add_executable(pbvp_plugin_abi_test tests/test_plugin_abi.cpp)
        target_compile_features(pbvp_plugin_abi_test PRIVATE cxx_std_20)
        target_compile_options(pbvp_plugin_abi_test PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus)
        set_target_properties(pbvp_plugin_abi_test PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        add_dependencies(pbvp_plugin_abi_test PipBoyVideoPlayer)
        add_test(NAME pbvp_plugin_abi_test COMMAND pbvp_plugin_abi_test $<TARGET_FILE:PipBoyVideoPlayer>)

        add_executable(pbvp_hook_conflict_test tests/test_hook_conflict_win32.cpp)
        target_link_libraries(pbvp_hook_conflict_test PRIVATE pbvp_core minhook)
        target_include_directories(pbvp_hook_conflict_test PRIVATE "${PBVP_MINHOOK_ROOT}/include")
        target_compile_features(pbvp_hook_conflict_test PRIVATE cxx_std_20)
        target_compile_options(pbvp_hook_conflict_test PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus)
        set_target_properties(pbvp_hook_conflict_test PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        add_test(NAME pbvp_hook_conflict_test COMMAND pbvp_hook_conflict_test)

        find_program(PBVP_LLVM_PDBUTIL_EXECUTABLE NAMES llvm-pdbutil REQUIRED)
        add_test(
            NAME pbvp_public_pdb_sanitizer_test
            COMMAND ${PBVP_POWERSHELL_EXECUTABLE}
                -NoProfile
                -ExecutionPolicy Bypass
                -File ${CMAKE_SOURCE_DIR}/tests/test-public-pdb-sanitizer.ps1
                -SanitizerPath ${CMAKE_SOURCE_DIR}/scripts/sanitize-public-pdb.ps1
                -InputPdb $<TARGET_PDB_FILE_DIR:PipBoyVideoPlayer>/PipBoyVideoPlayer-public.pdb
                -BinaryPath $<TARGET_FILE:PipBoyVideoPlayer>
                -LlvmPdbUtil ${PBVP_LLVM_PDBUTIL_EXECUTABLE})
        set_tests_properties(pbvp_public_pdb_sanitizer_test PROPERTIES CONFIGURATIONS Release)
    endif()
endif()

if(PBVP_BUILD_TESTS)
    add_test(
        NAME pbvp_phase1_log_checker_test
        COMMAND ${PBVP_POWERSHELL_EXECUTABLE}
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_SOURCE_DIR}/tests/test-phase1-log-checker.ps1
            -CheckerPath ${CMAKE_SOURCE_DIR}/scripts/check-phase1-log.ps1)

    add_test(
        NAME pbvp_phase1_profile_setup_test
        COMMAND ${PBVP_POWERSHELL_EXECUTABLE}
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_SOURCE_DIR}/tests/test-phase1-profile-setup.ps1
            -ProfileScript ${CMAKE_SOURCE_DIR}/scripts/create-phase1-profiles.ps1)

    add_test(
        NAME pbvp_phase1_test_case_test
        COMMAND ${PBVP_POWERSHELL_EXECUTABLE}
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_SOURCE_DIR}/tests/test-phase1-test-case.ps1
            -TestCaseScript ${CMAKE_SOURCE_DIR}/scripts/set-phase1-test-case.ps1)

    add_test(
        NAME pbvp_data_contract_test
        COMMAND ${CMAKE_COMMAND}
            -DPBVP_SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -P ${CMAKE_SOURCE_DIR}/tests/verify_data_contract.cmake)

    add_executable(pbvp_tests
        tests/frame_cadence_tests.cpp
        tests/hook_probe_tests.cpp
        tests/recreate_context_tests.cpp
        tests/recreate_gate_tests.cpp
        tests/recreate_observer_tests.cpp
        tests/recreate_result_tests.cpp
        tests/rect_math_tests.cpp
        tests/test_main.cpp)
    target_link_libraries(pbvp_tests PRIVATE pbvp_core)
    target_compile_features(pbvp_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(pbvp_tests PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus)
    endif()
    set_target_properties(pbvp_tests PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_test(NAME pbvp_core_tests COMMAND pbvp_tests)
endif()
