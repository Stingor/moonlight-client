// LES EVENTAILS DU CLIENT : ce que chaque site d'appel passe a son batisseur,
// et COMBIEN DE FOIS il le lui passe.
//
// 9 eventails, tous du sous-type 89. Chacun pose PLUSIEURS noeuds - le
// nombre de tours de la boucle de son site - et l'index de ce tour decale
// l'inclinaison de chaque noeud. C'est cela, l'eventail.
//
// ENGENDRE par scripts/gen_client_effect_cone_wedges.py - ne pas
// modifier a la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_cone_wedge_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectConeWedgePattern, 9> kConeWedges = {{
    {366, "Effect_Build_DirectionalFanNode", "data\\texture\\effect\\alpha_center.tga", 0, 5},
    {377, "Effect_Build_DirectionalFanNode", "data\\texture\\effect\\alpha_center.tga", 2, 8},
    {386, "Effect_Build_DirectionalFanNode", "data\\texture\\effect\\alpha_center.tga", 3, 12},
    {399, "Effect_Build_DirectionalFanNode", "data\\texture\\effect\\alpha_center.tga", 4, 6},
    {400, "Effect_Build_DirectionalFanNode", "data\\texture\\effect\\alpha_center.tga", 5, 6},
    {618, "Effect_Build_DirectionalFanNode", "data\\texture\\effect\\alpha_center.tga", 2, 2},
    {628, "Effect_Build_DirectionalFanNode", "data\\texture\\effect\\alpha_center.tga", 5, 6},
    {885, "Effect_Build_DirectionalFanNode", "data\\texture\\effect\\alpha_center.tga", 6, 8},
    {2425, "Effect_Build_DirectionalFanNode",
     "data\\texture\\effect\\WINDHAWK\\WH_WILD_WALK\\alpha_center_green.tga", 3, 12},
}};

}  // namespace

std::span<const EffectConeWedgePattern> EffectConeWedgePatternTable() { return kConeWedges; }

}  // namespace ro::render
