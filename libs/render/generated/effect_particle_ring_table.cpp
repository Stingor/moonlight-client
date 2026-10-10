// CE QU'UN STYLE D'ANNEAU DE PARTICULES CHOISIT, et quel effet l'emploie.
//
// 3 bâtisseurs : chacun choisit, par une valeur de style, les
// constantes de ses pistes.
//
// ⚠ LES DEUX PREMIERS SONT LE MÊME CODE À TROIS CONSTANTES PRÈS, et le numéro de
// style ne veut donc PAS dire la même chose chez l'un et chez l'autre : un
// patron désigne sa graine par son RANG, jamais par son style.
//
// 7 styles pour 15 nœuds répartis sur 15 effets.
//
// ⚠ NI L'AZIMUT NI LE RAYON NE SONT ICI : ils se déduisent du RANG de la
// piste — quatre-vingt-dix fois le rang, et un plus le rang. Le générateur
// l'a vérifié pour chacune des 28 pistes.
//
// ENGENDRÉ par scripts/gen_client_effect_particle_rings.py — ne pas
// modifier à la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_particle_ring_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectParticleRingStyleSeed, 7> kParticleRingStyles = {{
    {"Effect_Init_ParticleRing4_Sized", 0, 0, 80, 25.0F, 0.0F},
    {"Effect_Init_ParticleRing4_Sized", 1, 0, 80, 25.0F, 0.0F},
    {"Effect_Init_ParticleRing4_Sized", 2, 0, 70, 15.0F, 0.0F},
    {"Effect_Init_ParticleRing4_Sized", 3, 17, 80, 25.0F, 0.0F},
    {"Effect_Init_ParticleRing4_Sized", 4, 0, 80, 25.0F, 0.0F},
    {"Effect_Init_ParticleRing4", 0, 0, 80, 25.0F, 99.0F},
    {"Effect_InitSpinRings4", 5, 0, 80, 25.0F, 99.0F},
}};

constexpr std::array<EffectParticleRingNodePattern, 15> kParticleRingNodes = {{
    {223, "Effect_Init_ParticleRing4_Sized", "data\\texture\\effect\\ring_white.tga", 2, 2},
    {225, "Effect_Init_ParticleRing4_Sized", "data\\texture\\effect\\ring_red.tga", 0, 0},
    {236, "Effect_Init_ParticleRing4_Sized", "data\\texture\\effect\\ring_blue.tga", 0, 0},
    {237, "Effect_Init_ParticleRing4_Sized", "data\\texture\\effect\\magic_green.tga", 0, 0},
    {238, "Effect_Init_ParticleRing4_Sized", "data\\texture\\effect\\ring_white.tga", 0, 0},
    {261, "Effect_Init_ParticleRing4", "data\\texture\\effect\\ring_yellow.tga", 0, 5},
    {263, "Effect_Init_ParticleRing4_Sized", "data\\texture\\effect\\ring_yellow.tga", 1, 1},
    {773, "Effect_Init_ParticleRing4_Sized", "data\\texture\\effect\\ring_purple.tga", 1, 1},
    {886, "Effect_Init_ParticleRing4_Sized", "data\\texture\\effect\\ring_blue.tga", 3, 3},
    {934, "Effect_Init_ParticleRing4_Sized", "data\\texture\\effect\\ring_white.tga", 4, 4},
    {936, "Effect_Init_ParticleRing4_Sized", "data\\texture\\effect\\ring_red.tga", 4, 4},
    {937, "Effect_Init_ParticleRing4_Sized", "data\\texture\\effect\\ring_yellow.tga", 4, 4},
    {938, "Effect_Init_ParticleRing4_Sized", "data\\texture\\effect\\ring_brown.tga", 4, 4},
    {2338, "Effect_InitSpinRings4",
     "data\\texture\\effect\\the_one_fighter_rises\\the_one_fighter_rises\\the_ring_00.tga", 5, 6},
    {2373, "Effect_Init_ParticleRing4_Sized", "data\\texture\\effect\\ring_black.tga", 4, 4},
}};

}  // namespace

std::span<const EffectParticleRingStyleSeed> EffectParticleRingStyleSeedTable() {
  return kParticleRingStyles;
}

std::span<const EffectParticleRingNodePattern> EffectParticleRingNodePatternTable() {
  return kParticleRingNodes;
}

std::span<const EffectParticleRingNodePattern> EffectParticleRingNodePatternsFor(
    std::uint32_t effect_id) {
  const auto [first, last] = std::ranges::equal_range(kParticleRingNodes, effect_id, {},
                                                      &EffectParticleRingNodePattern::effect_id);
  return {first, last};
}

}  // namespace ro::render
