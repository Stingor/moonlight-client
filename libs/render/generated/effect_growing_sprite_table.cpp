// LES SPRITES QUI ENFLENT : ce que chaque effet passe a son batisseur.
//
// 14 sprites pour 9 effets, tous du sous-type 88. Le RETARD est negatif :
// la piste ne bouge pas tant que son compteur n'est pas remonte a un, et
// c'est ce qui fait paraitre six boules de foudre l'une apres l'autre.
// Ce que le style decide vit dans le semis : voir
// effect_growing_sprite_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_growing_sprites.py - ne pas
// modifier a la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_growing_sprite_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectGrowingSpritePattern, 14> kGrowingSprites = {{
    {365, "Effect_Build_SpriteFromParam58", "data\\texture\\effect\\explosive_1_128.bmp", -28, 0},
    {497, "Effect_Build_SpriteFromParam58", "data\\texture\\effect\\bbbb.bmp", -16, 1},
    {498, "Effect_Build_SpriteFromParam58", "data\\texture\\effect\\bbbb.bmp", -16, 2},
    {499, "Effect_Build_SpriteFromParam58", "data\\texture\\effect\\bbbb.bmp", -16, 0},
    {799, "Effect_Build_SpriteFromParam58", "data\\texture\\effect\\thunder_ball_0001.bmp", -10, 3},
    {799, "Effect_Build_SpriteFromParam58", "data\\texture\\effect\\thunder_ball_0002.bmp", -14, 3},
    {799, "Effect_Build_SpriteFromParam58", "data\\texture\\effect\\thunder_ball_0003.bmp", -18, 3},
    {799, "Effect_Build_SpriteFromParam58", "data\\texture\\effect\\thunder_ball_0004.bmp", -22, 3},
    {799, "Effect_Build_SpriteFromParam58", "data\\texture\\effect\\thunder_ball_0005.bmp", -26, 3},
    {799, "Effect_Build_SpriteFromParam58", "data\\texture\\effect\\thunder_ball_0006.bmp", -30, 3},
    {894, "Effect_Build_SpriteFromParam58", "data\\texture\\effect\\explosive_1_128.bmp", 0, 4},
    {905, "Effect_Build_SpriteFromParam58", "data\\texture\\effect\\explosive_1_128.bmp", 0, 10},
    {906, "Effect_Build_SpriteFromParam58", "data\\texture\\effect\\explosive_1_128.bmp", -28, 0},
    {930, "Effect_Build_SpriteFromParam58", "data\\texture\\effect\\explosive_1_128.bmp", -28, 0},
}};

}  // namespace

std::span<const EffectGrowingSpritePattern> EffectGrowingSpritePatternTable() {
  return kGrowingSprites;
}

}  // namespace ro::render
