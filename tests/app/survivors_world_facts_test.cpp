// LE RELEVÉ DU MONDE RÉEL QUE LA PARTIE COMPOSE, sur un `World` de test : la
// garde d'entrée (5d) rejouée sur lui — le personnage, les monstres hostiles
// vivants à la borne des 14 cases, le dialogue, le stockage —, la veille qui
// voit un coup ou une mort, et le compte des messages privés reçus.

#include "survivors/survivors_world_facts.h"

#include <cstdint>
#include <optional>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/actor.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/combat_notice.h"
#include "ro/sim/whisper.h"
#include "ro/sim/world.h"

#include "survivors/real_world_watch.h"

using moonclient::EntryFactsOf;
using moonclient::EntryRefusal;
using moonclient::Judge;
using moonclient::kHostileGuardCells;
using moonclient::RealFactsOf;
using moonclient::RealWorldAlert;
using moonclient::RealWorldWatch;
using moonclient::WhisperCounter;
using moonclient::WorldAside;

namespace {

constexpr std::uint32_t kPlayerId = 2000001;
constexpr std::uint32_t kMonsterId = 110001;
constexpr std::uint32_t kNpcId = 110002;
constexpr std::uint16_t kPlayerCellX = 150;
constexpr std::uint16_t kPlayerCellY = 180;
constexpr std::int32_t kFullHp = 500;
constexpr std::int32_t kHurtHp = 420;
constexpr std::int32_t kBlowDamage = 80;
constexpr std::uint32_t kNowMs = 1000;

// Un personnage debout en ville, sa vie connue.
ro::sim::World TownWorld() {
  ro::sim::World world;
  world.EnterMap("prontera", kPlayerCellX, kPlayerCellY);
  ro::sim::Actor player;
  player.id = kPlayerId;
  player.kind = ro::sim::ActorKind::kPlayer;
  player.position = ro::sim::CellPosition{.x = kPlayerCellX, .y = kPlayerCellY};
  world.Spawn(player);
  world.set_player_id(kPlayerId);
  REQUIRE(world.SetHealth(kPlayerId, kFullHp, kFullHp));
  return world;
}

WorldAside InTown() { return WorldAside{.on_town_map = true}; }

void SpawnMonster(ro::sim::World& world, std::uint16_t gap_cells, ro::sim::Posture posture) {
  ro::sim::Actor monster;
  monster.id = kMonsterId;
  monster.kind = ro::sim::ActorKind::kMonster;
  monster.position = ro::sim::CellPosition{
      .x = static_cast<std::uint16_t>(kPlayerCellX + gap_cells), .y = kPlayerCellY};
  world.Spawn(monster);
  REQUIRE(world.SetPosture(kMonsterId, posture, kNowMs));
}

}  // namespace

TEST_CASE("survivors relevé : un personnage seul en ville peut entrer", "[app][survivors]") {
  const ro::sim::World world = TownWorld();
  CHECK_FALSE(Judge(EntryFactsOf(world, InTown())).has_value());
}

TEST_CASE("survivors relevé : sans personnage nommé, la carte se charge", "[app][survivors]") {
  const ro::sim::World world;
  CHECK(Judge(EntryFactsOf(world, InTown())) == EntryRefusal::kLoading);
}

TEST_CASE("survivors relevé : hors ville et hors intérieur, refusé", "[app][survivors]") {
  const ro::sim::World world = TownWorld();
  CHECK(Judge(EntryFactsOf(world, WorldAside{})) == EntryRefusal::kNotSheltered);
  CHECK_FALSE(Judge(EntryFactsOf(world, WorldAside{.on_indoor_map = true})).has_value());
}

TEST_CASE("survivors relevé : un monstre vivant à moins de 14 cases refuse", "[app][survivors]") {
  ro::sim::World world = TownWorld();
  SpawnMonster(world, static_cast<std::uint16_t>(kHostileGuardCells - 1),
               ro::sim::Posture::kStanding);
  CHECK(Judge(EntryFactsOf(world, InTown())) == EntryRefusal::kHostileNearby);

  // À la borne, il ne refuse plus.
  SpawnMonster(world, static_cast<std::uint16_t>(kHostileGuardCells), ro::sim::Posture::kStanding);
  CHECK(EntryFactsOf(world, InTown()).nearest_hostile_cells == kHostileGuardCells);
  CHECK_FALSE(Judge(EntryFactsOf(world, InTown())).has_value());

  // Mort tout près, il ne compte pas.
  SpawnMonster(world, 1, ro::sim::Posture::kDead);
  CHECK_FALSE(EntryFactsOf(world, InTown()).nearest_hostile_cells.has_value());
}

TEST_CASE("survivors relevé : mort, dialogue et stockage se lisent dans le monde",
          "[app][survivors]") {
  ro::sim::World dead = TownWorld();
  REQUIRE(dead.SetPosture(kPlayerId, ro::sim::Posture::kDead, kNowMs));
  CHECK(Judge(EntryFactsOf(dead, InTown())) == EntryRefusal::kDead);

  ro::sim::World talking = TownWorld();
  talking.SayNpcDialog(kNpcId, "Bonjour.", kNowMs);
  CHECK(Judge(EntryFactsOf(talking, InTown())) == EntryRefusal::kNpcDialogueOpen);

  ro::sim::World storing = TownWorld();
  storing.OpenStorage();
  CHECK(Judge(EntryFactsOf(storing, InTown())) == EntryRefusal::kStorageOpen);
}

