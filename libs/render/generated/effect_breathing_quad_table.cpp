// LES CARRES QUI RESPIRENT : ce que chaque site d'appel passe a son batisseur.
//
// Une ligne par site d'appel du batisseur.
//
// 9 carres, toutes du sous-type 39. L'image vient du SITE et non du
// batisseur, qui n'en connait aucune ; l'octet choisit QUATRE valeurs d'un coup - rayon, azimut,
// alpha, style : fois, et ce qu'il decide vit dans le semis : voir effect_breathing_quad_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_breathing_quads.py - ne pas
// modifier a la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_breathing_quad_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectBreathingQuadPattern, 9> kBreathingQuads = {{
    {242, "Effect_Build_ScaledSpriteVariant", "data\\texture\\effect\\aaa copy.bmp", 1},
    {404, "Effect_Build_ScaledSpriteVariant", "data\\texture\\effect\\spiderweb.tga", 2},
    {671, "Effect_Build_ScaledSpriteVariant", "data\\texture\\effect\\hanmoon1.tga", 3},
    {672, "Effect_Build_ScaledSpriteVariant", "data\\texture\\effect\\hanmoon2.tga", 3},
    {822, "Effect_Build_ScaledSpriteVariant", "data\\texture\\effect\\twirl.bmp", 4},
    {825, "Effect_Build_ScaledSpriteVariant", "data\\texture\\effect\\구체15.bmp", 4},
    {828, "Effect_Build_ScaledSpriteVariant",
     "data\\texture\\effect\\new_maelstrom\\new_maelstrom\\mae_flare_00.bmp", 10},
    {828, "Effect_Build_ScaledSpriteVariant",
     "data\\texture\\effect\\new_maelstrom\\new_maelstrom\\mae_ring_00.tga", 11},
    {828, "Effect_Build_ScaledSpriteVariant",
     "data\\texture\\effect\\new_maelstrom\\new_maelstrom\\mae_ring_01.tga", 12},
}};

}  // namespace

std::span<const EffectBreathingQuadPattern> EffectBreathingQuadPatternTable() {
  return kBreathingQuads;
}

}  // namespace ro::render
