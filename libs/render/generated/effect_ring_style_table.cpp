// CE QU'UN STYLE D'ANNEAU CHOISIT, par numéro de style.
//
// Un seul bâtisseur sème les quatre pistes
// du sous-type 30 pour ses 30 identifiants.
//
// 21 styles portent une branche ; les 20 que les identifiants emploient
// sont 0, 1, 3, 5, 6, 15, 20, 21, 22, 23, 31, 42, 43, 44, 45, 46, 47, 48, 49, 50.
//
// ⚠ AUCUNE CLÉ DE DÉCALAGE N'EST UNE HAUTEUR : le dessinateur les
// compare à deux flottants exacts, et aucune des douze n'en est un.
// L'anneau du sous-type 30 ne se soulève donc jamais.
//
// ENGENDRÉ par scripts/gen_client_effect_ring_styles.py — ne pas
// modifier à la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_track_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectRingStyle, 21> kRingStyles = {{
    {1, 8, {20.0F, 19.0F, 18.0F, 17.0F}},   {2, 5, {25.0F, 24.0F, 23.0F, 22.0F}},
    {3, 10, {20.0F, 19.0F, 18.0F, 17.0F}},  {4, 11, {20.0F, 19.0F, 18.0F, 17.0F}},
    {5, 4, {20.0F, 19.0F, 18.0F, 17.0F}},   {6, 12, {20.0F, 19.0F, 18.0F, 17.0F}},
    {15, 7, {20.0F, 19.0F, 18.0F, 17.0F}},  {20, 24, {20.0F, 19.0F, 18.0F, 17.0F}},
    {21, 25, {20.0F, 19.0F, 18.0F, 17.0F}}, {22, 26, {15.0F, 14.0F, 13.0F, 12.0F}},
    {23, 27, {20.0F, 19.0F, 18.0F, 17.0F}}, {31, 31, {20.0F, 19.0F, 18.0F, 17.0F}},
    {42, 42, {20.0F, 19.0F, 18.0F, 17.0F}}, {43, 43, {20.0F, 19.0F, 18.0F, 17.0F}},
    {44, 44, {20.0F, 19.0F, 18.0F, 17.0F}}, {45, 45, {20.0F, 19.0F, 18.0F, 17.0F}},
    {46, 46, {20.0F, 19.0F, 18.0F, 17.0F}}, {47, 47, {20.0F, 19.0F, 18.0F, 17.0F}},
    {48, 48, {20.0F, 19.0F, 18.0F, 17.0F}}, {49, 49, {20.0F, 19.0F, 18.0F, 17.0F}},
    {50, 50, {20.0F, 19.0F, 18.0F, 17.0F}},
}};

// Aucune branche du bâtisseur ne nomme ce style : il mène à son défaut,
// et la table ne le cherche jamais.
constexpr std::uint8_t kStyleWithoutBranch = 255;
constexpr EffectRingStyle kDefaultRingStyle{kStyleWithoutBranch, 5, {20.0F, 19.0F, 18.0F, 17.0F}};

}  // namespace

EffectRingStyle DefaultEffectRingStyle() { return kDefaultRingStyle; }

std::span<const EffectRingStyle> EffectRingStyleTable() { return kRingStyles; }

EffectRingStyle EffectRingStyleFor(std::uint8_t style) {
  const auto found = std::ranges::lower_bound(kRingStyles, style, {}, &EffectRingStyle::style);
  if (found == kRingStyles.end() || found->style != style) {
    return kDefaultRingStyle;
  }
  return *found;
}

}  // namespace ro::render
