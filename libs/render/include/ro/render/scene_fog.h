#pragma once

#include <array>

namespace ro::render {

// LE BROUILLARD D'UNE IMAGE, tel que les renderers le peignent.
//
// Linéaire sur la PROFONDEUR DE VUE — la distance le long de l'axe de la
// caméra, et non la distance à l'œil : nul à `start_depth`, complet à
// `end_depth`. Chaque SOMMET prend sa part, et le triangle la mélange ensuite
// d'un sommet à l'autre ; la part que la couleur garde se tronque au
// deux-cent-cinquante-cinquième. Voir `kFogVertexGlsl`.
//
// ⚠ ÉTEINT PAR DÉFAUT : un dessin qui ne dit rien ne s'embrume pas.
struct SceneFog {
  bool on = false;
  float start_depth = 0.0F;
  float end_depth = 1.0F;
  // De zéro à un, dans l'ordre rouge, vert, bleu.
  std::array<float, 3> color{};
};

}  // namespace ro::render
