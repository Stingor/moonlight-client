// LES SPRITES QUI S'ELEVENT : les noeuds du sous-type 85.
//
// 2 identifiants, 6 appels : le site choisit le sprite, et les ailes se
// resement tous les trois tics jusqu'au dix-huitieme.
//
// ENGENDRE par scripts/gen_client_effect_fading_sprites.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_fading_sprite_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectFadingSpritePattern, 6> kFadingSprites = {{
    {338,
     "Effect_BuildUpdate_ActorSpriteRing55",
     "data\\sprite\\이팩트\\천사",
     0,
     {0, 0, 1}},  // EF_ANGEL2
    {338,
     "Effect_BuildUpdate_ActorSpriteRing55",
     "data\\sprite\\이팩트\\천사날개",
     1,
     {0, 18, 3}},  // EF_ANGEL2
    {338,
     "Effect_BuildUpdate_ActorSpriteRing55",
     "data\\sprite\\이팩트\\깃털1",
     2,
     {0, 0, 1}},  // EF_ANGEL2
    {338,
     "Effect_BuildUpdate_ActorSpriteRing55",
     "data\\sprite\\이팩트\\깃털2",
     3,
     {0, 0, 1}},  // EF_ANGEL2
    {582,
     "Effect_BuildUpdate_ActorSpriteRing55",
     "data\\sprite\\이팩트\\한복천사(날개)",
     11,
     {0, 18, 3}},  // EF_ANGEL3
    {582,
     "Effect_BuildUpdate_ActorSpriteRing55",
     "data\\sprite\\이팩트\\한복천사",
     10,
     {0, 0, 1}},  // EF_ANGEL3
}};

}  // namespace

std::span<const EffectFadingSpritePattern> EffectFadingSpritePatternTable() {
  return kFadingSprites;
}

}  // namespace ro::render
