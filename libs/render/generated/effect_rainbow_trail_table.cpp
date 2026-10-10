// LES ARCS-EN-CIEL : ce que chaque site d'appel passe a son batisseur.
//
// 1 arc(s) pour 1 effet(s), tous du sous-type 93. Le batisseur ne recoit
// qu'une image ; tout le reste est ecrit en dur et vit dans le semis et la
// figure, voir effect_rainbow_trail_seed.h et effect_rainbow_trail.h.
//
// ENGENDRE par scripts/gen_client_effect_rainbow_trails.py - ne pas
// modifier a la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_rainbow_trail_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectRainbowTrailPattern, 1> kRainbowTrails = {{
    {410, "Effect_Build_CameraAlignedSprite5d", "data\\texture\\effect\\alpha_center.tga"},
}};

}  // namespace

std::span<const EffectRainbowTrailPattern> EffectRainbowTrailPatternTable() {
  return kRainbowTrails;
}

}  // namespace ro::render
