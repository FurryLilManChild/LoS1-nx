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
//   los_engine_vars = "R_ShadowMaps=0;R_LightLimit=2"   set these (every second and at every Swap: the game sets some
//                                                       back itself, R_DistMLOD1 every frame)
//   los_engine_vars_ab = 20     the phase length in seconds: alternate between los_engine_vars and the game's own
//                               values (0 = los_engine_vars always)
//   los_engine_vars_cycle = "|R_ShadowMaps=0||R_LightLimit=1"   instead: phases separated by '|', each a set of
//                               settings or empty (the game's own values), in turn, los_engine_vars_ab seconds each
//                               (20 if 0), round and round
//   los_engine_vars_dump = true every variable, its type and value, in the log once
// Each phase's end logs its frame rate, counted from the Swaps of that phase alone ([engine vars] phase N ... fps), so
// the phases need not line up with the 10 s reports (the shadows test of 2026-10-28: half of its 10 s reports
// straddled two phases).
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
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
REXCVAR_DEFINE_STRING(los_engine_vars_cycle, "", "LoS1",
                      "Test: phases of engine variable settings separated by '|' (an empty phase = the game's own "
                      "values), los_engine_vars_ab seconds each, round and round, each phase's frame rate logged");
REXCVAR_DEFINE_BOOL(los_engine_vars_dump, false, "LoS1",
                    "Test: write every engine console variable with its type and value to the log once");

