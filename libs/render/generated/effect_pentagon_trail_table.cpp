// LES TRAINEES PENTAGONALES : ce que chaque site d'appel passe a son batisseur.
//
// 7 trainees, toutes du sous-type 66. L'image vient du SITE et non du
// batisseur, qui n'en connait aucune ; l'octet decide de quatre choses a la
// fois, et ce qu'il decide vit dans le semis : voir
// effect_pentagon_trail_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_pentagon_trails.py - ne pas
// modifier a la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_pentagon_trail_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectPentagonTrailPattern, 7> kPentagonTrails = {{
    {307, "Effect_Build_StackedLayers4FromParam", "data\\texture\\effect\\cloud11.tga", 0},
    {322, "Effect_Build_StackedLayers4FromParam", "data\\texture\\effect\\cloud11.tga", 1},
    {323, "Effect_Build_StackedLayers4FromParam", "data\\texture\\effect\\cloud11.tga", 2},
    {324, "Effect_Build_StackedLayers4FromParam", "data\\texture\\effect\\cloud11.tga", 10},
    {581, "Effect_Build_StackedLayers4FromParam", "data\\texture\\effect\\cloud11.tga", 11},
    {719, "Effect_Build_StackedLayers4FromParam", "data\\texture\\effect\\cloud11.tga", 12},
    {825, "Effect_Build_StackedLayers4FromParam", "data\\texture\\effect\\cloud11.tga", 13},
}};

}  // namespace

std::span<const EffectPentagonTrailPattern> EffectPentagonTrailPatternTable() {
  return kPentagonTrails;
}

}  // namespace ro::render
