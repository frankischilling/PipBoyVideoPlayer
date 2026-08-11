cmake_minimum_required(VERSION 3.25)
set(CMAKE_VS_GLOBALS "VcpkgEnabled=false")
project(PipBoyVideoPlayer VERSION 0.1.0 LANGUAGES C CXX RC)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(PBVP_BUILD_PLUGIN "Build the Win32 xNVSE plugin" ON)
option(PBVP_BUILD_TESTS "Build automated tests" ON)
option(PBVP_ENABLE_MEDIA_SMOKE_TEST "Arm the private live MO2 media decoder diagnostic" OFF)
option(PBVP_ENABLE_AUDIO_SMOKE_TEST "Arm the private live MO2 decoded-audio diagnostic" OFF)
option(PBVP_ENABLE_PLAYBACK_SMOKE_TEST "Arm the private live integrated playback diagnostic" OFF)
option(PBVP_ENABLE_PLAYBACK_LONG_TEST "Arm the private 30-minute integrated playback diagnostic" OFF)
if(PBVP_ENABLE_PLAYBACK_SMOKE_TEST AND PBVP_ENABLE_PLAYBACK_LONG_TEST)
    message(FATAL_ERROR "Only one private playback diagnostic may be armed at a time.")
endif()
set(PBVP_XNVSE_ROOT "${CMAKE_SOURCE_DIR}/external/NVSE-6.4.5" CACHE PATH "xNVSE 6.4.5 release source root")
set(PBVP_FFMPEG_ROOT "${CMAKE_SOURCE_DIR}/external/ffmpeg-8.1.2-i686" CACHE PATH "Verified FFmpeg 8.1.2 x86 install root")

include(CTest)

