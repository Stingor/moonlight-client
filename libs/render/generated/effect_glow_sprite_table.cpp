// CE QU'UN STYLE DE SPRITE ÉCLATANT SÈME, et quel effet emploie quel style.
//
// Une ligne par valeur de style.
//
// 15 styles pour 15 nœuds sur 15 effets, et 13 MONSTRES distincts —
// le bâtisseur les charge lui-même, le site d'appel ne les nomme pas.
//
// ENGENDRÉ par scripts/gen_client_effect_glow_sprites.py — ne pas
// modifier à la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_glow_sprite_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectGlowSpriteStyleSeed, 15> kGlowStyles = {{
    {0, 0, "data\\sprite\\몬스터\\geographer"},
    {2, 2, "data\\sprite\\몬스터\\워그"},
    {3, 3, "data\\sprite\\몬스터\\violy"},
    {4, 4, "data\\sprite\\몬스터\\succubus"},
    {5, 4, "data\\sprite\\몬스터\\mineral"},
    {6, 6, "data\\sprite\\몬스터\\ktullanux"},
    {7, 7, "data\\sprite\\몬스터\\garm"},
    {8, 8, "data\\sprite\\몬스터\\miyabi_ningyo"},
    {9, 9, "data\\sprite\\몬스터\\garden_keeper"},
    {10, 4, "data\\sprite\\몬스터\\ifrit"},
    {11, 6, "data\\sprite\\몬스터\\ifrit"},
    {12, 8, "data\\sprite\\몬스터\\ifrit"},
    {13, 13, "data\\sprite\\몬스터\\ginseng_bullet"},
    {14, 8, "data\\sprite\\몬스터\\nepenthes2_bullet"},
    {15, 8, "data\\sprite\\몬스터\\imp"},
}};

constexpr std::array<EffectGlowSpriteNodePattern, 15> kGlowNodes = {{
    {853, "Effect_InitScatterSprAct", 0},
    {866, "Effect_InitScatterSprAct", 2},
    {868, "Effect_InitScatterSprAct", 3},
    {870, "Effect_InitScatterSprAct", 4},
    {872, "Effect_InitScatterSprAct", 5},
    {874, "Effect_InitScatterSprAct", 6},
    {876, "Effect_InitScatterSprAct", 7},
    {878, "Effect_InitScatterSprAct", 8},
    {915, "Effect_InitScatterSprAct", 9},
    {916, "Effect_InitScatterSprAct", 10},
    {917, "Effect_InitScatterSprAct", 11},
    {918, "Effect_InitScatterSprAct", 12},
    {922, "Effect_InitScatterSprAct", 13},
    {927, "Effect_InitScatterSprAct", 14},
    {929, "Effect_InitScatterSprAct", 15},
}};

}  // namespace

std::span<const EffectGlowSpriteStyleSeed> EffectGlowSpriteStyleSeedTable() { return kGlowStyles; }

std::span<const EffectGlowSpriteNodePattern> EffectGlowSpriteNodePatternTable() {
  return kGlowNodes;
}

std::span<const EffectGlowSpriteNodePattern> EffectGlowSpriteNodePatternsFor(
    std::uint32_t effect_id) {
  const auto [first, last] =
      std::ranges::equal_range(kGlowNodes, effect_id, {}, &EffectGlowSpriteNodePattern::effect_id);
  return {first, last};
}

}  // namespace ro::render
