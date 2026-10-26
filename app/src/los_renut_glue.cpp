// LoS1-nx: the game-side pieces Renut's renderer needs that are not in src/renut_engine.
//
// 1. The window offset/scissor writer. Renut's draws take the window offset and scissor from the device's window
//    packet (+0x28C0, registers 0x2080-0x2082), but this Direct3D (like SA2's) writes them only into the command
//    buffer and keeps no copy in the device (docs/d3d-map.md: its writer, sub_826D7A48, stores nothing at
//    +0x28C0..+0x28C8). As sa2-nx does in src/me/sa2_me_hooks.cpp: the packet gets what the original writes in the
//    untiled case (offset 0, then x / y of the two corners), then the original runs (its packets go to the ring sink).
//    sub_826D7A48(device, x0, y0, x1, y1) = SA2's sub_8254FCE8 (d3d_match 1.00).
//
// 2. Symbols sa2-nx's other app files define, which the renderer and the enhancement switches use (found by a full
//    link on Linux, 2026-10-23):
//    - g_vsync_relaxed: the relaxed vsync switch (sa2-nx's src/me/me_native_system.cpp); Renut's pacing reads it;
//    - g_banjo_store_cards: whether the generated code marks store cards (sa2-nx's src/cpu/sa2_hot_guest.cpp, true
//      only for its optimised codegen flavour). This port uses the plain flavour: false;
//    - CodegenChecksInit/Report, FormsSelfCheck: sa2-nx's post-codegen self-checks (none here);
//    - RexSwitchPerfCount: the Switch profiler's counter (the SDK's switch_perf.cpp); nothing on the PC.
//
// 3. A stand-in for sa2-nx's frame capture (src/sa2_capture.cpp, which also needs sa2-nx's old renderer): the renderer
//    asks it for lockstep captures, which are off here. Replace with the real module when frame-by-frame comparisons
//    are needed.

#include <array>
#include <atomic>
#include <unordered_map>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <cstdint>
#include <cstring>
#include <functional>

#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/platform.h>

#include "cpu/sa2_enhancements.h"
#include "renut_engine/gpu/banjo_renut_select.h"
#include "renut_engine/gpu/banjo_shared_hooks.h"
#include "sa2_capture.h"

REX_EXTERN(__imp__sub_826D7A48);
REX_HOOK_RAW(sub_826D7A48) {
  if (::banjo::renut_gpu::Active()) {
    const uint32_t d = ctx.r3.u32;
    const uint32_t x0 = ctx.r4.u32, y0 = ctx.r5.u32, x1 = ctx.r6.u32, y1 = ctx.r7.u32;
    const uint32_t words[3] = {0u, ((y0 << 16) & 0x3FFF0000u) | (x0 & 0x3FFFu),
                               ((y1 << 16) & 0x3FFF0000u) | (x1 & 0x3FFFu)};
    for (uint32_t i = 0; i < 3; ++i) {
      const uint32_t v = __builtin_bswap32(words[i]);
      std::memcpy(base + d + 0x28C0 + 4 * i, &v, 4);
    }
  }
  __imp__sub_826D7A48(ctx, base);
}

extern std::atomic<uint64_t> g_sa2_game_frames;

// los_resource_copies.cpp: the draws take stable copies of their resource headers (see there).
namespace los {
struct SavedPointers {
  uint32_t device = 0;
  uint32_t words[48];
};
void UseCopies(uint8_t* base, uint32_t device, SavedPointers& s);
void RestorePointers(uint8_t* base, const SavedPointers& s);
}  // namespace los

