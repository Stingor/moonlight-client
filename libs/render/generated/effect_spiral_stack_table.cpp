// LES PILES EN SPIRALE : ce que chaque effet passe a son batisseur.
//
// 7 piles, toutes du sous-type 103. L'image vient du SITE et non du
// batisseur, qui n'en connait aucune ; l'octet est le STYLE, et pour une valeur le GABARIT tout
// entier. Ce qu'il decide vit dans le semis : voir effect_spiral_stack_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_spiral_stacks.py - ne pas
// modifier a la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_spiral_stack_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectSpiralStackPattern, 7> kSpiralStacks = {{
    {435, "Effect_Build_Sprite67FromParam", "data\\texture\\effect\\storm2.tga", 0},
    {459, "Effect_Build_Sprite67FromParam", "data\\texture\\effect\\storm2.tga", 1},
    {460, "Effect_Build_Sprite67FromParam", "data\\texture\\effect\\storm2.tga", 6},
    {461, "Effect_Build_Sprite67FromParam", "data\\texture\\effect\\storm2.tga", 3},
    {464, "Effect_Build_Sprite67FromParam", "data\\texture\\effect\\storm2.tga", 11},
    {465, "Effect_Build_Sprite67FromParam", "data\\texture\\effect\\storm2.tga", 2},
    {796, "Effect_Build_Sprite67FromParam", "data\\texture\\effect\\storm2.tga", 10},
}};

}  // namespace

std::span<const EffectSpiralStackPattern> EffectSpiralStackPatternTable() { return kSpiralStacks; }

}  // namespace ro::render
