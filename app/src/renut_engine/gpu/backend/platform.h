/**
 * @file    gpu/backend/platform.h
 * @brief   Window and Vulkan runtime integration, selected by the build system.
 */
#pragma once

#include <cstdint>

#include <plume_render_interface.h>

namespace renut::gpu {

// Prepare the host Vulkan runtime before plume creates its interface.
// Platforms with a system loader need no additional setup.
bool PrepareVulkanRuntime();

// Windows: the HWND of this process's game window (the SDK's SDL window).
// Linux: the Xlib display + window of the SDL window (plume's Linux Vulkan
// surface is Xlib, so SDL is pinned to its X11 driver).
// macOS: the SDL Cocoa window and its CAMetalLayer (created on the UI thread).
bool GetNativeRenderWindow(plume::RenderWindow &out);

// The game window's client area, in pixels.
bool GetRenderWindowPixelSize(uint32_t &width, uint32_t &height);

} // namespace renut::gpu
