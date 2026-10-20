#include "ro/ui/loading_screen.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace ro::ui {
namespace {

// La bande : ses marges, la barre et l'écart entre le texte et elle.
constexpr float kBandMarginPx = 24.0F;
constexpr float kBarHeightPx = 10.0F;
constexpr float kTextGapPx = 6.0F;
constexpr float kBarRounding = 3.0F;
// L'encadré de la barre, et son remplissage — l'or chaud des barres du jeu.
constexpr ImU32 kBarFrameColor = IM_COL32(70, 70, 70, 255);
constexpr ImU32 kBarTrackColor = IM_COL32(25, 25, 25, 255);
constexpr ImU32 kBarFillColor = IM_COL32(222, 178, 82, 255);
// Indéterminée, un tronçon d'un quart de la barre la parcourt, un aller par
// seconde et demie.
constexpr float kSweepShare = 0.25F;
constexpr float kSweepSeconds = 1.5F;

// LES POINTS QUI TOURNENT, quand l'animation du jeu manque : huit points sur un
// cercle, la tête pleine, la traîne qui s'efface, un tour par seconde.
constexpr int kSpinnerDots = 8;
constexpr float kSpinnerRadiusPx = 18.0F;
constexpr float kSpinnerDotRadiusPx = 3.5F;
constexpr float kSpinnerTurnsPerSecond = 1.0F;
constexpr float kSpinnerTailFloor = 0.15F;

constexpr float kHalf = 0.5F;
constexpr int kAlphaShift = IM_COL32_A_SHIFT;
constexpr ImU32 kAlphaMask = IM_COL32_A_MASK;

// La même couleur, son opacité multipliée par `opacity`.
[[nodiscard]] ImU32 Faded(ImU32 color, float opacity) {
  const float alpha =
      static_cast<float>((color & kAlphaMask) >> kAlphaShift) * std::clamp(opacity, 0.0F, 1.0F);
  return (color & ~kAlphaMask) | (static_cast<ImU32>(std::lround(alpha)) << kAlphaShift);
}

[[nodiscard]] ImU32 WhiteAt(float opacity) { return Faded(IM_COL32_WHITE, opacity); }

// Les points qui tournent, centrés sur `center`.
void DrawSpinnerDots(ImDrawList* list, ImVec2 center, float clock_s, float opacity) {
  const float turn = std::fmod(std::max(clock_s, 0.0F) * kSpinnerTurnsPerSecond, 1.0F);
  const int head = static_cast<int>(turn * static_cast<float>(kSpinnerDots));
  for (int dot = 0; dot < kSpinnerDots; ++dot) {
    const float angle = std::numbers::pi_v<float> * 2.0F * static_cast<float>(dot) /
                        static_cast<float>(kSpinnerDots);
    // Combien de pas derrière la tête : zéro pour elle, sept pour la queue.
    const int behind = (head - dot + kSpinnerDots) % kSpinnerDots;
    const float strength = std::max(
        kSpinnerTailFloor, 1.0F - static_cast<float>(behind) / static_cast<float>(kSpinnerDots));
    list->AddCircleFilled(ImVec2(center.x + std::sin(angle) * kSpinnerRadiusPx,
                                 center.y - std::cos(angle) * kSpinnerRadiusPx),
                          kSpinnerDotRadiusPx, Faded(kBarFillColor, opacity * strength));
  }
}

// La barre entre `left` et `right`, à la hauteur `top`.
void DrawBar(ImDrawList* list, float left, float right, float top, const LoadingScreenView& view) {
  const ImVec2 bar_min(left, top);
  const ImVec2 bar_max(right, top + kBarHeightPx);
  list->AddRectFilled(bar_min, bar_max, Faded(kBarTrackColor, view.opacity), kBarRounding);
  const float width = right - left;
  float fill_from = 0.0F;
  float fill_to = 0.0F;
  if (view.fraction >= 0.0F) {
    fill_to = std::min(view.fraction, 1.0F);
  } else {
    // Le tronçon entre par la gauche et sort par la droite.
    const float sweep = std::fmod(std::max(view.clock_s, 0.0F) / kSweepSeconds, 1.0F);
    const float head = sweep * (1.0F + kSweepShare);
    fill_from = std::clamp(head - kSweepShare, 0.0F, 1.0F);
    fill_to = std::clamp(head, 0.0F, 1.0F);
  }
  if (fill_to > fill_from) {
    list->AddRectFilled(ImVec2(left + width * fill_from, top),
                        ImVec2(left + width * fill_to, top + kBarHeightPx),
                        Faded(kBarFillColor, view.opacity), kBarRounding);
  }
  list->AddRect(bar_min, bar_max, Faded(kBarFrameColor, view.opacity), kBarRounding);
}

}  // namespace

