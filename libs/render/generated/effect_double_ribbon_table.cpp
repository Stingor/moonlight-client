// LES AURAS EN ANNEAU DOUBLE : ce que chaque site d'appel passe a son
// batisseur.
//
// Une ligne par site d'appel du batisseur.
//
// 1 auras pour 1 effets, toutes du sous-type 106. Le batisseur ne recoit
// qu'une image et pose DEUX noeuds ; tout le reste est ecrit en dur et vit
// dans le semis, voir effect_double_ribbon_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_double_ribbons.py - ne pas
// modifier a la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_double_ribbon_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectDoubleRibbonPattern, 1> kDoubleRibbons = {{
    {438, "Effect_Build_RadialFanNode", "data\\texture\\effect\\cloud11.tga"},
}};

}  // namespace

std::span<const EffectDoubleRibbonPattern> EffectDoubleRibbonPatternTable() {
  return kDoubleRibbons;
}

}  // namespace ro::render
