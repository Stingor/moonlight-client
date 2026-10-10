#pragma once

#include <cstdint>

#include <imgui.h>

namespace ro::ui {

// ── LE REMPLISSAGE D'UNE JAUGE, INSCRIT DANS SON CADRE ──────────────────────
//
// 🔴 UNE SEULE GÉOMÉTRIE POUR TOUTES LES JAUGES PEINTES : barres d'entité,
// jauges libres, HUD de cible, barre d'incantation, tuiles du groupe.
//
// 🔴 LE REMPLISSAGE N'EST PAS UN RECTANGLE ARRONDI PLUS COURT. C'est la forme
// arrondie de L'INTÉRIEUR du cadre, DÉCOUPÉE par la boîte remplie. Un rectangle
// arrondi plus court ne le peut pas : ImGui borne son rayon à sa propre largeur,
// et une part de quelques pixels redevient un carré qui dépasse les coins du
// cadre. Le découpage donne trois propriétés à la fois :
//
//   · rien ne sort du cadre — ni par la bordure, ni par les coins —, quelle que
//     soit la part : un remplissage minuscule suit la courbe du coin ;
//   · le front reste une ARÊTE FRANCHE au milieu du creux, et ne suit la courbe
//     que là où elle le rattrape — un bout arrondi au milieu du creux aurait
//     l'air de s'effacer avant d'arriver ;
//   · plein, il est exactement l'intérieur.
//
// ⚠ L'INTÉRIEUR EST LE CADRE MOINS SA BORDURE, de chaque côté, et son rayon est
// celui du cadre moins la bordure, SUR LE MÊME CENTRE : sa courbe est parallèle
// à celle du cadre. Le rayon du cadre est celui qu'ImGui lui donne vraiment, et
// non celui demandé — voir `DrawnRounding`.

// Le sens dans lequel une jauge se remplit : le bord d'où part la part pleine.
enum class GaugeGrowth : std::uint8_t {
  // Depuis la gauche.
  kRightward,
  // Depuis la droite.
  kLeftward,
  // Depuis le bas.
  kUpward,
  // Depuis le haut.
  kDownward,
};

// Un rectangle aux quatre coins arrondis du même rayon. Un rayon nul le laisse
// droit.
struct RoundedBox {
  ImVec2 min{0.0F, 0.0F};
  ImVec2 max{0.0F, 0.0F};
  float rounding = 0.0F;
};

// Un rectangle droit, bornes comprises.
struct AxisBox {
  ImVec2 min{0.0F, 0.0F};
  ImVec2 max{0.0F, 0.0F};
};

// LE RAYON QU'IMGUI DONNE VRAIMENT à un rectangle plein arrondi aux quatre
// coins : il le borne à la moitié du plus petit côté moins un pixel, et peint
// droit en dessous d'un demi-pixel. Rend zéro dans ce dernier cas.
[[nodiscard]] float DrawnRounding(ImVec2 min, ImVec2 max, float rounding);

// L'INTÉRIEUR d'un cadre arrondi : `border_pixels` retiré de chaque côté, et du
// rayon effectif du cadre, borné à zéro. Un cadre plus étroit que ses deux
// bordures donne un intérieur vide.
[[nodiscard]] RoundedBox GaugeInterior(ImVec2 frame_min, ImVec2 frame_max, float frame_rounding,
                                       float border_pixels);

// LA BOÎTE REMPLIE de l'intérieur pour une part de `fill` — bornée à [0, 1],
// et nulle quand elle n'est pas un nombre.
[[nodiscard]] AxisBox GaugeFillBox(const RoundedBox& interior, float fill, GaugeGrowth growth);

// LE RETRAIT HORIZONTAL du bord de `shape` à l'ordonnée `y` : zéro sur les côtés
// droits, et ce que la courbe d'un coin mange à cette hauteur.
[[nodiscard]] float RoundedBoxInsetAt(const RoundedBox& shape, float y);

// POSE DANS LE CHEMIN COURANT de `into` la part de `shape` comprise dans `box`.
// Le chemin est convexe, et se remplit donc par `PathFillConvex`. Rend faux, et
// laisse le chemin vide, quand cette part est vide.
bool PathRoundedBoxWithin(ImDrawList& into, const RoundedBox& shape, const AxisBox& box);

// La part de `shape` comprise dans `box`, peinte d'une couleur.
void FillRoundedBoxWithin(ImDrawList& into, const RoundedBox& shape, const AxisBox& box,
                          ImU32 color);

// Le chemin du remplissage d'une jauge : voir `PathRoundedBoxWithin`.
bool PathGaugeFill(ImDrawList& into, const RoundedBox& interior, float fill, GaugeGrowth growth);

// Le remplissage d'une jauge, d'une couleur.
void PaintGaugeFill(ImDrawList& into, const RoundedBox& interior, float fill, GaugeGrowth growth,
                    ImU32 color);

}  // namespace ro::ui
