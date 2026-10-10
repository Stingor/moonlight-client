// LES BILLBOARDS DE PALETTE : ce que chaque site d'appel passe a son batisseur.
//
// Une ligne par site d'appel du batisseur.
//
// 8 eclats, toutes du sous-type 132. L'image vient du SITE et non du
// batisseur, qui n'en connait aucune ; l'octet est la PALETTE que la figure relit dans la
// fois, et ce qu'il decide vit dans le semis : voir
// effect_palette_billboard_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_palette_billboards.py - ne pas
// modifier a la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_palette_billboard_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectPaletteBillboardPattern, 8> kPaletteBillboards = {{
    {861, "Effect_InitSpiralSprite", "data\\texture\\effect\\freeze_a.bmp", 2, 100},
    {865, "Effect_InitSpiralSprite", "data\\texture\\effect\\freeze_a.bmp", 3, 100},
    {867, "Effect_InitSpiralSprite", "data\\texture\\effect\\freeze_a.bmp", 4, 100},
    {871, "Effect_InitSpiralSprite", "data\\texture\\effect\\freeze_a.bmp", 5, 100},
    {873, "Effect_InitSpiralSprite", "data\\texture\\effect\\freeze_a.bmp", 6, 100},
    {875, "Effect_InitSpiralSprite", "data\\texture\\effect\\freeze_a.bmp", 7, 100},
    {877, "Effect_InitSpiralSprite", "data\\texture\\effect\\freeze_a.bmp", 8, 100},
    {1122, "Effect_InitSpiralSpriteLooping", "data\\texture\\effect\\emp shock.tga", 0, 200},
}};

}  // namespace

std::span<const EffectPaletteBillboardPattern> EffectPaletteBillboardPatternTable() {
  return kPaletteBillboards;
}

}  // namespace ro::render