add_library(pbvp_core STATIC
    src/core/checked_math.cpp
    src/core/configuration.cpp
    src/core/controller_input.cpp
    src/core/frame_cadence.cpp
    src/core/input_edge.cpp
    src/core/input_prompts.cpp
    src/core/media_catalog.cpp
    src/core/media_limits.cpp
    src/core/menu_keyboard.cpp
    src/core/menu_vtable_validation.cpp
    src/core/playback_clock.cpp
    src/core/playback_state.cpp
    src/core/rect_math.cpp
    src/core/texture_contract.cpp
    src/core/video_scaler.cpp
    src/core/video_scheduler.cpp)
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
    if(NOT EXISTS "${PBVP_FFMPEG_ROOT}/include/libavformat/avformat.h" OR
       NOT EXISTS "${PBVP_FFMPEG_ROOT}/bin/avcodec-62.dll")
        message(FATAL_ERROR "PBVP_FFMPEG_ROOT must point to the verified FFmpeg 8.1.2 x86 build. Run scripts/build-ffmpeg.ps1.")
    endif()

    add_library(pbvp_audio STATIC
        src/audio/win32_playback_clock.cpp
        src/audio/xaudio_stream.cpp)
    target_link_libraries(pbvp_audio PUBLIC pbvp_core xaudio2)
    target_include_directories(pbvp_audio PUBLIC include)
    target_compile_features(pbvp_audio PUBLIC cxx_std_20)
    target_compile_definitions(pbvp_audio PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(pbvp_audio PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus)
    set_target_properties(pbvp_audio PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    add_library(pbvp_media STATIC
        src/media/ffmpeg_runtime.cpp
        src/media/media_decoder.cpp
        src/media/win32_avio.cpp
        src/media/win32_virtual_allocator.cpp)
    target_include_directories(pbvp_media PUBLIC include)
    target_include_directories(pbvp_media SYSTEM PUBLIC "${PBVP_FFMPEG_ROOT}/include")
    target_compile_features(pbvp_media PUBLIC cxx_std_20)
    target_compile_definitions(pbvp_media PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(pbvp_media PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus)
    set_target_properties(pbvp_media PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    add_library(pbvp_playback STATIC
        src/playback/playback_controller.cpp)
    target_link_libraries(pbvp_playback PUBLIC pbvp_audio pbvp_core pbvp_media)
    target_include_directories(pbvp_playback PUBLIC include)
    target_compile_features(pbvp_playback PUBLIC cxx_std_20)
    target_compile_definitions(pbvp_playback PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(pbvp_playback PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus)
    set_target_properties(pbvp_playback PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    add_library(PipBoyVideoPlayer SHARED
        src/plugin/plugin_main.cpp
        src/render/d3d_renderer.cpp
        src/ui/ui_bridge.cpp
        src/diagnostics/logger.cpp
        src/plugin/version.rc
        src/plugin/exports.def)
    target_link_libraries(PipBoyVideoPlayer PRIVATE pbvp_audio pbvp_core pbvp_media pbvp_playback d3d9 psapi)
    target_include_directories(PipBoyVideoPlayer PRIVATE
        include
        "${PBVP_XNVSE_ROOT}"
        "${PBVP_XNVSE_ROOT}/nvse")
    target_compile_features(PipBoyVideoPlayer PRIVATE cxx_std_20)
    target_compile_definitions(PipBoyVideoPlayer PRIVATE
        WIN32 _WINDOWS RUNTIME RUNTIME_VERSION=0x040020D0
        WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS
        PBVP_VERSION_STRING="${PROJECT_VERSION}")
    if(PBVP_ENABLE_MEDIA_SMOKE_TEST)
        target_compile_definitions(PipBoyVideoPlayer PRIVATE PBVP_ENABLE_MEDIA_SMOKE_TEST=1)
    endif()
    if(PBVP_ENABLE_AUDIO_SMOKE_TEST)
        target_sources(PipBoyVideoPlayer PRIVATE src/diagnostics/audio_smoke_test.cpp)
        target_compile_definitions(PipBoyVideoPlayer PRIVATE PBVP_ENABLE_AUDIO_SMOKE_TEST=1)
    endif()
    if(PBVP_ENABLE_PLAYBACK_SMOKE_TEST)
        target_compile_definitions(PipBoyVideoPlayer PRIVATE PBVP_ENABLE_PLAYBACK_SMOKE_TEST=1)
    endif()
    if(PBVP_ENABLE_PLAYBACK_LONG_TEST)
        target_compile_definitions(PipBoyVideoPlayer PRIVATE PBVP_ENABLE_PLAYBACK_LONG_TEST=1)
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

        add_executable(pbvp_ffmpeg_loader_test tests/ffmpeg_runtime_tests.cpp)
        target_link_libraries(pbvp_ffmpeg_loader_test PRIVATE pbvp_media)
        target_compile_features(pbvp_ffmpeg_loader_test PRIVATE cxx_std_20)
        target_compile_options(pbvp_ffmpeg_loader_test PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus)
        set_target_properties(pbvp_ffmpeg_loader_test PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        add_test(NAME pbvp_ffmpeg_loader_test COMMAND pbvp_ffmpeg_loader_test "${PBVP_FFMPEG_ROOT}/bin")

        add_executable(pbvp_win32_avio_test tests/win32_avio_tests.cpp)
        target_link_libraries(pbvp_win32_avio_test PRIVATE pbvp_media)
        target_include_directories(pbvp_win32_avio_test SYSTEM PRIVATE "${PBVP_FFMPEG_ROOT}/include")
        target_compile_features(pbvp_win32_avio_test PRIVATE cxx_std_20)
        target_compile_options(pbvp_win32_avio_test PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus)
        set_target_properties(pbvp_win32_avio_test PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        add_test(NAME pbvp_win32_avio_test COMMAND pbvp_win32_avio_test "${PBVP_FFMPEG_ROOT}/bin")

        add_executable(pbvp_media_decoder_test tests/media_decoder_tests.cpp)
        target_link_libraries(pbvp_media_decoder_test PRIVATE pbvp_media pbvp_core psapi)
        target_compile_features(pbvp_media_decoder_test PRIVATE cxx_std_20)
        target_compile_options(pbvp_media_decoder_test PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus /Zm200)
        set_target_properties(pbvp_media_decoder_test PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        add_test(
            NAME pbvp_media_decoder_test
            COMMAND pbvp_media_decoder_test
                "${PBVP_FFMPEG_ROOT}/bin"
                "${CMAKE_SOURCE_DIR}/tests/fixtures")

        add_executable(pbvp_xaudio_stream_test tests/xaudio_stream_tests.cpp)
        target_link_libraries(pbvp_xaudio_stream_test PRIVATE pbvp_audio)
        target_compile_features(pbvp_xaudio_stream_test PRIVATE cxx_std_20)
        target_compile_options(pbvp_xaudio_stream_test PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus)
        set_target_properties(pbvp_xaudio_stream_test PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        add_test(NAME pbvp_xaudio_stream_test COMMAND pbvp_xaudio_stream_test)

        add_executable(pbvp_media_audio_pipeline_test tests/media_audio_pipeline_tests.cpp)
        target_link_libraries(pbvp_media_audio_pipeline_test PRIVATE pbvp_audio pbvp_media)
        target_compile_features(pbvp_media_audio_pipeline_test PRIVATE cxx_std_20)
        target_compile_options(pbvp_media_audio_pipeline_test PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus /Zm200)
        set_target_properties(pbvp_media_audio_pipeline_test PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        add_test(
            NAME pbvp_media_audio_pipeline_test
            COMMAND pbvp_media_audio_pipeline_test
                "${PBVP_FFMPEG_ROOT}/bin"
                "${CMAKE_SOURCE_DIR}/tests/fixtures")

        add_executable(pbvp_playback_controller_test tests/playback_controller_tests.cpp)
        target_link_libraries(pbvp_playback_controller_test PRIVATE pbvp_playback)
        target_compile_features(pbvp_playback_controller_test PRIVATE cxx_std_20)
        target_compile_options(pbvp_playback_controller_test PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus /Zm200)
        set_target_properties(pbvp_playback_controller_test PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        add_test(
            NAME pbvp_playback_controller_test
            COMMAND pbvp_playback_controller_test
                "${PBVP_FFMPEG_ROOT}/bin"
                "${CMAKE_SOURCE_DIR}/tests/fixtures")

        find_program(PBVP_LLVM_PDBUTIL_EXECUTABLE NAMES llvm-pdbutil REQUIRED)
        find_program(PBVP_LLVM_READOBJ_EXECUTABLE NAMES llvm-readobj REQUIRED)
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

        add_test(
            NAME pbvp_ffmpeg_runtime_audit_test
            COMMAND ${PBVP_POWERSHELL_EXECUTABLE}
                -NoProfile
                -ExecutionPolicy Bypass
                -File ${CMAKE_SOURCE_DIR}/tests/test-ffmpeg-runtime-audit.ps1
                -AuditScript ${CMAKE_SOURCE_DIR}/scripts/audit-ffmpeg-runtime.ps1
                -RuntimeDirectory ${PBVP_FFMPEG_ROOT}/bin
                -ManifestPath ${CMAKE_SOURCE_DIR}/dependencies/ffmpeg-8.1.2.json
                -LlvmReadObj ${PBVP_LLVM_READOBJ_EXECUTABLE}
                -PrivatePathMarker ${CMAKE_SOURCE_DIR})
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
        NAME pbvp_phase1_focus_counter_test
        COMMAND ${PBVP_POWERSHELL_EXECUTABLE}
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_SOURCE_DIR}/tests/test-phase1-focus-counter.ps1
            -CounterModule ${CMAKE_SOURCE_DIR}/scripts/phase1-focus-counter.psm1)

    add_test(
        NAME pbvp_ffmpeg_manifest_test
        COMMAND ${PBVP_POWERSHELL_EXECUTABLE}
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_SOURCE_DIR}/tests/test-ffmpeg-manifest.ps1
            -ManifestPath ${CMAKE_SOURCE_DIR}/dependencies/ffmpeg-8.1.2.json
            -FetchScript ${CMAKE_SOURCE_DIR}/scripts/fetch-dependencies.ps1
            -BuildScript ${CMAKE_SOURCE_DIR}/scripts/build-ffmpeg.ps1)

    add_test(
        NAME pbvp_phase2_media_profile_test
        COMMAND ${PBVP_POWERSHELL_EXECUTABLE}
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_SOURCE_DIR}/tests/test-phase2-media-profile.ps1
            -ProfileScript ${CMAKE_SOURCE_DIR}/scripts/create-phase2-media-profile.ps1
            -FixturePath ${CMAKE_SOURCE_DIR}/tests/fixtures/h264-aac-1080p.mp4)

    add_test(
        NAME pbvp_phase2_media_smoke_log_test
        COMMAND ${PBVP_POWERSHELL_EXECUTABLE}
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_SOURCE_DIR}/tests/test-phase2-media-smoke-log.ps1
            -CheckerPath ${CMAKE_SOURCE_DIR}/scripts/check-phase2-media-smoke-log.ps1)

    add_test(
        NAME pbvp_phase3_audio_profile_test
        COMMAND ${PBVP_POWERSHELL_EXECUTABLE}
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_SOURCE_DIR}/tests/test-phase3-audio-profile.ps1
            -ProfileScript ${CMAKE_SOURCE_DIR}/scripts/create-phase3-audio-profile.ps1
            -FixturePath ${CMAKE_SOURCE_DIR}/tests/fixtures/h264-aac-44100-stereo.mp4)

    add_test(
        NAME pbvp_phase3_audio_smoke_log_test
        COMMAND ${PBVP_POWERSHELL_EXECUTABLE}
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_SOURCE_DIR}/tests/test-phase3-audio-smoke-log.ps1
            -CheckerPath ${CMAKE_SOURCE_DIR}/scripts/check-phase3-audio-smoke-log.ps1)

    add_test(
        NAME pbvp_phase4_playback_profile_test
        COMMAND ${PBVP_POWERSHELL_EXECUTABLE}
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_SOURCE_DIR}/tests/test-phase4-playback-profile.ps1
            -ProfileScript ${CMAKE_SOURCE_DIR}/scripts/create-phase4-playback-profile.ps1
            -FixturePath ${CMAKE_SOURCE_DIR}/tests/fixtures/h264-aac-44100-stereo.mp4)

    add_test(
        NAME pbvp_phase4_playback_smoke_log_test
        COMMAND ${PBVP_POWERSHELL_EXECUTABLE}
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_SOURCE_DIR}/tests/test-phase4-playback-smoke-log.ps1
            -CheckerPath ${CMAKE_SOURCE_DIR}/scripts/check-phase4-playback-smoke-log.ps1)

    add_test(
        NAME pbvp_phase4_long_playback_log_test
        COMMAND ${PBVP_POWERSHELL_EXECUTABLE}
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_SOURCE_DIR}/tests/test-phase4-long-playback-log.ps1
            -CheckerPath ${CMAKE_SOURCE_DIR}/scripts/check-phase4-long-playback-log.ps1)

    add_test(
        NAME pbvp_phase4_low_fps_log_test
        COMMAND ${PBVP_POWERSHELL_EXECUTABLE}
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_SOURCE_DIR}/tests/test-phase4-low-fps-log.ps1
            -CheckerPath ${CMAKE_SOURCE_DIR}/scripts/check-phase4-low-fps-log.ps1)

    add_test(
        NAME pbvp_phase5_catalog_profile_test
        COMMAND ${PBVP_POWERSHELL_EXECUTABLE}
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_SOURCE_DIR}/tests/test-phase5-catalog-profile.ps1
            -ProfileScript ${CMAKE_SOURCE_DIR}/scripts/create-phase5-catalog-profile.ps1
            -FixturePath ${CMAKE_SOURCE_DIR}/tests/fixtures/h264-title-metadata.mp4)

    add_test(
        NAME pbvp_phase5_catalog_log_test
        COMMAND ${PBVP_POWERSHELL_EXECUTABLE}
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_SOURCE_DIR}/tests/test-phase5-catalog-log.ps1
            -CheckerPath ${CMAKE_SOURCE_DIR}/scripts/check-phase5-catalog-log.ps1)

    add_test(
        NAME pbvp_data_contract_test
        COMMAND ${CMAKE_COMMAND}
            -DPBVP_SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -P ${CMAKE_SOURCE_DIR}/tests/verify_data_contract.cmake)

    add_executable(pbvp_tests
        tests/audio_callback_state_tests.cpp
        tests/bounded_queue_tests.cpp
        tests/checked_math_tests.cpp
        tests/configuration_tests.cpp
        tests/controller_input_tests.cpp
        tests/frame_cadence_tests.cpp
        tests/input_edge_tests.cpp
        tests/input_prompts_tests.cpp
        tests/media_catalog_tests.cpp
        tests/media_limits_tests.cpp
        tests/menu_keyboard_tests.cpp
        tests/menu_vtable_validation_tests.cpp
        tests/playback_clock_tests.cpp
        tests/playback_state_tests.cpp
        tests/rect_math_tests.cpp
        tests/texture_contract_tests.cpp
        tests/video_scaler_tests.cpp
        tests/video_scheduler_tests.cpp
        tests/test_main.cpp)
    target_link_libraries(pbvp_tests PRIVATE pbvp_core)
    target_compile_features(pbvp_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(pbvp_tests PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus)
    endif()
    set_target_properties(pbvp_tests PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_test(NAME pbvp_core_tests COMMAND pbvp_tests)
endif()
