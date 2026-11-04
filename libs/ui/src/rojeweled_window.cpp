#include "ro/ui/rojeweled_window.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <utility>

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
constexpr float kChromeHeight = 116.0F;

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
// La gemme choisie respire : son pouls, en radians par seconde, et l'écart de
// taille au plus fort.
constexpr float kBreathPerS = 5.0F;
constexpr float kBreathScale = 0.07F;
// À partir de ce compte, les coups restants se disent en rouge.
constexpr int kFewMovesLeft = 5;
constexpr ImVec4 kFewMovesColor(1.0F, 0.42F, 0.38F, 1.0F);
// LES POINTS GAGNÉS montrent de presque une case en s'effaçant, un peu plus
// gros que le texte ; ceux d'une cascade sont dorés. Une ombre les détache des
// gemmes.
constexpr float kGainRiseCells = 0.9F;
constexpr float kGainFontScale = 1.3F;
constexpr ImU32 kGainColor = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kShadowColor = IM_COL32(0, 0, 0, 255);
constexpr float kShadowOffset = 1.5F;
// Ce que porte « +123456 », zéro final compris.
constexpr std::size_t kGainTextSize = 16;
// Le bandeau qui annonce un remélange.
constexpr ImU32 kBandColor = IM_COL32(0, 0, 0, 170);
// Ce que porte une ligne du bilan, zéro final compris.
constexpr std::size_t kOutcomeTextSize = 96;

// L'ÉCLAT D'UNE GEMME EFFACÉE : une lueur de sa couleur, un cœur blanc qui
// s'éteint vite, une onde qui s'élargit, et des étincelles qui fusent.
constexpr int kSparks = 8;
constexpr float kGlowAlpha = 0.6F;
constexpr float kCoreAlpha = 0.85F;
constexpr float kRingAlpha = 0.8F;

// LES MARQUES DES GEMMES SPÉCIALES. La ligne : une barre blanche dans son sens,
// épaisse de cette part de la case, et une flèche à chaque bout.
constexpr float kLineBarShare = 0.16F;
constexpr float kLineArrowShare = 0.14F;
constexpr ImU32 kMarkColor = IM_COL32(255, 255, 255, 255);
constexpr float kLineBarAlpha = 0.75F;
// La bombe : un anneau blanc qui bat, au bord de sa pastille.
constexpr float kBombPulsePerS = 7.0F;
constexpr float kBombRingThickness = 3.0F;
// L'étoile : un anneau aux six couleurs, autour d'une pastille claire.
constexpr float kStarRingThickness = 4.0F;
constexpr float kStarPadAlpha = 0.3F;
constexpr int kStarArcSegments = 8;
// Le tour de l'anneau de l'étoile tourne, en radians par seconde.
constexpr float kStarSpinPerS = 1.5F;

// LES SOUFFLES. Une ligne : une bande de lumière sur toute la rangée ou la
// colonne, qui s'amincit. Une bombe : une onde jusqu'au carré qu'elle souffle.
// Une étoile : une onde blanche qui court sur toute la grille, et des rayons.
constexpr float kBeamAlpha = 0.7F;
constexpr float kBeamCoreShare = 0.35F;
constexpr float kBombWaveCells = 1.6F;
constexpr float kBombWaveAlpha = 0.45F;
constexpr float kStarWaveCells = 5.0F;
constexpr int kStarRays = 12;
constexpr float kStarRayThickness = 3.0F;

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

[[nodiscard]] float Seconds() { return static_cast<float>(ImGui::GetTime()); }

// La couleur d'une espèce ; blanche pour une étoile.
[[nodiscard]] ImU32 ColorOf(int kind) {
  return kind >= 0 && kind < kRojeweledKinds ? kKindColors[static_cast<std::size_t>(kind)]
                                             : IM_COL32_WHITE;
}

// La flèche d'une gemme de ligne, pointe en `tip`, tournée vers `outward`.
void DrawArrow(ImDrawList* draw, ImVec2 tip, ImVec2 outward, float size, ImU32 color) {
  const ImVec2 back(tip.x - outward.x * size, tip.y - outward.y * size);
  const ImVec2 side(-outward.y * size * 0.6F, outward.x * size * 0.6F);
  draw->AddTriangleFilled(tip, ImVec2(back.x + side.x, back.y + side.y),
                          ImVec2(back.x - side.x, back.y - side.y), color);
}

