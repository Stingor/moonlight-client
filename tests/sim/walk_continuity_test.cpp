// LA MARCHE MONTRÉE TRAVERSE UNE RÉANNONCE SANS SAUTER.

#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/sim/movement.h"
#include "ro/sim/server_clock.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"

using Catch::Approx;
using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::CellMovement;
using ro::sim::CellPosition;
using ro::sim::World;

namespace {

constexpr std::uint32_t kActorId = 7;
constexpr std::uint16_t kMsPerCell = 100;
constexpr std::uint16_t kRowY = 10;
constexpr std::uint8_t kCenter = 8;
constexpr std::uint32_t kFirstStartMs = 1000;
constexpr float kExact = 0.001F;

[[nodiscard]] CellMovement EastFrom(std::uint16_t from_x, std::uint16_t to_x = 40) {
  return CellMovement{CellPosition{from_x, kRowY, 0}, CellPosition{to_x, kRowY, 0}, kCenter,
                      kCenter};
}

[[nodiscard]] Actor Walker(std::uint16_t speed = kMsPerCell) {
  Actor actor;
  actor.id = kActorId;
  actor.kind = ActorKind::kPlayer;
  actor.speed_ms_per_cell = speed;
  actor.position = CellPosition{10, kRowY, 0};
  return actor;
}

// Un monde sans carte où l'acteur marche vers l'est depuis la case 10.
[[nodiscard]] World Walking() {
  World world;
  world.Spawn(Walker());
  world.StartMoving(kActorId, EastFrom(10), kFirstStartMs);
  return world;
}

[[nodiscard]] float ShownX(const World& world, std::uint32_t at_ms) {
  return ro::sim::InterpolatedCell(*world.Find(kActorId), at_ms).x;
}

}  // namespace

TEST_CASE("Une réannonce laisse l'acteur où il se montrait, puis l'écart s'éteint") {
  World world = Walking();
  // À 1560, le chemin le montre à 15,6 ; le serveur dit qu'il a quitté la case 15
  // à 1520, ce qui le placerait à 15,4.
  REQUIRE(ShownX(world, 1560) == Approx(15.6F).margin(kExact));
  world.StartMoving(kActorId, EastFrom(15), 1520, 1560);

  CHECK(ShownX(world, 1560) == Approx(15.6F).margin(kExact));
  // À mi-résorption, la moitié des 0,2 case reste.
  CHECK(ShownX(world, 1635) == Approx(16.25F).margin(kExact));
  // Une fois résorbé, seul le chemin compte.
  CHECK(ShownX(world, 1710) == Approx(16.9F).margin(kExact));
}

TEST_CASE("Un acteur en avance ralentit à la réannonce et ne recule jamais") {
  World world = Walking();
  // 0,4 case d'avance : la réannonce le replace à 15,0 alors qu'il montre 15,4.
  world.StartMoving(kActorId, EastFrom(15), 1540, 1540);
  float previous_x = ShownX(world, 1540);
  REQUIRE(previous_x == Approx(15.4F).margin(kExact));
  constexpr std::uint32_t kFrameMs = 7;
  for (std::uint32_t at_ms = 1540 + kFrameMs; at_ms < 1800; at_ms += kFrameMs) {
    const float x = ShownX(world, at_ms);
    CHECK(x > previous_x);
    previous_x = x;
  }
}

TEST_CASE("Le départ daté du serveur ne fait pas bondir qui était immobile") {
  World world;
  world.Spawn(Walker());
  // Annoncé 30 ms après son départ : le chemin l'a déjà emmené à 10,3.
  world.StartMoving(kActorId, EastFrom(10), 1000, 1030);
  CHECK(ShownX(world, 1030) == Approx(10.0F).margin(kExact));
  CHECK(ShownX(world, 1180) == Approx(11.8F).margin(kExact));
}

TEST_CASE("Les jambes traversent une réannonce sans changer de distance") {
  World world = Walking();
  // Le chemin précédent a parcouru 5,6 cases ; le neuf, depuis la case 15, n'en
  // compterait que 5,4.
  const auto travelled = [&world](std::uint32_t at_ms) {
    const Actor& actor = *world.Find(kActorId);
    return ro::sim::TravelledCells(
        actor, ro::sim::StraightWalkPath(actor.movement->from, actor.movement->to), at_ms);
  };
  REQUIRE(travelled(1560) == Approx(5.6F).margin(kExact));
  world.StartMoving(kActorId, EastFrom(15), 1520, 1560);
  CHECK(travelled(1560) == Approx(5.6F).margin(kExact));
  CHECK(travelled(1660) == Approx(6.6F).margin(kExact));
}

TEST_CASE("L'arrêt se juge sur le chemin annoncé, pas sur l'écart à résorber") {
  World world = Walking();
  world.StartMoving(kActorId, EastFrom(15), 1520, 1560);
  const Actor& actor = *world.Find(kActorId);
  const ro::sim::WalkPath path =
      ro::sim::StraightWalkPath(actor.movement->from, actor.movement->to);
  // Montré à 15,6, mais à 15,4 sur son chemin : le serveur l'arrêterait en 15.
  CHECK(ro::sim::StoppedCell(actor, path, 1560).x == 15);
}