// 4. The six Direct3D functions both renderers hook. Renut's side of each is only a function (renut_native_sub_X,
//    banjo_shared_hooks.h); in sa2-nx, sa2-nx's own hook at that address calls it first thing when Renut is chosen
//    (SA2_RENUT_YIELD_SHARED in src/me/sa2_me_hooks.cpp, sa2_d3d_api.cpp, src/cpu/sa2_gpu_wait.cpp). This port has
//    no such hooks, so these do it, and run the game's original otherwise. Without them no draw reached Renut
//    (first Renut boot on Mark's PC, 2026-10-23: "0 draws" in every report, black screen; the game itself ran).
//    The four draw functions hand Renut the device with stable copies of its resource headers
//    (los_resource_copies.cpp), and put the game's pointers back after.
//    Diagnostics (LOS_FRAME_DIAG=<seconds>): from that many seconds after the first draw, 150 frames are logged one
//    line each: the draws of each kind the game issued in the frame, and a signature of the set of draws (the sum of
//    a hash of each draw's primitive type and shaders), to tell whether the game issues the same draws in every frame
//    of a still scene (flicker in the renderer) or not (flicker in the game's own choices).
namespace {
struct FrameDiag {
  uint64_t frame = ~0ull;
  uint32_t counts[4] = {};
  uint64_t signature = 0;
  uint32_t logged = 0;
};
double FrameDiagSeconds() {
  static const double v = [] {
    const char* e = std::getenv("LOS_FRAME_DIAG");
    return e ? std::atof(e) : -1.0;
  }();
  return v;
}
void FrameNote(int fn, const PPCContext& ctx, const uint8_t* base) {
  static std::mutex m;
  static FrameDiag d;
  static std::chrono::steady_clock::time_point first{};
  std::lock_guard lock(m);
  const auto now = std::chrono::steady_clock::now();
  if (first == std::chrono::steady_clock::time_point{}) first = now;
  const uint64_t frame = g_sa2_game_frames.load(std::memory_order_relaxed);
  if (frame != d.frame) {
    if (d.frame != ~0ull && d.logged < 150 &&
        std::chrono::duration<double>(now - first).count() >= FrameDiagSeconds()) {
      ++d.logged;
      REXLOG_INFO("[framediag] frame {} draws: begin {} up {} draw {} indexed {}; signature {:016X}", d.frame,
                  d.counts[0], d.counts[1], d.counts[2], d.counts[3], d.signature);
    }
    d.frame = frame;
    std::memset(d.counts, 0, sizeof(d.counts));
    d.signature = 0;
  }
  ++d.counts[fn];
  const uint32_t dev = ctx.r3.u32;
  uint32_t vs = 0, ps = 0;
  std::memcpy(&ps, base + dev + 13068, 4);
  std::memcpy(&vs, base + dev + 13072, 4);
  uint64_t h = (uint64_t(vs) << 32 | ps) * 0x9E3779B97F4A7C15ull;
  h ^= (uint64_t(ctx.r4.u32) << 8 | uint64_t(fn)) * 0xC2B2AE3D27D4EB4Full;
  d.signature += h ^ (h >> 29);
  // Resource headers seen at this address before with other words (LOS_FRAME_DIAG): textures (16 slots, 52-byte
  // headers), vertex buffers (16 streams, 32 bytes), the index buffer (32 bytes).
  static std::unordered_map<uint32_t, std::array<uint8_t, 52>> seen;
  static uint64_t reuse[3] = {}, checks[3] = {};
  auto check = [&](int kind, uint32_t va, uint32_t size) {
    if (va < 0x40000000u || va >= 0xC0000000u) return;
    std::array<uint8_t, 52> words{};
    std::memcpy(words.data(), base + va, size);
    ++checks[kind];
    auto [it, fresh] = seen.try_emplace(va, words);
    if (!fresh && it->second != words) {
      ++reuse[kind];
      it->second = words;
    }
  };
  for (uint32_t r = 0; r < 16; ++r) {
    uint32_t va;
    std::memcpy(&va, base + dev + 12920 + 4 * r, 4);
    check(0, __builtin_bswap32(va), 52);
    std::memcpy(&va, base + dev + 12836 + 4 * r, 4);
    check(1, __builtin_bswap32(va), 32);
  }
  uint32_t ib;
  std::memcpy(&ib, base + dev + 12812, 4);
  check(2, __builtin_bswap32(ib), 32);
  static uint32_t draws = 0;
  if (++draws % 3000 == 0)
    REXLOG_INFO("[framediag] headers changed at the same address: textures {} of {}, vertex buffers {} of {}, "
                "index buffers {} of {}; {} addresses", reuse[0], checks[0], reuse[1], checks[1], reuse[2], checks[2],
                seen.size());
}
}  // namespace

#define LOS_RENUT_SHARED(sub, fn)                                 \
  REX_EXTERN(__imp__##sub);                                       \
  REX_HOOK_RAW(sub) {                                             \
    if (fn >= 0 && FrameDiagSeconds() >= 0) FrameNote(fn, ctx, base); \
    if (::banjo::renut_gpu::Active()) {                           \
      if (fn >= 0) {                                              \
        los::SavedPointers saved;                                 \
        los::UseCopies(base, ctx.r3.u32, saved);                  \
        renut_native_##sub(ctx, base);                            \
        los::RestorePointers(base, saved);                        \
      } else {                                                    \
        renut_native_##sub(ctx, base);                            \
      }                                                           \
      return;                                                     \
    }                                                             \
    __imp__##sub(ctx, base);                                      \
  }
LOS_RENUT_SHARED(sub_826F8AA8, -1)  // AllocateInlineData (SA2 sub_8254D9A8)
LOS_RENUT_SHARED(sub_826F2B60, -1)  // CBlocker::Check (SA2 sub_8254F400)
LOS_RENUT_SHARED(sub_826DDBF0, 0)   // D3DDevice_BeginVertices (SA2 sub_82565758)
LOS_RENUT_SHARED(sub_826DE0B0, 1)   // D3DDevice_DrawVerticesUP (SA2 sub_82565C18)
LOS_RENUT_SHARED(sub_826DE6D8, 2)   // D3DDevice_DrawVertices (SA2 sub_82565C60)
LOS_RENUT_SHARED(sub_826DEAF0, 3)   // D3DDevice_DrawIndexedVertices (SA2 sub_82566078)
#undef LOS_RENUT_SHARED

SA2_ENHANCEMENT(g_vsync_relaxed, sa2_vsync_relaxed, sa2::enh::kNonCpu, true,
                "Relaxed vsync: a frame that misses 60 fps is shown when it is ready instead of at the next refresh: "
                "no fall from 60 straight to 30");

extern "C" const bool g_banjo_store_cards = false;


#if !REX_PLATFORM_SWITCH
extern "C" void RexSwitchPerfCount(unsigned) {}
#endif

std::atomic<uint64_t> g_sa2_game_frames{0};

namespace sa2::capture {
void Start(std::function<rex::ui::Presenter*()>) {}
void StartWithSource(std::function<bool(rex::ui::RawImage&)>) {}
bool RenutLockstepOn() { return false; }
void RenutLockstepFrame(uint64_t, const rex::ui::RawImage*) {}
void Stop() {}
void BeforeFlip() {}
}  // namespace sa2::capture
