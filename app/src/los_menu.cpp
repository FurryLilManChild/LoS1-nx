// LoS1-nx: the port menu (2026-10-29).
//
// Asked for: a menu where a player changes the performance settings, with presets from the stock-clock tests
// (docs/progress-log.md, 2026-10-28 and 2026-10-29). Opened with - and + together on the Switch
// (switch_input_driver.cpp sends F5), F5 on the PC. Every choice applies at once; "Save and close" writes it to
// los.toml so it holds at the next start.
//
// The settings:
//   los_shadows              full (the game's own) / world (R_ShadowMapsMask=1: the world casts shadows, not the
//                            characters; +9 % in the heavy test scene) / off (R_ShadowMaps=0: +35 %)
//   los_handheld_resolution  auto / 540 / 720 (los_stock_clocks.cpp)
//   los_fps_cap              -1 auto / 0 none / 30 (los_stock_clocks.cpp)
// Presets: Quality = full shadows, 1280x720; Balanced = world shadows, auto; Performance = no shadows, 960x540.
// The frame cap stays the player's choice (auto in all three).
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <imgui.h>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>
#include <rex/ui/keybinds.h>

REXCVAR_DEFINE_STRING(los_shadows, "full", "LoS1",
                      "Shadows: full (the game's own), world (the world casts shadows, the characters do not; "
                      "faster), off (fastest). Changed from the port menu (- and + together)")
    .allowed({"full", "world", "off"});

namespace los {
void ApplyOptionsNow();                            // los_stock_clocks.cpp
std::string ClockStatus();                         // los_stock_clocks.cpp
void SetMenuEngineVars(const std::string& spec);  // los_engine_vars.cpp

namespace {

const char* SpecForShadows(const std::string& v) {
  if (v == "world") return "R_ShadowMapsMask=1";
  if (v == "off") return "R_ShadowMaps=0";
  return "";  // full: the game's own values
}

struct Preset {
  const char* name;
  const char* shadows;
  const char* resolution;
  const char* help;
};
constexpr Preset kPresets[] = {
    {"Quality", "full", "720", "All shadows, 1280x720 in handheld"},
    {"Balanced", "world", "auto", "World shadows only, resolution by clock"},
    {"Performance", "off", "540", "No shadows, 960x540 in handheld"},
};

std::filesystem::path g_config_path;
constexpr float kScale = 2.0f;  // padding and widths (1.0 was too small in handheld)
ImFont* g_font = nullptr;       // ImGui's default font at 26 px (twice its 13 px design, so it stays sharp)
std::atomic<bool> g_open{false};

void SetShadows(const std::string& v) {
  rex::cvar::SetFlagByName("los_shadows", v);
  SetMenuEngineVars(SpecForShadows(v));
}

void SetResolution(const std::string& v) {
  rex::cvar::SetFlagByName("los_handheld_resolution", v);
  ApplyOptionsNow();
}

void SetCap(int v) {
  rex::cvar::SetFlagByName("los_fps_cap", std::to_string(v));
  ApplyOptionsNow();
}

std::string Trim(const std::string& s) {
  size_t a = s.find_first_not_of(" \t\r");
  if (a == std::string::npos) return "";
  return s.substr(a, s.find_last_not_of(" \t\r") - a + 1);
}

// The key a line sets ("name = value"), or "" for comments and blank lines.
std::string KeyOf(const std::string& line) {
  const std::string t = Trim(line);
  if (t.empty() || t[0] == '#' || t[0] == '[') return "";
  const size_t eq = t.find('=');
  return eq == std::string::npos ? "" : Trim(t.substr(0, eq));
}

// Rewrites the menu's three keys in los.toml, keeping every other line as it is. An older
// sa2_render_height_handheld line is commented out: los_handheld_resolution replaces it.
bool SaveToConfig(std::string* error) {
  if (g_config_path.empty()) {
    *error = "no los.toml path";
    return false;
  }
  std::vector<std::string> lines;
  {
    std::ifstream in(g_config_path);
    std::string line;
    while (std::getline(in, line)) lines.push_back(line);
  }
  struct Key {
    const char* name;
    std::string value;
    bool written = false;
  };
  Key keys[] = {
      {"los_shadows", "\"" + REXCVAR_GET(los_shadows) + "\""},
      {"los_handheld_resolution", "\"" + rex::cvar::GetFlagByName("los_handheld_resolution") + "\""},
      {"los_fps_cap", rex::cvar::GetFlagByName("los_fps_cap")},
  };
  for (std::string& line : lines) {
    const std::string key = KeyOf(line);
    if (key == "sa2_render_height_handheld") {
      line = "# " + line + "  (replaced by los_handheld_resolution, the port menu)";
      continue;
    }
    for (Key& k : keys)
      if (key == k.name) {
        line = std::string(k.name) + " = " + k.value;
        k.written = true;
      }
  }
  bool header = false;
  for (Key& k : keys)
    if (!k.written) {
      if (!header) {
        lines.push_back("");
        lines.push_back("# The port menu's settings (- and + together).");
        header = true;
      }
      lines.push_back(std::string(k.name) + " = " + k.value);
    }
  std::ofstream out(g_config_path, std::ios::binary | std::ios::trunc);
  if (!out) {
    *error = "could not write " + g_config_path.string();
    return false;
  }
  for (const std::string& line : lines) out << line << "\n";
  out.close();
  if (!out) {
    *error = "could not write " + g_config_path.string();
    return false;
  }
  REXLOG_INFO("[menu] saved to {}: shadows {}, handheld resolution {}, frame cap {}", g_config_path.string(),
              keys[0].value, keys[1].value, keys[2].value);
  return true;
}

// A row's label, then its choices on the same line.
void Label(const char* text) {
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(text);
  ImGui::SameLine(170 * kScale);
}

class LosMenuDialog final : public rex::ui::ImGuiDialog {
 public:
  explicit LosMenuDialog(rex::ui::ImGuiDrawer* drawer) : ImGuiDialog(drawer) { g_open = true; }
  void RequestClose() { Close(); }