// LA MARQUE D'UNE GEMME SPÉCIALE, par-dessus son monstre.
void DrawSpecialMark(ImDrawList* draw, RojeweledSpecial special, ImU32 color, ImVec2 center,
                     float pad_half) {
  switch (special) {
    case RojeweledSpecial::kNone:
    case RojeweledSpecial::kStar:
      return;
    case RojeweledSpecial::kRowBlast:
    case RojeweledSpecial::kColumnBlast: {
      const bool row = special == RojeweledSpecial::kRowBlast;
      const ImVec2 along = row ? ImVec2(1.0F, 0.0F) : ImVec2(0.0F, 1.0F);
      const float bar_half = pad_half * kLineBarShare;
      const ImVec2 bar(row ? pad_half : bar_half, row ? bar_half : pad_half);
      draw->AddRectFilled(ImVec2(center.x - bar.x, center.y - bar.y),
                          ImVec2(center.x + bar.x, center.y + bar.y),
                          WithAlpha(kMarkColor, kLineBarAlpha));
      const float arrow = pad_half * 2.0F * kLineArrowShare;
      DrawArrow(draw, ImVec2(center.x + along.x * pad_half, center.y + along.y * pad_half), along,
                arrow, kMarkColor);
      DrawArrow(draw, ImVec2(center.x - along.x * pad_half, center.y - along.y * pad_half),
                ImVec2(-along.x, -along.y), arrow, kMarkColor);
      return;
    }
    case RojeweledSpecial::kBomb: {
      const float pulse = 0.5F + 0.5F * std::sin(Seconds() * kBombPulsePerS);
      draw->AddCircle(center, pad_half, WithAlpha(kMarkColor, 0.4F + 0.6F * pulse), 0,
                      kBombRingThickness);
      draw->AddCircle(center, pad_half * (0.8F + 0.1F * pulse), WithAlpha(color, 1.0F), 0,
                      kBombRingThickness * 0.5F);
      return;
    }
  }
}

// L'ÉTOILE : une pastille claire, un anneau aux six couleurs qui tourne, et
// l'Angeling, ou un disque blanc sans lui.
void DrawStar(ImDrawList* draw, const RojeweledView& view, ImVec2 center, float pad_half,
              float gem_half) {
  draw->AddRectFilled(ImVec2(center.x - pad_half, center.y - pad_half),
                      ImVec2(center.x + pad_half, center.y + pad_half),
                      WithAlpha(kMarkColor, kStarPadAlpha), kRounding);
  const float turn = 2.0F * std::numbers::pi_v<float>;
  const float spin = Seconds() * kStarSpinPerS;
  for (int kind = 0; kind < kRojeweledKinds; ++kind) {
    const float from = spin + turn * static_cast<float>(kind) / kRojeweledKinds;
    draw->PathArcTo(center, pad_half, from, from + turn / kRojeweledKinds, kStarArcSegments);
    draw->PathStroke(kKindColors[static_cast<std::size_t>(kind)], 0, kStarRingThickness);
  }
  if (view.star_art.usable()) {
    DrawFitted(draw, view.star_art, ImVec2(center.x - gem_half, center.y - gem_half),
               ImVec2(center.x + gem_half, center.y + gem_half));
    return;
  }
  draw->AddCircleFilled(center, gem_half * 2.0F * kFallbackRadius, kMarkColor);
}

// LA GEMME, à la taille `scale` de sa case, autour du milieu de la case.
void DrawGem(ImDrawList* draw, const RojeweledView& view, const RojeweledGem& gem, ImVec2 corner,
             float scale) {
  if (scale <= 0.0F) {
    return;
  }
  const ImVec2 center(corner.x + kCell * 0.5F, corner.y + kCell * 0.5F);
  const float pad_half = (kCell * 0.5F - kPadInset) * scale;
  const float gem_half = (kCell * 0.5F - kGemInset) * scale;
  if (gem.special == RojeweledSpecial::kStar) {
    DrawStar(draw, view, center, pad_half, gem_half);
    return;
  }
  const ImU32 color = kKindColors[static_cast<std::size_t>(gem.kind)];
  const ImVec2 pad_low(center.x - pad_half, center.y - pad_half);
  const ImVec2 pad_high(center.x + pad_half, center.y + pad_half);
  draw->AddRectFilled(pad_low, pad_high, WithAlpha(color, kPadAlpha), kRounding * scale);
  draw->AddRect(pad_low, pad_high, color, kRounding * scale, 0, kRingThickness);
  const ImVec2 low(center.x - gem_half, center.y - gem_half);
  const ImVec2 high(center.x + gem_half, center.y + gem_half);
  const SkinImage& image = view.art[static_cast<std::size_t>(gem.kind)];
  if (image.usable()) {
    DrawFitted(draw, image, low, high);
  } else {
    draw->AddCircleFilled(center, (high.x - low.x) * kFallbackRadius, color);
  }
  DrawSpecialMark(draw, gem.special, color, center, pad_half);
}

