// LES DISPERSIONS D'ECLAIRS : les noeuds du sous-type 135.
//
// 1 identifiant et 2 lignes : deux arcs qui tournent l'un contre
// l'autre.
//
// ENGENDRE par scripts/gen_client_effect_electric_arcs.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_electric_arc_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectElectricArcPattern, 2> kElectricArcs = {{
    {794, "Effect_InitElectricArc", "data\\texture\\effect\\elec1.tga",
     0},  // le cap DECROIT de deux degres par tic
    {794, "Effect_InitElectricArc", "data\\texture\\effect\\elec1.tga",
     1},  // le cap CROIT de deux degres par tic
}};

}  // namespace

std::span<const EffectElectricArcPattern> EffectElectricArcPatternTable() { return kElectricArcs; }

}  // namespace ro::render
