// UN WARP ENTRE DANS LA SCÈNE SANS CORPS : c'est ce qui le rend survolable et
// cliquable. Sa désignation s'éprouve avec `PickActor` ; ici, ce qui l'y amène.

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_warp_ring_seed.h"
#include "ro/render/sprite_renderer.h"
#include "ro/sim/actor.h"

#include "world/actor_visibility.h"

namespace {

constexpr std::uint32_t kWarpId = 3'016'846;
// La classe qui SUIT celle des portails : un villageois, qui n'en est pas un.
constexpr std::uint16_t kVillagerJob = ro::sim::kPortalJob + 1;

ro::sim::Actor Someone(ro::sim::ActorKind kind, std::uint16_t job) {
  ro::sim::Actor actor;
  actor.kind = kind;
  actor.look.job = job;
  return actor;
}

}  // namespace

TEST_CASE("Warp : un PNJ de la classe des portails, et lui seul", "[app][warp]") {
  CHECK(moonclient::IsWarp(Someone(ro::sim::ActorKind::kNonPlayer, ro::sim::kPortalJob)));
  CHECK_FALSE(moonclient::IsWarp(Someone(ro::sim::ActorKind::kNonPlayer, kVillagerJob)));
  // Le numéro seul ne suffit pas : il se relit sur n'importe quel genre d'acteur.
  CHECK_FALSE(moonclient::IsWarp(Someone(ro::sim::ActorKind::kPlayer, ro::sim::kPortalJob)));
  CHECK_FALSE(moonclient::IsWarp(Someone(ro::sim::ActorKind::kMonster, ro::sim::kPortalJob)));
}

TEST_CASE("Warp : il entre dans la scène par le disque de ses anneaux, sans corps", "[app][warp]") {
  std::vector<ro::render::DrawnActor> drawn;
  std::vector<std::uint32_t> ids;
  const glm::vec3 ground{87.5F, 0.0F, 87.5F};
  moonclient::KeepWarp(kWarpId, ground, drawn, ids);

  REQUIRE(drawn.size() == 1);
  REQUIRE(ids.size() == 1);
  CHECK(ids.front() == kWarpId);
  CHECK(drawn.front().actor == nullptr);
  CHECK(drawn.front().ground_position == ground);
  // Le disque visible : celui dont repart le plus grand des anneaux.
  CHECK(drawn.front().ground_pick_radius == ro::render::kWarpRingRewindRadius);
}

TEST_CASE("PNJ caché : il entre dans la scène par le point de ses pieds, sans corps",
          "[app][warp]") {
  std::vector<ro::render::DrawnActor> drawn;
  std::vector<std::uint32_t> ids;
  constexpr std::uint32_t kFlagId = 110'003'412;
  constexpr float kNpcBias = 100.0F;
  const glm::vec3 ground{502.5F, 0.0F, 1162.5F};
  moonclient::KeepPointPick(kFlagId, ground, kNpcBias, drawn, ids);

  REQUIRE(drawn.size() == 1);
  REQUIRE(ids.size() == 1);
  CHECK(ids.front() == kFlagId);
  CHECK(drawn.front().actor == nullptr);
  CHECK(drawn.front().point_pick);
  CHECK(drawn.front().ground_pick_radius == 0.0F);
  CHECK(drawn.front().ground_position == ground);
  CHECK(drawn.front().pick_bias == kNpcBias);
}
