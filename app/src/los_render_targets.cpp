// LoS1-nx: the render targets the game binds.
//
// 1. For Renut's renderer: each surface's descriptor words are remembered at its bind (renut::gpu::
//    RememberBoundSurface). Found in Mirror of Fate (mof-nx), whose code this is: the game builds a surface object for each SetRenderTarget (all from MoF's sub_824C6630, the
//    call at 0x824C67E0: dozens of distinct objects, each a copy of the 1280x720 back buffer's header, word 0
//    0x00100004 where Direct3D's own have 0x80100004) in memory it reuses soon after; the render thread reads a draw's
//    targets a frame or more later and found other data there (Mark's PC, 2026-10-23: targets of 1x8705 and
//    7384x27854, 231 failed creations, flicker; found with 2. on the Linux build).
//
// 2. Diagnostics, on only with the environment variable LOS_RT_DIAG=1: logs each distinct surface pointer passed to D3DDevice_SetRenderTarget (sub_82498388), SetDepthStencilSurface
// (sub_824986D8) and the four-target setter sub_824989B0 (LoS's addresses: d3d_match against MoF's, docs/d3d-map.md), with the first 0x30
// bytes at the pointer and the caller. Written to find out what the colour "surfaces" with impossible sizes are
// (first Renut boots, 2026-10-23: 1x8705 and 7384x27854).

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_set>

#include <rex/hook.h>
#include <rex/logging.h>

#if LOS_RENUT_RENDERER
#include "renut_engine/gpu/banjo_renut_select.h"
#endif

#if LOS_RENUT_RENDERER
namespace renut::gpu {
void RememberBoundSurface(uint32_t surface_va);  // renut_engine/gpu/render/surfaces.h
}
#endif

namespace {

bool On() {
  static const bool on = [] {
    const char* v = std::getenv("LOS_RT_DIAG");
    return v && *v == '1';
  }();
  return on;
}

uint32_t Load32(const uint8_t* base, uint32_t a) {
  uint32_t v;
  std::memcpy(&v, base + a, 4);
  return __builtin_bswap32(v);
}

void Note(const char* what, uint8_t* base, uint32_t surface, uint32_t lr, uint32_t index) {
  static std::mutex m;
  static std::unordered_set<uint64_t> seen;
  if (!surface) return;
  std::lock_guard lock(m);
  if (seen.size() > 400 || !seen.insert((uint64_t(surface) << 32) | lr).second) return;
  if (surface < 0x40000000u || surface >= 0xC0000000u) {
    REXLOG_INFO("[rtdiag] {} [{}] {:08X} (outside the heaps) from {:08X}", what, index, surface, lr);
    return;
  }
  uint32_t w[12];
  for (int i = 0; i < 12; ++i) w[i] = Load32(base, surface + 4 * i);
  REXLOG_INFO("[rtdiag] {} [{}] {:08X} from {:08X}: {:08X} {:08X} {:08X} {:08X} {:08X} {:08X} | {:08X} {:08X} {:08X} "
           "{:08X} {:08X} {:08X}",
           what, index, surface, lr, w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7], w[8], w[9], w[10], w[11]);
}

void Remember(uint32_t surface) {
#if LOS_RENUT_RENDERER
  if (surface && ::banjo::renut_gpu::Active()) ::renut::gpu::RememberBoundSurface(surface);
#else
  (void)surface;
#endif
}

}  // namespace

REX_EXTERN(__imp__sub_82498388);
REX_HOOK_RAW(sub_82498388) {
  Remember(ctx.r5.u32);
  if (On()) Note("SetRenderTarget", base, ctx.r5.u32, uint32_t(ctx.lr), ctx.r4.u32);
  __imp__sub_82498388(ctx, base);
}

REX_EXTERN(__imp__sub_824986D8);
REX_HOOK_RAW(sub_824986D8) {
  Remember(ctx.r4.u32);
  if (On()) Note("SetDepthStencil", base, ctx.r4.u32, uint32_t(ctx.lr), 0);
  __imp__sub_824986D8(ctx, base);
}

REX_EXTERN(__imp__sub_824989B0);
REX_HOOK_RAW(sub_824989B0) {
  for (uint32_t i = 0; i < 5; ++i) Remember(Load32(base, ctx.r4.u32 + 4 * i));
  if (On()) {
    const uint32_t s = ctx.r4.u32;
    for (uint32_t i = 0; i < 5; ++i) Note("SetSurfaces", base, Load32(base, s + 4 * i), uint32_t(ctx.lr), i);
  }
  __imp__sub_824989B0(ctx, base);
}
