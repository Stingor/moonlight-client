// LA TEINTE D'UN RUBAN, par variante.
//
// La primitive de ruban ne reçoit aucune couleur : elle la choisit sur un
// champ du nœud, la variante. Une variante absente de la table dessine
// en blanc.
//
// 44 variantes posent autre chose que du blanc, sur 101 cas.
//
// ENGENDRÉ par scripts/gen_client_effect_trail_palette.py — ne pas
// modifier à la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_trail_palette.h"

namespace ro::render {
namespace {

constexpr std::array<EffectTrailVariant, 44> kTrailVariants = {{
    {0, {255, 255, 255}, 0, 6},  {1, {255, 175, 175}, 0, 2},   {2, {195, 195, 255}, 0, 6},
    {3, {255, 255, 255}, 80, 6}, {4, {100, 100, 255}, 0, 2},   {6, {255, 100, 100}, 0, 2},
    {7, {100, 255, 100}, 0, 2},  {8, {255, 255, 170}, 0, 2},   {9, {140, 210, 140}, 0, 2},
    {10, {255, 89, 182}, 0, 2},  {11, {170, 170, 255}, 0, 2},  {12, {50, 50, 50}, 0, 6},
    {13, {255, 50, 255}, 0, 6},  {14, {120, 120, 255}, 0, 6},  {15, {10, 255, 10}, 0, 2},
    {16, {20, 255, 20}, 0, 2},   {17, {0, 0, 255}, 0, 2},      {18, {255, 0, 0}, 0, 2},
    {19, {0, 255, 0}, 0, 2},     {20, {255, 255, 0}, 0, 2},    {21, {255, 0, 255}, 0, 2},
    {22, {0, 255, 255}, 0, 2},   {23, {41, 94, 188}, 0, 6},    {24, {10, 66, 114}, 0, 2},
    {25, {35, 146, 185}, 0, 2},  {26, {164, 205, 254}, 0, 2},  {27, {84, 169, 205}, 0, 2},
    {28, {108, 26, 255}, 0, 2},  {29, {177, 255, 58}, 0, 2},   {30, {85, 30, 185}, 0, 2},
    {31, {244, 187, 124}, 0, 2}, {32, {0, 0, 0}, 0, 6},        {40, {70, 70, 255}, 0, 2},
    {41, {30, 30, 255}, 0, 2},   {42, {43, 40, 46}, 0, 6},     {43, {125, 182, 160}, 0, 6},
    {44, {180, 55, 30}, 0, 6},   {45, {65, 141, 231}, 0, 2},   {46, {164, 47, 29}, 0, 2},
    {47, {172, 154, 56}, 0, 2},  {48, {80, 20, 20}, 0, 2},     {49, {200, 130, 84}, 0, 2},
    {50, {255, 198, 91}, 0, 2},  {100, {250, 250, 255}, 0, 6},
}};

constexpr EffectTrailVariant kNeutral{0, {255, 255, 255}, 0, 2};

}  // namespace

EffectTrailVariant NeutralEffectTrailVariant() { return kNeutral; }

std::span<const EffectTrailVariant> EffectTrailVariantTable() { return kTrailVariants; }

EffectTrailVariant EffectTrailVariantFor(std::uint8_t variant) {
  const auto found =
      std::ranges::lower_bound(kTrailVariants, variant, {}, &EffectTrailVariant::variant);
  if (found == kTrailVariants.end() || found->variant != variant) {
    return kNeutral;
  }
  return *found;
}

}  // namespace ro::render
