// LA ROTATION PROPRE D'UN SPRITE DE NŒUD, en degrés par tic : l'angle du quad
// avance de ce pas à chaque tic, et le dessinateur l'ajoute à celui de chaque
// calque. Les poisons, la désintoxication et le venin tournent de trois degrés
// par tic ; la fumée du dragon, comme celle d'une cheminée, de trois quarts.

#include <array>
#include <cstdint>
#include <span>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_node_sprite_seed.h"

using Catch::Approx;
using ro::render::EffectNodePattern;

namespace {

constexpr std::uint32_t kPoisonAttackEffect = 20;
constexpr std::uint32_t kDetoxicationEffect = 21;
constexpr std::uint32_t kSmokeEffect = 44;
constexpr std::uint32_t kEnchantPoisonEffect = 125;
constexpr std::uint32_t kVenomDust2Effect = 171;
constexpr std::uint32_t kDragonSmokeEffect = 373;
constexpr std::uint32_t kDeadlyPoisonEffect = 493;
constexpr std::uint32_t kPoisonSmokeEffect = 772;
constexpr std::uint32_t kPoisonSmoke2Effect = 924;

constexpr std::array kPoisonEffects = {
    kPoisonAttackEffect, kDetoxicationEffect, kEnchantPoisonEffect, kVenomDust2Effect,
    kDeadlyPoisonEffect, kPoisonSmokeEffect,  kPoisonSmoke2Effect,
};

constexpr float kPoisonSpinDegreesPerTick = 3.0F;
constexpr float kSmokeSpinDegreesPerTick = 0.75F;

// Combien de tics le nœud semé avance avant qu'on lise son angle.
constexpr int kAdvancedTicks = 3;

std::span<const EffectNodePattern> SpritePatterns(std::uint32_t effect_id) {
  const std::span<const EffectNodePattern> patterns = ro::render::EffectNodePatternsFor(effect_id);
  REQUIRE_FALSE(patterns.empty());
  for (const EffectNodePattern& pattern : patterns) {
    REQUIRE(pattern.shape == ro::render::EffectNodeShape::kSprite);
  }
  return patterns;
}

}  // namespace

TEST_CASE("les poisons et la désintoxication tournent de trois degrés par tic",
          "[render][effect_spin]") {
  for (const std::uint32_t each_effect_id : kPoisonEffects) {
    INFO("effet " << each_effect_id);
    for (const EffectNodePattern& pattern : SpritePatterns(each_effect_id)) {
      CHECK(pattern.sprite_spin_per_tick == Approx(kPoisonSpinDegreesPerTick));
    }
  }
}

TEST_CASE("la fumée du dragon tourne comme celle d'une cheminée", "[render][effect_spin]") {
  for (const std::uint32_t each_effect_id : {kDragonSmokeEffect, kSmokeEffect}) {
    INFO("effet " << each_effect_id);
    for (const EffectNodePattern& pattern : SpritePatterns(each_effect_id)) {
      CHECK(pattern.sprite_spin_per_tick == Approx(kSmokeSpinDegreesPerTick));
    }
  }
}

TEST_CASE("la rotation propre d'un poison atteint le nœud semé, depuis un angle nul",
          "[render][effect_spin]") {
  for (const std::uint32_t each_effect_id : kPoisonEffects) {
    for (const EffectNodePattern& pattern : SpritePatterns(each_effect_id)) {
      INFO("effet " << each_effect_id << ", période " << pattern.period_ticks);
      const ro::render::EffectNodeSeed seed =
          ro::render::EffectNodeSeedFromPattern(pattern, pattern.life_ticks, 0, 0);
      CHECK(seed.spin_speed_per_tick == Approx(kPoisonSpinDegreesPerTick));
      std::uint32_t generator = 1;
      ro::render::EffectNode node = ro::render::SowEffectNode(seed, generator);
      CHECK(node.draws_rotated);
      // L'ANGLE DE DÉPART EST NUL : aucun de ces nœuds ne naît tourné.
      CHECK(node.spin_degrees == Approx(0.0F));
      for (int tick = 0; tick < kAdvancedTicks; ++tick) {
        REQUIRE(ro::render::AdvanceEffectNode(node));
      }
      CHECK(node.spin_degrees ==
            Approx(static_cast<float>(kAdvancedTicks) * kPoisonSpinDegreesPerTick));
    }
  }
}
