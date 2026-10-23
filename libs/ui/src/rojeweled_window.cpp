#include "ro/ui/rojeweled_window.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>

#include <imgui.h>

#include "ro/ui/minigames_window.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_bounds.h"
#include "ro/ui/window_name.h"

#include "ui_text.h"

namespace ro::ui {
namespace {

// La case, en pixels, et la marge du plateau.
constexpr float kCell = 44.0F;
constexpr float kPad = 6.0F;
constexpr float kBoard = kRojeweledSide * kCell + kPad * 2.0F;
// La place que prennent le bandeau du haut et les marges de la fenêtre.
constexpr float kChromeWidth = 16.0F;
constexpr float kChromeHeight = 96.0F;

// Le monstre, en retrait de sa case ; la pastille, un peu moins.
constexpr float kGemInset = 4.0F;
constexpr float kPadInset = 2.0F;
constexpr float kRounding = 6.0F;
constexpr float kRingThickness = 2.5F;
// La part de la case qu'occupe la pastille de repli.
constexpr float kFallbackRadius = 0.42F;

// LA COULEUR DE CHAQUE ESPÈCE, sur tout le cercle des teintes : rose, orange,
// or, vert, bleu, violet.
constexpr std::array<ImU32, kRojeweledKinds> kKindColors = {
    IM_COL32(255, 95, 165, 255), IM_COL32(255, 140, 20, 255), IM_COL32(255, 215, 55, 255),
    IM_COL32(70, 205, 85, 255),  IM_COL32(55, 150, 240, 255), IM_COL32(180, 85, 225, 255),
};
// La pastille sous le monstre : sa couleur, à demi transparente.
constexpr float kPadAlpha = 0.43F;

constexpr ImU32 kBoardColor = IM_COL32(24, 26, 42, 255);
constexpr ImU32 kBoardEdge = IM_COL32(80, 90, 140, 255);
constexpr ImU32 kCellLight = IM_COL32(34, 37, 58, 255);
constexpr ImU32 kCellDark = IM_COL32(28, 31, 50, 255);
constexpr ImU32 kSelectedColor = IM_COL32(255, 240, 120, 255);
constexpr ImU32 kHintColor = IM_COL32(120, 255, 160, 255);
constexpr ImU32 kVeilColor = IM_COL32(0, 0, 0, 160);
constexpr ImU32 kComboColor = IM_COL32(255, 230, 120, 255);
// Ce que porte « Cascade ×N », zéro final compris.
constexpr std::size_t kComboTextSize = 48;
// Le pouls de l'indice, en radians par seconde, et son alpha le plus bas.
constexpr float kHintPulsePerS = 6.0F;
constexpr float kHintLowestAlpha = 0.4F;

// L'ÉCLAT D'UNE GEMME EFFACÉE : une lueur de sa couleur, un cœur blanc qui
// s'éteint vite, une onde qui s'élargit, et des étincelles qui fusent.
constexpr int kSparks = 8;
constexpr float kGlowAlpha = 0.6F;
constexpr float kCoreAlpha = 0.85F;
constexpr float kRingAlpha = 0.8F;

[[nodiscard]] ImVec2 CellCorner(ImVec2 origin, RojeweledCell cell) {
  return ImVec2(origin.x + kPad + static_cast<float>(cell.column) * kCell,
                origin.y + kPad + static_cast<float>(cell.row) * kCell);
}

[[nodiscard]] ImU32 WithAlpha(ImU32 color, float alpha) {
  const auto bits = static_cast<ImU32>(std::clamp(alpha, 0.0F, 1.0F) * 255.0F);
  return (color & ~IM_COL32_A_MASK) | (bits << IM_COL32_A_SHIFT);
}

// L'image, la plus grande qui tienne dans la boîte sans se déformer.
void DrawFitted(ImDrawList* draw, const SkinImage& image, ImVec2 low, ImVec2 high) {
  const float scale = std::min((high.x - low.x) / image.width, (high.y - low.y) / image.height);
  const ImVec2 size(image.width * scale, image.height * scale);
  const ImVec2 corner(low.x + (high.x - low.x - size.x) * 0.5F,
                      low.y + (high.y - low.y - size.y) * 0.5F);
  draw->AddImage(image.id, corner, ImVec2(corner.x + size.x, corner.y + size.y));
}

void DrawGem(ImDrawList* draw, const RojeweledView& view, int kind, ImVec2 corner) {
  const ImU32 color = kKindColors[static_cast<std::size_t>(kind)];
  const ImVec2 pad_low(corner.x + kPadInset, corner.y + kPadInset);
  const ImVec2 pad_high(corner.x + kCell - kPadInset, corner.y + kCell - kPadInset);
  draw->AddRectFilled(pad_low, pad_high, WithAlpha(color, kPadAlpha), kRounding);
  draw->AddRect(pad_low, pad_high, color, kRounding, 0, kRingThickness);
  const ImVec2 low(corner.x + kGemInset, corner.y + kGemInset);
  const ImVec2 high(corner.x + kCell - kGemInset, corner.y + kCell - kGemInset);
  const SkinImage& image = view.art[static_cast<std::size_t>(kind)];
  if (image.usable()) {
    DrawFitted(draw, image, low, high);
    return;
  }
  const ImVec2 middle((low.x + high.x) * 0.5F, (low.y + high.y) * 0.5F);
  draw->AddCircleFilled(middle, (high.x - low.x) * kFallbackRadius, color);
}

void DrawBurst(ImDrawList* draw, ImVec2 origin, const RojeweledBurst& burst) {
  if (burst.kind < 0 || burst.kind >= kRojeweledKinds) {
    return;
  }
  const ImU32 color = kKindColors[static_cast<std::size_t>(burst.kind)];
  const float done = burst.progress;
  const float left = 1.0F - done;
  const ImVec2 corner = CellCorner(origin, burst.cell);
  const ImVec2 middle(corner.x + kCell * 0.5F, corner.y + kCell * 0.5F);
  draw->AddCircleFilled(middle, kCell * (0.20F + done * 0.45F),
                        WithAlpha(color, left * left * kGlowAlpha));
  draw->AddCircleFilled(middle, kCell * 0.24F * left,
                        WithAlpha(IM_COL32_WHITE, left * left * kCoreAlpha));
  draw->AddCircle(middle, kCell * (0.20F + done * 0.85F), WithAlpha(color, left * kRingAlpha), 0,
                  3.0F * left + 0.6F);
  for (int spark = 0; spark < kSparks; ++spark) {
    // L'angle de départ dépend de la case, pour que deux éclats voisins ne
    // soient pas identiques.
    const float angle = static_cast<float>(spark) * (2.0F * std::numbers::pi_v<float> / kSparks) +
                        static_cast<float>(burst.cell.row) * 0.7F +
                        static_cast<float>(burst.cell.column) * 0.4F;
    const float reach = kCell * (0.10F + done * 0.95F);
    draw->AddCircleFilled(
        ImVec2(middle.x + std::cos(angle) * reach, middle.y + std::sin(angle) * reach),
        left * 3.5F + 0.5F, WithAlpha(color, left));
  }
}

void DrawHeader(const RojeweledView& view, RojeweledAsk& asked, const SkinArt& art) {
  ImGui::Text(core::Tr("Score : %d"), view.score);
  ImGui::SameLine(0.0F, ImGui::GetFontSize());
  ImGui::Text(core::Tr("Coups : %d"), view.moves_left);
  ImGui::SameLine(0.0F, ImGui::GetFontSize());
  if (view.best_score > 0) {
    FaintText(core::Tr("Meilleur :"));
    ImGui::SameLine();
    ImGui::Text("%d", view.best_score);
    ImGui::SameLine(0.0F, ImGui::GetFontSize());
  }
  if (SkinSmallButton(core::Tr("Nouvelle partie"), art)) {
    asked.new_game = true;
  }
  FaintText(core::Tr("Cliquez deux monstres voisins pour les échanger."));
}

void DrawOutcome(ImDrawList* draw, ImVec2 origin, const RojeweledView& view) {
  const ImVec2 far(origin.x + kBoard, origin.y + kBoard);
  if (view.over) {
    draw->AddRectFilled(origin, far, kVeilColor, kRounding);
    const char* title = core::Tr("Partie finie !");
    const ImVec2 size = ImGui::CalcTextSize(title);
    draw->AddText(ImVec2(origin.x + (kBoard - size.x) * 0.5F, origin.y + kBoard * 0.5F - size.y),
                  kSelectedColor, title);
    const char* hint = core::Tr("« Nouvelle partie » pour rejouer.");
    const ImVec2 hint_size = ImGui::CalcTextSize(hint);
    draw->AddText(ImVec2(origin.x + (kBoard - hint_size.x) * 0.5F, origin.y + kBoard * 0.5F),
                  IM_COL32_WHITE, hint);
    return;
  }
  if (view.combo >= 2) {
    std::array<char, kComboTextSize> text = {};
    std::snprintf(text.data(), text.size(), core::Tr("Cascade ×%d"), view.combo);
    const ImVec2 size = ImGui::CalcTextSize(text.data());
    draw->AddText(ImVec2(origin.x + (kBoard - size.x) * 0.5F, far.y - size.y - kPad * 2.0F),
                  kComboColor, text.data());
  }
}

}  // namespace

RojeweledAsk DrawRojeweledWindow(RojeweledState& state, const RojeweledView& view,
                                 const SkinArt& art) {
  RojeweledAsk asked;
  if (!state.open) {
    return asked;
  }
  static WindowName name(kRojeweledTitle, kRojeweledId);
  ImGui::SetNextWindowSize(ImVec2(kBoard + kChromeWidth, kBoard + kChromeHeight),
                           ImGuiCond_FirstUseEver);
  ConstrainNextWindowToScreen(name.Get());
  if (!BeginSkinnedWindow(name.Get(), &state.open, art, SkinnedTitle::kBar,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
    EndSkinnedWindow();
    return asked;
  }
  DrawHeader(view, asked, art);

  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const bool clicked = ImGui::InvisibleButton("##rojeweled_plateau", ImVec2(kBoard, kBoard));
  if (clicked && !view.over) {
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const RojeweledCell cell{
        .row = static_cast<int>(std::floor((mouse.y - origin.y - kPad) / kCell)),
        .column = static_cast<int>(std::floor((mouse.x - origin.x - kPad) / kCell))};
    if (cell.row >= 0 && cell.row < kRojeweledSide && cell.column >= 0 &&
        cell.column < kRojeweledSide) {
      asked.clicked = cell;
    }
  }

  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImVec2 far(origin.x + kBoard, origin.y + kBoard);
  draw->AddRectFilled(origin, far, kBoardColor, kRounding);
  draw->AddRect(origin, far, kBoardEdge, kRounding);
  // ⚠ LES GEMMES QUI TOMBENT VIENNENT D'AU-DESSUS DE LA GRILLE : on les coupe
  // au bord du plateau, sans quoi elles passeraient sur le bandeau.
  draw->PushClipRect(ImVec2(origin.x + kPad, origin.y + kPad), ImVec2(far.x - kPad, far.y - kPad),
                     true);
  for (int row = 0; row < kRojeweledSide; ++row) {
    for (int column = 0; column < kRojeweledSide; ++column) {
      const ImVec2 corner = CellCorner(origin, {row, column});
      draw->AddRectFilled(ImVec2(corner.x + 1.0F, corner.y + 1.0F),
                          ImVec2(corner.x + kCell - 1.0F, corner.y + kCell - 1.0F),
                          (row + column) % 2 == 1 ? kCellLight : kCellDark, kPadInset);
    }
  }
  for (int index = 0; index < kRojeweledCells; ++index) {
    const RojeweledGem& gem = view.gems[static_cast<std::size_t>(index)];
    if (gem.kind < 0 || gem.kind >= kRojeweledKinds) {
      continue;
    }
    const ImVec2 corner = CellCorner(origin, {index / kRojeweledSide, index % kRojeweledSide});
    DrawGem(draw, view, gem.kind,
            ImVec2(corner.x + gem.offset_columns * kCell, corner.y + gem.offset_rows * kCell));
  }
  for (const RojeweledBurst& burst : view.bursts) {
    DrawBurst(draw, origin, burst);
  }
  draw->PopClipRect();

  if (view.selected.has_value()) {
    const ImVec2 corner = CellCorner(origin, *view.selected);
    draw->AddRect(corner, ImVec2(corner.x + kCell, corner.y + kCell), kSelectedColor, kPadInset, 0,
                  kRingThickness);
  }
  if (view.hint.has_value()) {
    const float pulse =
        0.5F + 0.5F * std::sin(static_cast<float>(ImGui::GetTime()) * kHintPulsePerS);
    const ImU32 color = WithAlpha(kHintColor, kHintLowestAlpha + (1.0F - kHintLowestAlpha) * pulse);
    for (const RojeweledCell cell : *view.hint) {
      const ImVec2 corner = CellCorner(origin, cell);
      draw->AddRect(corner, ImVec2(corner.x + kCell, corner.y + kCell), color, kGemInset, 0,
                    kRingThickness);
    }
  }
  DrawOutcome(draw, origin, view);
  DrawTrophyToast(view.toast, view.toast_alpha, origin, kBoard);

  EndSkinnedWindow();
  return asked;
}

}  // namespace ro::ui
