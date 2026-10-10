#pragma once

#include <imgui.h>

namespace ro::ui {

// ── LE FRONT D'UNE JAUGE, ET QUAND IL DOIT S'ARRONDIR ───────────────────────
//
// 🔴 UN SEUL FOYER, PARCE QUE TROIS SURFACES SE TROMPAIENT PAREIL. Les barres
// d'entité, les six jauges libres et le HUD de cible décidaient chacune de leur
// côté que le front reste une ARÊTE FRANCHE « tant que la jauge n'est pas
// pleine » — un seuil exprimé en PART REMPLIE : 99,9 % pour deux d'entre elles,
// 100 % pour la troisième.
//
// 🔴 ET CE SEUIL NE RÉPOND PAS À LA BONNE QUESTION. Ce qui compte n'est pas la
// part remplie, c'est la distance qui reste au front avant le bord — comparée au
// RAYON. À quatre-vingt-dix-neuf pour cent d'une barre de soixante pixels, il
// reste moins d'un pixel : le front est DANS le coin arrondi du creux, et un
// coin carré posé là dépasse la courbe. C'est ce qu'on voyait sur la barre d'un
// boss presque intact — arrondie à gauche, carrée à droite.
//
// ⚠ L'ARÊTE FRANCHE RESTE LA RÈGLE AILLEURS : un bout arrondi au milieu du creux
// a l'air de s'effacer avant d'arriver. Elle ne cède que là où la courbe du
// creux la rattrape.

// VRAI quand le front est entré dans la zone de l'arrondi, et doit donc la
// suivre. `remaining_pixels` est ce qui reste entre le front et le bord OPPOSÉ à
// celui d'où la jauge se remplit.
[[nodiscard]] constexpr bool GaugeFrontRounds(float remaining_pixels, float rounding) noexcept {
  return rounding > 0.0F && remaining_pixels < rounding;
}

// LES COINS D'UN REMPLISSAGE : ceux de l'arrière toujours, ceux du front
// seulement quand la courbe du creux les rattrape.
//
// ⚠ `back` ET `front` SONT DONNÉS PAR L'APPELANT, parce que le sens lui
// appartient : une barre se remplit vers la droite ou vers la gauche, une jauge
// verticale vers le haut ou vers le bas.
[[nodiscard]] constexpr ImDrawFlags GaugeFillCorners(float remaining_pixels, float rounding,
                                                     ImDrawFlags back, ImDrawFlags front) noexcept {
  return GaugeFrontRounds(remaining_pixels, rounding) ? (back | front) : back;
}

}  // namespace ro::ui
