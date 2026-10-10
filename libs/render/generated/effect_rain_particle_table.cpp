// LES PLUIES DE GRAINS : ce que chaque site d'appel passe a son batisseur.
//
// 2 pluies pour 2 effets, toutes du sous-type 157. Le batisseur ne recoit
// qu'une image ; tout le reste est tire au sort et vit dans le semis, voir
// effect_rain_particle_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_rain_particles.py - ne pas
// modifier a la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_rain_particle_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectRainParticlePattern, 2> kRainParticles = {{
    {857, "Effect_InitRisingParticlesEmitter", "data\\texture\\effect\\번개1.bmp"},
    {944, "Effect_InitRisingParticlesEmitter", "data\\texture\\effect\\stone_part.bmp"},
}};

}  // namespace

std::span<const EffectRainParticlePattern> EffectRainParticlePatternTable() {
  return kRainParticles;
}

}  // namespace ro::render
