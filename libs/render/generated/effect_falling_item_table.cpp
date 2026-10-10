// CE QU'UN STYLE D'OBJET QUI TOMBE SÈME, et quel effet emploie quel style.
//
// 5 styles pour 8 nœuds répartis sur 6 effets — le 930 en lance trois,
// à trois hauteurs tirées.
//
// ENGENDRÉ par scripts/gen_client_effect_falling_items.py — ne pas
// modifier à la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_falling_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectFallingStyleSeed, 5> kFallingStyles = {{
    {0, 7, 0, 12.0F, 115.0F, 0.0F},
    {1, 9, 1, 8.0F, 80.0F, 0.0F},
    {2, 9, 3, 8.0F, 80.0F, 0.0F},
    {3, 9, 4, 8.0F, 80.0F, 0.0F},
    {4, 7, 0, 14.0F, 115.0F, 56.0F},
}};

constexpr std::array<EffectFallingNodePattern, 8> kFallingNodes = {{
    {365, "Effect_Build_RisingSpriteFromParam57", "data\\texture\\effect\\cross_old.bmp", 0},
    {497, "Effect_Build_RisingSpriteFromParam57",
     "data\\texture\\유저인터페이스\\item\\레드슬림포션.bmp", 1},
    {498, "Effect_Build_RisingSpriteFromParam57",
     "data\\texture\\유저인터페이스\\item\\옐로우슬림포션.bmp", 2},
    {499, "Effect_Build_RisingSpriteFromParam57",
     "data\\texture\\유저인터페이스\\item\\화이트슬림포션.bmp", 3},
    {906, "Effect_Build_RisingSpriteFromParam57", "data\\texture\\effect\\shield.bmp", 0},
    {930, "Effect_Build_RisingSpriteFromParam57", "data\\texture\\유저인터페이스\\item\\스피어.bmp",
     4},
    {930, "Effect_Build_RisingSpriteFromParam57", "data\\texture\\유저인터페이스\\item\\파이크.bmp",
     4},
    {930, "Effect_Build_RisingSpriteFromParam57", "data\\texture\\유저인터페이스\\item\\궁그닐.bmp",
     4},
}};

}  // namespace

std::span<const EffectFallingStyleSeed> EffectFallingStyleSeedTable() { return kFallingStyles; }

std::span<const EffectFallingNodePattern> EffectFallingNodePatternTable() { return kFallingNodes; }

std::span<const EffectFallingNodePattern> EffectFallingNodePatternsFor(std::uint32_t effect_id) {
  const auto [first, last] =
      std::ranges::equal_range(kFallingNodes, effect_id, {}, &EffectFallingNodePattern::effect_id);
  return {first, last};
}

}  // namespace ro::render
