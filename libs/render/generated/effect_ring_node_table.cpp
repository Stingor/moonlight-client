// LES ANNEAUX QU'UN EFFET SÈME, par identifiant d'effet.
//
// Une ligne par site d'appel du batisseur.
//
// 62 anneaux pour 32 effets — un effet en sème souvent deux, à deux
// alphas de départ, et parfois à deux styles.
//
// ENGENDRÉ par scripts/gen_client_effect_ring_nodes.py — ne pas
// modifier à la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_track_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectRingNodePattern, 62> kRingNodes = {{
    {12, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_yellow.tga", 45, 1},
    {12, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_yellow.tga", 25, 1},
    {58, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 45, 0},
    {58, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 25, 0},
    {328, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 45, 2},
    {328, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 25, 2},
    {342, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_red.tga", 45, 3},
    {342, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_red.tga", 25, 3},
    {367, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 45, 22},
    {367, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_yellow.tga", 25, 22},
    {441, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_blue.tga", 45, 5},
    {441, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_blue.tga", 25, 5},
    {454, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_black.tga", 45, 6},
    {454, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_black.tga", 25, 6},
    {510, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 45, 2},
    {510, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 25, 2},
    {564, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_red.tga", 45, 3},
    {564, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_red.tga", 25, 3},
    {826, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_black.tga", 45, 6},
    {826, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_black.tga", 25, 6},
    {911, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 45, 15},
    {911, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 25, 15},
    {1418, "Effect_Build_Rings4FromParam1e",
     "data\\texture\\effect\\new_comet\\new_comet_cast_ring\\ring_rainbow.tga", 45, 20},
    {1556, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_blue.tga", 45, 20},
    {1556, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_blue.tga", 25, 21},
    {1560, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 45, 22},
    {1560, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 25, 22},
    {1565, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_yellow.tga", 45, 23},
    {1565, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_yellow.tga", 25, 23},
    {1677, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 45, 31},
    {1677, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 25, 31},
    {1872, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 45, 42},
    {1872, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 25, 42},
    {1876, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 45, 43},
    {1876, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 25, 43},
    {1902, "Effect_Build_Rings4FromParam1e",
     "data\\texture\\effect\\new_abr_mother\\abr_mother.tga", 45, 0},
    {1902, "Effect_Build_Rings4FromParam1e",
     "data\\texture\\effect\\new_abr_mother\\abr_mother.tga", 25, 0},
    {1910, "Effect_Build_Rings4FromParam1e",
     "data\\texture\\effect\\new_abr_infinity\\ring_infinty.tga", 45, 0},
    {1910, "Effect_Build_Rings4FromParam1e",
     "data\\texture\\effect\\new_abr_infinity\\ring_infinty.tga", 25, 0},
    {1918, "Effect_Build_Rings4FromParam1e",
     "data\\texture\\effect\\new_d_cannon\\ABR_DUAL_CANNON.tga", 45, 0},
    {1918, "Effect_Build_Rings4FromParam1e",
     "data\\texture\\effect\\new_d_cannon\\ABR_DUAL_CANNON.tga", 25, 0},
    {1923, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_red.tga", 45, 44},
    {1923, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_red.tga", 25, 44},
    {1932, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\new_pramen\\ring_blue.tga", 90,
     5},
    {1938, "Effect_Build_Rings4FromParam1e",
     "data\\texture\\effect\\new_battle_w\\ABR_BATTLE_WARIOR.tga", 45, 0},
    {1938, "Effect_Build_Rings4FromParam1e",
     "data\\texture\\effect\\new_battle_w\\ABR_BATTLE_WARIOR.tga", 25, 0},
    {1959, "Effect_Build_Rings4FromParam1e",
     "data\\texture\\effect\\pneumaticusprocella\\pneumaticusprocella_cast\\ring_yellow.tga", 45,
     0},
    {1959, "Effect_Build_Rings4FromParam1e",
     "data\\texture\\effect\\pneumaticusprocella\\pneumaticusprocella_cast\\ring_yellow.tga", 25,
     0},
    {1963, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 45, 45},
    {1963, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 25, 45},
    {1986, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 45, 46},
    {1986, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 25, 46},
    {1998, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 45, 47},
    {1998, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 25, 47},
    {2240, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 45, 48},
    {2240, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 25, 48},
    {2249, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 45, 49},
    {2249, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 25, 49},
    {2255, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 45, 50},
    {2255, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_white.tga", 25, 50},
    {2367, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_yellow.tga", 45, 0},
    {2367, "Effect_Build_Rings4FromParam1e", "data\\texture\\effect\\ring_yellow.tga", 25, 0},
}};

}  // namespace

std::span<const EffectRingNodePattern> EffectRingNodePatternTable() { return kRingNodes; }

std::span<const EffectRingNodePattern> EffectRingNodePatternsFor(std::uint32_t effect_id) {
  const auto [first, last] =
      std::ranges::equal_range(kRingNodes, effect_id, {}, &EffectRingNodePattern::effect_id);
  return {first, last};
}

}  // namespace ro::render