LoadingLayout LayoutLoadingScreen(ImVec2 screen_min, ImVec2 screen_size, float art_width,
                                  float art_height) {
  LoadingLayout layout;
  const float band_height = std::min(kLoadingBandHeightPx, screen_size.y);
  layout.band_min = ImVec2(screen_min.x, screen_min.y + screen_size.y - band_height);
  layout.band_max = ImVec2(screen_min.x + screen_size.x, screen_min.y + screen_size.y);
  const float room_width = screen_size.x;
  const float room_height = screen_size.y - band_height;
  if (art_width <= 0.0F || art_height <= 0.0F || room_width <= 0.0F || room_height <= 0.0F) {
    layout.art_min = layout.band_min;
    layout.art_max = layout.band_min;
    return layout;
  }
  // Aussi grande qu'elle tient, sans rien couper : des bandes noires sur les
  // côtés d'un écran large, dessus et dessous d'un écran haut.
  const float scale = std::min(room_width / art_width, room_height / art_height);
  const float shown_width = art_width * scale;
  const float shown_height = art_height * scale;
  layout.art_min = ImVec2(screen_min.x + (room_width - shown_width) * kHalf,
                          screen_min.y + (room_height - shown_height) * kHalf);
  layout.art_max = ImVec2(layout.art_min.x + shown_width, layout.art_min.y + shown_height);
  return layout;
}

void DrawLoadingScreen(const LoadingScreenView& view) {
  DrawLoadingScreen("");
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  ImDrawList* list = ImGui::GetBackgroundDrawList();
  const LoadingLayout layout = LayoutLoadingScreen(
      screen->Pos, screen->Size, view.backdrop.usable() ? view.backdrop.width : 0.0F,
      view.backdrop.usable() ? view.backdrop.height : 0.0F);
  if (view.backdrop.usable()) {
    list->AddImage(view.backdrop.id, layout.art_min, layout.art_max, ImVec2(0.0F, 0.0F),
                   ImVec2(1.0F, 1.0F), WhiteAt(view.opacity));
  }

  // L'ANIMATION À DROITE de la bande, centrée sur sa hauteur.
  const float band_middle = (layout.band_min.y + layout.band_max.y) * kHalf;
  float spinner_left = layout.band_max.x - kBandMarginPx;
  if (view.spinner.usable()) {
    spinner_left -= view.spinner.width;
    const ImVec2 at(spinner_left, band_middle - view.spinner.height * kHalf);
    list->AddImage(view.spinner.id, at,
                   ImVec2(at.x + view.spinner.width, at.y + view.spinner.height),
                   ImVec2(0.0F, 0.0F), ImVec2(1.0F, 1.0F), WhiteAt(view.opacity));
  } else {
    spinner_left -= kSpinnerRadiusPx * 2.0F;
    DrawSpinnerDots(list, ImVec2(spinner_left + kSpinnerRadiusPx, band_middle), view.clock_s,
                    view.opacity);
  }

  // LA BARRE ET SES DEUX TEXTES, à gauche de l'animation : l'étape au-dessus de
  // son début, le pourcentage au-dessus de sa fin.
  const float bar_left = layout.band_min.x + kBandMarginPx;
  const float bar_right = std::max(bar_left, spinner_left - kBandMarginPx);
  const float text_height = ImGui::GetTextLineHeight();
  const float block_height = text_height + kTextGapPx + kBarHeightPx;
  const float block_top = band_middle - block_height * kHalf;
  const ImU32 text_color = Faded(kLoadingTextColor, view.opacity);
  if (view.step_text != nullptr && view.step_text[0] != '\0') {
    list->AddText(ImVec2(bar_left, block_top), text_color, view.step_text);
  }
  if (view.percent_text != nullptr && view.percent_text[0] != '\0') {
    const float width = ImGui::CalcTextSize(view.percent_text).x;
    list->AddText(ImVec2(bar_right - width, block_top), text_color, view.percent_text);
  }
  DrawBar(list, bar_left, bar_right, block_top + text_height + kTextGapPx, view);
}

void DrawLoadingScreen(const char* text) {
  // ⚠ SUR LA LISTE DE FOND, et non dans une fenêtre : une fenêtre se déplace,
  // prend le focus et se range dans le fichier d'ImGui. Cet écran n'est rien de
  // tout cela, et il doit couvrir la totalité du cadre.
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  ImDrawList* list = ImGui::GetBackgroundDrawList();
  const ImVec2 corner(screen->Pos.x + screen->Size.x, screen->Pos.y + screen->Size.y);
  list->AddRectFilled(screen->Pos, corner, kLoadingBackground);
  if (text == nullptr || text[0] == '\0') {
    return;
  }
  const ImVec2 size = ImGui::CalcTextSize(text);
  const ImVec2 at(screen->Pos.x + (screen->Size.x - size.x) * kHalf,
                  screen->Pos.y + (screen->Size.y - size.y) * kHalf);
  list->AddText(at, kLoadingTextColor, text);
}

}  // namespace ro::ui
