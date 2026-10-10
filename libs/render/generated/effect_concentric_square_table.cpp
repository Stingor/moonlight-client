// LES CARRES CONCENTRIQUES : les noeuds du sous-type 32.
//
// 8 identifiants, UN seul batisseur, et tout decoule d'un OCTET.
// Le COMPTE de noeuds n'est pas ici : il vient d'un argument de creation,
// que render ne connait pas.
//
// ENGENDRE par scripts/gen_client_effect_concentric_squares.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_concentric_square_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectConcentricSquarePattern, 8> kConcentricSquares = {{
    {228, "Effect_Build_ElementalScatterParticles", "data\\texture\\effect\\thunder_center.bmp", 0},
    {504, "Effect_Build_ElementalScatterParticles", "data\\texture\\effect\\thunder_center.bmp", 1},
    {629, "Effect_Build_ElementalScatterParticles", "data\\texture\\effect\\thunder_center.bmp", 3},
    {735, "Effect_Build_ElementalScatterParticles", "data\\texture\\effect\\fire-1.bmp", 10},
    {736, "Effect_Build_ElementalScatterParticles", "data\\texture\\effect\\lightning-1.bmp", 11},
    {737, "Effect_Build_ElementalScatterParticles", "data\\texture\\effect\\water-1.bmp", 12},
    {738, "Effect_Build_ElementalScatterParticles", "data\\texture\\effect\\stone-1.bmp", 13},
    {1135, "Effect_Build_ElementalScatterParticles", "data\\texture\\effect\\rcoin1.bmp", 4},
}};

}  // namespace

std::span<const EffectConcentricSquarePattern> EffectConcentricSquarePatternTable() {
  return kConcentricSquares;
}

}  // namespace ro::render
