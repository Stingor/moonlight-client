// LES VOLEES DE NEIGE : ce que leur batisseur ecrit en dur.
//
// Un seul batisseur, sous-type 134.
//
// 2 volee(s), et leurs 4 images viennent du BATISSEUR et non du site
// d'appel : celui-ci ne pousse qu'un octet nul. L'ordre des images est celui
// du client, et la marque d'image les indexe — voir effect_fading_streak.h.
//
// ENGENDRE par scripts/gen_client_effect_fading_streaks.py - ne pas
// modifier a la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_fading_streak_seed.h"

namespace ro::render {
namespace {

constexpr std::array<std::string_view, 4> kImages = {{
    "data\\texture\\effect\\E9_snow.tga",
    "data\\texture\\effect\\E9_snow01.tga",
    "data\\texture\\effect\\E9_snow02.tga",
    "data\\texture\\effect\\E9_snow03.tga",
}};

constexpr std::array<EffectFadingStreakPattern, 2> kFadingStreaks = {{
    {791, "Effect_Build_SnowE9", kImages, 3},
    {928, "Effect_Build_SnowE9", kImages, 3},
}};

}  // namespace

std::span<const EffectFadingStreakPattern> EffectFadingStreakPatternTable() {
  return kFadingStreaks;
}

}  // namespace ro::render
