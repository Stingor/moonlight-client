// LES MURS TRIPLES : les noeuds du sous-type 75.
//
// 6 identifiants, QUATRE batisseurs. Chaque effet pose DEUX noeuds, aux
// azimuts zero et quarante-cinq : l'azimut appartient au noeud, pas au
// patron, et il n'est donc pas dans cette table.
//
// ET TOUS NE SEMENT PAS AU PREMIER TIC : le vert attend le cinquieme, le
// site de 590 le quinzieme, et l'ange pose un azimut au soixante-cinquieme
// puis l'autre au suivant. Ce sont les deux derniers champs.
//
// ENGENDRE par scripts/gen_client_effect_triple_walls.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_triple_wall_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectTripleWallPattern, 6> kTripleWalls = {{
    {337,
     "Effect_BuildUpdate_MagicGreenRing",
     "data\\texture\\effect\\magic_green.tga",
     9,
     2.0F,
     0,
     {5, 5, 0},
     false},
    {338,
     "Effect_BuildUpdate_ActorSpriteRing55",
     "data\\texture\\effect\\ring_blue.tga",
     4,
     2.0F,
     0,
     {65, 66, 1},
     true},
    {444,
     "Effect_Build_MagicBlueRing4b",
     "data\\texture\\effect\\magic_blue.tga",
     4,
     0.0F,
     1,
     {0, 0, 0},
     false},
    {582,
     "Effect_BuildUpdate_ActorSpriteRing55",
     "data\\texture\\effect\\ring_blue.tga",
     4,
     2.0F,
     0,
     {65, 66, 1},
     true},
    {590,
     "Effect_Build_MagicBlueRing",
     "data\\texture\\effect\\magic_blue.tga",
     4,
     1.5F,
     0,
     {15, 15, 0},
     false},
    {834,
     "Effect_BuildUpdate_MagicGreenRing",
     "data\\texture\\effect\\magic_green.tga",
     9,
     2.0F,
     0,
     {5, 5, 0},
     false},
}};

}  // namespace

std::span<const EffectTripleWallPattern> EffectTripleWallPatternTable() { return kTripleWalls; }

}  // namespace ro::render
