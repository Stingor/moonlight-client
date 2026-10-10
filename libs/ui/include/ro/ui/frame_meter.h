#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <imgui.h>

namespace ro::ui {

// LE COMPTEUR D'IMAGES, posé par-dessus le jeu.
//
// ⚠ IL NE MESURE RIEN LUI-MÊME. La cadence et le temps d'image sont ceux que
// l'interface tient déjà — elle les calcule à chaque `NewFrame`, et en refaire
// une seconde mesure donnerait deux chiffres qui ne s'accorderaient jamais tout
// à fait.
//
// 🔴 CE QU'IL SERT À VOIR N'EST PAS LA MOYENNE. Un compteur moyenné cache
// exactement ce qu'on vient chercher : l'à-coup. Vingt images à 5 ms et une à
// 200 ms se lisent « 60 images par seconde », et c'est la seule qu'on a sentie.
// D'où la courbe, qui est la vraie mesure — le nombre n'est là que pour dire
// l'ordre de grandeur.

// Combien d'images la courbe garde. Deux secondes à soixante : assez pour voir
// un à-coup repasser, assez peu pour qu'il reste identifiable.
inline constexpr std::size_t kFrameHistoryLength = 120;

// Le haut de l'échelle, en millisecondes. Trente-trois : une image à trente par
// seconde touche le plafond, ce qui fait du sommet une limite qu'on reconnaît
// plutôt qu'un nombre rond.
inline constexpr float kFrameGraphCeilingMs = 33.3F;

// Ce que le compteur montre, et ce que le joueur en a réglé.
struct FrameMeterState {
  bool shown = false;
  // 🔴 UN SEUL RÉGLAGE POUR DEUX EFFETS, et c'est délibéré : figer le compteur
  // et le rendre traversant sont la même intention — « il est posé, oublie-le ».
  // Deux cases permettraient l'état absurde d'un panneau qui avale les clics
  // sans qu'on puisse le déplacer.
  bool locked = false;
  bool graph = true;
  bool ping = true;

  // Les temps d'image, en anneau. ⚠ ICI ET NON DANS UNE STATIQUE : `ui` ne
  // possède rien de global, et l'historique est ce que la fenêtre garde.
  std::array<float, kFrameHistoryLength> history{};
  std::size_t head = 0;
};

// Dessine le compteur, et note l'image qui vient de passer.
//
// ⚠ À APPELER À CHAQUE IMAGE, même caché : l'historique se perdrait sinon dès
// qu'on referme le compteur, et il repartirait vide à sa réouverture — juste au
// moment où l'on vient regarder ce qui s'est passé.
//
// `round_trip_ms` NÉGATIF veut dire « pas encore mesuré », et la ligne le dit
// alors en toutes lettres : un « 0 ms » se lirait comme une mesure.
void DrawFrameMeter(FrameMeterState& state, std::int32_t round_trip_ms);

}  // namespace ro::ui
