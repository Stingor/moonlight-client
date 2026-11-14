// CE QUE L'APPARITION ET LE DÉPART D'UN ACTEUR MONTRENT.
//
// 🔴 L'APPARITION (0x09FE, `clif_spawn`) N'EST PAS L'ENTRÉE EN VUE (0x09FF) : le
// client fait fondre une créature qui apparaît et entoure un joueur d'anneaux,
// mais ne fait rien pour qui entre simplement dans le champ de vision, ni pour
// qui est réannoncé.

#include <algorithm>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_update.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::net::ApplyFrame;
using ro::net::FrameOutcome;
using ro::sim::CellPosition;
using ro::sim::World;
using ro::test_support::FrameOf;
using ro::test_support::IdleUnit;

namespace {

constexpr std::uint32_t kPlayerId = 150'001;
constexpr std::uint32_t kMonsterId = 110'001;
constexpr std::uint8_t kObjectMonster = 5;
constexpr std::uint32_t kEntry2 = 344;
constexpr std::uint32_t kTeleportation2 = 304;
constexpr std::uint32_t kExit2 = 314;

[[nodiscard]] ro::proto::PacketSpawnUnit Spawn(std::uint32_t id, std::uint8_t object_type,
                                               std::int32_t option = 0) {
  ro::proto::PacketSpawnUnit unit;
  unit.objecttype = object_type;
  unit.AID = id;
  unit.speed = 150;
  unit.job = object_type == 0 ? 4054 : 1002;
  unit.effectState = option;
  unit.name = "Nouveau";
  unit.PosDir = ro::sim::PackPosition(CellPosition{20, 30, 0});
  return unit;
}

[[nodiscard]] bool Wears(const World& world, std::uint32_t id, std::uint32_t effect) {
  const ro::sim::Actor* actor = world.Find(id);
  return actor != nullptr &&
         std::ranges::any_of(actor->effects, [effect](const auto& e) { return e.id == effect; });
}

void Vanish(World& world, std::uint32_t id, std::uint8_t type, std::uint32_t now_ms) {
  ro::proto::ZcNotifyVanish vanish;
  vanish.gid = id;
  vanish.type = type;
  REQUIRE(ApplyFrame(FrameOf(Encode(vanish)), world, now_ms) == FrameOutcome::kApplied);
}

}  // namespace

TEST_CASE("Apparition : une créature qui apparaît monte de la transparence", "[net][arrival]") {
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(Spawn(kMonsterId, kObjectMonster))), world, 500) ==
          FrameOutcome::kApplied);
  REQUIRE(world.Find(kMonsterId) != nullptr);
  CHECK(world.Find(kMonsterId)->appeared_ms == 500U);
  CHECK(world.Find(kMonsterId)->effects.empty());
}

TEST_CASE("Apparition : un joueur qui apparaît reçoit les anneaux du portail", "[net][arrival]") {
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(Spawn(kPlayerId, 0))), world, 500) == FrameOutcome::kApplied);
  CHECK(Wears(world, kPlayerId, kEntry2));
  CHECK_FALSE(world.Find(kPlayerId)->appeared_ms.has_value());
  // Invisible, il n'en reçoit pas.
  REQUIRE(ApplyFrame(FrameOf(Encode(Spawn(kPlayerId + 1, 0, ro::sim::kOptionInvisible))), world,
                     500) == FrameOutcome::kApplied);
  CHECK_FALSE(Wears(world, kPlayerId + 1, kEntry2));
}

TEST_CASE("Apparition : un acteur déjà en vue, réannoncé, ne montre rien", "[net][arrival]") {
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(kPlayerId, CellPosition{20, 30, 0}))), world, 100) ==
          FrameOutcome::kApplied);
  REQUIRE(ApplyFrame(FrameOf(Encode(Spawn(kPlayerId, 0))), world, 500) == FrameOutcome::kApplied);
  CHECK_FALSE(Wears(world, kPlayerId, kEntry2));
}

