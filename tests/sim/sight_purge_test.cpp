// LES ACTEURS RESTÉS HORS DE LA VUE DU SERVEUR : purgés après le délai, au-delà
// de la marge, jamais le personnage, et rien tant que le rayon n'est pas dit.

#include "ro/sim/sight_purge.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/actor.h"
#include "ro/sim/world.h"

using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::CellPosition;
using ro::sim::kSightPurgeGraceMs;
using ro::sim::kSightPurgeMarginCells;
using ro::sim::PurgedActor;
using ro::sim::SightPurge;
using ro::sim::World;

namespace {

constexpr std::uint32_t kPlayerId = 100;
constexpr std::uint32_t kGhostId = 200;
constexpr std::uint32_t kNearId = 300;
constexpr std::uint16_t kPupaJob = 1008;
constexpr std::uint16_t kCenter = 100;
constexpr int kViewCells = 18;
// Un instant de départ quelconque, loin de zéro.
constexpr std::uint32_t kStartMs = 100000;

void Place(World& world, std::uint32_t id, ActorKind kind, std::uint16_t x, std::uint16_t y) {
  Actor actor;
  actor.id = id;
  actor.kind = kind;
  actor.look.job = kPupaJob;
  actor.position = CellPosition{x, y, 0};
  world.Spawn(actor);
}

[[nodiscard]] World Scene() {
  World world;
  Place(world, kPlayerId, ActorKind::kPlayer, kCenter, kCenter);
  world.set_player_id(kPlayerId);
  // Au-delà du carré ET de la marge, sur la diagonale : la mesure est le carré.
  constexpr auto kFar =
      static_cast<std::uint16_t>(kCenter + kViewCells + kSightPurgeMarginCells + 1);
  Place(world, kGhostId, ActorKind::kMonster, kFar, kFar);
  // Dans la marge : peut-être un acteur qui sort et dont la sortie arrive.
  constexpr auto kInMargin =
      static_cast<std::uint16_t>(kCenter + kViewCells + kSightPurgeMarginCells);
  Place(world, kNearId, ActorKind::kMonster, kInMargin, kCenter);
  return world;
}

}  // namespace

TEST_CASE("Purge hors de vue : après le délai, au-delà de la marge seulement", "[sim][sight]") {
  World world = Scene();
  SightPurge purge;
  CHECK(purge.Sweep(world, kViewCells, kStartMs).empty());
  CHECK(purge.Sweep(world, kViewCells, kStartMs + kSightPurgeGraceMs - 1).empty());
  const std::vector<PurgedActor> purged =
      purge.Sweep(world, kViewCells, kStartMs + kSightPurgeGraceMs);
  REQUIRE(purged.size() == 1);
  CHECK(purged.front().actor_id == kGhostId);
  CHECK(purged.front().job == kPupaJob);
  CHECK(purged.front().dx_cells == kViewCells + kSightPurgeMarginCells + 1);
  CHECK(world.Find(kGhostId) == nullptr);
  // Ni l'acteur dans la marge, ni le personnage.
  CHECK(world.Find(kNearId) != nullptr);
  CHECK(world.Find(kPlayerId) != nullptr);
}

TEST_CASE("Purge hors de vue : un acteur revenu dans le carré repart de zéro", "[sim][sight]") {
  World world = Scene();
  SightPurge purge;
  static_cast<void>(purge.Sweep(world, kViewCells, kStartMs));
  // Il revient près du personnage, puis repart au loin.
  Place(world, kGhostId, ActorKind::kMonster, kCenter + 1, kCenter);
  static_cast<void>(purge.Sweep(world, kViewCells, kStartMs + 1));
  constexpr auto kFar =
      static_cast<std::uint16_t>(kCenter + kViewCells + kSightPurgeMarginCells + 1);
  Place(world, kGhostId, ActorKind::kMonster, kFar, kCenter);
  CHECK(purge.Sweep(world, kViewCells, kStartMs + kSightPurgeGraceMs).empty());
  CHECK(world.Find(kGhostId) != nullptr);
}

TEST_CASE("Purge hors de vue : rien tant que le serveur n'a pas dit son rayon", "[sim][sight]") {
  World world = Scene();
  SightPurge purge;
  static_cast<void>(purge.Sweep(world, 0, kStartMs));
  CHECK(purge.Sweep(world, 0, kStartMs + kSightPurgeGraceMs).empty());
  CHECK(world.Find(kGhostId) != nullptr);
}

