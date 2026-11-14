// LE COMPTEUR DE DÉGÂTS DANS L'ARÈNE : les coups du héros s'y comptent comme
// ceux du personnage, au nom du skill, de son niveau et du monstre touché ; ce
// qu'il encaisse va aux dégâts subis.

#include "survivors/arena_meter.h"

#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/damage_meter.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_run.h"

namespace {

constexpr std::uint32_t kSelfId = 2000001;
constexpr std::uint16_t kFireBoltSkill = 19;
constexpr std::uint16_t kPoringId = 1002;
constexpr std::uint16_t kLunaticId = 1063;
constexpr std::uint32_t kNowMs = 50000;

[[nodiscard]] std::string NameOf(std::uint16_t foe_id) {
  return foe_id == kPoringId ? "Poring" : foe_id == kLunaticId ? "Lunatic" : "?";
}

}  // namespace

TEST_CASE("Compteur de l'arène : les coups du héros comptent comme les nôtres",
          "[app][survivors]") {
  ro::sim::DamageMeter meter;
  meter.Track(kSelfId, "Héros", {});
  ro::sim::SurvivorsField field;
  field.hero.skills.push_back(ro::sim::OwnedSkill{.skill_id = kFireBoltSkill, .level = 4});
  field.foes.push_back(ro::sim::SurvivorsFoe{.serial = 7, .foe_id = kPoringId});

  ro::sim::SurvivorsNews news;
  news.hits.push_back(
      ro::sim::SurvivorsHit{.foe_serial = 7, .amount = 120, .skill_id = kFireBoltSkill});
  // Le Lunatic est tombé sous ce coup : sa chute le nomme.
  news.hits.push_back(ro::sim::SurvivorsHit{.foe_serial = 9, .amount = 300, .skill_id = 0});
  news.falls.push_back(ro::sim::SurvivorsFall{.foe_serial = 9, .foe_id = kLunaticId});
  news.hurt_by_hero[0] = 45;
  // Ce qu'un allié porte ou encaisse n'est pas à ce joueur.
  news.hits.push_back(ro::sim::SurvivorsHit{.foe_serial = 7, .amount = 999, .hero_index = 1});
  news.hurt_by_hero[1] = 77;

  moonclient::RecordArenaBlows(news, field, NameOf, meter, kNowMs);

  CHECK(meter.total(ro::sim::MeterMode::kDamageDone) == 420);
  CHECK(meter.total(ro::sim::MeterMode::kDamageTaken) == 45);
  CHECK(meter.active(ro::sim::MeterMode::kDamageDone));

  const auto records = meter.records(ro::sim::MeterMode::kDamageDone);
  REQUIRE(records.size() == 2U);
  CHECK(records[0].blow.amount == 300);
  CHECK(records[0].blow.other_name == "Lunatic");
  CHECK(records[0].blow.skill_level == 0);
  CHECK(records[1].blow.other_name == "Poring");
  CHECK(records[1].blow.skill_id == kFireBoltSkill);
  CHECK(records[1].blow.skill_level == 4);

  const auto& by_actor = meter.current().of(ro::sim::MeterMode::kDamageDone);
  REQUIRE(by_actor.contains(kSelfId));
  CHECK(by_actor.at(kSelfId).total.amount == 420);
}

TEST_CASE("Compteur de l'arène : une image sans coup ne compte rien", "[app][survivors]") {
  ro::sim::DamageMeter meter;
  meter.Track(kSelfId, "Héros", {});
  moonclient::RecordArenaBlows(ro::sim::SurvivorsNews{}, ro::sim::SurvivorsField{}, NameOf, meter,
                               kNowMs);
  CHECK(meter.total(ro::sim::MeterMode::kDamageDone) == 0);
  CHECK(meter.total(ro::sim::MeterMode::kDamageTaken) == 0);
  CHECK_FALSE(meter.active(ro::sim::MeterMode::kDamageDone));
}
