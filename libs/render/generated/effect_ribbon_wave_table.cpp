// LES ONDES DE RUBAN QU'UN EFFET SÈME, par identifiant d'effet.
//
// 4 nœuds pour 4 effets, un chacun. Le paramètre ne choisit que la
// teinte du ruban : les teintes employées sont 0, 14.
//
// ⚠ LA VIE N'EST POSÉE NULLE PART, ni par le bâtisseur ni par le
// mouvement : le nœud garde celle du constructeur du client.
//
// ENGENDRÉ par scripts/gen_client_effect_ribbon_waves.py — ne pas
// modifier à la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_ribbon_wave_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectRibbonWavePattern, 4> kRibbonWaves = {{
    {239, "Effect_Build_SpinRingNode36", "data\\texture\\effect\\ring_red.tga", 0},
    {240, "Effect_Build_SpinRingNode36", "data\\texture\\effect\\ring_blue.tga", 0},
    {241, "Effect_Build_SpinRingNode36", "data\\texture\\effect\\magic_green.tga", 0},
    {620, "Effect_Build_SpinRingNode36", "data\\texture\\effect\\ring_blue.tga", 14},
}};

}  // namespace

std::span<const EffectRibbonWavePattern> EffectRibbonWavePatternTable() { return kRibbonWaves; }

std::span<const EffectRibbonWavePattern> EffectRibbonWavePatternsFor(std::uint32_t effect_id) {
  const auto [first, last] =
      std::ranges::equal_range(kRibbonWaves, effect_id, {}, &EffectRibbonWavePattern::effect_id);
  return {first, last};
}

}  // namespace ro::render