 protected:
  void OnClose() override;

  void OnDraw(ImGuiIO& io) override {
    // Large on purpose (Mark, 2026-10-29: the first build's text was hard to read in handheld): the text at
    // kScale of the overlay font, padding and spacing with it, the window most of the screen's width.
    const float s = kScale;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10 * s, 4 * s));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12 * s, 5 * s));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14 * s, 12 * s));
    const ImVec2 size(std::min(io.DisplaySize.x * 0.92f, 620 * s), 0);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92f);
    if (first_) ImGui::SetNextWindowFocus();
    if (!ImGui::Begin("Lords of Shadow: port menu##los", nullptr,
                      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::End();
      ImGui::PopStyleVar(3);
      return;
    }
    if (g_font) ImGui::PushFont(g_font, g_font->LegacySize);
    if (first_) {
      ImGui::SetKeyboardFocusHere();
      first_ = false;
    }

    const std::string shadows = REXCVAR_GET(los_shadows);
    const std::string res = rex::cvar::GetFlagByName("los_handheld_resolution");
    const std::string cap_text = rex::cvar::GetFlagByName("los_fps_cap");
    const int cap = cap_text == "0" ? 0 : cap_text == "30" ? 30 : -1;

    ImGui::TextUnformatted("Presets");
    for (const Preset& p : kPresets) {
      const bool on = shadows == p.shadows && res == p.resolution;
      if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
      if (ImGui::Button(p.name, ImVec2(180 * s, 0))) {
        SetShadows(p.shadows);
        SetResolution(p.resolution);
      }
      if (on) ImGui::PopStyleColor();
      if (&p != &kPresets[2]) ImGui::SameLine();
    }
    const char* help = "Custom";
    for (const Preset& p : kPresets)
      if (shadows == p.shadows && res == p.resolution) help = p.help;
    ImGui::TextDisabled("%s", help);
    ImGui::Separator();

    Label("Shadows");
    if (ImGui::RadioButton("Full##sh", shadows == "full")) SetShadows("full");
    ImGui::SameLine();
    if (ImGui::RadioButton("World only##sh", shadows == "world")) SetShadows("world");
    ImGui::SameLine();
    if (ImGui::RadioButton("Off##sh", shadows == "off")) SetShadows("off");

    Label("Resolution");
    if (ImGui::RadioButton("Auto##res", res == "auto")) SetResolution("auto");
    ImGui::SameLine();
    if (ImGui::RadioButton("960x540##res", res == "540")) SetResolution("540");
    ImGui::SameLine();
    if (ImGui::RadioButton("1280x720##res", res == "720")) SetResolution("720");
    ImGui::TextDisabled("Handheld. Auto: 960x540 at the stock CPU clock, 1280x720 overclocked");

    Label("Frame cap");
    if (ImGui::RadioButton("Auto##cap", cap == -1)) SetCap(-1);
    ImGui::SameLine();
    if (ImGui::RadioButton("30 fps##cap", cap == 30)) SetCap(30);
    ImGui::SameLine();
    if (ImGui::RadioButton("None##cap", cap == 0)) SetCap(0);
    ImGui::TextDisabled("Handheld. Auto: 30 fps at the stock CPU clock, none overclocked");
    ImGui::Separator();

    ImGui::TextWrapped("%s", ClockStatus().c_str());
    ImGui::Separator();

    if (ImGui::Button("Save and close", ImVec2(270 * s, 0))) {
      std::string error;
      if (SaveToConfig(&error)) {
        Close();
      } else {
        message_ = "Not saved: " + error;
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Close", ImVec2(270 * s, 0))) Close();
    ImGui::TextDisabled("Changes apply at once. Close keeps them until the game is restarted.");
    if (!message_.empty()) ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "%s", message_.c_str());
    if (g_font) ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleVar(3);
  }

 private:
  bool first_ = true;
  std::string message_;
};

