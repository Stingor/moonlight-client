#include "survivors/arena_decor_veil.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_rules.h"

using moonclient::ArenaVeilTargets;
using moonclient::HordeBodyKind;
using moonclient::HordePose;
using moonclient::kMostArenaVeilTargets;

namespace {

// Baphomet, le boss final, et Poring, un monstre commun : des règles livrées.
constexpr std::uint16_t kBossFoeId = 1039;
constexpr std::uint16_t kCommonFoeId = 1002;

// Le sol d'une case : sa position, à hauteur nulle, que l'essai relit.
[[nodiscard]] glm::vec3 GroundOf(glm::vec2 cell) { return {cell.x, 0.0F, cell.y}; }

[[nodiscard]] HordePose PoseAt(HordeBodyKind kind, float x, std::uint16_t look_id = 0) {
  return HordePose{.kind = kind, .look_id = look_id, .cell = glm::vec2(x, 0.0F)};
}

}  // namespace

TEST_CASE("Fondu du feuillage : le héros, les boss, le sol, les plantes, puis les gemmes",
          "[app][survivors][veil]") {
  const std::vector<HordePose> poses{
      PoseAt(HordeBodyKind::kGem, 1.0F),    PoseAt(HordeBodyKind::kFoe, 2.0F, kCommonFoeId),
      PoseAt(HordeBodyKind::kPlant, 3.0F),  PoseAt(HordeBodyKind::kFalling, 4.0F, kBossFoeId),
      PoseAt(HordeBodyKind::kPickup, 5.0F), PoseAt(HordeBodyKind::kFoe, 6.0F, kBossFoeId),
      PoseAt(HordeBodyKind::kHero, 7.0F),
  };
  std::vector<glm::vec3> targets;
  ArenaVeilTargets(poses, ro::sim::ShippedSurvivorsRules(), GroundOf, targets);
  // Ni le monstre commun ni le boss qui tombe ne se regardent.
  REQUIRE(targets.size() == 5);
  CHECK(targets[0].x == 7.0F);
  CHECK(targets[1].x == 6.0F);
  CHECK(targets[2].x == 5.0F);
  CHECK(targets[3].x == 3.0F);
  CHECK(targets[4].x == 1.0F);
}

TEST_CASE("Fondu du feuillage : au plafond, ce sont des gemmes qui restent sans regard",
          "[app][survivors][veil]") {
  std::vector<HordePose> poses(kMostArenaVeilTargets + 1, PoseAt(HordeBodyKind::kGem, 1.0F));
  poses.push_back(PoseAt(HordeBodyKind::kPlant, 2.0F));
  poses.push_back(PoseAt(HordeBodyKind::kHero, 3.0F));
  std::vector<glm::vec3> targets{glm::vec3(9.0F)};
  ArenaVeilTargets(poses, ro::sim::ShippedSurvivorsRules(), GroundOf, targets);
  REQUIRE(targets.size() == kMostArenaVeilTargets);
  CHECK(targets[0].x == 3.0F);
  CHECK(targets[1].x == 2.0F);
  CHECK(targets.back().x == 1.0F);
}