// LE SOUFFLE D'UNE GEMME SPÉCIALE, sur tout ce qu'il efface.
void DrawBlast(ImDrawList* draw, ImVec2 origin, const RojeweledBlast& blast) {
  const ImU32 color = ColorOf(blast.kind);
  const float done = blast.progress;
  const float left = 1.0F - done;
  const ImVec2 corner = CellCorner(origin, blast.cell);
  const ImVec2 middle(corner.x + kCell * 0.5F, corner.y + kCell * 0.5F);
  const float board_low_x = origin.x + kPad;
  const float board_high_x = origin.x + kBoard - kPad;
  const float board_low_y = origin.y + kPad;
  const float board_high_y = origin.y + kBoard - kPad;
  switch (blast.special) {
    case RojeweledSpecial::kNone:
      return;
    case RojeweledSpecial::kRowBlast:
    case RojeweledSpecial::kColumnBlast: {
      const bool row = blast.special == RojeweledSpecial::kRowBlast;
      const float half = kCell * 0.5F * left;
      for (const auto& [share, tint] :
           {std::pair{1.0F, color}, std::pair{kBeamCoreShare, kMarkColor}}) {
        const float thick = half * share;
        const ImVec2 low =
            row ? ImVec2(board_low_x, middle.y - thick) : ImVec2(middle.x - thick, board_low_y);
        const ImVec2 high =
            row ? ImVec2(board_high_x, middle.y + thick) : ImVec2(middle.x + thick, board_high_y);
        draw->AddRectFilled(low, high, WithAlpha(tint, left * kBeamAlpha));
      }
      return;
    }
    case RojeweledSpecial::kBomb: {
      const float radius = kCell * kBombWaveCells * done;
      draw->AddCircleFilled(middle, radius, WithAlpha(color, left * kBombWaveAlpha));
      draw->AddCircle(middle, radius, WithAlpha(kMarkColor, left), 0, kBombRingThickness);
      return;
    }
    case RojeweledSpecial::kStar: {
      const float radius = kCell * kStarWaveCells * done;
      draw->AddCircle(middle, radius, WithAlpha(kMarkColor, left), 0, kBombRingThickness);
      const float turn = 2.0F * std::numbers::pi_v<float>;
      for (int ray = 0; ray < kStarRays; ++ray) {
        const float angle = turn * static_cast<float>(ray) / kStarRays;
        const ImU32 tint = kKindColors[static_cast<std::size_t>(ray % kRojeweledKinds)];
        draw->AddLine(
            middle,
            ImVec2(middle.x + std::cos(angle) * radius, middle.y + std::sin(angle) * radius),
            WithAlpha(tint, left), kStarRayThickness * left);
      }
      return;
    }
  }
}

void DrawBurst(ImDrawList* draw, ImVec2 origin, const RojeweledBurst& burst) {
  if (burst.kind < 0 || burst.kind > kRojeweledStarKind) {
    return;
  }
  const ImU32 color = ColorOf(burst.kind);
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
  if (!view.over && view.moves_left <= kFewMovesLeft) {
    ImGui::TextColored(kFewMovesColor, core::Tr("Coups : %d"), view.moves_left);
  } else {
    ImGui::Text(core::Tr("Coups : %d"), view.moves_left);
  }
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
  FaintText(core::Tr("4 : une ligne · L ou T : une bombe · 5 : un Angeling"));
}

// Une ligne centrée sur le plateau, à la hauteur `top`.
void CenteredLine(ImDrawList* draw, ImVec2 origin, float top, ImU32 color, const char* text) {
  const ImVec2 size = ImGui::CalcTextSize(text);
  draw->AddText(ImVec2(origin.x + (kBoard - size.x) * 0.5F, top), color, text);
}