TEST_CASE("survivors relevé : ce que le monde ne porte pas vient de l'aparté", "[app][survivors]") {
  const ro::sim::World world = TownWorld();
  WorldAside aside = InTown();
  aside.trade_open = true;
  CHECK(Judge(EntryFactsOf(world, aside)) == EntryRefusal::kTradeOpen);
  aside = InTown();
  aside.shop_open = true;
  CHECK(Judge(EntryFactsOf(world, aside)) == EntryRefusal::kShopOpen);
  aside = InTown();
  aside.loading = true;
  CHECK(Judge(EntryFactsOf(world, aside)) == EntryRefusal::kLoading);
}

TEST_CASE("survivors relevé : une liaison perdue refuse l'entrée et « Rejouer »",
          "[app][survivors]") {
  const ro::sim::World world = TownWorld();
  WorldAside aside = InTown();
  aside.link_open = false;
  CHECK(EntryFactsOf(world, aside).link_closed);
  CHECK(Judge(EntryFactsOf(world, aside)) == EntryRefusal::kLinkClosed);
  // Elle passe avant toute autre raison.
  aside.loading = true;
  CHECK(Judge(EntryFactsOf(world, aside)) == EntryRefusal::kLinkClosed);
}

TEST_CASE("survivors relevé : la veille voit le coup, la mort et la liaison", "[app][survivors]") {
  ro::sim::World world = TownWorld();
  RealWorldWatch watch;
  watch.Start(RealFactsOf(world, InTown(), 0));
  CHECK(watch.Observe(RealFactsOf(world, InTown(), 0)).alerts.empty());

  SpawnMonster(world, 1, ro::sim::Posture::kStanding);
  REQUIRE(world.RecordStrike(
      ro::sim::Strike{.attacker = kMonsterId, .target = kPlayerId, .damage = kBlowDamage}));
  CHECK(watch.Observe(RealFactsOf(world, InTown(), 0)).banner() == RealWorldAlert::kHit);

  WorldAside lost = InTown();
  lost.link_open = false;
  CHECK(watch.Observe(RealFactsOf(world, lost, 0)).link_lost());

  WorldAside invited = InTown();
  invited.party_invited = true;
  RealWorldWatch fresh;
  fresh.Start(RealFactsOf(world, InTown(), 0));
  CHECK(fresh.Observe(RealFactsOf(world, invited, 0)).banner() == RealWorldAlert::kInvite);
}

TEST_CASE("survivors relevé : une perte de PV sans coup n'alerte pas", "[app][survivors]") {
  // Un équipement qui draine les PV, en ville : ils baissent sans qu'aucun
  // coup n'arrive.
  ro::sim::World world = TownWorld();
  RealWorldWatch watch;
  watch.Start(RealFactsOf(world, InTown(), 0));
  REQUIRE(world.SetHealth(kPlayerId, kHurtHp, kFullHp));
  CHECK(watch.Observe(RealFactsOf(world, InTown(), 0)).alerts.empty());
}

TEST_CASE("survivors relevé : le coup qu'on se porte n'alerte pas", "[app][survivors]") {
  ro::sim::World world = TownWorld();
  RealWorldWatch watch;
  watch.Start(RealFactsOf(world, InTown(), 0));
  REQUIRE(world.RecordStrike(
      ro::sim::Strike{.attacker = kPlayerId, .target = kPlayerId, .damage = kBlowDamage}));
  CHECK(watch.Observe(RealFactsOf(world, InTown(), 0)).alerts.empty());
}

TEST_CASE("survivors relevé : seuls les messages privés reçus se comptent, une fois",
          "[app][survivors]") {
  ro::sim::World world = TownWorld();
  world.RecordWhisper(
      ro::sim::WhisperFacts{.side = ro::sim::WhisperSide::kReceived, .peer_name = "Avant"},
      "dit avant la partie", kNowMs);
  WhisperCounter counter;
  counter.Start(world.chat_lines());
  CHECK(counter.Follow(world.chat_lines()) == 0);

  world.RecordWhisper(
      ro::sim::WhisperFacts{.side = ro::sim::WhisperSide::kReceived, .peer_name = "Ami"}, "coucou",
      kNowMs);
  world.RecordWhisper(
      ro::sim::WhisperFacts{.side = ro::sim::WhisperSide::kSent, .peer_name = "Ami"}, "réponse",
      kNowMs);
  world.RecordChat(ro::sim::ChatKind::kPublic, 0, std::string("parole publique"), kNowMs);
  CHECK(counter.Follow(world.chat_lines()) == 1);
  CHECK(counter.Follow(world.chat_lines()) == 1);

  // Le compteur croît : la veille ne met pas en pause pour lui.
  RealWorldWatch watch;
  watch.Start(RealFactsOf(world, InTown(), 0));
  const moonclient::RealWorldReport report = watch.Observe(RealFactsOf(world, InTown(), 1));
  CHECK(report.new_whispers == 1);
  CHECK_FALSE(report.pauses());
}