namespace los {
namespace {

constexpr uint32_t kListHead = 0x82D1AFDC;

bool GuestAddress(uint32_t va) {
  return va >= 0x40000000u && va < 0xA0000000u;  // heap and image (the newest variables are on the heap)
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

// The settings in force (phase A), kept by EngineVarsOnSwap at every Swap too: the game sets some of its variables
// again on its own (R_DistMLOD1 every frame, Linux run 2026-10-28), faster than the thread's once a second.
constexpr size_t kMaxActive = 32;
struct Active {
  std::atomic<uint32_t> va{0};
  std::atomic<uint32_t> word{0};
};
Active g_active[kMaxActive];
std::atomic<uint32_t> g_swaps{0};  // every Swap, for the phases' frame rates

// The port menu's settings (los_menu.cpp: the shadows choice), in force whenever no test key is set.
std::mutex g_menu_mutex;
std::string g_menu_spec;

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

std::vector<std::string> Split(const std::string& text, char sep) {
  std::vector<std::string> out;
  size_t at = 0;
  for (;;) {
    const size_t end = text.find(sep, at);
    out.push_back(text.substr(at, end == std::string::npos ? std::string::npos : end - at));
    if (end == std::string::npos) return out;
    at = end + 1;
  }
}

// The word to write for a setting into a variable of its type.
uint32_t WordFor(const Setting& s, const Var& v) {
  uint32_t word = s.word;
  if (v.type == 4 && !s.is_float) {
    const float f = float(int32_t(s.word));
    std::memcpy(&word, &f, 4);
  } else if (v.type == 2 && s.is_float) {
    float f;
    std::memcpy(&f, &s.word, 4);
    word = uint32_t(int32_t(f));
  }
  return word;
}

void Run() {
  std::this_thread::sleep_for(std::chrono::seconds(8));
  // The phases: a cycle, or los_engine_vars against the game's own values, or los_engine_vars always.
  std::vector<std::string> specs;
  const std::string cycle = REXCVAR_GET(los_engine_vars_cycle);
  int seconds = REXCVAR_GET(los_engine_vars_ab);
  if (!cycle.empty()) {
    specs = Split(cycle, '|');
    if (seconds <= 0) seconds = 20;
  } else if (!REXCVAR_GET(los_engine_vars).empty()) {
    specs.push_back(REXCVAR_GET(los_engine_vars));
    if (seconds > 0) specs.push_back("");
  }
  std::vector<std::vector<Setting>> phases;
  for (const std::string& spec : specs) phases.push_back(Parse(spec));
  const bool test = !specs.empty();  // a test key wins over the port menu
  bool dumped = !REXCVAR_GET(los_engine_vars_dump);

  struct Touched {
    std::string name;
    uint32_t va = 0;
    uint32_t game_word = 0;  // the game's value, from before the phase that set it
    bool ours = false;       // set by the current phase
  };
  std::vector<Touched> touched;
  int phase = -1, missing_logged = 0;
  uint32_t phase_swaps = 0;
  auto phase_start = std::chrono::steady_clock::now();
  const auto start = phase_start;

  for (;;) {
    const auto now = std::chrono::steady_clock::now();
    const auto secs = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
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
    if (!test) {
      // The port menu's settings, as one phase that changes when the player changes them.
      std::string menu;
      {
        std::lock_guard<std::mutex> lock(g_menu_mutex);
        menu = g_menu_spec;
      }
      if (specs.empty() || specs[0] != menu) {
        specs = {menu};
        phases = {Parse(menu)};
        if (phase >= 0) {
          for (size_t k = 0; k < kMaxActive; ++k) g_active[k].va.store(0, std::memory_order_release);
          for (Touched& t : touched)
            if (t.ours) {
              Wr32(t.va, t.game_word);
              t.ours = false;
            }
        }
        phase = -1;
      }
    }
    const int want = seconds > 0 && phases.size() > 1 ? int((secs / seconds) % int64_t(phases.size())) : 0;
    if (want != phase) {
      // The phase that ends: its frame rate from its own Swaps.
      const uint32_t swaps = g_swaps.load(std::memory_order_relaxed);
      if (phase >= 0) {
        const double t = std::chrono::duration<double>(now - phase_start).count();
        REXLOG_INFO("[engine vars] phase {} ({}) ended: {} frames in {:.1f} s = {:.1f} fps", phase,
                    specs[phase].empty() ? "the game's own values" : specs[phase], swaps - phase_swaps, t,
                    t > 0 ? double(swaps - phase_swaps) / t : 0.0);
      }
      phase_swaps = swaps;
      phase_start = now;
      // Off with the old phase's settings: the game's values back.
      for (size_t k = 0; k < kMaxActive; ++k) g_active[k].va.store(0, std::memory_order_release);
      for (Touched& t : touched)
        if (t.ours) {
          Wr32(t.va, t.game_word);
          t.ours = false;
        }
      phase = want;
      REXLOG_INFO("[engine vars] phase {} at {} s: {}", phase, secs,
                  specs[phase].empty() ? "the game's own values" : specs[phase]);
    }
    // The current phase's settings: kept every second here and at every Swap (EngineVarsOnSwap).
    size_t slot = 0;
    for (const Setting& s : phases[phase]) {
      const Var* found = nullptr;
      for (const Var& v : vars)
        if (v.name == s.name) found = &v;
      if (!found || (found->type != 2 && found->type != 4)) {
        if (missing_logged++ < 32)
          REXLOG_WARN("[engine vars] {}: {}", s.name, found ? "not an integer or float variable" : "not found");
        continue;
      }
      const uint32_t va = found->va + 44, word = WordFor(s, *found);
      Touched* t = nullptr;
      for (Touched& x : touched)
        if (x.name == s.name) t = &x;
      if (!t) {
        touched.push_back({s.name, va, 0, false});
        t = &touched.back();
      }
      if (!t->ours) {
        t->va = va;
        t->game_word = Rd32(va);
        t->ours = true;
        REXLOG_INFO("[engine vars]   {}: {} -> {}", s.name, ValueText(*found, t->game_word), ValueText(*found, word));
      }
      if (Rd32(va) != word) Wr32(va, word);
      if (slot < kMaxActive) {
        g_active[slot].word.store(word, std::memory_order_relaxed);
        g_active[slot].va.store(va, std::memory_order_release);
        ++slot;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
  }
}

}  // namespace

// From the Swap hook (sa2_renut_pacing.cpp, the game thread): the settings again, before the next frame.
void EngineVarsOnSwap() {
  g_swaps.fetch_add(1, std::memory_order_relaxed);
  for (Active& a : g_active) {
    const uint32_t va = a.va.load(std::memory_order_acquire);
    if (!va) continue;
    const uint32_t word = a.word.load(std::memory_order_relaxed);
    if (Rd32(va) != word) Wr32(va, word);
  }
}

// The port menu's settings (NAME=VALUE;...), kept from the next second on.
void SetMenuEngineVars(const std::string& spec) {
  std::lock_guard<std::mutex> lock(g_menu_mutex);
  g_menu_spec = spec;
}

void StartEngineVars() {
  new std::thread(Run);  // runs for the session (never joined: the process ends with it)
}

}  // namespace los
