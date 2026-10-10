// LES RUBANS TYPES DU CLIENT : ce que chaque site d'appel passe a son batisseur.
//
// 10 rubans, tous du sous-type 149, separes par UN SEUL octet - le type.
// Le jeu d'images se deduit de ce type : voir effect_typed_ribbon_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_typed_ribbons.py - ne pas
// modifier a la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_typed_ribbon_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectTypedRibbonPattern, 10> kTypedRibbons = {{
    {831, "Effect_InitSpellBubblesOrFire", "data\\texture\\effect\\W_bubble%02d.tga", 27, 0},
    {840, "Effect_InitSpellBubblesOrFire", "data\\texture\\effect\\W_bubble%02d.tga", 27, 3},
    {841, "Effect_InitSpellBubblesOrFire", "data\\texture\\effect\\W_bubble%02d.tga", 27, 4},
    {842, "Effect_InitSpellBubblesOrFire", "data\\texture\\effect\\W_bubble%02d.tga", 27, 5},
    {843, "Effect_InitSpellBubblesOrFire", "data\\texture\\effect\\W_bubble%02d.tga", 27, 6},
    {844, "Effect_InitSpellBubblesOrFire", "data\\texture\\effect\\W_bubble%02d.tga", 27, 7},
    {845, "Effect_InitSpellBubblesOrFire", "data\\texture\\effect\\W_bubble%02d.tga", 27, 8},
    {903, "Effect_InitSpellBubblesOrFire", "data\\texture\\effect\\YBfire_%02d.bmp", 4, 10},
    {1656, "Effect_InitSpellBubblesOrFire", "data\\texture\\effect\\W_bubble%02d.tga", 27, 9},
    {2368, "Effect_InitSpellBubblesOrFire", "data\\texture\\effect\\W_bubble%02d.tga", 27, 11},
}};

}  // namespace

std::span<const EffectTypedRibbonPattern> EffectTypedRibbonPatternTable() { return kTypedRibbons; }

}  // namespace ro::render
