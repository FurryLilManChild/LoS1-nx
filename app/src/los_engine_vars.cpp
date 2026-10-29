// LoS1-nx: the game engine's own console variables, read and set from los.toml (2026-10-28).
//
// Why: at stock clocks the heavy scenes are bound by the main game thread (los_stock_clocks.cpp: 67 % of it the game's
// own code, no hot spot), so the remaining lever is making the game do less. The engine has a console-variable
// system with the PC version's detail switches (strings in the decrypted image, 2026-10-28: R_ShadowMaps "Enable
// shadow maps", R_LightShadows, R_LightLimit "Max. enabled lights per model (0..6)", R_UseMLOD "Use model LODs",
// R_DrawMLODMin "Set most detailed LOD to draw (1..n)", R_DistMLOD1 "Model LOD 1 distance coef.", ...), and its Lua
// has the PC's video menu (Advanced_Shadows, Advanced_TextureLOD, SetShadowSettings) and a debug menu over them.
//
// The variables, read from the generated code of their static constructors (e.g. R_ShadowMaps, los_recomp.85.cpp;
// the constructor sub_821A2290, los_recomp.18.cpp):
//   +0  vtable            +28 next variable (the list's head at 0x82D1AFDC, newest first)
//   +32 name (char *)     +36 description (char *)
//   +40 flags (byte)      +41 type (byte: 2 integer, 4 float; others not touched)
//   +44 value (big-endian s32 or f32)    +48 default
//
// Keys (los.toml):
//   los_engine_vars = "R_ShadowMaps=0;R_LightLimit=2"   set these, and again every second (the game's scripts set
//                                                       some per scene: SetShadowSettings)
//   los_engine_vars_ab = 20     alternate every 20 s between the set above and the game's own values, logging each
//                               switch ([engine vars] A / B), so one session measures both (0 = always set)
//   los_engine_vars_dump = true every variable, its type and value, in the log once (20 s after start)
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <rex/cvar.h>
#include <rex/logging.h>

#include "renut_engine/gpu/guest/guest_memory.h"

REXCVAR_DEFINE_STRING(los_engine_vars, "", "LoS1",
                      "The game engine's own console variables to set, as NAME=VALUE;NAME=VALUE (integer or float "
                      "variables), applied every second");
REXCVAR_DEFINE_INT32(los_engine_vars_ab, 0, "LoS1",
                     "Test: alternate every this many seconds between los_engine_vars and the game's own values "
                     "(0 = always los_engine_vars)")
    .range(0, 600);
REXCVAR_DEFINE_BOOL(los_engine_vars_dump, false, "LoS1",
                    "Test: write every engine console variable with its type and value to the log once");