TEST_CASE("Purge : un monstre debout à zéro PV, après le délai, même sans rayon connu",
          "[sim][sight]") {
  constexpr std::uint32_t kAliveId = 400;
  constexpr std::uint32_t kLyingId = 500;
  constexpr std::int32_t kMaxHealth = 427;
  World world = Scene();
  const auto set_health = [&world](std::uint32_t id, std::int32_t health) {
    Actor actor = *world.Find(id);
    actor.health = health;
    actor.max_health = kMaxHealth;
    world.Spawn(actor);
  };
  set_health(kNearId, 0);
  // UN PV RESTANT : la jauge dit « 0 % », le monstre vit.
  Place(world, kAliveId, ActorKind::kMonster, kCenter + 1, kCenter);
  set_health(kAliveId, 1);
  // Couché : sa mort est arrivée, l'agonie le retirera.
  Place(world, kLyingId, ActorKind::kMonster, kCenter + 2, kCenter);
  set_health(kLyingId, 0);
  REQUIRE(world.SetPosture(kLyingId, ro::sim::Posture::kDead, kStartMs));

  SightPurge purge;
  CHECK(purge.Sweep(world, 0, kStartMs).empty());
  CHECK(purge.Sweep(world, 0, kStartMs + ro::sim::kZeroHealthGraceMs - 1).empty());
  const std::vector<PurgedActor> purged =
      purge.Sweep(world, 0, kStartMs + ro::sim::kZeroHealthGraceMs);
  REQUIRE(purged.size() == 1);
  CHECK(purged.front().actor_id == kNearId);
  CHECK(purged.front().reason == ro::sim::PurgeReason::kZeroHealth);
  CHECK(world.Find(kNearId) == nullptr);
  CHECK(world.Find(kAliveId) != nullptr);
  CHECK(world.Find(kLyingId) != nullptr);
}

namespace {

constexpr std::uint16_t kSlowMsPerCell = 300;
constexpr std::uint16_t kWalkCells = 18;

// Met l'acteur en marche lente vers `to_x`, à partir de `kStartMs`.
void Walk(World& world, std::uint32_t id, std::uint16_t to_x) {
  Actor actor = *world.Find(id);
  actor.speed_ms_per_cell = kSlowMsPerCell;
  world.Spawn(actor);
  const CellPosition from = world.Find(id)->position;
  REQUIRE(world.StartMoving(id,
                            ro::sim::CellMovement{.from = from,
                                                  .to = CellPosition{to_x, from.y, 0},
                                                  .sub_x = ro::sim::kCellCenterSubCell,
                                                  .sub_y = ro::sim::kCellCenterSubCell},
                            kStartMs));
}

}  // namespace

TEST_CASE("Purge hors de vue : la vue se juge d'où le personnage est, pas d'où il va",
          "[sim][sight]") {
  World world;
  Place(world, kPlayerId, ActorKind::kPlayer, kCenter, kCenter);
  world.set_player_id(kPlayerId);
  // TROIS CASES DERRIÈRE LUI : au-delà du carré vu de l'arrivée, à portée vue d'ici.
  constexpr std::uint16_t kBehind = 3;
  constexpr auto kBehindX = static_cast<std::uint16_t>(kCenter - kBehind);
  Place(world, kNearId, ActorKind::kNonPlayer, kBehindX, kCenter);
  Walk(world, kPlayerId, kCenter + kWalkCells);
  SightPurge purge;
  CHECK(purge.Sweep(world, kViewCells, kStartMs).empty());
  CHECK(purge.Sweep(world, kViewCells, kStartMs + kSightPurgeGraceMs).empty());
  CHECK(world.Find(kNearId) != nullptr);
  // ARRIVÉ, il l'a vraiment laissé hors de vue : le délai court de là.
  const std::uint32_t arrived_ms = kStartMs + std::uint32_t{kWalkCells} * kSlowMsPerCell;
  CHECK(purge.Sweep(world, kViewCells, arrived_ms).empty());
  const std::vector<PurgedActor> purged =
      purge.Sweep(world, kViewCells, arrived_ms + kSightPurgeGraceMs);
  REQUIRE(purged.size() == 1);
  CHECK(purged.front().actor_id == kNearId);
  CHECK(purged.front().dx_cells == -(kBehind + kWalkCells));
}

TEST_CASE("Purge hors de vue : un acteur lent en route vers le lointain reste en vue",
          "[sim][sight]") {
  World world;
  Place(world, kPlayerId, ActorKind::kPlayer, kCenter, kCenter);
  world.set_player_id(kPlayerId);
  Place(world, kGhostId, ActorKind::kMonster, kCenter + 1, kCenter);
  constexpr std::uint16_t kFarWalkCells = 30;
  Walk(world, kGhostId, kCenter + 1 + kFarWalkCells);
  SightPurge purge;
  CHECK(purge.Sweep(world, kViewCells, kStartMs).empty());
  CHECK(purge.Sweep(world, kViewCells, kStartMs + kSightPurgeGraceMs).empty());
  CHECK(world.Find(kGhostId) != nullptr);
}
