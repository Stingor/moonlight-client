// LES ANNEAUX QUI S'ELARGISSENT ET RENAISSENT : les noeuds du sous-type 169.
//
// 1 identifiant, UN SEUL batisseur, UN SEUL site : l'image et l'octet
// de palette viennent du site, tout le reste est en dur.
//
// ENGENDRE par scripts/gen_client_effect_expanding_rings.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_expanding_ring_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectExpandingRingPattern, 1> kExpandingRings = {{
    {939, "Effect_Build_QuadRingExpand", "data\\texture\\effect\\ring_blue.tga", 0},
}};

}  // namespace

std::span<const EffectExpandingRingPattern> EffectExpandingRingPatternTable() {
  return kExpandingRings;
}

}  // namespace ro::render
