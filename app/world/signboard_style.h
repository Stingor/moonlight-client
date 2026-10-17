#pragma once

#include <string>

#include "ro/render/signboard.h"

#include "world/overlay_view.h"

namespace moonclient {

// ── CE QUE LE JOUEUR RÈGLE DES ENSEIGNES ────────────────────────────────────
//
// Leur dessin — voir `ro::render::SignboardLook` — et la police de leur titre.
// ⚠ LE DÉFAUT EST CELUI DU CLIENT : la police du jeu, à sa taille.

// Les bornes de la taille du titre, en pixels. ⚠ AU-DELÀ DE SEIZE, la ligne
// ne tient plus dans le corps de l'enseigne et le bas des lettres se rogne.
inline constexpr int kSmallestSignboardFontPixels = 8;
inline constexpr int kLargestSignboardFontPixels = 16;

struct SignboardStyle {
  ro::render::SignboardLook look;
  // LA FAMILLE PAR SON NOM, vide pour la police du jeu. Le nom et non un rang :
  // une police retirée de la machine retombe sur celle du jeu au lieu de
  // désigner sa voisine.
  std::string font_name;
  // Le gras d'une famille choisie. ⚠ SANS EFFET SUR LA POLICE DU JEU.
  bool font_bold = false;
  int font_pixels = kNameFontPixels;
};

// Vrai quand le titre s'écrit avec la police que le jeu a déjà gravée.
[[nodiscard]] inline bool UsesGameFont(const SignboardStyle& style) {
  return style.font_name.empty() && style.font_pixels == kNameFontPixels;
}

}  // namespace moonclient
