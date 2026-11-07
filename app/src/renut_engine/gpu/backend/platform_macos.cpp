/**
 * @file    gpu/backend/platform_macos.cpp
 */
#include "renut_engine/gpu/backend/platform.h"

#include <dlfcn.h>

#include <SDL3/SDL_metal.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>

#include "renut_engine/gpu/log.h"
#include "vulkan_moltenvk.h"

namespace renut::gpu {

bool PrepareVulkanRuntime() {
  // Renut bypasses the SDK's VulkanInstance. Use its same bundle/SDK discovery
  // and preload the loader, which lets plume's volk open it by its basename.
  const auto paths = rex::ui::vulkan::DetectMacOSVulkanRuntimePaths();
  rex::ui::vulkan::ConfigureMacOSVulkanEnvironment(paths);
  static void *loader = nullptr;  // Retained while plume's Vulkan functions live.
  if (!loader) {
    for (const auto &candidate : paths.loader_candidates) {
      loader = dlopen(candidate.c_str(), RTLD_NOW | RTLD_LOCAL);
      if (loader) {
        RGPU_INFO("Vulkan loader: {}", candidate.string());
        break;
      }
    }
    if (!loader) {
      RGPU_ERROR("No loadable Vulkan runtime; package the vulkan directory beside los");
      return false;
    }
  }
  return true;
}

namespace {

// The app owns one window.
SDL_Window *FindGameSdlWindow() {
  int count = 0;
  SDL_Window **windows = SDL_GetWindows(&count);
  SDL_Window *sdl_window = (windows && count > 0) ? windows[0] : nullptr;
  SDL_free(windows);
  return sdl_window;
}

} // namespace

bool GetNativeRenderWindow(plume::RenderWindow &out) {
  SDL_Window *sdl_window = FindGameSdlWindow();
  if (!sdl_window) {
    RGPU_ERROR("No SDL window for the Vulkan surface");
    return false;
  }

  const SDL_PropertiesID props = SDL_GetWindowProperties(sdl_window);
  // StartRenderer calls this on the UI thread before starting the guest. Tie
  // the view to the SDL window so Cocoa releases it on that thread as well.
  constexpr const char *kMetalView = "los.renut.metal_view";
  auto view = SDL_GetPointerProperty(props, kMetalView, nullptr);
  if (!view) {
    view = SDL_Metal_CreateView(sdl_window);
    if (!view) {
      RGPU_ERROR("SDL Metal view creation failed: {}", SDL_GetError());
      return false;
    }
    if (!SDL_SetPointerPropertyWithCleanup(props, kMetalView, view,
        [](void *, void *value) { SDL_Metal_DestroyView(value); }, nullptr)) {
      RGPU_ERROR("Could not retain SDL Metal view: {}", SDL_GetError());
      return false;
    }
  }
  out.window = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
  out.view = SDL_Metal_GetLayer(view);
  if (!out.window || !out.view) {
    RGPU_ERROR("SDL window has no Cocoa window or Metal layer");
    return false;
  }
  return true;
}

bool GetRenderWindowPixelSize(uint32_t &width, uint32_t &height) {
  SDL_Window *sdl_window = FindGameSdlWindow();
  int w = 0, h = 0;
  if (!sdl_window || !SDL_GetWindowSizeInPixels(sdl_window, &w, &h))
    return false;
  width = uint32_t(w);
  height = uint32_t(h);
  return true;
}

} // namespace renut::gpu
