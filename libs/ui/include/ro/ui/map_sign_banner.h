#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include <imgui.h>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// LE BANDEAU DU NOM DE LA CARTE, à l'arrivée : une image de fond — ville,
// champ, donjon… —, un sous-titre et un titre blancs par-dessus, au tiers haut
// de l'écran. Il tient cinq secondes, puis s'efface en un quart de seconde.
//
// ⚠ TOUTE LA GÉOMÉTRIE SE RAPPORTE À LA HAUTEUR DE L'ÉCRAN, comme chez le
// client : la bande prend un cinquième de la hauteur, et l'image comme les
// textes suivent l'échelle qu'elle leur donne. Le bandeau a donc la même
// allure à toutes les résolutions.

// Où la bande commence, et quelle part de la hauteur elle prend.
inline constexpr float kMapSignTopShare = 0.12F;
inline constexpr float kMapSignHeightShare = 0.2F;

// Les textes, en pixels DE L'IMAGE : leur taille et leur haut, que l'échelle
// de la bande convertit en pixels d'écran.
inline constexpr float kMapSignSubtitleSize = 16.0F;
inline constexpr float kMapSignSubtitleTop = 72.0F;
inline constexpr float kMapSignTitleSize = 44.0F;
inline constexpr float kMapSignTitleTop = 95.0F;

// Pleinement visible pendant ce temps, puis effacé linéairement sur le suivant.
inline constexpr std::uint32_t kMapSignShownMs = 5000;
inline constexpr std::uint32_t kMapSignFadeMs = 255;

// Où tout se pose, pour un écran et une image donnés.
struct MapSignLayout {
  ImVec2 image_top_left{0.0F, 0.0F};
  ImVec2 image_size{0.0F, 0.0F};
  // Ce qu'un pixel de l'image vaut à l'écran.
  float scale = 1.0F;
  float subtitle_size = 0.0F;
  float subtitle_top = 0.0F;
  float title_size = 0.0F;
  float title_top = 0.0F;
};

[[nodiscard]] MapSignLayout MapSignLayoutOf(ImVec2 screen, ImVec2 image);

// L'opacité, de 0 à 255, après ce temps ; rien quand le bandeau est fini.
[[nodiscard]] std::optional<std::uint8_t> MapSignAlpha(std::uint32_t elapsed_ms);

// CE QUE LE BANDEAU MONTRE, composé par `app`.
struct MapSignView {
  SkinImage background;
  // ⚠ EN UTF-8 ; vides, ils ne se dessinent pas.
  std::string_view subtitle;
  std::string_view title;
  std::uint32_t elapsed_ms = 0;
};

// Peint le bandeau PAR-DESSUS les fenêtres, comme le client.
//
// ⚠ SANS IMAGE, RIEN : le client ne pose pas les textes seuls sur le monde.
void DrawMapSign(const MapSignView& sign);

}  // namespace ro::ui
