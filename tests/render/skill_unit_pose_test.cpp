#include "ro/render/skill_unit_pose.h"

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::render::SkillUnitPoseKind;
using ro::render::SkillUnitPoseOf;
using ro::render::SkillUnitPoseStep;

TEST_CASE("Unité : la table est triée comme la recherche le suppose", "[render][unitpose]") {
  const std::span<const SkillUnitPoseStep> table = ro::render::SkillUnitPoseTable();
  REQUIRE_FALSE(table.empty());
  CHECK(std::ranges::is_sorted(table, {}, &SkillUnitPoseStep::unit_id));
}

TEST_CASE("Unité : une sphère élémentaire fait naître ses quatre effets", "[render][unitpose]") {
  // La table à la main n'en gardait que le premier.
  constexpr std::uint16_t kSphere = 278;
  std::vector<std::uint32_t> effects;
  for (const SkillUnitPoseStep& step : SkillUnitPoseOf(kSphere)) {
    if (step.kind == SkillUnitPoseKind::kEffect) {
      effects.push_back(step.effect_id);
    }
  }
  CHECK(effects == std::vector<std::uint32_t>{2216, 2217, 2218, 2219});
}

TEST_CASE("Unité : 270 retire son effet AVANT de le semer", "[render][unitpose]") {
  constexpr std::uint16_t kUnit = 270;
  constexpr std::uint32_t kEffectId = 1793;
  const std::span<const SkillUnitPoseStep> suite = SkillUnitPoseOf(kUnit);
  REQUIRE(suite.size() == 2);
  CHECK(suite[0].kind == SkillUnitPoseKind::kRemoveEffect);
  CHECK(suite[0].effect_id == kEffectId);
  CHECK(suite[1].kind == SkillUnitPoseKind::kEffect);
  CHECK(suite[1].effect_id == kEffectId);
}

TEST_CASE("Unité : 266 ne retire rien — son retrait ne vise qu'un effet absent",
          "[render][unitpose]") {
  // Le client ne retire que s'il NE porte PAS l'effet : réapparue, l'unité
  // double sa figure chez lui, et chez nous aussi.
  constexpr std::uint16_t kUnit = 266;
  for (const SkillUnitPoseStep& step : SkillUnitPoseOf(kUnit)) {
    CHECK(step.kind == SkillUnitPoseKind::kEffect);
  }
}

TEST_CASE("Unité : 260 choisit son effet selon le niveau", "[render][unitpose]") {
  constexpr std::uint16_t kUnit = 260;
  const auto effects_at_level = [](std::uint8_t niveau) {
    std::vector<std::uint32_t> effects;
    for (const SkillUnitPoseStep& step : SkillUnitPoseOf(kUnit)) {
      if (ro::render::SkillUnitPoseStepPlays(step, niveau)) {
        effects.push_back(step.effect_id);
      }
    }
    return effects;
  };
  CHECK(effects_at_level(1) == std::vector<std::uint32_t>{1069});
  CHECK(effects_at_level(3) == std::vector<std::uint32_t>{1070});
  CHECK(effects_at_level(5) == std::vector<std::uint32_t>{1071});
  CHECK(effects_at_level(6).empty());
}

TEST_CASE("Unité : Pneuma ne resème pas le voile qu'elle porte déjà", "[render][unitpose]") {
  constexpr std::uint16_t kPneuma = 148;
  const std::span<const SkillUnitPoseStep> suite = SkillUnitPoseOf(kPneuma);
  REQUIRE(suite.size() == 1);
  CHECK(suite.front().unless_carried);
}
