#include "ro/render/skill_held_pose.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>

#include <catch2/catch_test_macros.hpp>

using ro::render::HeldPoseFrame;
using ro::render::HeldPoseFrameAt;
using ro::render::SkillHeldPose;
using ro::render::SkillHeldPoseOf;

namespace {

constexpr std::uint32_t kTickMs = 16;
constexpr std::uint8_t kRangIncantation = 12;

std::uint8_t ImageA(const SkillHeldPose& pose, std::uint32_t elapsed_ms) {
  const std::optional<HeldPoseFrame> view = HeldPoseFrameAt(pose, elapsed_ms, kTickMs);
  REQUIRE(view.has_value());
  return view->frame;
}

}  // namespace

TEST_CASE("Pose : la table est triée comme la recherche le suppose", "[render][heldpose]") {
  const std::span<const SkillHeldPose> table = ro::render::SkillHeldPoseTable();
  REQUIRE_FALSE(table.empty());
  CHECK(std::ranges::is_sorted(table, {}, &SkillHeldPose::skill_id));
}

TEST_CASE("Pose : 270 joue les images une et deux, puis tient la deuxième jusqu'à l'échéance",
          "[render][heldpose]") {
  // Sa période est posée par la compétence : trente avances par image.
  const SkillHeldPose* pose = SkillHeldPoseOf(270);
  REQUIRE(pose != nullptr);
  CHECK(pose->play_rank == kRangIncantation);
  CHECK(pose->period_ticks == 30);
  CHECK_FALSE(pose->period_inherited);
  CHECK(ImageA(*pose, 0) == 1);
  CHECK(ImageA(*pose, 29 * kTickMs) == 1);
  CHECK(ImageA(*pose, 30 * kTickMs) == 2);
  CHECK(ImageA(*pose, 900) == 2);
  CHECK_FALSE(HeldPoseFrameAt(*pose, 1000, kTickMs).has_value());
}

TEST_CASE("Pose : 318 reboucle ses deux images au lieu de tenir", "[render][heldpose]") {
  const SkillHeldPose* pose = SkillHeldPoseOf(318);
  REQUIRE(pose != nullptr);
  REQUIRE(pose->loops);
  CHECK(ImageA(*pose, 0) == 4);
  CHECK(ImageA(*pose, 10 * kTickMs) == 5);
  CHECK(ImageA(*pose, 20 * kTickMs) == 4);
  CHECK_FALSE(HeldPoseFrameAt(*pose, 1800, kTickMs).has_value());
}

TEST_CASE("Pose : Triple Attack tient sa pose à l'impact, le temps du délai de combo",
          "[render][heldpose]") {
  constexpr std::uint16_t kTripleAttack = 263;
  constexpr std::uint8_t kSingleHit = 4;
  constexpr std::uint8_t kMultiHit = 8;
  constexpr std::uint8_t kKindWithoutPose = 5;
  const SkillHeldPose* pose = ro::render::SkillImpactPoseOf(kTripleAttack, kMultiHit);
  REQUIRE(pose != nullptr);
  CHECK(ro::render::SkillImpactPoseOf(kTripleAttack, kSingleHit) == pose);
  // Hors des sortes relevées — ici, une compétence qui n'est pas « classe B » —,
  // rien.
  CHECK(ro::render::SkillImpactPoseOf(kTripleAttack, kKindWithoutPose) == nullptr);
  CHECK(pose->hold_from_combo_delay);
  CHECK(pose->play_rank == 11);
  CHECK(pose->period_ticks == 4);
  // L'annonce et l'impact ne se confondent pas.
  CHECK(SkillHeldPoseOf(kTripleAttack) == nullptr);

  constexpr std::uint32_t kHitMs = 10'000;
  constexpr std::uint32_t kDelaiMs = 800;
  // Le délai annoncé avant le coup : la pose dure le délai, depuis le coup.
  CHECK(ro::render::ImpactPoseHoldMs(*pose, kHitMs, kDelaiMs, kHitMs - 50) == kDelaiMs);
  // Annoncé APRÈS le coup, il repousse l'échéance.
  CHECK(ro::render::ImpactPoseHoldMs(*pose, kHitMs, kDelaiMs, kHitMs + 100) == kDelaiMs + 100);
}

TEST_CASE("Pose : une durée fixe ne lit pas le délai de combo", "[render][heldpose]") {
  constexpr std::uint16_t kAsuraStrike = 271;
  constexpr std::uint8_t kSingleHit = 4;
  const SkillHeldPose* pose = ro::render::SkillImpactPoseOf(kAsuraStrike, kSingleHit);
  REQUIRE(pose != nullptr);
  CHECK_FALSE(pose->hold_from_combo_delay);
  CHECK(ro::render::ImpactPoseHoldMs(*pose, 10'000, 800, 10'000) == 3500);
}

TEST_CASE("Pose : une compétence sans pose n'en a pas", "[render][heldpose]") {
  constexpr std::uint16_t kBash = 5;
  CHECK(SkillHeldPoseOf(kBash) == nullptr);
}
