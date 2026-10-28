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
// Player keys (los.toml): sa2_render_height_handheld set there always wins over the 540p choice; los_stock_540p =
// false keeps 1280x720 at stock clocks; los_fps_cap = 0 (no cap), 30 (always 30 in handheld), -1 (default: 30 at
// stock clocks in handheld).
#include <atomic>
#include <cstdint>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/platform.h>

#include "me/sa2_render_mode.h"

#if REX_PLATFORM_SWITCH
#include <switch.h>
#endif

REXCVAR_DEFINE_BOOL(los_stock_540p, true, "LoS1",
                    "Switch, handheld: when the CPU is at its stock clock (1020 MHz), render at 960x540 unless "
                    "sa2_render_height_handheld is set in los.toml. Checked at start and every 5 s");
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

bool g_player_set = false;
int g_state = -1;  // -1 not decided, 0 not stock (or docked), 1 stock handheld

// Decides from the clock and the console's mode; logs only a change.
void Update(bool first) {
  const bool docked = appletGetOperationMode() == AppletOperationMode_Console;
  const uint32_t mhz = CpuMhz();
  if (mhz == 0) return;
  const int state = (!docked && mhz <= 1100) ? 1 : 0;
  if (state == g_state) return;
  g_state = state;

  if (!g_player_set && REXCVAR_GET(los_stock_540p))
    rex::cvar::SetFlagByName("sa2_render_height_handheld", state ? "540" : "720");
  const int32_t cap = REXCVAR_GET(los_fps_cap);
  const bool capped = !docked && (cap == 30 || (cap < 0 && state == 1));
  g_min_refreshes.store(capped ? 2u : 1u, std::memory_order_relaxed);

  REXLOG_INFO("[clocks] {}CPU {} MHz, {}: {} handheld render height {}{}, frame rate {}", first ? "at start: " : "now ",
              mhz, docked ? "docked" : "handheld", state ? "stock clocks:" : "not stock clocks:",
              sa2::render::RenderHeightOf(false), g_player_set ? " (set in los.toml)" : "",
              capped ? "capped at 30" : "not capped");
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

void ApplyStockClockDefaults() {
#if REX_PLATFORM_SWITCH
  g_player_set = rex::cvar::GetFlagSource("sa2_render_height_handheld") != rex::cvar::Source::kDefault;
  Update(true);
  // A small thread of its own on the third core: one clock read every 5 s.
  if (R_SUCCEEDED(threadCreate(&g_watch, WatchThread, nullptr, nullptr, 0x10000, 0x3B, 2)))
    threadStart(&g_watch);
#endif
}

}  // namespace los
