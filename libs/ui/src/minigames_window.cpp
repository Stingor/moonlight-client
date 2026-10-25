#include "ro/ui/minigames_window.h"

#include <array>
#include <cstddef>
#include <cstdio>

#include <imgui.h>

#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_bounds.h"
#include "ro/ui/window_name.h"

#include "minigame_trophy_text.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

constexpr float kFirstWidth = 380.0F;
constexpr float kFirstHeight = 420.0F;

// Un trophée gagné en or, un trophée à gagner effacé.
constexpr ImU32 kUnlockedColor = IM_COL32(255, 215, 90, 255);
// Le fond de l'avis d'un trophée gagné, et sa marge.
constexpr ImU32 kToastBackground = IM_COL32(30, 24, 8, 220);
constexpr float kToastPadding = 6.0F;
// Ce que l'avis peut porter, zéro final compris : un nom de trophée est court.
constexpr std::size_t kToastTextSize = 160;

// LES TROPHÉES. ⚠ LEURS TEXTES SE TRADUISENT À LA LECTURE, par `TrophyText` : ils sont
// marqués par `TrText` dans le catalogue de `sim`.
void DrawTrophies(const MinigamesView& view) {
  int unlocked = 0;
  for (const MinigameTrophyRow& row : view.trophies) {
    unlocked += row.unlocked ? 1 : 0;
  }
  ImGui::SeparatorText(core::Tr("Trophées"));
  ImGui::Text(core::Tr("%d sur %d"), unlocked, static_cast<int>(view.trophies.size()));
  for (const MinigameTrophyRow& row : view.trophies) {
    if (row.unlocked) {
      ColoredText(kUnlockedColor, TrophyText(row.name));
    } else {
      FaintText(TrophyText(row.name));
    }
    ImGui::Indent();
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", TrophyText(row.description));
    ImGui::PopStyleColor();
    ImGui::Unindent();
  }
}

// UNE LIGNE DE JEU : son meilleur score, et le bouton qui l'ouvre.
[[nodiscard]] bool DrawGameLine(const char* name, int best, const SkinArt& art) {
  ImGui::SeparatorText(name);
  if (best > 0) {
    ImGui::Text(core::Tr("Meilleur score : %d"), best);
  } else {
    FaintText(core::Tr("Aucune partie finie"));
  }
  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - SkinButtonWidth(core::Tr("Jouer"), art));
  ImGui::PushID(name);
  const bool play = SkinButton(core::Tr("Jouer"), art);
  ImGui::PopID();
  return play;
}

// LA LIGNE DES SURVIVANTS : sans meilleur score avant les records (lot 6), le
// bouton ouvre la préparation.
[[nodiscard]] bool DrawSurvivorsLine(const SkinArt& art) {
  const char* name = core::Tr("Survivants de Midgard");
  ImGui::SeparatorText(name);
  ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - SkinButtonWidth(core::Tr("Jouer"), art));
  ImGui::PushID(name);
  const bool play = SkinButton(core::Tr("Jouer"), art);
  ImGui::PopID();
  FaintText(core::Tr("Survivez quinze minutes à la horde de Prontera Field."));
  return play;
}

}  // namespace

MinigamesAsk DrawMinigamesWindow(MinigamesState& state, const MinigamesView& view,
                                 const SkinArt& art) {
  MinigamesAsk asked;
  if (!state.open) {
    return asked;
  }
  static WindowName name(kMinigamesTitle, kMinigamesId);
  ImGui::SetNextWindowSize(ImVec2(kFirstWidth, kFirstHeight), ImGuiCond_FirstUseEver);
  ConstrainNextWindowToScreen(name.Get());
  if (!BeginSkinnedWindow(name.Get(), &state.open, art)) {
    EndSkinnedWindow();
    return asked;
  }
  asked.open_rojeweled = DrawGameLine(core::Tr("Rojeweled"), view.rojeweled_best, art);
  FaintText(core::Tr("Alignez trois monstres de la même couleur, en trente coups."));
  asked.open_roggle = DrawGameLine(core::Tr("Roggle"), view.roggle_best, art);
  FaintText(core::Tr("Effacez tous les plots orange avant d'épuiser vos billes."));
  if (view.survivors_offered) {
    asked.open_survivors = DrawSurvivorsLine(art);
  }
  DrawTrophies(view);
  EndSkinnedWindow();
  return asked;
}

void DrawTrophyToast(const char* name, float alpha, ImVec2 board_corner, float board_width) {
  if (name == nullptr || alpha <= 0.0F) {
    return;
  }
  std::array<char, kToastTextSize> text = {};
  std::snprintf(text.data(), text.size(), core::Tr("Trophée gagné : %s"), TrophyText(name));
  const ImVec2 size = ImGui::CalcTextSize(text.data());
  const ImVec2 corner(board_corner.x + (board_width - size.x) * 0.5F - kToastPadding,
                      board_corner.y + kToastPadding);
  const ImVec2 far(corner.x + size.x + 2.0F * kToastPadding,
                   corner.y + size.y + 2.0F * kToastPadding);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(corner, far, ImGui::GetColorU32(kToastBackground, alpha), kToastPadding);
  draw->AddText(ImVec2(corner.x + kToastPadding, corner.y + kToastPadding),
                ImGui::GetColorU32(kUnlockedColor, alpha), text.data());
}

}  // namespace ro::ui
