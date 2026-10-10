// GRAND CROSS : les deux noeuds de l'effet 226 et de son jumeau 450.
//
// Le cas du repartiteur appelle DEUX batisseurs avec le meme octet :
// celui du ruban a quatre bras (sous-type 34) et celui du cadre tournant
// (sous-type 37).
//
// L'octet choisit les TROIS valeurs de la rangee a la fois.
//
// ENGENDRE par scripts/gen_client_effect_grand_cross.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_grand_cross_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectGrandCrossPattern, 2> kGrandCrosses = {{
    {226, "data\\texture\\effect\\ring_white.tga", "data\\texture\\effect\\ring_yellow.tga",
     1.0F},  // EF_GRANDCROSS
    {450, "data\\texture\\effect\\ring_black.tga", "data\\texture\\effect\\ring_black.tga",
     12.0F},  // EF_GRANDCROSS2
}};

}  // namespace

std::span<const EffectGrandCrossPattern> EffectGrandCrossPatternTable() { return kGrandCrosses; }

}  // namespace ro::render
