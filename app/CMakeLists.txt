# LoS1-nx: Castlevania: Lords of Shadow (Xbox 360 disc), recompiled with ReXGlue.
#
# Configure with REXSDK_DIR pointing at a rexglue-sdk source tree (tools/win/build_pc.ps1 does this).
# The build runs the code generator itself (target los_codegen) whenever the manifest, overrides or xex change.
# app/cmake/rexglue.cmake is the SDK boilerplate written by `rexglue init` (ReXGlue 0.10, commit c94f5eb).

cmake_minimum_required(VERSION 3.25)
project(los LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The Switch (devkitA64 GCC 16.1): no AArch64 "early register allocation" pass. sa2-nx (2026-10-19, with banjo-nx's
# codegen round; peggle2-nx, sasrt-nx and banjo-nx build this way): at -O2 and above it dropped table lookups of the
# XDK's streaming copy in Peggle 2's console build (garbage textures on the console only). Before the SDK is added, so
# its libraries get it too.
if(REXGLUE_PLATFORM_SWITCH)
    add_compile_options(-mearly-ra=none)
    add_link_options(-mearly-ra=none)
endif()

include(cmake/rexglue.cmake)

set(LOS_SOURCES
    src/main.cpp
    src/los_diag.cpp
    src/los_stubs.cpp
    # From sa2-nx, used by Renut's renderer: the enhancement switches and the console's render mode.
    src/cpu/sa2_enhancements.cpp
    src/cpu/sa2_gpu_boost.cpp
    src/me/sa2_render_mode.cpp
    # The Switch at its stock clocks (2026-10-28): 960x540 handheld at the stock CPU clock.
    src/los_stock_clocks.cpp
)

# Renut's Direct3D-level renderer (src/renut_engine), chosen at boot with sa2_renderer = "renut". Its third-party code
# comes from sa2-nx: tools/fetch_renut_deps.py fills app/thirdparty.
# OFF until its Direct3D map is made for this game's XDK (2.0.11626; docs/scan.md): the hooks in src/ and
# src/renut_engine still carry Mirror of Fate's addresses.
option(LOS_RENUT_RENDERER "Build Renut's renderer (sa2_renderer = renut)" OFF)
if(LOS_RENUT_RENDERER)
    list(APPEND LOS_SOURCES src/los_renut_glue.cpp src/los_resource_copies.cpp src/los_render_targets.cpp
         src/los_engine_vars.cpp)
endif()

if(WIN32)
    add_executable(los WIN32 ${LOS_SOURCES})
else()
    add_executable(los ${LOS_SOURCES})
endif()

# The emulated Xenos GPU (D3D12 on Windows, Vulkan on Linux) stays built in as the reference renderer.
rexglue_setup_target(los GPU_PLUGINS xenos)
# An emulator starts NROs without argv[0]: the SDK then takes the NRO's usual place on the SD card for its folder
# (sdk/src/core/filesystem_posix.cpp). The console's loader always passes argv[0].
if(REXGLUE_PLATFORM_SWITCH AND TARGET rexcore)
    target_compile_definitions(rexcore PRIVATE REX_SWITCH_FALLBACK_EXE="/switch/los1-nx/los.nro")
endif()

# The generated game code must round as the Xbox 360 does: GCC's default for GNU C++ is -ffp-contract=fast, which fuses
# separate PPC multiplies and adds into FMAs; strict aliasing is unsafe for recompiled code (sa2-nx's settings, for
# both compilers, so the PC rounds like the console).
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    foreach(target los los_recomp)
        if(TARGET ${target})
            target_compile_options(${target} PRIVATE -ffp-contract=off -fno-strict-aliasing)
        endif()
    endforeach()
endif()

if(LOS_RENUT_RENDERER)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/plume/CMakeLists.txt")
        message(FATAL_ERROR "app/thirdparty is empty: run python tools/fetch_renut_deps.py first")
    endif()
    include(cmake/los_renut_gpu.cmake)
endif()

# rexruntime absorbs the SDK's UI object library (rexui) but does not pass on its usage requirements, so the app's
# own sources (main.cpp, rex_app.cpp) would not find imgui.h. Add the header paths only: linking imgui's objects
# again would give the executable a second, separate ImGui state.
foreach(_dep imgui renderdoc)
    if(TARGET ${_dep})
        target_include_directories(los PRIVATE $<TARGET_PROPERTY:${_dep},INTERFACE_INCLUDE_DIRECTORIES>)
    endif()
endforeach()

# The settings file next to the executable: copied once, never overwritten, so local edits survive rebuilds.
if(NOT REXGLUE_PLATFORM_SWITCH)
    add_custom_command(TARGET los POST_BUILD
        COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/los.toml -DDST=$<TARGET_FILE_DIR:los>/los.toml
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_if_missing.cmake
        VERBATIM)
endif()

if(REXGLUE_PLATFORM_SWITCH)
    # los_register.cpp is one generated function of ~50,000 SetFunction calls that runs once at start: GCC -O3 spends
    # many minutes on SA2's smaller one (771-927 s, sa2-nx from sasrt-nx). -O0 compiles it in seconds.
    set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/los_register.cpp" APPEND PROPERTY
                 COMPILE_OPTIONS -O0)
    # The fixed settings, compiled in (src/los_app.h BuiltInConfig), and the player's los.toml, written next to the
    # NRO when it is missing (DefaultConfigFile).
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/los_switch_defaults.toml" LOS_BUILTIN_CONFIG)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/los_switch.toml" LOS_PLAYER_CONFIG)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                 "${CMAKE_CURRENT_SOURCE_DIR}/los_switch_defaults.toml" "${CMAKE_CURRENT_SOURCE_DIR}/los_switch.toml")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/los_builtin_config.h.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/builtin_config/los_builtin_config.h" @ONLY)
    target_include_directories(los PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/builtin_config")
    target_compile_definitions(los PRIVATE LOS_BUILTIN_CONFIG=1)
    # The homebrew menu's entry (no icon of our own yet: libnx's default).
    rexglue_switch_add_nro(los
        NAME "Castlevania: Lords of Shadow"
        AUTHOR "LoS1-nx"
        VERSION "0.1.0")
endif()
