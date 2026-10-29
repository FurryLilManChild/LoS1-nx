// LoS1-nx: the application class. Hooks from rex::ReXApp are overridden here as the port needs them.
//
// Two renderers, chosen at boot with sa2_renderer (the key keeps sa2-nx's name; src/renut_engine/gpu/
// banjo_renut_select.h):
//   "xenos"  the SDK's emulated Xenos GPU (the reference picture)
//   "renut"  Renut's Direct3D-level renderer (src/renut_engine/gpu, from reNut by masterspike52 through banjo-nx and
//            sa2-nx): the game's Direct3D calls are drawn directly with plume (D3D12 or Vulkan on the PC, deko3d on
//            the Switch), with no PM4 ring and no EDRAM. docs/renut-renderer.md.
// The app wiring below follows sa2-nx's src/sa2_app.h.

#pragma once

#include <filesystem>
#include <functional>
#include <optional>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>
#include <rex/rex_app.h>
#include <rex/runtime.h>

#include "cpu/sa2_enhancements.h"
#include "me/sa2_render_mode.h"

#if LOS_RENUT_RENDERER
#include "renut_engine/gpu/banjo_renut_select.h"
#include "renut_engine/gpu/overlay/app_integration.h"
#include "renut_engine/gpu/banjo_renut_capture.h"
#endif

#if defined(LOS_BUILTIN_CONFIG)
#include "los_builtin_config.h"  // generated from app/los_switch_defaults.toml and app/los_switch.toml (CMake)
#endif

namespace los {
void ApplyStockClockDefaults();  // los_stock_clocks.cpp
void StartEngineVars();          // los_engine_vars.cpp
void ApplyMenuSettings();        // los_menu.cpp
bool MenuOpen();                 // los_menu.cpp
void AddMenuFont(ImFontAtlas* atlas);  // los_menu.cpp
void RegisterMenu(rex::ui::ImGuiDrawer* drawer, const std::filesystem::path& config_path);  // los_menu.cpp
}

class LosApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<LosApp>(new LosApp(ctx, "los", PPCImageConfig));
  }

  // The Switch build's fixed settings (app/los_switch_defaults.toml, compiled in): applied before los.toml, which
  // holds only the player's options (and may still override any of these). As sa2-nx's.
  std::string_view BuiltInConfig() const override {
#if defined(LOS_BUILTIN_CONFIG)
    return kLosBuiltInConfig;
#else
    return {};
#endif
  }
  // Without a los.toml next to the NRO, the player's default one is written there (app/los_switch.toml).
  std::string_view DefaultConfigFile() const override {
#if defined(LOS_BUILTIN_CONFIG)
    return kLosPlayerConfig;
#else
    return {};
#endif
  }

  void OnPostInitLogging() override {
    // The runtime defaults to no GPU plugin: select ReXGlue's Xenos implementation, keeping an explicit choice.
    if (!rex::cvar::HasNonDefaultValue("gpu_plugin")) {
      rex::cvar::SetFlagByName("gpu_plugin", "xenos");
    }
    // Which enhancements run (own key + group master), in one line; before any guest thread exists.
    sa2::enh::Resolve();
  }

  // Without --game_data_root: a game_root/ folder next to the executable (on the Switch, beside the NRO), or the
  // repository's assets/game_root from a PC build tree. An explicit path always wins.
  void OnConfigurePaths(rex::PathConfig& paths) override {
    if (!paths.game_data_root.empty()) return;
    std::error_code ec;
    const auto folder = rex::filesystem::GetExecutableFolder();
    for (auto candidate = folder; !candidate.empty(); candidate = candidate.parent_path()) {
      for (const auto& sub : {std::filesystem::path("game_root"), std::filesystem::path("assets") / "game_root"}) {
        auto pack = candidate / sub;
        pack += ".nxpack";
        if (std::filesystem::is_regular_file(candidate / sub / "default.xex", ec) ||
            std::filesystem::is_regular_file(pack, ec)) {
          paths.game_data_root = candidate / sub;
          return;
        }
      }
      if (candidate == candidate.parent_path()) break;
    }
  }

  // The port menu (src/los_menu.cpp, 2026-10-29): - and + together on the Switch, F5 on the PC.
  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
#if LOS_RENUT_RENDERER
    los::RegisterMenu(drawer, config_file_path());
#else
    (void)drawer;
#endif
  }
  void OnConfigureFonts(ImFontAtlas* atlas) override {
#if LOS_RENUT_RENDERER
    los::AddMenuFont(atlas);
#else
    (void)atlas;
#endif
  }
  bool AppOverlayOpen() const override {
#if LOS_RENUT_RENDERER
    return los::MenuOpen();
#else
    return false;
#endif
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    // LoS1-nx (2026-10-28): stock clocks: 960x540 in handheld at the stock CPU clock, and the official 460.8 MHz
    // handheld GPU profile (src/los_stock_clocks.cpp, src/cpu/sa2_gpu_boost.cpp).
    los::ApplyStockClockDefaults();
#if LOS_RENUT_RENDERER
    los::ApplyMenuSettings();  // the port menu's saved shadows choice (2026-10-29)
    los::StartEngineVars();    // the engine's own console variables from los.toml (2026-10-28)
#endif
    sa2::enh::ApplyGpuBoost();
#if LOS_RENUT_RENDERER
    if (banjo::renut_gpu::Active()) {
      // No SDK graphics system and no GPU plugin: the renderer is the app's (its own swapchain and thread).
      renut::gpu::ConfigureRuntime(config);
      REXLOG_INFO("graphics: Renut's Direct3D-level renderer (sa2_renderer = renut: no PM4 ring, no EDRAM)");
      return;
    }
#endif
    (void)config;
    REXLOG_INFO("graphics: the SDK's emulated Xenos GPU (sa2_renderer = xenos)");
  }

#if LOS_RENUT_RENDERER
  // Renut draws the SDK's menus through its own plume drawer, resizes its swapchain itself, and starts once the
  // window exists (here, before the game: the paths are final).
  std::unique_ptr<rex::ui::ImmediateDrawer> OnCreateImmediateDrawer() override {
    if (banjo::renut_gpu::Active()) return renut::gpu::CreateOverlayDrawer();
    return rex::ReXApp::OnCreateImmediateDrawer();
  }
  void OnWindowPixelSizeChanged(uint32_t pixel_width, uint32_t pixel_height) override {
    rex::ReXApp::OnWindowPixelSizeChanged(pixel_width, pixel_height);
    if (banjo::renut_gpu::Active()) renut::gpu::OnWindowResized();
  }
  std::optional<rex::PathConfig> OnFinalizePaths(const rex::PathConfig& defaults,
                                                 std::function<void(rex::PathConfig)> resume) override {
    if (banjo::renut_gpu::Active()) renut::gpu::StartRenderer(app_context(), [this] { return imgui_drawer(); });
    return rex::ReXApp::OnFinalizePaths(defaults, std::move(resume));
  }
#endif

  void OnPostSetup() override {
    // Every 10 s: how often each active enhancement was used.
    sa2::enh::StartReport();
    // PC: the key that simulates docking / undocking (render resolution; bind_sa2_docked = F9).
    sa2::render::RegisterKeys();
  }

  void OnShutdown() override {
#if LOS_RENUT_RENDERER
    if (banjo::renut_gpu::Active()) renut::gpu::ShutdownRenderer();
#endif
  }
};
