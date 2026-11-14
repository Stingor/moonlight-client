// LE BOUCLIER DE KYRIE AU LIEU DU « MISS » : un monstre qui rate le joueur
// pendant que Kyrie Eleison ou Parrying le protège fait naître l'effet 336 sur
// le joueur, et aucun mot ne paraît.

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/damage_mark.h"
#include "ro/sim/actor.h"
#include "ro/sim/combat_notice.h"

#include "world/hit_popup_look.h"
#include "world/hit_popup_rules.h"

namespace {

using moonclient::CarriesMissGuard;
using moonclient::HitPopupLook;
using moonclient::MissShowsGuardShield;
using moonclient::PlanStrike;
using moonclient::StrikePlan;
using moonclient::StrikeSides;

constexpr ro::sim::StatusEffectId kKyrie = 19;
constexpr ro::sim::StatusEffectId kParrying = 104;
constexpr ro::sim::StatusEffectId kTwoHandQuicken = 2;
constexpr std::uint8_t kLuckyDodgeType = 11;
constexpr std::int32_t kSomeDamage = 120;
// Un Poring, un chevalier, un homoncule, et un PNJ.
constexpr std::uint32_t kPoringClass = 1002;
constexpr std::uint32_t kKnightClass = 7;
constexpr std::uint32_t kHomunculusClass = 6001;
constexpr std::uint32_t kNpcClass = 111;

// Un monstre me frappe, et je porte l'un des deux états.
constexpr StrikeSides kMonsterOnGuardedMe{.target_is_player = true,
                                          .target_is_self = true,
                                          .attacker_is_known = true,
                                          .attacker_job = kPoringClass,
                                          .target_carries_miss_guard = true};

ro::sim::Strike Blow(std::int32_t damage, std::uint8_t type = 0) {
  ro::sim::Strike strike;
  strike.damage = damage;
  strike.type = type;
  strike.hits = 1;
  return strike;
}

}  // namespace

TEST_CASE("Kyrie : un monstre qui me rate montre le bouclier, pas le « Miss »",
          "[app][degats][kyrie]") {
  const ro::sim::Strike absorbed = Blow(0);
  CHECK(MissShowsGuardShield(absorbed, kMonsterOnGuardedMe));
  const StrikePlan plan = PlanStrike(absorbed, kMonsterOnGuardedMe, HitPopupLook{});
  CHECK_FALSE(plan.shown);
  CHECK(moonclient::kMissGuardShieldEffect == 336);
}

TEST_CASE("Kyrie : le « Miss » reste hors des quatre conditions", "[app][degats][kyrie]") {
  // Sans l'état : le mot, blanc, sur le monstre.
  StrikeSides unguarded = kMonsterOnGuardedMe;
  unguarded.target_carries_miss_guard = false;
  CHECK_FALSE(MissShowsGuardShield(Blow(0), unguarded));
  const StrikePlan miss = PlanStrike(Blow(0), unguarded, HitPopupLook{});
  REQUIRE(miss.mark.has_value());
  CHECK(*miss.mark == ro::render::DamageMark::kMiss);

  // Un JOUEUR qui rate : le client ne consulte pas les états, le mot reste. Un
  // homoncule non plus n'est pas une créature pour cette règle ; un PNJ l'est.
  StrikeSides by_player = kMonsterOnGuardedMe;
  by_player.attacker_is_player = true;
  by_player.attacker_job = kKnightClass;
  CHECK_FALSE(MissShowsGuardShield(Blow(0), by_player));
  StrikeSides by_homunculus = kMonsterOnGuardedMe;
  by_homunculus.attacker_job = kHomunculusClass;
  CHECK_FALSE(MissShowsGuardShield(Blow(0), by_homunculus));
  StrikeSides by_npc = kMonsterOnGuardedMe;
  by_npc.attacker_job = kNpcClass;
  CHECK(MissShowsGuardShield(Blow(0), by_npc));

  // Un coup qui entame, et une esquive chanceuse, ne sont pas des ratés.
  CHECK_FALSE(MissShowsGuardShield(Blow(kSomeDamage), kMonsterOnGuardedMe));
  CHECK_FALSE(MissShowsGuardShield(Blow(0, kLuckyDodgeType), kMonsterOnGuardedMe));

  // Sans l'acteur de l'attaquant, rien ne paraît du tout.
  StrikeSides unknown = kMonsterOnGuardedMe;
  unknown.attacker_is_known = false;
  CHECK_FALSE(MissShowsGuardShield(Blow(0), unknown));

  // Et seulement pour le joueur : le client ne connaît que ses propres états.
  StrikeSides other = kMonsterOnGuardedMe;
  other.target_is_self = false;
  CHECK_FALSE(MissShowsGuardShield(Blow(0), other));
}

TEST_CASE("Kyrie : Kyrie Eleison et Parrying changent le « Miss », pas les autres états",
          "[app][degats][kyrie]") {
  ro::sim::Actor me;
  CHECK_FALSE(CarriesMissGuard(me));
  me.status_effects.insert(kTwoHandQuicken);
  CHECK_FALSE(CarriesMissGuard(me));
  me.status_effects.insert(kKyrie);
  CHECK(CarriesMissGuard(me));
  me.status_effects = {kParrying};
  CHECK(CarriesMissGuard(me));
}
