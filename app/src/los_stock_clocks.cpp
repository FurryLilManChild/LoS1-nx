// LoS1-nx: the Switch at its stock clocks (2026-10-28).
//
// Asked for: playable on a console without an overclock (no sys-clk profile). Mark's console log of 2026-10-26 ran at
// CPU 1785-2091 MHz, GPU 460.8 MHz: 37-53 fps in light scenes, 17-25 fps in heavy ones with the GPU at 40-49 ms a
// frame at 1280x720. At the stock handheld clocks (CPU 1020 MHz, GPU 307.2 MHz) that is too slow.
//
// What this does, as ds3-nx (VasilkovND, Dead Space 3 on ReXGlue) does on stock clocks:
//   - handheld, CPU at its stock clock at start: render at 960x540 (sa2_render_height_handheld = 540, Renut's scale
//     code 7, render_scale.h), shown at 1280x720 by the present. 56 % of the pixels of 720p.
//   - the official 460.8 MHz handheld GPU profile (sa2_gpu_boost, sa2_gpu_boost.cpp from sa2-nx), asked for in every
//     handheld start; it is not an overclock (apm, as commercial games use it).
// Not done when the player's los.toml sets sa2_render_height_handheld (that wins), when los_stock_540p = false, when
// docked, or when the CPU already runs faster (an overclock profile: 720p as before).
#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/platform.h>

#include "me/sa2_render_mode.h"

#if REX_PLATFORM_SWITCH
#include <switch.h>
#endif

REXCVAR_DEFINE_BOOL(los_stock_540p, true, "LoS1",
                    "Switch, handheld: when the CPU is at its stock clock (1020 MHz) at start, render at 960x540 "
                    "unless sa2_render_height_handheld is set in los.toml");

namespace los {

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
}  // namespace
#endif

void ApplyStockClockDefaults() {
#if REX_PLATFORM_SWITCH
  const bool docked = appletGetOperationMode() == AppletOperationMode_Console;
  const uint32_t mhz = CpuMhz();
  const bool player_set = rex::cvar::GetFlagSource("sa2_render_height_handheld") != rex::cvar::Source::kDefault;
  const bool stock = mhz != 0 && mhz <= 1100;
  if (REXCVAR_GET(los_stock_540p) && stock && !player_set) {
    rex::cvar::SetFlagByName("sa2_render_height_handheld", "540");
    REXLOG_INFO("[clocks] CPU {} MHz at start (stock): handheld renders at 960x540 (los_stock_540p; "
                "sa2_render_height_handheld = 720 in los.toml keeps 1280x720)", mhz);
  } else {
    REXLOG_INFO("[clocks] CPU {} MHz at start, {}: handheld render height {} ({})", mhz,
                docked ? "docked" : "handheld", sa2::render::RenderHeightOf(false),
                player_set ? "set in los.toml" : !REXCVAR_GET(los_stock_540p) ? "los_stock_540p off"
                : stock ? "-" : "faster than stock");
  }
#endif
}

}  // namespace los
