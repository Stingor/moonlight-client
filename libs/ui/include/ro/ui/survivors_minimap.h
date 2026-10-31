#pragma once

#include <array>
#include <cstdint>
#include <span>

#include <imgui.h>

namespace ro::ui {

// LA MINIMAP D'UNE PARTIE DE « SURVIVANTS DE MIDGARD » : un carré nord en
// haut, comme la caméra figée de l'arène, le héros au centre, le cadre de
// l'écran, et un symbole par cible. Une cible au-delà du rayon se colle au
// bord, sur la droite qui va du héros vers elle.
//
// 🔴 ELLE NE LIT AUCUNE ENTRÉE et ne sait rien de la partie : `app` lui passe
// des écarts au héros, en cases, l'est vers la droite et le nord vers le haut.

// LE CÔTÉ DU CARRÉ, en pixels, et le rayon qu'il couvre, en cases : du centre
// à un bord, rentré de la taille d'un symbole.
inline constexpr float kSurvivorsMinimapSidePx = 160.0F;
inline constexpr float kSurvivorsMinimapRadiusCells = 64.0F;

// Ce que marque un symbole. Les monstres communs et les gemmes sous
// l'Emperium n'en ont pas.
enum class SurvivorsMinimapMark : std::uint8_t {
  kPlant,
  kChest,
  kCard,
  kItem,
  kEmperium,
  kMiniBoss,
  kBoss,
};

struct SurvivorsMinimapDot {
  // L'écart au héros, en cases : x vers l'est, y vers le nord.
  ImVec2 offset_cells;
  SurvivorsMinimapMark mark = SurvivorsMinimapMark::kItem;
};

struct SurvivorsMinimapView {
  bool shown = false;
  // LE CADRE DE L'ÉCRAN : ses quatre coins au sol, en écarts au héros, dans
  // l'ordre du tour. Faux quand la caméra ne le donne pas : rien ne se trace.
  bool screen_known = false;
  std::array<ImVec2, 4> screen_corners_cells{};
  // Le cap du héros sur 64 valeurs, 0 au nord, sens trigonométrique.
  std::uint8_t hero_facing = 0;
  // Peints dans l'ordre : les derniers par-dessus.
  std::span<const SurvivorsMinimapDot> dots;
};

// LA PLACE D'UN ÉCART DANS LE CARRÉ de centre `center` et de demi-côté
// `half_px` : à l'échelle du rayon, ramenée sur le bord quand elle en sort.
struct SurvivorsMinimapPlace {
  ImVec2 at;
  bool on_edge = false;
};
[[nodiscard]] SurvivorsMinimapPlace PlaceOnSurvivorsMinimap(ImVec2 center, float half_px,
                                                            ImVec2 offset_cells) noexcept;

// Peint la minimap, son coin haut droit en `top_right`, et rend l'ordonnée
// sous elle. `clock_ms` fait battre le symbole du boss.
float DrawSurvivorsMinimap(ImDrawList& draw, ImVec2 top_right, const SurvivorsMinimapView& view,
                           std::uint64_t clock_ms);

}  // namespace ro::ui
