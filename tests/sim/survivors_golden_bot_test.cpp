#include "support/survivors_golden_bot.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_journal.h"
#include "ro/sim/survivors_play.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/survivors_trophies.h"

#include "support/survivors_fixture.h"
#include "support/survivors_golden.h"
#include "support/survivors_golden_loaded.h"
#include "support/survivors_golden_terrain.h"

using ro::sim::MinigameTrophy;
using ro::sim::SurvivorsOutcome;
using ro::sim::SurvivorsPlay;
using ro::test_support::GoldenLoadedStart;
using ro::test_support::GoldenOwned;
using ro::test_support::GoldenRules;
using ro::test_support::GoldenTerrainStart;
using ro::test_support::kGoldenArenaCells;
using ro::test_support::kGoldenBotCards;
using ro::test_support::kGoldenBotHeroPosition;
using ro::test_support::kGoldenBotOwnedCount;
using ro::test_support::kGoldenBotRulesetVersion;
using ro::test_support::kGoldenBotSkills;
using ro::test_support::kGoldenBotTally;
using ro::test_support::kGoldenLoadedCards;
using ro::test_support::kGoldenLoadedCardsDropped;
using ro::test_support::kGoldenLoadedCardsFound;
using ro::test_support::kGoldenLoadedChills;
using ro::test_support::kGoldenLoadedHeroPosition;
using ro::test_support::kGoldenLoadedLivesLeft;
using ro::test_support::kGoldenLoadedRulesetVersion;
using ro::test_support::kGoldenLoadedSkills;
using ro::test_support::kGoldenLoadedTally;
using ro::test_support::kGoldenSeed;
using ro::test_support::PillarArena;
using ro::test_support::PlayGoldenBot;
using ro::test_support::PlayGoldenLoaded;
using ro::test_support::PlayGoldenTerrain;

namespace {

// Ce que le héros possède, dans l'ordre des emplacements, complété d'entrées
// vides jusqu'à la taille de l'épingle.
template <typename Owned, typename IdOf>
[[nodiscard]] std::array<GoldenOwned, kGoldenBotOwnedCount> OwnedPins(
    const std::vector<Owned>& owned, IdOf id_of) {
  std::array<GoldenOwned, kGoldenBotOwnedCount> pinned{};
  for (std::size_t index = 0; index < pinned.size() && index < owned.size(); ++index) {
    pinned[index] = GoldenOwned{.id = id_of(owned[index]), .level = owned[index].level};
  }
  return pinned;
}

}  // namespace

// L'étage 3 du mode : une partie entière jouée par le robot, de quinze à
// vingt minutes de jeu. Cachée de ctest par son coût ; lancée en Release sous
// Windows et dans l'intégration par `ro_tests "[.golden]"`.
TEST_CASE("Or long survivors : le robot joue la partie entière, bilan épinglé",
          "[sim][survivors][.golden]") {
  REQUIRE(GoldenRules().ruleset_version == kGoldenBotRulesetVersion);
  const std::optional<SurvivorsPlay> play = PlayGoldenBot(kGoldenSeed);
  REQUIRE(play.has_value());
  const ro::sim::SurvivorsField& field = play->run().field();
  // Le départ neutre : la méta n'a rien déplacé de ce que le classement juge.
  REQUIRE(ro::sim::IsNeutralStart(field.start));
  CHECK(field.tally.outcome != SurvivorsOutcome::kRunning);
  CHECK(field.tally == kGoldenBotTally);
  CHECK(field.hero.position == kGoldenBotHeroPosition);
  CHECK(OwnedPins(field.hero.skills, [](const ro::sim::OwnedSkill& skill) {
          return skill.skill_id;
        }) == kGoldenBotSkills);
  CHECK(OwnedPins(field.hero.cards, [](const ro::sim::OwnedCard& card) {
          return card.card_item_id;
        }) == kGoldenBotCards);

  // LES TROPHÉES DE LA PARTIE : cinq minutes, une évolution, mille monstres,
  // le niveau 40, une carte et Water Ball au niveau maximum (Déluge) ; ni la
  // victoire, ni mille gels : le robot tombe au niveau 44.
  ro::sim::MinigameRecord record;
  CHECK(ro::sim::RecordSurvivors(record, play->run()) ==
        std::vector<MinigameTrophy>{
            MinigameTrophy::kSurvivorsFiveMinutes, MinigameTrophy::kSurvivorsFirstEvolution,
            MinigameTrophy::kSurvivorsKills1000, MinigameTrophy::kSurvivorsLevel40,
            MinigameTrophy::kSurvivorsFirstCard, MinigameTrophy::kSurvivorsWaterBall});

  // Le journal du robot se rejoue au même bilan : le robot ne contourne pas
  // le journal.
  const auto replayed =
      ro::sim::ReplaySurvivors(GoldenRules(), PillarArena(kGoldenArenaCells), play->journal());
  REQUIRE(replayed.has_value());
  CHECK(*replayed == field.tally);
}

