// LES CADRES ET LES BARRES : les noeuds du sous-type 90.
//
// 5 identifiants pour 10 noeuds : chaque effet appelle son UNIQUE
// batisseur deux fois de suite, a deux octets consecutifs. L'octet porte
// sept nombres a lui seul ; l'image est la seule autre chose que le site
// donne.
//
// ENGENDRE par scripts/gen_client_effect_frame_and_bars.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_frame_and_bar_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectFrameAndBarPattern, 10> kFrameAndBars = {{
    {374, "Effect_Build_TieredFanNode", "data\\texture\\effect\\alpha_down.tga",
     0},  // EF_BOTTOM_BASILICA, haut, vert, cadre 12,5 barre 13,3
    {374, "Effect_Build_TieredFanNode", "data\\texture\\effect\\alpha_down.tga",
     1},  // EF_BOTTOM_BASILICA, bas, vert, cadre 14,0 barre 14,8
    {715, "Effect_Build_TieredFanNode", "data\\texture\\effect\\ring_blue.tga",
     10},  // EF_BOTTOM_BLUE, haut, bleu, duree = vie - 90
    {715, "Effect_Build_TieredFanNode", "data\\texture\\effect\\ring_blue.tga",
     11},  // EF_BOTTOM_BLUE, bas, bleu, duree = vie - 90, barres decalees
    {716, "Effect_Build_TieredFanNode", "data\\texture\\effect\\ring_blue.tga",
     10},  // EF_BOTTOM_BLUE2, haut, bleu, duree = vie - 90
    {716, "Effect_Build_TieredFanNode", "data\\texture\\effect\\ring_blue.tga",
     11},  // EF_BOTTOM_BLUE2, bas, bleu, duree = vie - 90, barres decalees
    {802, "Effect_Build_TieredFanNode", "data\\texture\\effect\\alpha_down.tga",
     20},  // EF_BOTTOM_BASILICA2, haut, bleu, s'arrete si le porteur bouge
    {802, "Effect_Build_TieredFanNode", "data\\texture\\effect\\alpha_down.tga",
     21},  // EF_BOTTOM_BASILICA2, bas, bleu, s'arrete si le porteur bouge
    {2401, "Effect_Build_TieredFanNode", "data\\texture\\effect\\alpha_down.tga",
     0},  // sans constante, haut, vert, cadre 12,5 barre 13,3
    {2401, "Effect_Build_TieredFanNode", "data\\texture\\effect\\alpha_down.tga",
     1},  // sans constante, bas, vert, cadre 14,0 barre 14,8
}};

}  // namespace

std::span<const EffectFrameAndBarPattern> EffectFrameAndBarPatternTable() { return kFrameAndBars; }

}  // namespace ro::render
