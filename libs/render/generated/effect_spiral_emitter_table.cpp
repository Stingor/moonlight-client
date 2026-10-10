// L'EMETTEUR EN SPIRALE : qui le pose.
//
// Le sous-type 151, qui lit de son cas une image et un octet.
//
// 3 appel(s). Colonnes : identifiant, effet que le noeud fait naitre,
// fonction, image, octet. Les appels
// d'un meme identifiant gardent leur ordre. Tout le reste est ecrit en dur
// par le batisseur : voir effect_spiral_emitter_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_spiral_emitters.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_spiral_emitter_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectSpiralEmitterPattern, 3> kSpiralEmitters = {{
    {849, 850, "Effect_InitSpinningSprite97", "data\\texture\\effect\\whitelight.tga", 0},
    {931, 932, "Effect_InitSpinningSprite97", "data\\texture\\effect\\whitelight.tga", 2},
    {931, 932, "Effect_InitSpinningSprite97", "data\\texture\\effect\\whitelight.tga", 1},
}};

}  // namespace

std::span<const EffectSpiralEmitterPattern> EffectSpiralEmitterPatternTable() {
  return kSpiralEmitters;
}

}  // namespace ro::render
