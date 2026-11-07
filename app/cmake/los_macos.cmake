# App defaults and entrypoint build setup. Included only for macOS builds.
set(LOS_DEFAULT_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/los_mac.toml")

# This executable registers through PPCImageConfig.func_mappings in los_init.cpp.
# The generated los_RegisterFunctions callback has no callers; compiling its
# ~60,000 calls wastes build time. Keep it out of the entrypoint target only,
# leaving the mappings, gameplay sources, and any DLL registration untouched.
if(TARGET los_recomp)
    get_target_property(los_entrypoint_sources los_recomp SOURCES)
    list(REMOVE_ITEM los_entrypoint_sources "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/los_register.cpp")
    set_property(TARGET los_recomp PROPERTY SOURCES "${los_entrypoint_sources}")
endif()