LosMenuDialog* g_menu = nullptr;  // the UI thread's only

void LosMenuDialog::OnClose() {
  g_menu = nullptr;
  g_open = false;
}

}  // namespace

// At start: the saved shadows choice to the engine variables (before StartEngineVars).
void ApplyMenuSettings() {
  SetMenuEngineVars(SpecForShadows(REXCVAR_GET(los_shadows)));
  REXLOG_INFO("[menu] shadows {}, handheld resolution {}, frame cap {}", REXCVAR_GET(los_shadows),
              rex::cvar::GetFlagByName("los_handheld_resolution"), rex::cvar::GetFlagByName("los_fps_cap"));
}

bool MenuOpen() {
  return g_open.load(std::memory_order_relaxed);
}

// From LosApp::OnConfigureFonts: the menu's own large font, baked into the overlay's font atlas.
void AddMenuFont(ImFontAtlas* atlas) {
  ImFontConfig cfg;
  cfg.SizePixels = 26.0f;
  cfg.OversampleH = cfg.OversampleV = 1;
  cfg.PixelSnapH = true;
  g_font = atlas->AddFontDefault(&cfg);
}

// From LosApp::OnCreateDialogs: - and + together (Switch) or F5 (PC) opens and closes the menu.
void RegisterMenu(rex::ui::ImGuiDrawer* drawer, const std::filesystem::path& config_path) {
  g_config_path = config_path;
  rex::ui::RegisterBind("bind_los_menu", "F5", "Toggle the port menu (- and + on the Switch)", [drawer] {
    if (g_menu) {
      g_menu->RequestClose();
    } else {
      g_menu = new LosMenuDialog(drawer);
    }
  });
}

}  // namespace los