// La seconde partie en or : la même, à départ chargé de toute la méta. Cachée
// de ctest comme la première, pour la même raison.
TEST_CASE("Or long survivors : à départ chargé, la méta joue sa part, bilan épinglé",
          "[sim][survivors][.golden]") {
  REQUIRE(GoldenRules().ruleset_version == kGoldenLoadedRulesetVersion);
  const ro::sim::SurvivorsStart start = GoldenLoadedStart(kGoldenSeed);
  REQUIRE_FALSE(ro::sim::IsNeutralStart(start));
  const std::optional<SurvivorsPlay> play = PlayGoldenLoaded(kGoldenSeed);
  REQUIRE(play.has_value());
  const ro::sim::SurvivorsField& field = play->run().field();
  CHECK(field.start == start);
  CHECK(field.tally == kGoldenLoadedTally);
  CHECK(field.hero.position == kGoldenLoadedHeroPosition);
  CHECK(field.lives_left == kGoldenLoadedLivesLeft);
  CHECK(field.next_card_serial - ro::sim::kFirstCardSerial == kGoldenLoadedCardsDropped);
  CHECK(std::equal(field.recap.cards_found.begin(), field.recap.cards_found.end(),
                   kGoldenLoadedCardsFound.begin(), kGoldenLoadedCardsFound.end()));
  CHECK(field.recap.chills == kGoldenLoadedChills);
  CHECK(OwnedPins(field.hero.skills, [](const ro::sim::OwnedSkill& skill) {
          return skill.skill_id;
        }) == kGoldenLoadedSkills);
  CHECK(OwnedPins(field.hero.cards, [](const ro::sim::OwnedCard& card) {
          return card.card_item_id;
        }) == kGoldenLoadedCards);

  // Le journal v2 porte tout le départ : encodé, décodé, rejoué, même bilan.
  const auto decoded =
      ro::sim::DecodeSurvivorsJournal(ro::sim::EncodeSurvivorsJournal(play->journal()));
  REQUIRE(decoded.has_value());
  CHECK(decoded->start == start);
  const auto replayed =
      ro::sim::ReplaySurvivors(GoldenRules(), PillarArena(kGoldenArenaCells), *decoded);
  REQUIRE(replayed.has_value());
  CHECK(*replayed == field.tally);
}

// La troisième partie en or : la graine en or sur l'arène générée du
// catalogue, à départ neutre. Elle épingle le terrain, les plantes au fil des
// tués et la horde sans bord ; son journal se rejoue sans carte. Cachée de
// ctest comme les deux autres.
TEST_CASE("Or long survivors : sur le sol généré, le robot joue la partie entière, bilan épinglé",
          "[sim][survivors][.golden]") {
  namespace ts = ro::test_support;
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  REQUIRE(rules.ruleset_version == ts::kGoldenTerrainRulesetVersion);
  const std::optional<SurvivorsPlay> play = PlayGoldenTerrain(kGoldenSeed);
  REQUIRE(play.has_value());
  const ro::sim::SurvivorsField& field = play->run().field();
  REQUIRE(field.start == GoldenTerrainStart(kGoldenSeed));
  REQUIRE_FALSE(field.ground.bounded());
  CHECK(field.tally.outcome != SurvivorsOutcome::kRunning);
  CHECK(field.tally == ts::kGoldenTerrainTally);
  CHECK(field.hero.position == ts::kGoldenTerrainHeroPosition);
  CHECK(field.plants.size() == ts::kGoldenTerrainPlants);
  CHECK(field.plant_kills_mark == ts::kGoldenTerrainPlantKillsMark);
  CHECK(field.plants_broken == ts::kGoldenTerrainPlantsBroken);
  CHECK(field.next_serial == ts::kGoldenTerrainNextSerial);
  CHECK(OwnedPins(field.hero.skills, [](const ro::sim::OwnedSkill& skill) {
          return skill.skill_id;
        }) == ts::kGoldenTerrainSkills);
  CHECK(OwnedPins(field.hero.cards, [](const ro::sim::OwnedCard& card) {
          return card.card_item_id;
        }) == ts::kGoldenTerrainCards);

  // Le journal se rejoue sans carte ni installation, au même bilan.
  const auto replayed = ro::sim::ReplaySurvivors(rules, play->journal());
  REQUIRE(replayed.has_value());
  CHECK(*replayed == field.tally);
}
