// CE QU'UN STYLE D'ARC SÈME, et quel effet emploie quel style.
//
// 19 styles pour 47 nœuds répartis sur 37 effets. 9 styles prennent leur
// rayon sur l'échelle de leur instance, et 11 tirent leur angle de départ.
//
// ENGENDRÉ par scripts/gen_client_effect_arc_ribbons.py — ne pas
// modifier à la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_arc_ribbon_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectArcRibbonStyleSeed, 19> kArcRibbonStyles = {{
    {0, 5.0F, 0.0F, 135, false, 2, 0, 10.0F, false, 0.0F},
    {1, 8.0F, 0.0F, 120, false, 0, 1, 30.0F, false, 1.0F},
    {2, 10.0F, 0.0F, 135, false, 2, 2, 10.0F, false, 0.0F},
    {3, 8.0F, 0.0F, 155, false, 3, 3, 30.0F, false, 2.0F},
    {4, 6.0F, 0.0F, 135, false, 0, 1, 30.0F, false, 0.0F},
    {10, 70.0F, 0.7F, 0, true, 10, 10, 0.0F, false, 0.0F},
    {11, 70.0F, 0.7F, 0, true, 10, 11, 0.0F, false, 0.0F},
    {12, 120.0F, 1.2F, 0, true, 12, 12, 0.0F, false, 0.0F},
    {13, 300.0F, 3.0F, 0, true, 12, 13, 0.0F, false, 0.0F},
    {14, 30.0F, 1.0F, 0, true, 4, 14, 0.0F, false, 0.0F},
    {15, 30.0F, 1.0F, 0, true, 15, 15, 0.0F, false, 0.0F},
    {16, 9.5F, 0.9F, 0, true, 5, 16, 0.0F, false, 0.0F},
    {17, 8.075F, 0.765F, 0, true, 5, 17, 0.0F, false, 0.0F},
    {18, 9.0F, 0.9F, 0, true, 5, 18, 0.0F, false, 0.0F},
    {19, 8.0F, 0.0F, 0, true, 15, 19, 30.0F, false, 0.0F},
    {21, 24.0F, 0.0F, 90, false, 4, 21, 10.0F, false, 0.0F},
    {22, 4.0F, 0.0F, 135, false, 16, 22, 0.0F, true, 0.0F},
    {31, 2.0F, 0.0F, 135, false, 0, 31, 30.0F, false, 0.0F},
    {32, 8.0F, 0.0F, 0, true, 32, 32, 30.0F, false, 0.0F},
}};

constexpr std::array<EffectArcRibbonNodePattern, 47> kArcRibbonNodes = {{
    {348, "Effect_Build_ElementalRisingSpriteFromParam",
     "data\\texture\\effect\\thunder_center.bmp", 0},
    {381, "Effect_Build_ElementalRisingSpriteFromParam",
     "data\\texture\\effect\\thunder_center.bmp", 0},
    {523, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\Tarot01.tga", 1},
    {524, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\Tarot02.tga", 1},
    {525, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\Tarot03.tga", 1},
    {526, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\Tarot04.tga", 1},
    {527, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\Tarot05.tga", 1},
    {528, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\Tarot06.tga", 1},
    {529, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\Tarot07.tga", 1},
    {530, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\Tarot08.tga", 1},
    {531, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\Tarot09.tga", 1},
    {532, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\Tarot10.tga", 1},
    {533, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\Tarot11.tga", 1},
    {534, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\Tarot12.tga", 1},
    {535, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\Tarot13.tga", 1},
    {536, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\Tarot14.tga", 1},
    {554, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\freeze_a.bmp", 2},
    {610, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\success.bmp", 3},
    {611, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\failed.bmp", 3},
    {676, "Effect_Build_ElementalRisingSpriteFromParam",
     "data\\texture\\effect\\blast_mine##clock.bmp", 4},
    {681, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\구체2.bmp", 12},
    {681, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\구체3.bmp", 13},
    {681, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\구체4.bmp", 11},
    {681, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\구체5.bmp", 10},
    {693, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\glow01.bmp", 14},
    {694, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\glow02.bmp", 14},
    {700, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\구체12.bmp", 12},
    {700, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\구체3.bmp", 13},
    {700, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\구체14.bmp", 11},
    {700, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\구체15.bmp", 10},
    {701, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\glow11.bmp", 15},
    {702, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\glow12b .bmp", 14},
    {710, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\b.bmp", 18},
    {710, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\glow02.bmp", 16},
    {710, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\glow02.bmp", 17},
    {712, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\b.bmp", 18},
    {712, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\glow03.bmp", 16},
    {712, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\glow03.bmp", 17},
    {755, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\마법진1-3.bmp",
     19},
    {786, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\target3.bmp", 21},
    {788, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\베놈_상태.bmp",
     22},
    {789, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\빙결_상태.bmp",
     22},
    {790, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\발화_상태.bmp",
     22},
    {902, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\pe_fur1_01.bmp", 1},
    {940, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\얼음결정체1.tga",
     22},
    {1525, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\Buff_Prison.bmp",
     31},
    {1601, "Effect_Build_ElementalRisingSpriteFromParam", "data\\texture\\effect\\마법진1-3.bmp",
     32},
}};

}  // namespace

std::span<const EffectArcRibbonStyleSeed> EffectArcRibbonStyleSeedTable() {
  return kArcRibbonStyles;
}

std::span<const EffectArcRibbonNodePattern> EffectArcRibbonNodePatternTable() {
  return kArcRibbonNodes;
}

std::span<const EffectArcRibbonNodePattern> EffectArcRibbonNodePatternsFor(
    std::uint32_t effect_id) {
  const auto [first, last] = std::ranges::equal_range(kArcRibbonNodes, effect_id, {},
                                                      &EffectArcRibbonNodePattern::effect_id);
  return {first, last};
}

}  // namespace ro::render
