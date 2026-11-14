#pragma once

// LA PALETTE D'ÉMOTICONES, MANIÉE À LA SOURIS PAR UN BANC : trouver son bouton
// sur une rangée, l'ouvrir, viser une case de sa grille.
//
// ⚠ PARTAGÉE PARCE QUE DEUX SAISIES L'OFFRENT — la bande de la conversation et
// la conversation 1:1 —, et que deux bancs qui la visent chacun à sa façon
// finiraient par chercher deux palettes différentes.
//
// `frame` dessine UNE image complète du banc ; tout se joue entre deux images.

#include <cstdint>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

namespace ro::test {

// Le pas du balayage, en pixels.
inline constexpr float kPaletteSweepStepPixels = 3.0F;
// Les images qu'il faut à un geste pour se poser.
inline constexpr int kPaletteSettleFrames = 3;
// L'identifiant du bouton de la palette, sous la pile d'ids de sa fenêtre.
inline constexpr const char* kEmoteButtonId = "##bouton_emotes";

// Un clic gauche à l'endroit où se trouve la souris, Maj tenue ou non.
template <typename Frame>
void ClickHere(bool shift, Frame frame) {
  ImGuiIO& io = ImGui::GetIO();
  io.AddKeyEvent(ImGuiMod_Shift, shift);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  frame();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  frame();
  io.AddKeyEvent(ImGuiMod_Shift, false);
}

// Balaie la rangée `row_y`, de droite à gauche, jusqu'à survoler `wanted`.
template <typename Frame>
[[nodiscard]] bool SweepRowFor(ImGuiID wanted, float row_y, float left, float right, Frame frame) {
  for (float x = right; x > left; x -= kPaletteSweepStepPixels) {
    ImGui::GetIO().AddMousePosEvent(x, row_y);
    frame();
    if (GImGui->HoveredId == wanted) {
      return true;
    }
  }
  return false;
}

// LA PALETTE EST OUVERTE : vise la case de cette émoticone, la première de la
// grille. Rend vrai quand elle est survolée.
template <typename Frame>
[[nodiscard]] bool AimAtFirstEmoteCell(std::uint16_t emotion_id, Frame frame) {
  if (GImGui->OpenPopupStack.empty()) {
    return false;
  }
  ImGuiWindow* palette = GImGui->OpenPopupStack.back().Window;
  if (palette == nullptr) {
    return false;
  }
  // LA GRILLE EST L'ENFANT QUI DÉFILE ; sa première case est dans son coin.
  ImGuiWindow* grid = nullptr;
  for (ImGuiWindow* child : palette->DC.ChildWindows) {
    if (std::string_view(child->Name).find("##defile_emotes") != std::string_view::npos) {
      grid = child;
    }
  }
  if (grid == nullptr) {
    return false;
  }
  const ImVec2 corner = grid->DC.CursorStartPos;
  ImGui::GetIO().AddMousePosEvent(corner.x + kPaletteSweepStepPixels,
                                  corner.y + kPaletteSweepStepPixels);
  for (int settle = 0; settle < kPaletteSettleFrames; ++settle) {
    frame();
  }
  const ImGuiID cell = ImHashStr("##emote", 0, grid->GetID(static_cast<int>(emotion_id)));
  return GImGui->HoveredId == cell;
}

}  // namespace ro::test
