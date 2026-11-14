#include "ro/sim/strike_log.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/world.h"

using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::CellPosition;
using ro::sim::Strike;
using ro::sim::StrikeCursor;
using ro::sim::StrikeLog;
using ro::sim::SupportCursor;
using ro::sim::SupportHit;
using ro::sim::World;

namespace {

constexpr std::uint32_t kAttackerId = 150'001;
constexpr std::uint32_t kTargetId = 150'002;
constexpr std::uint32_t kSameInstantMs = 5'000;

Actor Someone(std::uint32_t id, CellPosition where) {
  Actor actor;
  actor.id = id;
  actor.kind = ActorKind::kPlayer;
  actor.position = where;
  return actor;
}

Strike Blow(std::int32_t damage) {
  return Strike{
      .attacker = kAttackerId, .target = kTargetId, .started_ms = kSameInstantMs, .damage = damage};
}

}  // namespace

TEST_CASE("Coups : deux coups au même instant sur la même cible restent deux", "[sim][strike]") {
  // 🔴 L'EMPLACEMENT `Actor::hit` NE GARDE QUE LE SECOND, et un lecteur qui
  // reconnaissait un coup nouveau à son instant perdait le premier : son
  // nombre, son bruit, son effet.
  World world;
  world.Spawn(Someone(kAttackerId, CellPosition{100, 100, 0}));
  world.Spawn(Someone(kTargetId, CellPosition{101, 100, 0}));
  REQUIRE(world.RecordStrike(Blow(12)));
  REQUIRE(world.RecordStrike(Blow(34)));

  StrikeCursor cursor;
  const auto fresh = cursor.Take(world.strikes());
  REQUIRE(fresh.size() == 2);
  CHECK(fresh[0].strike.damage == 12);
  CHECK(fresh[1].strike.damage == 34);
  CHECK(fresh[0].serial < fresh[1].serial);
  // Déjà vus : l'image suivante ne les rejoue pas.
  CHECK(cursor.Take(world.strikes()).empty());
}

TEST_CASE("Coups : un coup sans personne pour le porter ni le recevoir n'est pas inscrit",
          "[sim][strike]") {
  World world;
  CHECK_FALSE(world.RecordStrike(Blow(12)));
  StrikeCursor cursor;
  CHECK(cursor.Take(world.strikes()).empty());
}

TEST_CASE("Coups : chaque lecteur a son curseur", "[sim][strike]") {
  StrikeLog log;
  log.Record(Blow(1));
  StrikeCursor sounds;
  StrikeCursor numbers;
  CHECK(sounds.Take(log).size() == 1);
  log.Record(Blow(2));
  CHECK(sounds.Take(log).size() == 1);
  CHECK(numbers.Take(log).size() == 2);
}

TEST_CASE("Coups : le journal borné garde les plus récents", "[sim][strike]") {
  constexpr int kManyStrikes = 2'000;
  StrikeLog log;
  StrikeCursor cursor;
  for (int count = 0; count < kManyStrikes; ++count) {
    log.Record(Blow(count));
  }
  const auto fresh = cursor.Take(log);
  REQUIRE_FALSE(fresh.empty());
  CHECK(fresh.size() < static_cast<std::size_t>(kManyStrikes));
  CHECK(fresh.back().strike.damage == kManyStrikes - 1);
  CHECK(fresh.back().serial == log.last_serial());
}

TEST_CASE("Coups : un journal neuf se relit depuis le début", "[sim][strike]") {
  // Un curseur qui a lu un premier monde ne doit pas rester aveugle au second,
  // dont les numéros recommencent à un.
  StrikeLog first;
  first.Record(Blow(1));
  first.Record(Blow(2));
  StrikeCursor cursor;
  CHECK(cursor.Take(first).size() == 2);
  StrikeLog second;
  second.Record(Blow(3));
  const auto fresh = cursor.Take(second);
  REQUIRE(fresh.size() == 1);
  CHECK(fresh[0].strike.damage == 3);
}

TEST_CASE("Coups : l'entrée en carte vide le journal sans renuméroter", "[sim][strike]") {
  World world;
  world.Spawn(Someone(kAttackerId, CellPosition{100, 100, 0}));
  world.Spawn(Someone(kTargetId, CellPosition{101, 100, 0}));
  REQUIRE(world.RecordStrike(Blow(12)));
  StrikeCursor cursor;
  CHECK(cursor.Take(world.strikes()).size() == 1);
  world.EnterMap("prontera", 150, 150);
  CHECK(world.strikes().Since(0).empty());
  CHECK(world.strikes().last_serial() == 1);
}

TEST_CASE("Soins : l'annonce et le soin qui la suit restent deux", "[sim][strike]") {
  // 🔴 POTION PITCHER : le serveur annonce la compétence depuis son lanceur,
  // puis, dans la même trame, un Heal SANS lanceur sur la même cible.
  // `Actor::support` ne garde que le second ; lu seul, il faisait perdre la
  // potion lancée.
  constexpr std::uint16_t kPotionPitcher = 231;
  constexpr std::uint16_t kHeal = 28;
  World world;
  world.Spawn(Someone(kAttackerId, CellPosition{100, 100, 0}));
  world.Spawn(Someone(kTargetId, CellPosition{101, 100, 0}));
  REQUIRE(world.RecordSupport(kTargetId, SupportHit{.skill_id = kPotionPitcher,
                                                    .source_id = kAttackerId,
                                                    .amount = 1,
                                                    .succeeded = true,
                                                    .started_ms = kSameInstantMs}));
  REQUIRE(world.RecordSupport(kTargetId, SupportHit{.skill_id = kHeal,
                                                    .source_id = 0,
                                                    .amount = 1994,
                                                    .succeeded = true,
                                                    .started_ms = kSameInstantMs}));

  SupportCursor cursor;
  const auto fresh = cursor.Take(world.supports());
  REQUIRE(fresh.size() == 2);
  CHECK(fresh[0].support.skill_id == kPotionPitcher);
  CHECK(fresh[0].target_id == kTargetId);
  CHECK(fresh[1].support.skill_id == kHeal);
  CHECK(cursor.Take(world.supports()).empty());
}