// LE BILAN : titre, score, meilleure cascade, record, et comment rejouer,
// centrés ensemble sur le plateau voilé.
void DrawSummary(ImDrawList* draw, ImVec2 origin, const RojeweledView& view) {
  const ImVec2 far(origin.x + kBoard, origin.y + kBoard);
  draw->AddRectFilled(origin, far, kVeilColor, kRounding);
  std::array<char, kOutcomeTextSize> score = {};
  std::snprintf(score.data(), score.size(), core::Tr("Score : %d"), view.score);
  std::array<char, kOutcomeTextSize> combo = {};
  if (view.best_combo >= 2) {
    std::snprintf(combo.data(), combo.size(), core::Tr("Meilleure cascade : ×%d"), view.best_combo);
  }
  struct Line {
    const char* text;
    ImU32 color;
  };
  std::array<Line, 5> lines = {};
  std::size_t count = 0;
  lines[count++] = {core::Tr("Partie finie !"), kSelectedColor};
  if (view.new_record) {
    lines[count++] = {core::Tr("Nouveau record !"), kComboColor};
  }
  lines[count++] = {score.data(), IM_COL32_WHITE};
  if (combo[0] != '\0') {
    lines[count++] = {combo.data(), IM_COL32_WHITE};
  }
  lines[count++] = {core::Tr("« Nouvelle partie » pour rejouer."), IM_COL32_WHITE};
  const float line_height = ImGui::GetTextLineHeightWithSpacing();
  float top = origin.y + (kBoard - line_height * static_cast<float>(count)) * 0.5F;
  for (std::size_t index = 0; index < count; ++index) {
    CenteredLine(draw, origin, top, lines[index].color, lines[index].text);
    top += line_height;
  }
}

// L'ANNONCE D'UN REMÉLANGE, sur un bandeau au milieu du plateau.
void DrawReshuffleNotice(ImDrawList* draw, ImVec2 origin) {
  const char* text = core::Tr("Plus aucun échange : on remélange !");
  const float line_height = ImGui::GetTextLineHeightWithSpacing();
  const float top = origin.y + (kBoard - line_height) * 0.5F;
  draw->AddRectFilled(ImVec2(origin.x, top - kPad),
                      ImVec2(origin.x + kBoard, top + line_height + kPad), kBandColor);
  CenteredLine(draw, origin, top, kComboColor, text);
}

// LES POINTS GAGNÉS, qui montent depuis le centre de ce qui s'est effacé.
void DrawGain(ImDrawList* draw, ImVec2 origin, const RojeweledGain& gain) {
  std::array<char, kGainTextSize> text = {};
  std::snprintf(text.data(), text.size(), "+%d", gain.points);
  ImFont* font = ImGui::GetFont();
  const float font_size = ImGui::GetFontSize() * kGainFontScale;
  const ImVec2 size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0F, text.data());
  const float left = 1.0F - gain.progress;
  const ImVec2 middle(origin.x + kPad + (gain.column + 0.5F) * kCell,
                      origin.y + kPad + (gain.row + 0.5F - gain.progress * kGainRiseCells) * kCell);
  const ImVec2 corner(middle.x - size.x * 0.5F, middle.y - size.y * 0.5F);
  const ImU32 color = gain.combo >= 2 ? kComboColor : kGainColor;
  draw->AddText(font, font_size, ImVec2(corner.x + kShadowOffset, corner.y + kShadowOffset),
                WithAlpha(kShadowColor, left), text.data());
  draw->AddText(font, font_size, corner, WithAlpha(color, left), text.data());
}

void DrawOutcome(ImDrawList* draw, ImVec2 origin, const RojeweledView& view) {
  const ImVec2 far(origin.x + kBoard, origin.y + kBoard);
  if (view.over) {
    DrawSummary(draw, origin, view);
    return;
  }
  if (view.reshuffling) {
    DrawReshuffleNotice(draw, origin);
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
    const bool star = gem.special == RojeweledSpecial::kStar;
    if (!star && (gem.kind < 0 || gem.kind >= kRojeweledKinds)) {
      continue;
    }
    const RojeweledCell cell{index / kRojeweledSide, index % kRojeweledSide};
    const ImVec2 corner = CellCorner(origin, cell);
    float scale = gem.scale;
    if (view.selected == cell) {
      scale *= 1.0F + kBreathScale * std::sin(Seconds() * kBreathPerS);
    }
    DrawGem(draw, view, gem,
            ImVec2(corner.x + gem.offset_columns * kCell, corner.y + gem.offset_rows * kCell),
            scale);
  }
  for (const RojeweledBurst& burst : view.bursts) {
    DrawBurst(draw, origin, burst);
  }
  for (const RojeweledBlast& blast : view.blasts) {
    DrawBlast(draw, origin, blast);
  }
  for (const RojeweledGain& gain : view.gains) {
    DrawGain(draw, origin, gain);
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
