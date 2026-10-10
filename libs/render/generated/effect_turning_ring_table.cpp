// LES ANNEAUX QUI TOURNENT, par identifiant d'effet.
//
// Un seul bâtisseur, appelé une fois par effet.
//
// 4 nœuds pour 4 effets, un chacun. Le paramètre ne choisit que la
// teinte : les teintes employées sont 4, 8.
//
// ⚠ LA VIE N'EST POSÉE NULLE PART, ni par le bâtisseur ni par le
// mouvement : le nœud garde celle du constructeur du client.
//
// ENGENDRÉ par scripts/gen_client_effect_turning_rings.py — ne pas
// modifier à la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_turning_ring_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectTurningRingPattern, 4> kTurningRings = {{
    {280, "Effect_Build_SpinSpriteNode50", "data\\texture\\effect\\twirl.bmp", 8},
    {285, "Effect_Build_SpinSpriteNode50", "data\\texture\\effect\\twirl.bmp", 4},
    {783, "Effect_Build_SpinSpriteNode50", "data\\texture\\effect\\slash01.tga", 8},
    {784, "Effect_Build_SpinSpriteNode50", "data\\texture\\effect\\썬더스톰파티클.tga", 8},
}};

}  // namespace

std::span<const EffectTurningRingPattern> EffectTurningRingPatternTable() { return kTurningRings; }

std::span<const EffectTurningRingPattern> EffectTurningRingPatternsFor(std::uint32_t effect_id) {
  const auto [first, last] =
      std::ranges::equal_range(kTurningRings, effect_id, {}, &EffectTurningRingPattern::effect_id);
  return {first, last};
}

}  // namespace ro::render
