// LES POISONS ET LA DÉSINTOXICATION : une seule branche de bâtisseur par
// effet, un cercle de naissance, une montée qui freine, et un alpha qui paraît
// en dix tics. Voir `scripts/lectures_a_la_main_des_patrons.py`.

#include <array>
#include <cstdint>
#include <span>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_node_sprite_seed.h"
#include "ro/render/effect_render_word.h"

using Catch::Approx;
using ro::render::EffectNodePattern;

namespace {

constexpr std::uint32_t kPoisonAttackEffect = 20;
constexpr std::uint32_t kDetoxicationEffect = 21;
constexpr std::uint32_t kEnchantPoisonEffect = 125;
constexpr std::uint32_t kDeadlyPoisonEffect = 493;
constexpr std::uint32_t kPoisonSmokeEffect = 772;
constexpr std::uint32_t kPoisonSmoke2Effect = 924;

constexpr std::array kPoisonEffects = {
    kPoisonAttackEffect, kDetoxicationEffect, kEnchantPoisonEffect,
    kDeadlyPoisonEffect, kPoisonSmokeEffect,  kPoisonSmoke2Effect,
};

// Les deux branches de l'attaque empoisonnée : un semis tous les cinq tics et
// une échelle sur cent pour le paramètre zéro, tous les neuf tics et sur cent
// soixante-dix pour le un.
constexpr std::int32_t kParamZeroPeriodTicks = 5;
constexpr std::int32_t kParamOnePeriodTicks = 9;
constexpr float kParamZeroScaleLow = 0.40F;
constexpr float kParamZeroScaleHigh = 0.89F;
constexpr float kParamOneScaleLow = 40.0F / 170.0F;
constexpr float kParamOneScaleHigh = 89.0F / 170.0F;
// Posé par la branche du un pour le seul effet 772.
constexpr std::uint32_t kPoisonSmokeRenderWord = 0x201;

constexpr float kBirthYawHigh = 359.0F;
constexpr float kBirthRadiusLow = 2.0F;
constexpr float kBirthRadiusHigh = 7.0F;
// La montée, en vitesse verticale : négative, notre ordonnée montant.
constexpr float kRiseSlowest = -0.30F;
constexpr float kRiseFastest = -0.59F;
constexpr float kDeadlyPoisonRiseFastest = -0.79F;
constexpr float kRiseBrakeShare = 0.5F;

constexpr float kDeadlyPoisonScaleLow = 0.20F;
constexpr float kDeadlyPoisonScaleHigh = 0.39F;

constexpr int kAlphaRiseTicks = 10;

const EffectNodePattern& OnlyPattern(std::uint32_t effect_id) {
  const std::span<const EffectNodePattern> patterns = ro::render::EffectNodePatternsFor(effect_id);
  REQUIRE(patterns.size() == 1);
  REQUIRE(patterns.front().shape == ro::render::EffectNodeShape::kSprite);
  return patterns.front();
}

}  // namespace

TEST_CASE("l'attaque empoisonnée ne sème que la branche de son paramètre",
          "[render][effect_poison]") {
  for (const std::uint32_t each_effect_id : {kPoisonAttackEffect, kEnchantPoisonEffect}) {
    INFO("effet " << each_effect_id);
    const EffectNodePattern& pattern = OnlyPattern(each_effect_id);
    CHECK(pattern.period_ticks == kParamZeroPeriodTicks);
    CHECK(pattern.sprite_scale == Approx(kParamZeroScaleLow));
    CHECK(pattern.sprite_scale_high == Approx(kParamZeroScaleHigh));
    CHECK(pattern.render_word == ro::render::kEffectRenderWordOfConstructor);
  }
  for (const std::uint32_t each_effect_id : {kPoisonSmokeEffect, kPoisonSmoke2Effect}) {
    INFO("effet " << each_effect_id);
    const EffectNodePattern& pattern = OnlyPattern(each_effect_id);
    CHECK(pattern.period_ticks == kParamOnePeriodTicks);
    CHECK(pattern.sprite_scale == Approx(kParamOneScaleLow));
    CHECK(pattern.sprite_scale_high == Approx(kParamOneScaleHigh));
  }
  CHECK(OnlyPattern(kPoisonSmokeEffect).render_word == kPoisonSmokeRenderWord);
  CHECK(OnlyPattern(kPoisonSmoke2Effect).render_word == ro::render::kEffectRenderWordOfConstructor);
}

TEST_CASE("les poisons naissent sur un cercle et montent en freinant", "[render][effect_poison]") {
  for (const std::uint32_t each_effect_id : kPoisonEffects) {
    INFO("effet " << each_effect_id);
    const EffectNodePattern& pattern = OnlyPattern(each_effect_id);
    CHECK(pattern.launch_yaw_low == Approx(0.0F));
    CHECK(pattern.launch_yaw_high == Approx(kBirthYawHigh));
    CHECK(pattern.launch_distance_low == Approx(kBirthRadiusLow));
    CHECK(pattern.launch_distance_high == Approx(kBirthRadiusHigh));
    // La montée ne passe que par la vitesse verticale : aucun pas à plat.
    CHECK(pattern.step_low == Approx(0.0F));
    CHECK(pattern.step_high == Approx(0.0F));
    CHECK(pattern.fall_speed_low ==
          Approx(each_effect_id == kDeadlyPoisonEffect ? kDeadlyPoisonRiseFastest : kRiseFastest));
    CHECK(pattern.fall_speed_high == Approx(kRiseSlowest));
    CHECK(pattern.fall_acceleration_share == Approx(kRiseBrakeShare));
  }
}

TEST_CASE("l'enchantement mortel sème de petites particules", "[render][effect_poison]") {
  const EffectNodePattern& pattern = OnlyPattern(kDeadlyPoisonEffect);
  CHECK(pattern.sprite_scale == Approx(kDeadlyPoisonScaleLow));
  CHECK(pattern.sprite_scale_high == Approx(kDeadlyPoisonScaleHigh));
}

TEST_CASE("l'alpha d'un poison part de zéro et atteint son plafond en dix tics",
          "[render][effect_poison]") {
  const float step = ro::render::kEffectNodeDefaultAlphaMax / static_cast<float>(kAlphaRiseTicks);
  for (const std::uint32_t each_effect_id : kPoisonEffects) {
    INFO("effet " << each_effect_id);
    const EffectNodePattern& pattern = OnlyPattern(each_effect_id);
    CHECK(pattern.alpha_start == Approx(0.0F));
    CHECK(pattern.alpha_step == Approx(step));
    CHECK(pattern.alpha_max == Approx(ro::render::kEffectNodeDefaultAlphaMax));

    const ro::render::EffectNodeSeed seed =
        ro::render::EffectNodeSeedFromPattern(pattern, pattern.life_ticks, 0, 0);
    std::uint32_t generator = 1;
    ro::render::EffectNode node = ro::render::SowEffectNode(seed, generator);
    CHECK(node.alpha == Approx(0.0F));
    for (int tick = 1; tick <= kAlphaRiseTicks; ++tick) {
      REQUIRE(ro::render::AdvanceEffectNode(node));
      CHECK(node.alpha == Approx(step * static_cast<float>(tick)));
    }
    REQUIRE(ro::render::AdvanceEffectNode(node));
    CHECK(node.alpha == Approx(ro::render::kEffectNodeDefaultAlphaMax));
  }
}
