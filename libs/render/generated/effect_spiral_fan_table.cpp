// CE QU'UN SEMIS D'ÉVENTAIL POSE, et quel effet emploie quel semis.
//
// 5 semis distincts pour 22 nœuds répartis sur 22 effets, posés par 2
// bâtisseurs.
//
// ⚠ LE NUMÉRO DE STYLE N'EST PAS LA CLEF : il vaut la marque cinq chez un
// bâtisseur et la marque un chez l'autre. Chaque nœud porte le RANG de son
// semis, et c'est la MARQUE que la figure lit.
//
// ENGENDRÉ par scripts/gen_client_effect_spiral_fans.py — ne pas
// modifier à la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_spiral_fan_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectSpiralFanStyleSeed, 5> kSpiralFanSeeds = {{
    {1, 18.0F, 0.0F},
    {5, 13.0F, 0.0F},
    {6, 21.0F, 0.0F},
    {7, 21.0F, 0.0F},
    {4, 5.0F, 1.0F},
}};

constexpr std::array<EffectSpiralFanNodePattern, 22> kSpiralFanNodes = {{
    {467, "Effect_Build_SkillIconNode", "data\\texture\\effect\\hanmoon1.tga", 0, 1, 0, false},
    {468, "Effect_Build_SkillIconNode", "data\\texture\\effect\\hanmoon2.tga", 0, 1, 0, false},
    {469, "Effect_Build_SkillIconNode", "data\\texture\\effect\\hanmoon3.tga", 0, 1, 0, false},
    {470, "Effect_Build_SkillIconNode", "data\\texture\\effect\\hanmoon4.tga", 0, 1, 0, false},
    {471, "Effect_Build_SkillIconNode", "data\\texture\\effect\\hanmoon7.tga", 0, 1, 0, false},
    {472, "Effect_Build_SkillIconNode", "data\\texture\\effect\\hanmoon5.tga", 0, 1, 0, false},
    {473, "Effect_Build_SkillIconNode", "data\\texture\\effect\\hanmoon6.tga", 0, 1, 0, false},
    {758, "Effect_InitSizedSprite", "data\\texture\\effect\\회전카운터1.tga", 1, 1, 0, false},
    {759, "Effect_InitSizedSprite", "data\\texture\\effect\\회전카운터2.tga", 1, 1, 0, false},
    {760, "Effect_InitSizedSprite", "data\\texture\\effect\\회전카운터3.tga", 1, 1, 0, false},
    {761, "Effect_InitSizedSprite", "data\\texture\\effect\\회전카운터4.tga", 1, 1, 0, false},
    {762, "Effect_InitSizedSprite", "data\\texture\\effect\\회전카운터5.tga", 1, 1, 0, false},
    {763, "Effect_InitSizedSprite", "data\\texture\\effect\\회전카운터6.tga", 1, 1, 0, false},
    {764, "Effect_InitSizedSprite", "data\\texture\\effect\\회전카운터7.tga", 1, 1, 0, false},
    {765, "Effect_InitSizedSprite", "data\\texture\\effect\\회전카운터8.tga", 1, 1, 0, false},
    {766, "Effect_InitSizedSprite", "data\\texture\\effect\\회전카운터9.tga", 1, 1, 0, false},
    {767, "Effect_InitSizedSprite", "data\\texture\\effect\\회전카운터10.tga", 1, 1, 0, false},
    {803, "Effect_Build_SkillIconNode", "", 4, 10, 30, true},
    {808, "Effect_Build_SkillIconNode", "data\\texture\\유저인터페이스\\item\\SC_REPRODUCE.bmp", 4,
     5, 30, false},
    {859, "Effect_InitSizedSprite", "data\\texture\\effect\\priest_spell.bmp", 2, 1, 0, false},
    {862, "Effect_InitSizedSprite", "data\\texture\\effect\\priest_spell.bmp", 3, 1, 0, false},
    {879, "Effect_InitSizedSprite", "data\\texture\\effect\\kiss2.bmp", 3, 1, 0, false},
}};

}  // namespace

std::span<const EffectSpiralFanStyleSeed> EffectSpiralFanStyleSeedTable() {
  return kSpiralFanSeeds;
}

std::span<const EffectSpiralFanNodePattern> EffectSpiralFanNodePatternTable() {
  return kSpiralFanNodes;
}

std::span<const EffectSpiralFanNodePattern> EffectSpiralFanNodePatternsFor(
    std::uint32_t effect_id) {
  const auto [first, last] = std::ranges::equal_range(kSpiralFanNodes, effect_id, {},
                                                      &EffectSpiralFanNodePattern::effect_id);
  return {first, last};
}

}  // namespace ro::render
