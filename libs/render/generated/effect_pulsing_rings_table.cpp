// LES ANNEAUX QUI PULSENT DU CLIENT : les noeuds du sous-type 63, et les
// quatre nombres qui changent d'un batisseur a l'autre.
//
// 11 noeuds, CINQ batisseurs - et non les neuf que comptent les noms.
// Leur figure est la jupe d'anneau, portee depuis le debut du lot 8 : ce
// sous-type n'a coute que son semis et son mouvement.
//
// ENGENDRE par scripts/gen_client_effect_pulsing_rings.py - ne pas
// modifier a la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_pulsing_rings_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectPulsingRingsPattern, 11> kPulsingRings = {{
    {316, "Effect_BuildUpdate_ReadyPortalRings", "data\\texture\\effect\\ring_blue.tga", 5, 1, 100,
     10, 0, 0},
    {317, "Effect_BuildUpdate_VioletRedRingsCloud", "data\\texture\\effect\\ring_blue.tga", 4, 0,
     3111, 10, 1, 0},
    {341, "Effect_BuildUpdate_VioletRedRingsCloud", "data\\texture\\effect\\ring_red.tga", 10, 3,
     3111, 20, 1, 0},
    {754, "Effect_BuildUpdate_WhiteRingPulse", "data\\texture\\effect\\ring_white.tga", 16, 1, 100,
     10, 0, 100},
    {822, "Effect_InitConcentricRings", "data\\texture\\effect\\ring_white.tga", 4, 4, 100, 10, 0,
     100},
    {825, "Effect_InitConcentricRings", "data\\texture\\effect\\ring_white.tga", 4, 4, 100, 10, 0,
     100},
    {827, "Effect_InitConcentricRings", "data\\texture\\effect\\ring_white.tga", 8, 4, 100, 10, 0,
     100},
    {829, "Effect_InitConcentricRings", "data\\texture\\effect\\ring_white.tga", 6, 4, 100, 10, 0,
     100},
    {883, "Effect_BuildUpdate_WhiteRingPulse", "data\\texture\\effect\\ring_white.tga", 16, 1, 100,
     10, 0, 100},
    {1569, "Effect_InitConcentricRingsScaled", "data\\texture\\effect\\ring_white.tga", 28, 1, 100,
     10, 0, 100},
    {1655, "Effect_InitConcentricRingsScaled", "data\\texture\\effect\\ring_white.tga", 29, 5, 100,
     10, 0, 100},
}};

}  // namespace

std::span<const EffectPulsingRingsPattern> EffectPulsingRingsPatternTable() {
  return kPulsingRings;
}

}  // namespace ro::render
