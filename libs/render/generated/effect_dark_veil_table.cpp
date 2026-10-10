// LES VOILES DU CLIENT : ce que chaque effet passe a son batisseur.
//
// 11 voiles, tous du sous-type 71, tous dessines par la meme figure et
// separes par UN SEUL octet. Ce qu'il decide vit dans le semis, pas ici :
// voir effect_dark_veil_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_dark_veils.py - ne pas
// modifier a la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_dark_veil_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectDarkVeilPattern, 11> kDarkVeils = {{
    {334,
     "Effect_Build_FullbGlow",
     {{"data\\texture\\effect\\fullb.tga", "data\\texture\\effect\\white02.bmp"}},
     0},
    {475,
     "Effect_Build_FullbGlow",
     {{"data\\texture\\effect\\fullb.tga", "data\\texture\\effect\\white02.bmp"}},
     1},
    {476,
     "Effect_Build_FullbGlow",
     {{"data\\texture\\effect\\fullb.tga", "data\\texture\\effect\\white02.bmp"}},
     2},
    {477,
     "Effect_Build_FullbGlow",
     {{"data\\texture\\effect\\fullb.tga", "data\\texture\\effect\\white02.bmp"}},
     3},
    {478,
     "Effect_Build_FullbGlow",
     {{"data\\texture\\effect\\fullb.tga", "data\\texture\\effect\\white02.bmp"}},
     4},
    {479,
     "Effect_Build_FullbGlow",
     {{"data\\texture\\effect\\fullb.tga", "data\\texture\\effect\\white02.bmp"}},
     5},
    {480,
     "Effect_Build_FullbGlow",
     {{"data\\texture\\effect\\fullb.tga", "data\\texture\\effect\\white02.bmp"}},
     6},
    {481,
     "Effect_Build_FullbGlow",
     {{"data\\texture\\effect\\fullb.tga", "data\\texture\\effect\\white02.bmp"}},
     7},
    {482,
     "Effect_Build_FullbGlow",
     {{"data\\texture\\effect\\fullb.tga", "data\\texture\\effect\\white02.bmp"}},
     8},
    {483,
     "Effect_Build_FullbGlow",
     {{"data\\texture\\effect\\fullb.tga", "data\\texture\\effect\\white02.bmp"}},
     9},
    {484,
     "Effect_Build_FullbGlow",
     {{"data\\texture\\effect\\fullb.tga", "data\\texture\\effect\\white02.bmp"}},
     10},
}};

}  // namespace

std::span<const EffectDarkVeilPattern> EffectDarkVeilPatternTable() { return kDarkVeils; }

}  // namespace ro::render
