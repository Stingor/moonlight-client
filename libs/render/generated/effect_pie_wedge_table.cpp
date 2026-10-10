// LES ECLATS DE QUARTIERS : ce que chaque site d'appel passe a son batisseur.
//
// 1 eclats pour 1 effets, tous du sous-type 162. Le batisseur ne recoit
// qu'une image : le semis est le meme pour tous, et ce qui fait la figure
// vient du TIC de naissance. Voir effect_pie_wedge_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_pie_wedges.py - ne pas
// modifier a la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_pie_wedge_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectPieWedgePattern, 1> kPieWedges = {{
    {895, "Effect_InitAngularBurstA2", "data\\texture\\effect\\alpha_center.tga"},
}};

}  // namespace

std::span<const EffectPieWedgePattern> EffectPieWedgePatternTable() { return kPieWedges; }

}  // namespace ro::render
