// LES TROIS PLANS : les noeuds des sous-types 111 et 115.
//
// 4 identifiants, DEUX batisseurs qui ne se ressemblent pas : l'un fait
// deriver ses plans en poursuivant des angles, l'autre les envoie en ligne
// droite. Et le second attend le TRENTIEME tic de son effet.
//
// ENGENDRE par scripts/gen_client_effect_tri_planes.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_tri_plane_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectTriPlanePattern, 4> kTriPlanes = {{
    {506,
     "Effect_Build_Scatter6fFromParam",
     "data\\texture\\effect\\whitelight.tga",
     EffectTriPlaneBuilder::kDrifting,
     0,
     {0, 0, 0},
     1},  // EF_SOULLIGHT, derive, bleue
    {518,
     "Effect_Build_ScatterFromParam73",
     "data\\texture\\effect\\whitelight.tga",
     EffectTriPlaneBuilder::kFlying,
     0,
     {30, 30, 0},
     4},  // EF_CARTTER, vol, teinte tiree entre trois roses, au TRENTIEME tic
    {850,
     "Effect_Build_Scatter6fFromParam",
     "data\\texture\\effect\\whitelight.tga",
     EffectTriPlaneBuilder::kDrifting,
     2,
     {0, 0, 0},
     1},  // EF_SOULLIGHT2, derive, trois bleus voisins - un par plan
    {932,
     "Effect_Build_Scatter6fFromParam",
     "data\\texture\\effect\\whitelight.tga",
     EffectTriPlaneBuilder::kDrifting,
     3,
     {0, 0, 0},
     1},  // EF_SOULLIGHT3, derive, teinte TIREE entre trois roses
}};

}  // namespace

std::span<const EffectTriPlanePattern> EffectTriPlanePatternTable() { return kTriPlanes; }

}  // namespace ro::render
