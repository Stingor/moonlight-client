// LES ANNEAUX EN GERBE : les noeuds du sous-type 107.
//
// 2 noeuds, UN batisseur, qui les seme a un tic d'ecart : le second
// part un degre et deux dixiemes de cycle plus loin que le premier.
//
// ENGENDRE par scripts/gen_client_effect_ring_starbursts.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_ring_starburst_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectRingStarburstPattern, 2> kRingStarbursts = {{
    {487,
     "Effect_Build_LayeredStarburst",
     "data\\texture\\effect\\ring_black.tga",
     1,
     2.5F,
     270,
     {53, 60, 55, 50},
     12.0F},
    {487,
     "Effect_Build_LayeredStarburst",
     "data\\texture\\effect\\ring_black.tga",
     2,
     2.7F,
     271,
     {52, 59, 54, 49},
     12.0F},
}};

}  // namespace

std::span<const EffectRingStarburstPattern> EffectRingStarburstPatternTable() {
  return kRingStarbursts;
}

}  // namespace ro::render