TEST_CASE("Départ : la téléportation et la déconnexion laissent leur effet sur place",
          "[net][arrival]") {
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(kPlayerId, CellPosition{20, 30, 0}))), world, 0) ==
          FrameOutcome::kApplied);
  Vanish(world, kPlayerId, 3, 1000);
  CHECK(world.Find(kPlayerId) == nullptr);
  REQUIRE(world.departures().pending().size() == 1);
  CHECK(world.departures().pending().front().effect_id == kTeleportation2);
  CHECK(world.departures().pending().front().cell.x == 20.0F);
  CHECK(world.departures().pending().front().cell.y == 30.0F);

  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(kPlayerId, CellPosition{5, 5, 0}))), world, 0) ==
          FrameOutcome::kApplied);
  Vanish(world, kPlayerId, 2, 1100);
  CHECK(world.departures().Take().back().effect_id == kExit2);
  CHECK(world.departures().pending().empty());
}

TEST_CASE("Départ : sortir de vue laisse une copie qui s'efface, et aucun effet",
          "[net][arrival]") {
  // 🔴 LE MONDE L'OUBLIE SUR-LE-CHAMP, comme le client lui retire son
  // identifiant : rien ne le désigne plus. C'est la COPIE qui s'efface.
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(kPlayerId, CellPosition{20, 30, 0}))), world, 0) ==
          FrameOutcome::kApplied);
  Vanish(world, kPlayerId, 0, 1000);
  CHECK(world.Find(kPlayerId) == nullptr);
  CHECK(world.departures().pending().empty());
  REQUIRE(world.departures().fading().size() == 1);
  CHECK(world.departures().fading().front().actor.id == kPlayerId);
  CHECK(world.departures().fading().front().since_ms == 1000U);
  CHECK(world.departures().TakeFading().size() == 1);
  CHECK(world.departures().fading().empty());
}

TEST_CASE("Départ : un invisible téléporté ne laisse rien", "[net][arrival]") {
  World world;

  REQUIRE(ApplyFrame(FrameOf(Encode(Spawn(kPlayerId, 0, ro::sim::kOptionInvisible))), world, 0) ==
          FrameOutcome::kApplied);
  Vanish(world, kPlayerId, 3, 1000);
  CHECK(world.departures().pending().empty());
}

TEST_CASE("Départ : un rafraîchissement retire l'acteur sans fondu ni effet", "[net][arrival]") {
  // Le serveur le fait suivre d'une réapparition : l'acteur ne meurt pas.
  constexpr std::uint8_t kRefreshed = 4;
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(Spawn(kMonsterId, kObjectMonster))), world, 0) ==
          FrameOutcome::kApplied);
  Vanish(world, kMonsterId, kRefreshed, 1000);
  CHECK(world.Find(kMonsterId) == nullptr);
  CHECK(world.departures().pending().empty());
  CHECK(world.departures().fading().empty());
  // Réapparu, il est neuf : il monte de la transparence.
  REQUIRE(ApplyFrame(FrameOf(Encode(Spawn(kMonsterId, kObjectMonster))), world, 1000) ==
          FrameOutcome::kApplied);
  CHECK(world.Find(kMonsterId)->appeared_ms == 1000U);
}

TEST_CASE("Mort : un PNJ reste debout, une créature et un corps de joueur tombent",
          "[net][arrival]") {
  constexpr std::uint8_t kDead = 1;
  constexpr std::uint8_t kObjectNonPlayer = 6;
  constexpr std::uint32_t kNonPlayerId = 120'001;
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(Spawn(kNonPlayerId, kObjectNonPlayer))), world, 0) ==
          FrameOutcome::kApplied);
  REQUIRE(ApplyFrame(FrameOf(Encode(Spawn(kMonsterId, kObjectMonster))), world, 0) ==
          FrameOutcome::kApplied);
  REQUIRE(ApplyFrame(FrameOf(Encode(Spawn(kPlayerId, 0))), world, 0) == FrameOutcome::kApplied);
  Vanish(world, kNonPlayerId, kDead, 1000);
  Vanish(world, kMonsterId, kDead, 1000);
  Vanish(world, kPlayerId, kDead, 1000);
  CHECK(world.Find(kNonPlayerId)->posture == ro::sim::Posture::kStanding);
  CHECK(world.Find(kMonsterId)->posture == ro::sim::Posture::kDead);
  CHECK(world.Find(kPlayerId)->posture == ro::sim::Posture::kDead);
}

