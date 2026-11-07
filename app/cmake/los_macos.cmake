# App defaults and Apple Clang workarounds. Included only for macOS builds.
set(LOS_DEFAULT_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/los_mac.toml")

# Registration runs once at startup. Apple Clang spends minutes optimizing
# its ~60,000 SetFunction calls. Gameplay translation units stay optimized.
set(los_register_source "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/los_register.cpp")
set_property(SOURCE "${los_register_source}" APPEND PROPERTY COMPILE_OPTIONS -O0)

# Clang rejects an -O3 PCH when compiling this one file at -O0.
set_source_files_properties("${los_register_source}" PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

# Apple Clang also spends minutes in GlobalISel's Localizer on this huge
# straight-line function at -O0 (observed with macOS sample).
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-fno-global-isel" LOS_HAS_NO_GLOBAL_ISEL)
if(LOS_HAS_NO_GLOBAL_ISEL)
    set_property(SOURCE "${los_register_source}" APPEND PROPERTY COMPILE_OPTIONS -fno-global-isel)
endif()
