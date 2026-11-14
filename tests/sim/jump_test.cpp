// LE SAUT COSMÉTIQUE : une parabole fermée, qui ne déplace personne.

#include "ro/sim/jump.h"

#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/sim/actor.h"
#include "ro/sim/world.h"

using Catch::Approx;
using ro::sim::JumpLiftAt;
using ro::sim::kJumpCrestUnits;
using ro::sim::kJumpDurationMs;

namespace {

constexpr std::uint32_t kAcolyteId = 2'000'101;
constexpr std::uint32_t kJumpMs = 5'000;

[[nodiscard]] ro::sim::World WorldWithAcolyte() {
  ro::sim::World world;
  ro::sim::Actor acolyte;
  acolyte.id = kAcolyteId;
  acolyte.kind = ro::sim::ActorKind::kPlayer;
  acolyte.position = ro::sim::CellPosition{40, 50, 0};
  world.Spawn(acolyte);
  return world;
}

}  // namespace

TEST_CASE("Saut : l'arc part du sol, culmine au milieu et retombe au sol", "[sim][jump]") {
  ro::sim::World world = WorldWithAcolyte();
  REQUIRE(world.RecordJump(kAcolyteId, kJumpMs));
  const ro::sim::Actor& acolyte = *world.Find(kAcolyteId);

  CHECK(JumpLiftAt(acolyte, kJumpMs) == 0.0F);
  CHECK(JumpLiftAt(acolyte, kJumpMs + kJumpDurationMs / 2) == Approx(kJumpCrestUnits));
  // Symétrique : la montée et la descente prennent le même temps.
  CHECK(JumpLiftAt(acolyte, kJumpMs + kJumpDurationMs / 4) ==
        Approx(JumpLiftAt(acolyte, kJumpMs + kJumpDurationMs * 3 / 4)));
  CHECK(JumpLiftAt(acolyte, kJumpMs + kJumpDurationMs) == 0.0F);
  // 🔴 LA CASE NE BOUGE PAS : le saut est purement cosmétique.
  CHECK(acolyte.position == ro::sim::CellPosition{40, 50, 0});
}

TEST_CASE("Saut : en l'air, un second saut ne relance pas l'arc", "[sim][jump]") {
  ro::sim::World world = WorldWithAcolyte();
  REQUIRE(world.RecordJump(kAcolyteId, kJumpMs));
  CHECK_FALSE(world.RecordJump(kAcolyteId, kJumpMs + kJumpDurationMs / 2));
  CHECK(*world.Find(kAcolyteId)->jump_since_ms == kJumpMs);
  // Retombé, il peut repartir.
  CHECK(world.RecordJump(kAcolyteId, kJumpMs + kJumpDurationMs));
}

TEST_CASE("Saut : un inconnu ne saute pas", "[sim][jump]") {
  ro::sim::World world;
  CHECK_FALSE(world.RecordJump(kAcolyteId, kJumpMs));
}

TEST_CASE("Saut : la réannonce d'un pas garde le saut en cours", "[sim][jump]") {
  // 🔴 LE SERVEUR RÉANNONCE L'ACTEUR À CHAQUE PAS : sans la garde de `Spawn`,
  // sauter en marchant retomberait au sol au premier pas.
  ro::sim::World world = WorldWithAcolyte();
  REQUIRE(world.RecordJump(kAcolyteId, kJumpMs));
  ro::sim::Actor announced;
  announced.id = kAcolyteId;
  announced.kind = ro::sim::ActorKind::kPlayer;
  announced.position = ro::sim::CellPosition{41, 50, 0};
  world.Spawn(announced);
  CHECK(JumpLiftAt(*world.Find(kAcolyteId), kJumpMs + kJumpDurationMs / 2) ==
        Approx(kJumpCrestUnits));
}
