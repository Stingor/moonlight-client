// LE MAÎTRE D'UN MONSTRE INVOQUÉ — `ZC_MOONLIGHT_UNIT_MASTER 0x0F37`, et ce que
// le monde en garde.

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_update.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/actor.h"
#include "ro/sim/summon_master.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::net::ApplyFrame;
using ro::net::FrameOutcome;
using ro::sim::SummonMasterOf;
using ro::test_support::FrameOf;
using ro::test_support::IdleUnit;

namespace {

constexpr std::uint32_t kPlantId = 110'042;
constexpr std::uint32_t kAlchemistId = 2'000'777;
constexpr std::uint32_t kNowMs = 5000;
// Le type d'objet d'un monstre dans l'annonce d'unité.
constexpr std::uint8_t kMonsterObject = 5;
// Celui d'un homoncule.
constexpr std::uint8_t kHomunculusObject = 8;
constexpr std::uint16_t kMandragoraJob = 1589;

ro::proto::PacketIdleUnit Plant(ro::sim::CellPosition where) {
  ro::proto::PacketIdleUnit unit = IdleUnit(kPlantId, where);
  unit.objecttype = kMonsterObject;
  unit.job = kMandragoraJob;
  unit.name = "Mandragora";
  return unit;
}

FrameOutcome TellMaster(ro::sim::World& world, std::uint32_t actor_id, std::uint32_t master_id) {
  ro::proto::ZcMoonlightUnitMaster told;
  told.GID = actor_id;
  told.master_id = master_id;
  return ApplyFrame(FrameOf(Encode(told)), world, kNowMs);
}

}  // namespace

TEST_CASE("Maître d'invocation : la trame le pose sur le monstre déjà apparu", "[net][summon]") {
  ro::sim::World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(Plant({100, 100}))), world, kNowMs) == FrameOutcome::kApplied);
  CHECK(SummonMasterOf(world, kPlantId) == 0);

  // 🔴 PAR L'AIGUILLAGE ORDINAIRE, comme toute trame du serveur.
  REQUIRE(TellMaster(world, kPlantId, kAlchemistId) == FrameOutcome::kApplied);
  CHECK(SummonMasterOf(world, kPlantId) == kAlchemistId);
  // Rien d'autre ne bouge : la fiche est celle qu'on avait.
  const ro::sim::Actor* plant = world.Find(kPlantId);
  REQUIRE(plant != nullptr);
  CHECK(plant->kind == ro::sim::ActorKind::kMonster);
  CHECK(plant->position.x == 100);
  CHECK(plant->position.y == 100);
}

TEST_CASE("Maître d'invocation : une réannonce de l'unité le garde", "[net][summon]") {
  ro::sim::World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(Plant({100, 100}))), world, kNowMs) == FrameOutcome::kApplied);
  REQUIRE(TellMaster(world, kPlantId, kAlchemistId) == FrameOutcome::kApplied);
  // Le serveur réannonce l'unité entière sans redire son maître.
  REQUIRE(ApplyFrame(FrameOf(Encode(Plant({101, 100}))), world, kNowMs) == FrameOutcome::kApplied);
  CHECK(SummonMasterOf(world, kPlantId) == kAlchemistId);
}

TEST_CASE("Maître d'invocation : un maître nul l'efface", "[net][summon]") {
  ro::sim::World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(Plant({100, 100}))), world, kNowMs) == FrameOutcome::kApplied);
  REQUIRE(TellMaster(world, kPlantId, kAlchemistId) == FrameOutcome::kApplied);
  REQUIRE(TellMaster(world, kPlantId, 0) == FrameOutcome::kApplied);
  CHECK(SummonMasterOf(world, kPlantId) == 0);
  // « Sans maître » est dit : la réannonce ne fait pas revenir l'ancien.
  REQUIRE(ApplyFrame(FrameOf(Encode(Plant({101, 100}))), world, kNowMs) == FrameOutcome::kApplied);
  CHECK(SummonMasterOf(world, kPlantId) == 0);
}

TEST_CASE("Maître d'invocation : avant l'apparition, la trame est ignorée", "[net][summon]") {
  ro::sim::World world;
  // Le serveur l'envoie toujours après l'apparition, et le redit à la suivante.
  CHECK(TellMaster(world, kPlantId, kAlchemistId) == FrameOutcome::kIgnored);
  CHECK(world.Find(kPlantId) == nullptr);
  REQUIRE(ApplyFrame(FrameOf(Encode(Plant({100, 100}))), world, kNowMs) == FrameOutcome::kApplied);
  CHECK(SummonMasterOf(world, kPlantId) == 0);
}

TEST_CASE("Maître d'invocation : il s'en va avec l'acteur et avec la carte", "[net][summon]") {
  ro::sim::World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(Plant({100, 100}))), world, kNowMs) == FrameOutcome::kApplied);
  REQUIRE(TellMaster(world, kPlantId, kAlchemistId) == FrameOutcome::kApplied);

  SECTION("sortie de vue : la rentrée ne le ramène pas toute seule") {
    REQUIRE(world.Remove(kPlantId));
    REQUIRE(ApplyFrame(FrameOf(Encode(Plant({100, 100}))), world, kNowMs) ==
            FrameOutcome::kApplied);
    CHECK(SummonMasterOf(world, kPlantId) == 0);
  }
  SECTION("changement de carte") {
    world.EnterMap("prontera", 150, 150);
    CHECK(SummonMasterOf(world, kPlantId) == 0);
    REQUIRE(ApplyFrame(FrameOf(Encode(Plant({100, 100}))), world, kNowMs) ==
            FrameOutcome::kApplied);
    CHECK(SummonMasterOf(world, kPlantId) == 0);
  }
}

TEST_CASE("Maître d'un compagnon : la même trame nomme le joueur d'un homoncule", "[net][summon]") {
  constexpr std::uint32_t kHomunculusId = 110'077;
  constexpr std::uint16_t kFilirJob = 6003;
  ro::sim::World world;
  ro::proto::PacketIdleUnit homunculus = IdleUnit(kHomunculusId, {100, 100});
  homunculus.objecttype = kHomunculusObject;
  homunculus.job = kFilirJob;
  REQUIRE(ApplyFrame(FrameOf(Encode(homunculus)), world, kNowMs) == FrameOutcome::kApplied);
  REQUIRE(world.Find(kHomunculusId)->familiar == ro::sim::Familiar::kHomunculus);

  REQUIRE(TellMaster(world, kHomunculusId, kAlchemistId) == FrameOutcome::kApplied);
  CHECK(SummonMasterOf(world, kHomunculusId) == kAlchemistId);
  // Une réannonce du compagnon le garde, comme pour un monstre.
  REQUIRE(ApplyFrame(FrameOf(Encode(homunculus)), world, kNowMs) == FrameOutcome::kApplied);
  CHECK(SummonMasterOf(world, kHomunculusId) == kAlchemistId);
  CHECK(world.Find(kHomunculusId)->familiar == ro::sim::Familiar::kHomunculus);
}

TEST_CASE("Maître d'invocation : une trame tronquée est refusée", "[net][summon]") {
  ro::sim::World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(Plant({100, 100}))), world, kNowMs) == FrameOutcome::kApplied);
  ro::proto::ZcMoonlightUnitMaster told;
  told.GID = kPlantId;
  told.master_id = kAlchemistId;
  std::vector<std::uint8_t> bytes = Encode(told);
  bytes.pop_back();
  CHECK(ApplyFrame(FrameOf(bytes), world, kNowMs) == FrameOutcome::kUndecodable);
  CHECK(SummonMasterOf(world, kPlantId) == 0);
}
