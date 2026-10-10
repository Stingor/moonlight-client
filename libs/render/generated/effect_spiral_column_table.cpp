// LES COLONNES QUI TOURNENT : les noeuds du sous-type 137.
//
// 8 identifiants, DEUX batisseurs qui ne different que par trois
// flottants : le second ecrit le vecteur vers la CIBLE la ou le premier
// met zero.
//
// La VARIANTE de teinte n'est pas ici : elle decoule de l'octet, et pour
// deux styles elle se TIRE AU SORT. Voir EffectSpiralColumnVariantFor.
//
// ENGENDRE par scripts/gen_client_effect_spiral_columns.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_spiral_column_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectSpiralColumnPattern, 8> kSpiralColumns = {{
    {718, "Effect_InitCircularBurst", "data\\texture\\effect\\ring_white.tga", 11, false, 3},
    {800, "Effect_InitCircularBurst", "data\\texture\\effect\\ring_red.tga", 3, false, 3},
    {804, "Effect_InitCircularBurstDirected", "data\\texture\\effect\\ring_red.tga", 10, true, 7},
    {899, "Effect_InitCircularBurst", "data\\texture\\effect\\ring_blue.tga", 12, false, 6},
    {970, "Effect_InitCircularBurstDirected", "data\\texture\\effect\\ring_red.tga", 10, true, 7},
    {971, "Effect_InitCircularBurstDirected", "data\\texture\\effect\\ring_red.tga", 10, true, 7},
    {972, "Effect_InitCircularBurstDirected", "data\\texture\\effect\\ring_red.tga", 10, true, 7},
    {973, "Effect_InitCircularBurstDirected", "data\\texture\\effect\\ring_red.tga", 10, true, 7},
}};

}  // namespace

std::span<const EffectSpiralColumnPattern> EffectSpiralColumnPatternTable() {
  return kSpiralColumns;
}

}  // namespace ro::render