// ── LES COULEURS D'UN JOUEUR PARTI ──────────────────────────────────────────

namespace {

constexpr std::uint32_t kSelfId = 150'100;
constexpr std::uint32_t kAnyBodyKey = 7;

void Colored(World& world, std::uint32_t gid) {
  world.styles().BeginBatchFor(gid);
  world.styles().Remember(gid, kAnyBodyKey, ro::sim::StyleRecipe{}, /*is_fallback=*/true);
}

// Nous, et un autre joueur en vue : tous deux portent des couleurs.
[[nodiscard]] World ColoredWorld() {
  World world;
  world.set_player_id(kSelfId);
  REQUIRE(ApplyFrame(FrameOf(Encode(Spawn(kPlayerId, 0))), world, 0) == FrameOutcome::kApplied);
  Colored(world, kSelfId);
  Colored(world, kPlayerId);
  return world;
}

}  // namespace

TEST_CASE("Départ : un autre joueur qui s'en va emporte ses couleurs", "[net][arrival]") {
  // 🔴 LE SERVEUR NE DIT RIEN D'UN JOUEUR SANS STYLE : celui qui a supprimé le
  // sien hors de notre vue resterait teint. Il redit les couleurs de qui
  // réapparaît ou rentre en vue, et le livre ne garde donc que ceux qu'on voit.
  constexpr std::uint8_t kOutOfSight = 0;
  constexpr std::uint8_t kLoggedOut = 2;
  constexpr std::uint8_t kTeleported = 3;
  for (const std::uint8_t type : {kOutOfSight, kLoggedOut, kTeleported}) {
    World world = ColoredWorld();
    Vanish(world, kPlayerId, type, 500);
    CHECK(world.styles().Find(kPlayerId) == nullptr);
    // ⚠ « ON NE SAIT PLUS », ET NON « PLUS RIEN » : le livre redevient ce qu'il
    // était avant le premier lot de ce joueur.
    CHECK(world.styles().RevisionOf(kPlayerId) == 0);
    CHECK(world.styles().Find(kSelfId) != nullptr);
  }
}

TEST_CASE("Départ : la mort et le rafraîchissement gardent les couleurs", "[net][arrival]") {
  constexpr std::uint8_t kDead = 1;
  constexpr std::uint8_t kRefreshed = 4;
  for (const std::uint8_t type : {kDead, kRefreshed}) {
    World world = ColoredWorld();
    Vanish(world, kPlayerId, type, 500);
    CHECK(world.styles().Find(kPlayerId) != nullptr);
  }
}

TEST_CASE("Départ : nos propres couleurs ne partent jamais", "[net][arrival]") {
  // 🔴 LE SERVEUR NE NOUS REDIT JAMAIS LES NÔTRES : les oublier les perdrait
  // jusqu'à la prochaine validation.
  constexpr std::uint8_t kOutOfSight = 0;
  World world = ColoredWorld();
  Vanish(world, kSelfId, kOutOfSight, 500);
  CHECK(world.styles().Find(kSelfId) != nullptr);

  ro::proto::ZcNpcackMapmove moved;
  moved.mapName = "prontera.gat";
  REQUIRE(ApplyFrame(FrameOf(Encode(moved)), world, 1000) == FrameOutcome::kApplied);
  CHECK(world.styles().Find(kSelfId) != nullptr);
  // Les autres restent sur la carte quittée.
  CHECK(world.styles().Find(kPlayerId) == nullptr);
  CHECK(world.styles().RevisionOf(kPlayerId) == 0);
}
