// CE QU'UN STYLE DE CARRÉ TEINTÉ SÈME, et quel effet emploie quel style.
//
// 10 styles pour 12 nœuds répartis sur 12 effets, dont 1 pose une
// sous-marque — et c'est exactement celui dont la marque de dessin délègue.
//
// ENGENDRÉ par scripts/gen_client_effect_tinted_quads.py — ne pas
// modifier à la main.

#include <algorithm>
#include <array>
#include <span>
#include <string_view>

#include "ro/render/effect_tinted_quad_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectTintedQuadStyleSeed, 10> kTintedQuadStyles = {{
    {0, 3.0F, 2, 0, 0.0F, 0, 0, false},
    {1, 5.0F, 2, 0, 0.0F, 0, 0, false},
    {2, 3.0F, 2, 0, 0.0F, 0, 2, true},
    {3, 3.0F, 3, 0, 0.0F, 2, 0, false},
    {4, 15.0F, 2, 0, 10.0F, 2, 0, false},
    {5, 3.0F, 4, 0, 0.0F, 2, 3, false},
    {6, 8.0F, 4, 0, 0.0F, 5, 0, false},
    {7, 3.0F, 7, 0, 0.0F, 5, 0, false},
    {8, 3.0F, 2, 0, 0.0F, 5, 8, false},
    {9, 1.5F, 9, 6, 0.0F, 13, 0, false},
}};

// LES TEXTURES ENTRE LESQUELLES UN STYLE TIRE, À PLAT : chaque graine y
// prend sa tranche. L'ORDRE EST CELUI DU TIRAGE du client.
constexpr std::array<std::string_view, 13> kTintedQuadStyleTextures = {{
    "data\\texture\\effect\\melody_a.bmp",
    "data\\texture\\effect\\melody_b.bmp",
    "data\\texture\\effect\\레드젬스톤.bmp",
    "data\\texture\\effect\\블루젬스톤.bmp",
    "data\\texture\\effect\\옐로우젬스톤.bmp",
    "data\\texture\\effect\\spell_01.bmp",
    "data\\texture\\effect\\spell_02.bmp",
    "data\\texture\\effect\\spell_03.bmp",
    "data\\texture\\effect\\spell_04.bmp",
    "data\\texture\\effect\\spell_05.bmp",
    "data\\texture\\effect\\spell_06.bmp",
    "data\\texture\\effect\\spell_07.bmp",
    "data\\texture\\effect\\spell_08.bmp",
}};

constexpr std::array<EffectTintedQuadNodePattern, 12> kTintedQuadNodes = {{
    {278, "Effect_Build_SpellMelodyBubbles", "data\\texture\\effect\\zz.bmp", 1},
    {279, "Effect_Build_SpellMelodyBubbles", "data\\texture\\effect\\pocket.bmp", 3},
    {281, "Effect_Build_SpellMelodyBubbles", "", 2},
    {282, "Effect_Build_SpellMelodyBubbles", "data\\texture\\effect\\twirl.bmp", 7},
    {284, "Effect_Build_SpellMelodyBubbles", "", 5},
    {286, "Effect_Build_SpellMelodyBubbles", "data\\texture\\effect\\melody_b.bmp", 0},
    {288, "Effect_Build_SpellMelodyBubbles", "", 8},
    {289, "Effect_Build_SpellMelodyBubbles", "data\\texture\\effect\\idun_apple.bmp", 6},
    {291, "Effect_Build_SpellMelodyBubbles", "data\\texture\\effect\\melody_a.bmp", 0},
    {293, "Effect_Build_SpellMelodyBubbles", "data\\texture\\effect\\kiss.bmp", 4},
    {370, "Effect_Build_SpellMelodyBubbles", "data\\texture\\effect\\cross_old.bmp", 9},
    {674, "Effect_Build_SpellMelodyBubbles", "data\\texture\\effect\\curse.bmp", 6},
}};

}  // namespace

std::span<const EffectTintedQuadStyleSeed> EffectTintedQuadStyleSeedTable() {
  return kTintedQuadStyles;
}

std::span<const std::string_view> EffectTintedQuadStyleTextures(
    const EffectTintedQuadStyleSeed& seed) {
  return std::span<const std::string_view>{kTintedQuadStyleTextures}.subspan(seed.first_texture,
                                                                             seed.texture_count);
}

std::span<const EffectTintedQuadNodePattern> EffectTintedQuadNodePatternTable() {
  return kTintedQuadNodes;
}

std::span<const EffectTintedQuadNodePattern> EffectTintedQuadNodePatternsFor(
    std::uint32_t effect_id) {
  const auto [first, last] = std::ranges::equal_range(kTintedQuadNodes, effect_id, {},
                                                      &EffectTintedQuadNodePattern::effect_id);
  return {first, last};
}

}  // namespace ro::render
