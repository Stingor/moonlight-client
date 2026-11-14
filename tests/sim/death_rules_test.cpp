// CE QUE LA MORT FAIT D'UN ACTEUR : un PNJ reste debout, une créature laisse un
// cadavre, un corps de joueur reste à terre jusqu'à ce que le serveur l'enlève.

#include "ro/sim/death_rules.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/actor.h"

using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::DeathAftermath;
using ro::sim::DeathAftermathOf;

namespace {

constexpr std::uint8_t kObjectPlayer = 0;
constexpr std::uint8_t kObjectMonster = 5;
constexpr std::uint8_t kObjectHomunculus = 8;
constexpr std::uint32_t kPoringJob = 1002;
constexpr std::uint32_t kKafraJob = 113;
constexpr std::uint32_t kLifJob = 6001;
constexpr std::uint32_t kKnightJob = 7;
constexpr std::uint32_t kExtendedMonsterJob = 20021;
constexpr std::uint32_t kLastPlayerJobBeforeBodies = 31;

[[nodiscard]] Actor ActorOf(ActorKind kind, std::uint8_t object_type, std::uint32_t job) {
  Actor actor;
  actor.kind = kind;
  actor.object_type = object_type;
  actor.look.job = static_cast<std::uint16_t>(job);
  return actor;
}

}  // namespace

TEST_CASE("Mort : une créature laisse un cadavre", "[sim][death]") {
  CHECK(DeathAftermathOf(ActorOf(ActorKind::kMonster, kObjectMonster, kPoringJob)) ==
        DeathAftermath::kLeavesCorpse);
  CHECK(DeathAftermathOf(ActorOf(ActorKind::kMonster, kObjectMonster, kExtendedMonsterJob)) ==
        DeathAftermath::kLeavesCorpse);
  CHECK(DeathAftermathOf(ActorOf(ActorKind::kMonster, kObjectHomunculus, kLifJob)) ==
        DeathAftermath::kLeavesCorpse);
  // Tout ce que le paquet annonce mercenaire, quelle que soit sa classe.
  CHECK(DeathAftermathOf(ActorOf(ActorKind::kMonster, ro::sim::kObjectTypeMercenary, kKnightJob)) ==
        DeathAftermath::kLeavesCorpse);
}

TEST_CASE("Mort : un PNJ reste debout", "[sim][death]") {
  // Annoncé PNJ, quelle que soit sa classe…
  CHECK(DeathAftermathOf(ActorOf(ActorKind::kNonPlayer, ro::sim::kObjectTypeNonPlayer,
                                 kPoringJob)) == DeathAftermath::kStandsUp);
  CHECK(DeathAftermathOf(ActorOf(ActorKind::kNonPlayer, ro::sim::kObjectTypeNonPlayerEvent,
                                 kPoringJob)) == DeathAftermath::kStandsUp);
  // …ou vêtu d'une classe de PNJ, quel que soit son type.
  CHECK(DeathAftermathOf(ActorOf(ActorKind::kMonster, kObjectMonster, kKafraJob)) ==
        DeathAftermath::kStandsUp);
  CHECK(DeathAftermathOf(ActorOf(ActorKind::kMonster, kObjectMonster,
                                 ro::sim::kFirstExtraNonPlayerJob)) == DeathAftermath::kStandsUp);
}

TEST_CASE("Mort : deux PNJ meurent comme des créatures", "[sim][death]") {
  // Le PNJ au sprite de monstre, et une classe de PNJ à part.
  CHECK(DeathAftermathOf(ActorOf(ActorKind::kNonPlayer, ro::sim::kObjectTypeMonsterLookingNonPlayer,
                                 kPoringJob)) == DeathAftermath::kLeavesCorpse);
  CHECK(DeathAftermathOf(ActorOf(ActorKind::kNonPlayer, ro::sim::kObjectTypeNonPlayer,
                                 ro::sim::kDyingNonPlayerJob)) == DeathAftermath::kLeavesCorpse);
}

TEST_CASE("Mort : un corps de joueur reste à terre", "[sim][death]") {
  // Un joueur, et un monstre que le serveur annonce avec un corps de joueur :
  // c'est lui qui les enlève.
  CHECK(DeathAftermathOf(ActorOf(ActorKind::kPlayer, kObjectPlayer, kKnightJob)) ==
        DeathAftermath::kStaysDown);
  CHECK(DeathAftermathOf(ActorOf(ActorKind::kPlayer, kObjectPlayer, kPoringJob)) ==
        DeathAftermath::kStaysDown);
  // Une créature au métier de joueur ne laisse pas de cadavre non plus.
  CHECK(DeathAftermathOf(ActorOf(ActorKind::kMonster, kObjectMonster,
                                 kLastPlayerJobBeforeBodies)) == DeathAftermath::kStaysDown);
}
