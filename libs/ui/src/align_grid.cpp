#include "ro/ui/align_grid.h"

#include <algorithm>
#include <cmath>

#include <imgui_internal.h>

namespace ro::ui {
namespace {

// L'épaisseur de la croix centrale : assez pour se distinguer du maillage sans
// devenir une barre.
constexpr float kCenterThickness = 1.5F;

// Où tombe la première ligne d'un axe. ⚠ LE MÊME CALCUL POUR LE DESSIN ET POUR
// L'AIMANT : la grille est ancrée sur le CENTRE de l'écran, pour que le
// maillage reste symétrique et que la croix du milieu tombe sur une ligne. Deux
// ancrages différents feraient coller les fenêtres à côté des lignes qu'on voit.
[[nodiscard]] float FirstLine(float extent, float step) { return std::fmod(extent * 0.5F, step); }

}  // namespace

float AlignGrid::step() const noexcept {
  return static_cast<float>(
      std::clamp(cell_pixels, kSmallestGridCellPixels, kLargestGridCellPixels));
}

float AlignGrid::Snap(float at, float extent) const noexcept {
  if (!magnetic) {
    return at;
  }
  const float pitch = step();
  const float first = FirstLine(extent, pitch);
  return first + pitch * std::floor((at - first) / pitch + 0.5F);
}

void DrawAlignGrid(const AlignGrid& grid) {
  if (!grid.shown) {
    return;
  }
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  const float pitch = grid.step();
  // ⚠ LA LISTE DE FOND : par-dessus le monde, SOUS toutes les fenêtres. Une
  // grille peinte au premier plan quadrillerait ce qu'on cherche à aligner.
  ImDrawList* into = ImGui::GetBackgroundDrawList();
  for (float x = FirstLine(screen.x, pitch); x <= screen.x; x += pitch) {
    into->AddLine(ImVec2(x, 0.0F), ImVec2(x, screen.y), grid.color);
  }
  for (float y = FirstLine(screen.y, pitch); y <= screen.y; y += pitch) {
    into->AddLine(ImVec2(0.0F, y), ImVec2(screen.x, y), grid.color);
  }
  // La croix du centre, plus franche : c'est elle qu'on vise pour centrer une
  // fenêtre, et la chercher dans un maillage régulier coûte un temps bête.
  ImVec4 bright = ImGui::ColorConvertU32ToFloat4(grid.color);
  bright.w = std::min(1.0F, bright.w * kGridCenterBoost);
  const ImU32 middle = ImGui::ColorConvertFloat4ToU32(bright);
  into->AddLine(ImVec2(screen.x * 0.5F, 0.0F), ImVec2(screen.x * 0.5F, screen.y), middle,
                kCenterThickness);
  into->AddLine(ImVec2(0.0F, screen.y * 0.5F), ImVec2(screen.x, screen.y * 0.5F), middle,
                kCenterThickness);
}

void SnapMovingWindow(const AlignGrid& grid, const EarlierMagnet& earlier) {
  ImGuiContext* context = ImGui::GetCurrentContext();
  if (context == nullptr || context->MovingWindow == nullptr) {
    return;
  }
  ImGuiWindow* window = context->MovingWindow->RootWindow;
  if (window == nullptr) {
    return;
  }
  WindowMagnetCatch caught{.at = window->Pos};
  if (grid.windows_magnetic && !context->IO.KeyAlt) {
    caught = MagnetizedMovingWindow(*window, earlier);
    DrawWindowMagnetCatch(caught);
  }
  const ImVec2 screen = context->IO.DisplaySize;
  // ⚠ LE COIN HAUT-GAUCHE, PAS LE CENTRE : c'est le bord qu'on voit rejoindre
  // la ligne, et c'est aussi ce qu'ImGui écrit quand il déplace une fenêtre.
  ImVec2 posed = caught.at;
  if (!caught.across.has_value()) {
    posed.x = grid.Snap(posed.x, screen.x);
  }
  if (!caught.down.has_value()) {
    posed.y = grid.Snap(posed.y, screen.y);
  }
  if (posed.x != window->Pos.x || posed.y != window->Pos.y) {
    ImGui::SetWindowPos(window, posed, ImGuiCond_Always);
  }
}

}  // namespace ro::ui
