#include "ro/ui/survivors_chest_window.h"

#include <array>
#include <cstddef>
#include <cstdio>

#include "ro/core/i18n.h"
#include "ro/ui/skin_look.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

#include "ui_text.h"

namespace ro::ui {
namespace {

inline constexpr const char* kChestTitle = core::TrText("Old Blue Box");
inline constexpr const char* kChestId = "survivors_chest";

constexpr float kRouletteSide = 96.0F;
constexpr float kRowIconSide = 32.0F;
constexpr float kListWidth = 320.0F;
constexpr ImU32 kRouletteBack = IM_COL32(10, 14, 30, 255);
constexpr ImU32 kRouletteEdge = IM_COL32(255, 215, 90, 255);
// Sur le corps clair de la liste des gains.
constexpr ImU32 kEvolveColor = kSkinEvolveTextColor;
constexpr std::size_t kLineSize = 96;
constexpr ImGuiWindowFlags kModalFlags =
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;

// La roulette : un cadre où défilent les icônes. Vrai quand on la clique.
[[nodiscard]] bool DrawRoulette(const SurvivorsChestView& view) {
  const ImVec2 corner = ImGui::GetCursorScreenPos();
  const bool clicked = ImGui::InvisibleButton("##roulette", ImVec2(kRouletteSide, kRouletteSide));
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImVec2 far(corner.x + kRouletteSide, corner.y + kRouletteSide);
  draw->AddRectFilled(corner, far, kRouletteBack);
  if (!view.roulette_icons.empty()) {
    const std::size_t shown =
        static_cast<std::size_t>(view.elapsed_ms / kRouletteStepMs) % view.roulette_icons.size();
    if (view.roulette_icons[shown] != 0) {
      draw->AddImage(view.roulette_icons[shown], corner, far);
    }
  }
  draw->AddRect(corner, far, kRouletteEdge);
  FaintText(core::Tr("Cliquez pour passer."));
  return clicked;
}

void DrawGain(const SurvivorsChestGainRow& gain) {
  std::array<char, kLineSize> line = {};
  if (gain.icon != 0) {
    ImGui::Image(gain.icon, ImVec2(kRowIconSide, kRowIconSide));
  } else {
    ImGui::Dummy(ImVec2(kRowIconSide, kRowIconSide));
  }
  ImGui::SameLine();
  switch (gain.look) {
    case SurvivorsChestGainLook::kEvolution:
      std::snprintf(line.data(), line.size(), core::Tr("Évolution : %s"), gain.name);
      ColoredText(kEvolveColor, line.data());
      break;
    case SurvivorsChestGainLook::kLevel:
      std::snprintf(line.data(), line.size(), core::Tr("%s — niveau %d"), gain.name, gain.level);
      ImGui::TextUnformatted(line.data());
      break;
    case SurvivorsChestGainLook::kZeny:
      ImGui::TextUnformatted(core::Tr("Zeny"));
      break;
  }
}

}  // namespace

bool DrawSurvivorsChestWindow(const SurvivorsChestView& view, const SkinArt& art) {
  static WindowName name(kChestTitle, kChestId);
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(screen->GetCenter(), ImGuiCond_Always, ImVec2(0.5F, 0.5F));
  ImGui::SetNextWindowFocus();
  if (!BeginSkinnedWindow(name.Get(), nullptr, art, SkinnedTitle::kBar, kModalFlags)) {
    EndSkinnedWindow();
    return false;
  }
  bool proceed = false;
  if (!view.list_shown) {
    proceed = DrawRoulette(view);
  } else {
    ImGui::Dummy(ImVec2(kListWidth, 0.0F));
    for (const SurvivorsChestGainRow& gain : view.gains) {
      DrawGain(gain);
    }
    proceed = SkinButton(core::Tr("Continuer"), art, ImVec2(-1.0F, 0.0F));
  }
  EndSkinnedWindow();
  return proceed && view.accepts_input;
}

}  // namespace ro::ui