namespace los {
namespace {

constexpr uint32_t kListHead = 0x82D1AFDC;

bool GuestAddress(uint32_t va) {
  return va >= 0x82000000u && va < 0x90000000u;
}

uint32_t Rd32(uint32_t va) {
  uint32_t v;
  std::memcpy(&v, ::renut::gpu::GuestPtr<uint8_t>(va), 4);
  return __builtin_bswap32(v);
}
void Wr32(uint32_t va, uint32_t v) {
  v = __builtin_bswap32(v);
  std::memcpy(::renut::gpu::GuestPtr<uint8_t>(va), &v, 4);
}
uint8_t Rd8(uint32_t va) {
  return *::renut::gpu::GuestPtr<uint8_t>(va);
}
std::string Str(uint32_t va) {
  std::string s;
  if (!GuestAddress(va)) return s;
  for (int i = 0; i < 96; ++i) {
    const char c = char(Rd8(va + i));
    if (!c) break;
    if (c < 32 || c > 126) return std::string();
    s += c;
  }
  return s;
}

struct Var {
  uint32_t va;
  std::string name;
  uint8_t type;
};

std::vector<Var> Walk() {
  std::vector<Var> out;
  uint32_t node = Rd32(kListHead);
  for (int n = 0; n < 8192 && GuestAddress(node); ++n) {
    Var v{node, Str(Rd32(node + 32)), Rd8(node + 41)};
    if (v.name.empty()) break;
    out.push_back(std::move(v));
    node = Rd32(node + 28);
  }
  return out;
}

std::string ValueText(const Var& v, uint32_t word) {
  if (v.type == 4) {
    float f;
    std::memcpy(&f, &word, 4);
    return std::to_string(f);
  }
  if (v.type == 2) return std::to_string(int32_t(word));
  return "0x" + [&] {
    char b[16];
    std::snprintf(b, sizeof b, "%08X", word);
    return std::string(b);
  }();
}

struct Setting {
  std::string name, text;
  bool is_float;
  uint32_t word;
};

std::vector<Setting> Parse(const std::string& spec) {
  std::vector<Setting> out;
  size_t at = 0;
  while (at < spec.size()) {
    size_t end = spec.find(';', at);
    if (end == std::string::npos) end = spec.size();
    const std::string item = spec.substr(at, end - at);
    at = end + 1;
    const size_t eq = item.find('=');
    if (eq == std::string::npos) continue;
    Setting s;
    s.name = item.substr(0, eq);
    s.text = item.substr(eq + 1);
    while (!s.name.empty() && s.name.back() == ' ') s.name.pop_back();
    while (!s.name.empty() && s.name.front() == ' ') s.name.erase(0, 1);
    s.is_float = s.text.find('.') != std::string::npos;
    const float f = float(std::atof(s.text.c_str()));
    const int32_t i = int32_t(std::atoi(s.text.c_str()));
    if (s.is_float)
      std::memcpy(&s.word, &f, 4);
    else
      s.word = uint32_t(i);
    if (!s.name.empty()) out.push_back(s);
  }
  return out;
}

void Run() {
  std::this_thread::sleep_for(std::chrono::seconds(8));
  const std::vector<Setting> settings = Parse(REXCVAR_GET(los_engine_vars));
  const int ab = REXCVAR_GET(los_engine_vars_ab);
  bool dumped = !REXCVAR_GET(los_engine_vars_dump);
  std::vector<uint32_t> saved;  // the game's own values, for the B phases
  int last_phase = -1, missing_logged = 0;
  const auto start = std::chrono::steady_clock::now();
  for (;;) {
    const auto secs = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count();
    if (!Rd32(kListHead)) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      continue;
    }
    const std::vector<Var> vars = Walk();
    if (!dumped && secs >= 12) {
      dumped = true;
      REXLOG_INFO("[engine vars] {} variables:", vars.size());
      for (const Var& v : vars)
        REXLOG_INFO("[engine vars]   {} type {} = {} (default {})", v.name, int(v.type),
                    ValueText(v, Rd32(v.va + 44)), ValueText(v, Rd32(v.va + 48)));
    }
    if (!settings.empty()) {
      const int phase = ab > 0 ? int((secs / ab) % 2) : 0;  // 0 = A (set), 1 = B (the game's)
      if (saved.size() != settings.size()) saved.assign(settings.size(), 0);
      for (size_t k = 0; k < settings.size(); ++k) {
        const Setting& s = settings[k];
        const Var* found = nullptr;
        for (const Var& v : vars)
          if (v.name == s.name) found = &v;
        if (!found || (found->type != 2 && found->type != 4)) {
          if (missing_logged++ < 32)
            REXLOG_WARN("[engine vars] {}: {}", s.name, found ? "not an integer or float variable" : "not found");
          continue;
        }
        uint32_t word = s.word;
        if (found->type == 4 && !s.is_float) {
          const float f = float(int32_t(s.word));
          std::memcpy(&word, &f, 4);
        } else if (found->type == 2 && s.is_float) {
          float f;
          std::memcpy(&f, &s.word, 4);
          word = uint32_t(int32_t(f));
        }
        const uint32_t now_word = Rd32(found->va + 44);
        if (phase == 0) {
          if (last_phase != 0) saved[k] = now_word;  // the game's value, from before A
          if (now_word != word) Wr32(found->va + 44, word);
        } else if (last_phase == 0) {
          Wr32(found->va + 44, saved[k]);  // back to the game's value for B
        }
      }
      if (phase != last_phase) {
        REXLOG_INFO("[engine vars] {} at {} s: {}", phase == 0 ? "A (los_engine_vars set)" : "B (the game's own values)",
                    secs, REXCVAR_GET(los_engine_vars));
        last_phase = phase;
      }
    } else if (dumped) {
      return;  // nothing to set, the dump written
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

}  // namespace

void StartEngineVars() {
  if (REXCVAR_GET(los_engine_vars).empty() && !REXCVAR_GET(los_engine_vars_dump)) return;
  new std::thread(Run);  // runs for the session (never joined: the process ends with it)
}

}  // namespace los
