// LoS1-nx: the Switch at its stock clocks (2026-10-28).
//
// Asked for: playable on a console without an overclock (no sys-clk profile). Mark's console log of 2026-10-26 ran at
// CPU 1785-2091 MHz, GPU 460.8 MHz: 37-53 fps in light scenes, 17-25 fps in heavy ones. His stock-clock tests of
// 2026-10-28 (CPU 1020 MHz, GPU 460.8 MHz from sa2_gpu_boost):
//   - 720p: light 46-60 fps (GPU 15-21 ms), medium 32-36, heavy 10-17 with the main game thread at 97-98 %;
//   - 540p: the heavy scene's GPU time 35-40 ms -> 20-23 ms, the frame rate the same (9-14 fps): the game thread is
//     the limit there. Its stack samples (switch_profile_stacks): 67 % the game's own code over hundreds of functions,
//     no hot spot; 10 % the resource copies (los_resource_copies.cpp), 8 % the game's Direct3D, 4 % the renderer.
//
// So at stock clocks, in handheld:
//   - 960x540 (sa2_render_height_handheld = 540, Renut's scale code 7, render_scale.h), shown at 1280x720;
//   - a 30 fps cap (each Swap at least two refreshes; sa2_renut_pacing.cpp asks MinRefreshesPerFrame), so the light
//     scenes hold 30 instead of jumping between 35 and 60;
//   - the official 460.8 MHz handheld GPU profile (sa2_gpu_boost, sa2_gpu_boost.cpp from sa2-nx; not an overclock).
// "Stock" is the CPU at 1100 MHz or less, read at start and again every 5 s, so a sys-clk profile changed while the
// game runs switches both (Mark changes his from the overlay after starting the game). Docked, or the CPU faster:
// 1280x720 and the game's own frame rate, as before.
//
// Player keys (los.toml; the port menu, los_menu.cpp, sets and saves them):
//   los_handheld_resolution = "auto" (960x540 at stock clocks, 1280x720 otherwise), "540" or "720"
//   los_fps_cap = -1 (30 fps at stock clocks in handheld), 0 (no cap), 30 (always 30 in handheld)
// An older los.toml's sa2_render_height_handheld still wins while los_handheld_resolution is not in the file.
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/platform.h>

#include <fmt/format.h>

#include "me/sa2_render_mode.h"

#if REX_PLATFORM_SWITCH
#include <switch.h>
#endif

REXCVAR_DEFINE_BOOL(los_stock_540p, true, "LoS1",
                    "Switch, handheld: when the CPU is at its stock clock (1020 MHz), render at 960x540 unless "
                    "sa2_render_height_handheld is set in los.toml. Checked at start and every 5 s");
REXCVAR_DEFINE_STRING(los_handheld_resolution, "auto", "LoS1",
                      "Handheld render resolution: auto (960x540 at the stock CPU clock, 1280x720 otherwise), "
                      "540 or 720")
    .allowed({"auto", "540", "720"});
REXCVAR_DEFINE_INT32(los_fps_cap, -1, "LoS1",
                     "Frame rate cap in handheld mode: -1 = 30 fps at the stock CPU clock (default), 0 = none, "
                     "30 = always 30 fps")
    .range(-1, 30);

namespace los {

namespace {
std::atomic<uint32_t> g_min_refreshes{1};
}

// Refreshes of 60 Hz each frame takes at least (sa2_renut_pacing.cpp): 2 = the 30 fps cap.
uint32_t MinRefreshesPerFrame() {
  return g_min_refreshes.load(std::memory_order_relaxed);
}

#if REX_PLATFORM_SWITCH
namespace {

// The CPU clock in MHz, 0 if it cannot be read.
uint32_t CpuMhz() {
  uint32_t hz = 0;
  if (hosversionAtLeast(8, 0, 0)) {
    if (R_FAILED(clkrstInitialize())) return 0;
    ClkrstSession s{};
    if (R_SUCCEEDED(clkrstOpenSession(&s, PcvModuleId_CpuBus, 3))) {
      if (R_FAILED(clkrstGetClockRate(&s, &hz))) hz = 0;
      clkrstCloseSession(&s);
    }
    clkrstExit();
  } else {
    if (R_FAILED(pcvInitialize())) return 0;
    if (R_FAILED(pcvGetClockRate(PcvModule_CpuBus, &hz))) hz = 0;
    pcvExit();
  }
  return hz / 1000000u;
}

bool g_player_set = false;  // an older los.toml's sa2_render_height_handheld (and no los_handheld_resolution)
std::mutex g_update_mutex;
uint32_t g_last_mhz = 0;
bool g_last_docked = false;
std::string g_last_line;

// Decides from the clock, the console's mode and the player's keys; logs only a change.
void Update(bool first) {
  std::lock_guard<std::mutex> lock(g_update_mutex);
  const bool docked = appletGetOperationMode() == AppletOperationMode_Console;
  const uint32_t mhz = CpuMhz();
  if (mhz == 0) return;
  g_last_mhz = mhz;
  g_last_docked = docked;
  const bool stock = !docked && mhz <= 1100;

  if (!g_player_set) {
    const std::string mode = REXCVAR_GET(los_handheld_resolution);
    const bool low = mode == "540" || (mode == "auto" && stock && REXCVAR_GET(los_stock_540p));
    const char* want = low ? "540" : "720";
    if (sa2::render::RenderHeightOf(false) != (low ? 540 : 720))
      rex::cvar::SetFlagByName("sa2_render_height_handheld", want);
  }
  const int32_t cap = REXCVAR_GET(los_fps_cap);
  const bool capped = !docked && (cap == 30 || (cap < 0 && stock));
  g_min_refreshes.store(capped ? 2u : 1u, std::memory_order_relaxed);

  const std::string line = fmt::format("CPU {} MHz, {}, {}: handheld render height {}{}, frame rate {}", mhz,
                                       docked ? "docked" : "handheld", stock ? "stock clocks" : "not stock clocks",
                                       sa2::render::RenderHeightOf(false),
                                       g_player_set ? " (sa2_render_height_handheld in los.toml)" : "",
                                       capped ? "capped at 30" : "not capped");
  if (line != g_last_line) {
    g_last_line = line;
    REXLOG_INFO("[clocks] {}{}", first ? "at start: " : "now ", line);
  }
}

void WatchThread(void*) {
  for (;;) {
    svcSleepThread(5000000000ULL);
    Update(false);
  }
}

Thread g_watch;

}  // namespace
#endif

// The port menu changed a key: decide again now.
void ApplyOptionsNow() {
#if REX_PLATFORM_SWITCH
  g_player_set = false;  // the menu's choice replaces an older sa2_render_height_handheld
  Update(false);
#endif
}

// For the port menu's status line.
std::string ClockStatus() {
#if REX_PLATFORM_SWITCH
  std::lock_guard<std::mutex> lock(g_update_mutex);
  return g_last_line;
#else
  return "PC: the clock checks are the Switch's";
#endif
}

void ApplyStockClockDefaults() {
#if REX_PLATFORM_SWITCH
  g_player_set = rex::cvar::GetFlagSource("sa2_render_height_handheld") != rex::cvar::Source::kDefault &&
                 rex::cvar::GetFlagSource("los_handheld_resolution") == rex::cvar::Source::kDefault;
  Update(true);
  // A small thread of its own on the third core: one clock read every 5 s.
  if (R_SUCCEEDED(threadCreate(&g_watch, WatchThread, nullptr, nullptr, 0x10000, 0x3B, 2)))
    threadStart(&g_watch);
#endif
}

}  // namespace los
