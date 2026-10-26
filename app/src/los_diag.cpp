// LoS1-nx: boot diagnostics for the game's break, bug-check and exception paths.
//
// The SDK implements DbgBreakPoint, RtlRaiseException (for C++ exceptions) and KeBugCheck(Ex) with a host debugger
// break, which ends the process with STATUS_BREAKPOINT (0x80000003) and no line in the log when no debugger is
// attached. The second boot (2026-10-23) ended that way. These replacements log what happened and the game's call
// stack, so the log says which path fired and from where:
//
//   DbgBreakPoint       the game's DebugBreak() (sub_829216A0 tail-jumps to it). Logged, then the game continues,
//                       as on a retail console with no debugger attached. The first 32 are logged.
//   RtlRaiseException   C++ throw (0xE06D7363): logs the thrown type names, then stops (the SDK cannot unwind
//                       guest exceptions yet). SetThreadName (0x406D1388): logs the name, continues. Others: stop.
//   KeBugCheck(Ex)      the game's fatal error: logs the code and parameters, then stops.
//
// They are app-level definitions of the import functions (__imp__*), which take priority over the SDK's: the
// translated code calls them as plain extern "C" functions.

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>

#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/logging/assert.h>

namespace {

uint32_t Load32(uint8_t* base, uint32_t addr) {
  uint32_t v;
  std::memcpy(&v, base + addr, 4);
  return __builtin_bswap32(v);
}

std::string LoadString(uint8_t* base, uint32_t addr, size_t max_len = 160) {
  std::string s;
  if (!addr) return s;
  for (size_t i = 0; i < max_len; ++i) {
    char c = static_cast<char>(base[addr + i]);
    if (!c) break;
    s += (c >= 0x20 && c < 0x7F) ? c : '?';
  }
  return s;
}

// The return addresses of the guest call stack. Xbox 360 frames: `mflr r12; stw r12,-8(r1); stwu r1,-N(r1)` (or
// __savegprlr_N, which stores r12 at the same place), so [r1] is the caller's frame and the caller's return address
// sits 8 bytes below that frame's top. ctx.lr holds the return address of the current call.
std::string GuestStack(PPCContext& ctx, uint8_t* base) {
  std::string s = fmt::format("lr={:08X}", static_cast<uint32_t>(ctx.lr));
  uint32_t fp = ctx.r1.u32;
  for (int i = 0; i < 16 && fp; ++i) {
    uint32_t prev = Load32(base, fp);
    if (prev <= fp || prev - fp > 0x100000) break;  // end of chain or not a plausible frame
    s += fmt::format(" <- {:08X}", Load32(base, prev - 8));
    fp = prev;
  }
  return s;
}

std::atomic<int> g_breaks{0};

}  // namespace

REX_HOOK_RAW(__imp__DbgBreakPoint) {
  int n = ++g_breaks;
  if (n <= 32) {
    REXLOG_ERROR("[los] game DebugBreak #{} (continuing, as on a retail console). Stack: {}", n,
                 GuestStack(ctx, base));
  } else if (n == 33) {
    REXLOG_ERROR("[los] more DebugBreak calls; no longer logged");
  }
}

REX_HOOK_RAW(__imp__RtlRaiseException) {
  const uint32_t rec = ctx.r3.u32;
  const uint32_t code = Load32(base, rec + 0x00);
  const uint32_t nparams = Load32(base, rec + 0x10);
  auto param = [&](uint32_t i) { return Load32(base, rec + 0x14 + 4 * i); };

  if (code == 0x406D1388) {  // SetThreadName: {type, name_ptr, thread_id, flags}
    REXLOG_INFO("[los] game names a thread: '{}'", LoadString(base, nparams >= 2 ? param(1) : 0, 64));
    return;
  }

  if (code == 0xE06D7363 && nparams >= 3) {  // MSVC C++ throw: {magic, thrown object, ThrowInfo}
    // ThrowInfo {attributes, unwind, forward_compat, catchable_type_array}; CatchableTypeArray {count, types[]};
    // CatchableType {properties, type_descriptor, ...}; TypeDescriptor {vftable, spare, name[] = ".?AV...@@"}.
    std::string types;
    const uint32_t throw_info = param(2);
    if (throw_info) {
      const uint32_t cta = Load32(base, throw_info + 12);
      const uint32_t count = cta ? Load32(base, cta) : 0;
      for (uint32_t i = 0; i < count && i < 6; ++i) {
        const uint32_t ct = Load32(base, cta + 4 + 4 * i);
        const uint32_t td = ct ? Load32(base, ct + 4) : 0;
        if (!types.empty()) types += ", ";
        types += td ? LoadString(base, td + 8) : "?";
      }
    }
    REX_FATAL("[los] game threw a C++ exception (object {:08X}), types [{}]. Stack: {}", param(1), types,
              GuestStack(ctx, base));
  }

  REX_FATAL("[los] game raised exception {:08X} ({} parameters: {:08X} {:08X} {:08X}). Stack: {}", code, nparams,
            nparams > 0 ? param(0) : 0, nparams > 1 ? param(1) : 0, nparams > 2 ? param(2) : 0,
            GuestStack(ctx, base));
}

REX_HOOK_RAW(__imp__KeBugCheckEx) {
  REX_FATAL("[los] game bug check {:08X} ({:08X}, {:08X}, {:08X}, {:08X}). Stack: {}", ctx.r3.u32, ctx.r4.u32,
            ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, GuestStack(ctx, base));
}

REX_HOOK_RAW(__imp__KeBugCheck) {
  REX_FATAL("[los] game bug check {:08X}. Stack: {}", ctx.r3.u32, GuestStack(ctx, base));
}
