// LES MURS TRIPLES : les noeuds du sous-type 75.
//
// 4 identifiants, TROIS batisseurs. Chaque effet pose DEUX noeuds, aux
// azimuts zero et quarante-cinq : l'azimut appartient au noeud, pas au
// patron, et il n'est donc pas dans cette table.
//
// ET TOUS NE SEMENT PAS AU PREMIER TIC : le vert attend le cinquieme, le
// site de 590 le quinzieme. C'est le dernier champ.
//
// REFUSE - l'identifiant 338 : son batisseur recoit un INDEX (0 a 3), pas un azimut.
//
// REFUSE - l'identifiant 582 : son batisseur recoit un INDEX (10 et 11), pas un azimut.
//
// ENGENDRE par scripts/gen_client_effect_triple_walls.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_triple_wall_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectTripleWallPattern, 4> kTripleWalls = {{
    {337,
     "Effect_BuildUpdate_MagicGreenRing",
     "data\\texture\\effect\\magic_green.tga",
     9,
     2.0F,
     0,
     {5, 5, 0}},
    {444,
     "Effect_Build_MagicBlueRing4b",
     "data\\texture\\effect\\magic_blue.tga",
     4,
     0.0F,
     1,
     {0, 0, 0}},
    {590,
     "Effect_Build_MagicBlueRing",
     "data\\texture\\effect\\magic_blue.tga",
     4,
     1.5F,
     0,
     {15, 15, 0}},
    {834,
     "Effect_BuildUpdate_MagicGreenRing",
     "data\\texture\\effect\\magic_green.tga",
     9,
     2.0F,
     0,
     {5, 5, 0}},
}};

}  // namespace

std::span<const EffectTripleWallPattern> EffectTripleWallPatternTable() { return kTripleWalls; }

}  // namespace ro::render