TEST_CASE("Un acteur vraiment replacé se pose au lieu de glisser") {
  World world = Walking();
  // Sur sa marche, un écart de vingt cases ne vient d'aucune gigue.
  world.StartMoving(kActorId, EastFrom(35), 1560, 1560);
  CHECK(ShownX(world, 1560) == Approx(35.0F).margin(kExact));
}

TEST_CASE("Le seuil de pose tient la gigue à la marche la plus rapide") {
  CHECK(ro::sim::kWalkAbsorbLargestCells == Approx(2.0F));
  World world;
  world.Spawn(Walker(ro::sim::kFastestStepMs));
  world.StartMoving(kActorId, EastFrom(10), 1000);
  // 1,5 case de retard, à cinquante cases par seconde : à résorber.
  world.StartMoving(kActorId, EastFrom(10), 1030, 1030);
  CHECK(ShownX(world, 1030) == Approx(11.5F).margin(kExact));
}

TEST_CASE("L'arrêt annoncé efface l'écart en cours") {
  World world = Walking();
  world.StartMoving(kActorId, EastFrom(15), 1520, 1560);
  world.PlaceAt(kActorId, CellPosition{16, kRowY, 0}, 1561);
  CHECK(ShownX(world, 1561) == Approx(16.0F).margin(kExact));
}

TEST_CASE("Un acteur sans vitesse connue n'a rien à résorber") {
  World world;
  world.Spawn(Walker(0));
  world.StartMoving(kActorId, EastFrom(10, 11), 1000, 1000);
  CHECK(ShownX(world, 1000) == Approx(11.0F).margin(kExact));
}

TEST_CASE("La réannonce d'un acteur entier se raccorde, son entrée en vue non") {
  World world;
  Actor walker = Walker();
  walker.movement = EastFrom(10);
  walker.movement_started_ms = kFirstStartMs;
  world.SpawnWalking(walker, kFirstStartMs + 10);
  // Inconnu jusque-là : il paraît là où son chemin le met.
  CHECK(ShownX(world, kFirstStartMs + 10) == Approx(10.1F).margin(kExact));

  Actor announced = Walker();
  announced.position = CellPosition{15, kRowY, 0};
  announced.movement = EastFrom(15);
  announced.movement_started_ms = 1520;
  world.SpawnWalking(announced, 1560);
  CHECK(ShownX(world, 1560) == Approx(15.6F).margin(kExact));
  CHECK(ShownX(world, 1710) == Approx(16.9F).margin(kExact));
}

TEST_CASE("Marche : une trame lue à une heure périmée ne fait pas glisser le joueur",
          "[sim][walk]") {
  // Un clic maintenu près du personnage : le serveur annonce un pas d'une case
  // à la fois, et chaque pas se date à l'heure du serveur.
  constexpr std::uint32_t kHeartbeatSentMs = 1'000;
  constexpr std::uint32_t kOneWayMs = 15;
  constexpr std::uint32_t kServerAtHeartbeat = 500'000;
  constexpr std::uint32_t kStalledMs = 3'000;
  constexpr std::uint32_t kHalfStepMs = kMsPerCell / 2;
  constexpr std::uint16_t kHops = 20;

  World world;
  world.Spawn(Walker());
  world.set_player_id(kActorId);
  ro::sim::ServerClock& clock = world.server_clock();
  // Le serveur sert un instant de `kOneWayMs` en retard sur notre horloge.
  const auto server_tick_at = [&](std::uint32_t local_ms) {
    return kServerAtHeartbeat + (local_ms - kHeartbeatSentMs) - kOneWayMs;
  };
  clock.NoteTickSent(kHeartbeatSentMs);
  clock.NoteServerTick(server_tick_at(kHeartbeatSentMs + 2 * kOneWayMs),
                       kHeartbeatSentMs + 2 * kOneWayMs);

  // LA FENÊTRE A GELÉ : l'image avait pris son heure avant le gel, et elle lit
  // ensuite une trame que le serveur a datée bien après.
  const std::uint32_t frozen_at_ms = kHeartbeatSentMs + 100;
  clock.NoteServerTimestamp(server_tick_at(frozen_at_ms + kStalledMs), frozen_at_ms);

  // Le battement suivant reste faux s'il ne pose pas son plancher.
  const std::uint32_t next_heartbeat_ms = frozen_at_ms + kStalledMs + 100;
  clock.NoteTickSent(next_heartbeat_ms);
  clock.NoteServerTick(server_tick_at(next_heartbeat_ms + 2 * kOneWayMs),
                       next_heartbeat_ms + 2 * kOneWayMs);

  std::uint32_t now_ms = next_heartbeat_ms + 200;
  std::uint16_t walking_hops = 0;
  for (std::uint16_t hop = 0; hop < kHops; ++hop) {
    const auto from_x = static_cast<std::uint16_t>(kRowY + hop);
    const std::uint32_t started_ms = clock.LocalTimeOf(server_tick_at(now_ms - kOneWayMs), now_ms);
    world.StartMoving(kActorId, EastFrom(from_x, static_cast<std::uint16_t>(from_x + 1)),
                      started_ms, now_ms);
    if (ro::sim::IsWalking(*world.Find(kActorId), now_ms + kHalfStepMs)) {
      ++walking_hops;
    }
    now_ms += kMsPerCell;
  }
  CHECK(walking_hops == kHops);
}
