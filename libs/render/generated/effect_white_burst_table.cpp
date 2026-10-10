// LES BOUFFÉES BLANCHES QU'UN EFFET SÈME, par identifiant d'effet.
//
// 4 patrons pour 4 effets, un chacun — et quatre styles pour quatre
// identifiants : aucun ne partage le sien. Chaque patron se sème
// plusieurs fois, en une seule volée à la naissance de l'effet.
//
// Textures employées : effect\white01.bmp, effect\white03.bmp.
//
// ⚠ LA VIE EST CALCULÉE, pas relevée : le bâtisseur n'en pose aucune.
// Voir le générateur.
//
// ENGENDRÉ par scripts/gen_client_effect_white_bursts.py — ne pas
// modifier à la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_white_burst_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectWhiteBurstPattern, 4> kWhiteBursts = {{
    {227, "Effect_Build_WhiteBurstParticles2b", "data\\texture\\effect\\white01.bmp", 3.5F, 0, 1,
     70, 1, 30, 146, 20},
    {798, "Effect_Build_WhiteBurstParticles2b", "data\\texture\\effect\\white01.bmp", 2.5F, 2, 1, 0,
     2, 26, 111, 16},
    {807, "Effect_Build_WhiteBurstParticles2b", "data\\texture\\effect\\white01.bmp", 4.0F, 3, 3,
     70, 1, 30, 160, 20},
    {810, "Effect_Build_WhiteBurstParticles2b", "data\\texture\\effect\\white03.bmp", 3.0F, 3, 3,
     30, 1, 20, 110, 10},
}};

}  // namespace

std::span<const EffectWhiteBurstPattern> EffectWhiteBurstPatternTable() { return kWhiteBursts; }

std::span<const EffectWhiteBurstPattern> EffectWhiteBurstPatternsFor(std::uint32_t effect_id) {
  const auto first =
      std::lower_bound(kWhiteBursts.begin(), kWhiteBursts.end(), effect_id,
                       [](const EffectWhiteBurstPattern& pattern, std::uint32_t target) {
                         return pattern.effect_id < target;
                       });
  const auto last =
      std::find_if(first, kWhiteBursts.end(), [effect_id](const EffectWhiteBurstPattern& pattern) {
        return pattern.effect_id != effect_id;
      });
  return {first, last};
}

}  // namespace ro::render
