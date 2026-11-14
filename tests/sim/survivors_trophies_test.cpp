// LES TROPHÉES DU MODE SURVIVORS : chaque règle à son seuil exact, une fois
// chacun, et la place des trophées dans le catalogue des mini-jeux.

#include "ro/sim/survivors_trophies.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/minigame_trophies.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

#include "support/survivors_fixture.h"
#include "support/survivors_golden.h"

using ro::sim::MinigameRecord;
using ro::sim::MinigameTrophy;
using ro::sim::OwnedCard;
using ro::sim::OwnedSkill;
using ro::sim::SurvivorsField;
using ro::sim::SurvivorsOutcome;
using ro::sim::SurvivorsRun;
using ro::test_support::kTinyCardId;
using ro::test_support::kTinyHomingId;
using ro::test_support::kTinyOrbitId;
using ro::test_support::kTinyStrikeId;
using ro::test_support::PillarArena;
using ro::test_support::QuietRules;
using ro::test_support::TinyStart;

namespace {

constexpr std::uint64_t kSeed = 7;
constexpr std::int32_t kArenaSideCells = 48;
// Les seuils des trophées, tels que la table de la conception les nomme.
constexpr std::uint32_t kFiveMinutesSteps = 300000 / ro::sim::kSurvivorsStepMs;
constexpr std::uint32_t kThousandKills = 1000;
constexpr std::uint16_t kLevelForty = 40;

// Le premier rang des trophées du mode, et leurs clés de fichier dans l'ordre.
constexpr std::size_t kFirstSurvivorsRank = 8;
constexpr std::array<std::string_view, 21> kSurvivorsKeys = {"survivors_5min",
                                                             "survivors_victory",
                                                             "survivors_first_evolution",
                                                             "survivors_kills_1000",
                                                             "survivors_level_40",
                                                             "survivors_first_card",
                                                             "survivors_pyromaniac",
                                                             "survivors_pure_mind",
                                                             "survivors_storm_gust",
                                                             "survivors_jupitel_thunder",
                                                             "survivors_lord_of_vermilion",
                                                             "survivors_heavens_drive",
                                                             "survivors_napalm_vulcan",
                                                             "survivors_grand_wizard",
                                                             "survivors_deep_freeze",
                                                             "survivors_blind_victory",
                                                             "survivors_fire_ball",
                                                             "survivors_water_ball",
                                                             "survivors_ice_wall",
                                                             "survivors_crimson_rock",
                                                             "survivors_jack_frost"};
// Les évolués des règles livrées, et cent gels.
constexpr std::array<std::uint16_t, 11> kShippedEvolved{83, 81, 80,  88,   89,  84,
                                                        85, 91, 400, 2211, 2204};
constexpr std::uint64_t kManyFreezes = 1000;

// Les trophées du Chevalier, et ses évolués dans le même ordre ; son dernier rang est
// « Grand chevalier ».
constexpr std::size_t kFirstKnightRank = 36;
constexpr std::array<std::string_view, 8> kKnightKeys = {
    "survivors_head_crush",     "survivors_spiral_pierce", "survivors_joint_beat",
    "survivors_aura_blade",     "survivors_concentration", "survivors_frenzy",
    "survivors_ignition_break", "survivors_grand_knight"};
struct EvolvedOfClass {
  std::uint16_t skill_id;
  MinigameTrophy trophy;
};
constexpr std::array<EvolvedOfClass, 7> kKnightEvolved{{
    {.skill_id = 398, .trophy = MinigameTrophy::kSurvivorsHeadCrush},
    {.skill_id = 397, .trophy = MinigameTrophy::kSurvivorsSpiralPierce},
    {.skill_id = 399, .trophy = MinigameTrophy::kSurvivorsJointBeat},
    {.skill_id = 355, .trophy = MinigameTrophy::kSurvivorsAuraBlade},
    {.skill_id = 357, .trophy = MinigameTrophy::kSurvivorsConcentration},
    {.skill_id = 359, .trophy = MinigameTrophy::kSurvivorsFrenzy},
    {.skill_id = 2006, .trophy = MinigameTrophy::kSurvivorsIgnitionBreak},
}};
constexpr std::uint16_t kMagnumBreakSkillId = 7;

// Les trophées du Voleur, à la fin du catalogue, et ses évolués dans le même ordre ;
// le dernier rang est « Grand voleur ».
constexpr std::size_t kFirstThiefRank = 44;
constexpr std::array<std::string_view, 8> kThiefKeys = {
    "survivors_sonic_blow",     "survivors_soul_destroyer",
    "survivors_venom_splasher", "survivors_enchant_deadly_poison",
    "survivors_cloaking",       "survivors_throw_venom_knife",
    "survivors_meteor_assault", "survivors_grand_thief"};
constexpr std::array<EvolvedOfClass, 7> kThiefEvolved{{
    {.skill_id = 136, .trophy = MinigameTrophy::kSurvivorsSonicBlow},
    {.skill_id = 379, .trophy = MinigameTrophy::kSurvivorsSoulDestroyer},
    {.skill_id = 141, .trophy = MinigameTrophy::kSurvivorsVenomSplasher},
    {.skill_id = 378, .trophy = MinigameTrophy::kSurvivorsEnchantDeadlyPoison},
    {.skill_id = 135, .trophy = MinigameTrophy::kSurvivorsCloaking},
    {.skill_id = 1004, .trophy = MinigameTrophy::kSurvivorsThrowVenomKnife},
    {.skill_id = 406, .trophy = MinigameTrophy::kSurvivorsMeteorAssault},
}};

// Les trophées de l'Archer, après ceux du Voleur, et ses évolués dans le même
// ordre ; le dernier rang est « Grand archer ».
constexpr std::size_t kFirstArcherRank = 52;
constexpr std::array<std::string_view, 8> kArcherKeys = {
    "survivors_beast_strafing",   "survivors_sharp_shooting", "survivors_falcon_assault",
    "survivors_true_sight",       "survivors_claymore_trap",  "survivors_blast_mine",
    "survivors_phantasmic_arrow", "survivors_grand_archer"};
constexpr std::array<EvolvedOfClass, 7> kArcherEvolved{{
    {.skill_id = 499, .trophy = MinigameTrophy::kSurvivorsBeastStrafing},
    {.skill_id = 382, .trophy = MinigameTrophy::kSurvivorsSharpShooting},
    {.skill_id = 381, .trophy = MinigameTrophy::kSurvivorsFalconAssault},
    {.skill_id = 380, .trophy = MinigameTrophy::kSurvivorsTrueSight},
    {.skill_id = 123, .trophy = MinigameTrophy::kSurvivorsClaymoreTrap},
    {.skill_id = 122, .trophy = MinigameTrophy::kSurvivorsBlastMine},
    {.skill_id = 1009, .trophy = MinigameTrophy::kSurvivorsPhantasmicArrow},
}};

// Les boss finals des deux arènes livrées.
constexpr std::array<ro::sim::BossRule, 1> kBaphometFight{{{.foe_id = 1039, .final = true}}};
constexpr std::array<ro::sim::BossRule, 1> kPharaohFight{{{.foe_id = 1157, .final = true}}};

[[nodiscard]] SurvivorsRun Fresh() {
  auto run = SurvivorsRun::Start(QuietRules(), PillarArena(kArenaSideCells), TinyStart(kSeed));
  REQUIRE(run.has_value());
  return std::move(*run);
}

// Les trophées que gagne une partie neuve retouchée par `touch`.
template <typename Touch>
[[nodiscard]] std::vector<MinigameTrophy> EarnedBy(Touch touch) {
  SurvivorsRun run = Fresh();
  touch(run.field_for_tests());
  MinigameRecord record;
  return RecordSurvivors(record, run);
}

using Trophies = std::vector<MinigameTrophy>;

// Une victoire sur Baphomet : le skill de départ (Fire) seul, sans carte.
void Win(SurvivorsField& field) {
  field.tally.outcome = SurvivorsOutcome::kVictory;
  field.content.bosses = kBaphometFight;
}

[[nodiscard]] bool Holds(const Trophies& earned, MinigameTrophy trophy) {
  return std::ranges::find(earned, trophy) != earned.end();
}

// Sous les règles livrées, le héros en scène devient celui de la classe `class_key`.
void PlayClass(SurvivorsField& field, std::string_view class_key) {
  field.rules = &ro::sim::ShippedSurvivorsRules();
  const auto found =
      std::ranges::find_if(field.rules->classes, [class_key](const auto& hero_class) {
        return std::string_view(hero_class.key) == class_key;
      });
  REQUIRE(found != field.rules->classes.end());
  field.hero_class = &*found;
}

void PlayKnight(SurvivorsField& field) { PlayClass(field, "knight"); }

void PlayThief(SurvivorsField& field) { PlayClass(field, "thief"); }

void PlayArcher(SurvivorsField& field) { PlayClass(field, "archer"); }

}  // namespace

TEST_CASE("Trophées survivors : à leurs rangs, sous leurs clés", "[sim][survivors]") {
  const auto catalog = ro::sim::MinigameTrophies();
  // Roggle prolonge le catalogue après eux.
  REQUIRE(catalog.size() >= kFirstSurvivorsRank + kSurvivorsKeys.size());
  for (std::size_t index = 0; index < kSurvivorsKeys.size(); ++index) {
    const ro::sim::MinigameTrophyText& text = catalog[kFirstSurvivorsRank + index];
    CHECK(std::string_view(text.key) == kSurvivorsKeys[index]);
    CHECK(static_cast<std::size_t>(text.trophy) == kFirstSurvivorsRank + index);
  }
  CHECK(static_cast<std::size_t>(MinigameTrophy::kSurvivorsFiveMinutes) == kFirstSurvivorsRank);
}

TEST_CASE("Trophées survivors : ceux du Chevalier, à leurs rangs", "[sim][survivors]") {
  const auto catalog = ro::sim::MinigameTrophies();
  REQUIRE(catalog.size() >= kFirstKnightRank + kKnightKeys.size());
  for (std::size_t index = 0; index < kKnightKeys.size(); ++index) {
    const ro::sim::MinigameTrophyText& text = catalog[kFirstKnightRank + index];
    CHECK(std::string_view(text.key) == kKnightKeys[index]);
    CHECK(static_cast<std::size_t>(text.trophy) == kFirstKnightRank + index);
  }
}

TEST_CASE("Trophées survivors : ceux du Voleur, puis ceux de l'Archer, ferment le catalogue",
          "[sim][survivors]") {
  CHECK(kFirstKnightRank + kKnightKeys.size() == kFirstThiefRank);
  CHECK(kFirstThiefRank + kThiefKeys.size() == kFirstArcherRank);
  const auto catalog = ro::sim::MinigameTrophies();
  REQUIRE(catalog.size() == kFirstArcherRank + kArcherKeys.size());
  for (std::size_t index = 0; index < kArcherKeys.size(); ++index) {
    const ro::sim::MinigameTrophyText& text = catalog[kFirstArcherRank + index];
    CHECK(std::string_view(text.key) == kArcherKeys[index]);
    CHECK(static_cast<std::size_t>(text.trophy) == kFirstArcherRank + index);
  }
  for (std::size_t index = 0; index < kThiefKeys.size(); ++index) {
    const ro::sim::MinigameTrophyText& text = catalog[kFirstThiefRank + index];
    CHECK(std::string_view(text.key) == kThiefKeys[index]);
    CHECK(static_cast<std::size_t>(text.trophy) == kFirstThiefRank + index);
  }
}

TEST_CASE("Trophées survivors : une partie neuve ne gagne rien", "[sim][survivors]") {
  CHECK(EarnedBy([](SurvivorsField&) {}).empty());
}

TEST_CASE("Trophées survivors : cinq minutes, à la seconde près", "[sim][survivors]") {
  CHECK(EarnedBy([](SurvivorsField& field) { field.step = kFiveMinutesSteps - 1; }).empty());
  CHECK(EarnedBy([](SurvivorsField& field) { field.step = kFiveMinutesSteps; }) ==
        Trophies{MinigameTrophy::kSurvivorsFiveMinutes});
}

TEST_CASE("Trophées survivors : mille monstres, le niveau 40, une évolution", "[sim][survivors]") {
  CHECK(EarnedBy([](SurvivorsField& field) { field.tally.kills = kThousandKills - 1; }).empty());
  CHECK(EarnedBy([](SurvivorsField& field) { field.tally.kills = kThousandKills; }) ==
        Trophies{MinigameTrophy::kSurvivorsKills1000});
  CHECK(EarnedBy([](SurvivorsField& field) { field.tally.level = kLevelForty - 1; }).empty());
  CHECK(EarnedBy([](SurvivorsField& field) { field.tally.level = kLevelForty; }) ==
        Trophies{MinigameTrophy::kSurvivorsLevel40});
  CHECK(EarnedBy([](SurvivorsField& field) { field.tally.evolutions = 1; }) ==
        Trophies{MinigameTrophy::kSurvivorsFirstEvolution});
}

TEST_CASE("Trophées survivors : une carte ramassée au sol, pas une carte prise en offre",
          "[sim][survivors]") {
  CHECK(EarnedBy([](SurvivorsField& field) { field.recap.cards_found.push_back(kTinyCardId); }) ==
        Trophies{MinigameTrophy::kSurvivorsFirstCard});
  CHECK(EarnedBy([](SurvivorsField& field) {
          field.hero.cards.push_back(OwnedCard{.card_item_id = kTinyCardId, .level = 1});
        }).empty());
}

TEST_CASE("Trophées survivors : la victoire, pure et toute de feu", "[sim][survivors]") {
  // Le seul skill de départ est Fire, et aucune carte : les trois à la fois.
  CHECK(EarnedBy(Win) == Trophies{MinigameTrophy::kSurvivorsVictory,
                                  MinigameTrophy::kSurvivorsPyromaniac,
                                  MinigameTrophy::kSurvivorsPureMind});
  // Un second skill Fire ne change rien.
  CHECK(EarnedBy([](SurvivorsField& field) {
          Win(field);
          field.hero.skills.push_back(OwnedSkill{.skill_id = kTinyOrbitId});
        }) == Trophies{MinigameTrophy::kSurvivorsVictory, MinigameTrophy::kSurvivorsPyromaniac,
                       MinigameTrophy::kSurvivorsPureMind});
  // Un skill Ghost ôte le Pyromane.
  CHECK(EarnedBy([](SurvivorsField& field) {
          Win(field);
          field.hero.skills.push_back(OwnedSkill{.skill_id = kTinyHomingId});
        }) == Trophies{MinigameTrophy::kSurvivorsVictory, MinigameTrophy::kSurvivorsPureMind});
  // Une carte possédée ôte l'Esprit pur ; seule une carte ramassée au sol
  // donne la Première carte.
  CHECK(EarnedBy([](SurvivorsField& field) {
          Win(field);
          field.hero.cards.push_back(OwnedCard{.card_item_id = kTinyCardId, .level = 1});
        }) == Trophies{MinigameTrophy::kSurvivorsVictory, MinigameTrophy::kSurvivorsPyromaniac});
  CHECK(EarnedBy([](SurvivorsField& field) {
          Win(field);
          field.recap.cards_found.push_back(kTinyCardId);
        }) == Trophies{MinigameTrophy::kSurvivorsVictory, MinigameTrophy::kSurvivorsFirstCard,
                       MinigameTrophy::kSurvivorsPyromaniac, MinigameTrophy::kSurvivorsPureMind});
}

TEST_CASE("Trophées survivors : sans victoire, ni Pyromane ni Esprit pur", "[sim][survivors]") {
  for (const SurvivorsOutcome outcome :
       {SurvivorsOutcome::kRunning, SurvivorsOutcome::kFell, SurvivorsOutcome::kSurvived,
        SurvivorsOutcome::kAbandoned}) {
    CHECK(EarnedBy([outcome](SurvivorsField& field) { field.tally.outcome = outcome; }).empty());
  }
}

TEST_CASE("Trophées survivors : une partie abandonnée garde ce qu'elle a atteint",
          "[sim][survivors]") {
  SurvivorsRun run = Fresh();
  run.field_for_tests().step = kFiveMinutesSteps;
  run.field_for_tests().tally.kills = kThousandKills;
  run.Abandon();
  REQUIRE(run.tally().outcome == SurvivorsOutcome::kAbandoned);
  MinigameRecord record;
  CHECK(RecordSurvivors(record, run) ==
        Trophies{MinigameTrophy::kSurvivorsFiveMinutes, MinigameTrophy::kSurvivorsKills1000});
}

TEST_CASE("Trophées survivors : un trophée ne se gagne qu'une fois", "[sim][survivors]") {
  SurvivorsRun run = Fresh();
  run.field_for_tests().tally.evolutions = 1;
  MinigameRecord record;
  CHECK(RecordSurvivors(record, run) == Trophies{MinigameTrophy::kSurvivorsFirstEvolution});
  CHECK(record.Has(MinigameTrophy::kSurvivorsFirstEvolution));
  CHECK(RecordSurvivors(record, run).empty());
  // Une partie suivante plus pauvre ne le reprend pas.
  SurvivorsRun poorer = Fresh();
  CHECK(RecordSurvivors(record, poorer).empty());
  CHECK(record.Has(MinigameTrophy::kSurvivorsFirstEvolution));
}

// Elle prend des cartes dans ses offres, mais aucune ne tombe au sol : pas de
// Première carte. Elle tient ses cinq minutes, encore en cours : Cinq minutes
// seul.
TEST_CASE("Trophées survivors : la partie en or courte tient cinq minutes, sans carte au sol",
          "[sim][survivors]") {
  const auto play =
      ro::test_support::PlayGolden(ro::test_support::kGoldenSeed, ro::test_support::kGoldenSteps);
  REQUIRE(play.has_value());
  REQUIRE(play->run().tally() == ro::test_support::kGoldenTally);
  MinigameRecord record;
  CHECK_FALSE(play->run().field().hero.cards.empty());
  CHECK(play->run().field().recap.cards_found.empty());
  CHECK(play->run().tally().outcome == ro::sim::SurvivorsOutcome::kRunning);
  CHECK(RecordSurvivors(record, play->run()) == Trophies{MinigameTrophy::kSurvivorsFiveMinutes});
}

TEST_CASE("Trophées survivors : chacun des cinq évolués neufs a le sien", "[sim][survivors]") {
  struct Evolved {
    std::uint16_t skill_id;
    MinigameTrophy trophy;
  };
  const std::array<Evolved, 5> evolved{{
      {.skill_id = 89, .trophy = MinigameTrophy::kSurvivorsStormGust},
      {.skill_id = 84, .trophy = MinigameTrophy::kSurvivorsJupitelThunder},
      {.skill_id = 85, .trophy = MinigameTrophy::kSurvivorsLordOfVermilion},
      {.skill_id = 91, .trophy = MinigameTrophy::kSurvivorsHeavensDrive},
      {.skill_id = 400, .trophy = MinigameTrophy::kSurvivorsNapalmVulcan},
  }};
  for (const Evolved& one : evolved) {
    CHECK(EarnedBy([&one](SurvivorsField& field) {
            field.hero.skills.push_back(OwnedSkill{.skill_id = one.skill_id});
          }) == Trophies{one.trophy});
  }
}

TEST_CASE("Trophées survivors : Fire Ball, Water Ball, Ice Wall au plus haut niveau",
          "[sim][survivors]") {
  struct Top {
    std::uint16_t skill_id;
    MinigameTrophy trophy;
  };
  const std::array<Top, 3> tops{{
      {.skill_id = 17, .trophy = MinigameTrophy::kSurvivorsFireBall},
      {.skill_id = 86, .trophy = MinigameTrophy::kSurvivorsWaterBall},
      {.skill_id = 87, .trophy = MinigameTrophy::kSurvivorsIceWall},
  }};
  constexpr std::uint8_t kTopLevel = 8;
  for (const Top& top : tops) {
    for (const std::uint8_t level : {std::uint8_t{kTopLevel - 1}, kTopLevel}) {
      SurvivorsRun run = Fresh();
      run.field_for_tests().rules = &ro::sim::ShippedSurvivorsRules();
      run.field_for_tests().hero.skills.push_back(
          OwnedSkill{.skill_id = top.skill_id, .level = level});
      MinigameRecord record;
      const Trophies earned = RecordSurvivors(record, run);
      const bool won = std::ranges::find(earned, top.trophy) != earned.end();
      CHECK(won == (level == kTopLevel));
    }
  }
  // Crimson Rock et Jack Frost, possédés.
  CHECK(EarnedBy([](SurvivorsField& field) {
          field.hero.skills.push_back(OwnedSkill{.skill_id = 2211});
        }) == Trophies{MinigameTrophy::kSurvivorsCrimsonRock});
  CHECK(EarnedBy([](SurvivorsField& field) {
          field.hero.skills.push_back(OwnedSkill{.skill_id = 2204});
        }) == Trophies{MinigameTrophy::kSurvivorsJackFrost});
}

TEST_CASE("Trophées survivors : Grand sorcier, les évolués au fil des parties",
          "[sim][survivors]") {
  MinigameRecord record;
  // Huit évolués dans une partie, le neuvième dans une autre.
  SurvivorsRun first = Fresh();
  first.field_for_tests().rules = &ro::sim::ShippedSurvivorsRules();
  for (std::size_t index = 0; index + 1 < kShippedEvolved.size(); ++index) {
    first.field_for_tests().hero.skills.push_back(OwnedSkill{.skill_id = kShippedEvolved[index]});
  }
  const Trophies earned_first = RecordSurvivors(record, first);
  CHECK(std::ranges::find(earned_first, MinigameTrophy::kSurvivorsGrandWizard) ==
        earned_first.end());
  CHECK(record.survivors_evolved.size() == kShippedEvolved.size() - 1);
  CHECK(std::ranges::is_sorted(record.survivors_evolved));
  SurvivorsRun second = Fresh();
  second.field_for_tests().rules = &ro::sim::ShippedSurvivorsRules();
  second.field_for_tests().hero.skills.push_back(OwnedSkill{.skill_id = kShippedEvolved.back()});
  const Trophies earned_second = RecordSurvivors(record, second);
  CHECK(std::ranges::find(earned_second, MinigameTrophy::kSurvivorsGrandWizard) !=
        earned_second.end());
  CHECK(record.survivors_evolved.size() == kShippedEvolved.size());
}

TEST_CASE("Trophées survivors : chacun des évolués du Chevalier a le sien", "[sim][survivors]") {
  for (const EvolvedOfClass& one : kKnightEvolved) {
    CHECK(EarnedBy([&one](SurvivorsField& field) {
            field.hero.skills.push_back(OwnedSkill{.skill_id = one.skill_id});
          }) == Trophies{one.trophy});
  }
}

TEST_CASE("Trophées survivors : Grand chevalier, ses évolués au fil des parties",
          "[sim][survivors]") {
  MinigameRecord record;
  SurvivorsRun first = Fresh();
  PlayKnight(first.field_for_tests());
  for (std::size_t index = 0; index + 1 < kKnightEvolved.size(); ++index) {
    first.field_for_tests().hero.skills.push_back(
        OwnedSkill{.skill_id = kKnightEvolved[index].skill_id});
  }
  CHECK_FALSE(Holds(RecordSurvivors(record, first), MinigameTrophy::kSurvivorsGrandKnight));
  SurvivorsRun second = Fresh();
  PlayKnight(second.field_for_tests());
  second.field_for_tests().hero.skills.push_back(
      OwnedSkill{.skill_id = kKnightEvolved.back().skill_id});
  const Trophies earned = RecordSurvivors(record, second);
  CHECK(Holds(earned, MinigameTrophy::kSurvivorsGrandKnight));
  // Les évolués du Chevalier n'entrent pas dans « Grand sorcier ».
  CHECK_FALSE(Holds(earned, MinigameTrophy::kSurvivorsGrandWizard));
}

TEST_CASE("Trophées survivors : Grand chevalier relit les évolués déjà mémorisés",
          "[sim][survivors]") {
  MinigameRecord record;
  for (const EvolvedOfClass& one : kKnightEvolved) {
    record.survivors_evolved.push_back(one.skill_id);
  }
  std::ranges::sort(record.survivors_evolved);
  SurvivorsRun run = Fresh();
  run.field_for_tests().rules = &ro::sim::ShippedSurvivorsRules();
  CHECK(RecordSurvivors(record, run) == Trophies{MinigameTrophy::kSurvivorsGrandKnight});
}

TEST_CASE("Trophées survivors : les règles sans Chevalier ne donnent pas Grand chevalier",
          "[sim][survivors]") {
  MinigameRecord record;
  for (const EvolvedOfClass& one : kKnightEvolved) {
    record.survivors_evolved.push_back(one.skill_id);
  }
  std::ranges::sort(record.survivors_evolved);
  SurvivorsRun run = Fresh();
  CHECK(RecordSurvivors(record, run).empty());
}

TEST_CASE("Trophées survivors : chaque évolué des règles du Voleur a le sien", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const auto thief = std::ranges::find_if(rules.classes, [](const auto& hero_class) {
    return std::string_view(hero_class.key) == "thief";
  });
  REQUIRE(thief != rules.classes.end());
  std::size_t evolved_count = 0;
  for (const ro::sim::EvolutionRule& evolution : rules.evolutions) {
    if (std::ranges::find(thief->skill_pool, evolution.skill_id) == thief->skill_pool.end()) {
      continue;
    }
    ++evolved_count;
    const auto listed =
        std::ranges::find(kThiefEvolved, evolution.evolved_skill_id, &EvolvedOfClass::skill_id);
    REQUIRE(listed != kThiefEvolved.end());
    CHECK(EarnedBy([&evolution](SurvivorsField& field) {
            field.hero.skills.push_back(OwnedSkill{.skill_id = evolution.evolved_skill_id});
          }) == Trophies{listed->trophy});
  }
  CHECK(evolved_count == kThiefEvolved.size());
}

TEST_CASE("Trophées survivors : Grand voleur, ses évolués au fil des parties", "[sim][survivors]") {
  MinigameRecord record;
  SurvivorsRun first = Fresh();
  PlayThief(first.field_for_tests());
  for (std::size_t index = 0; index + 1 < kThiefEvolved.size(); ++index) {
    first.field_for_tests().hero.skills.push_back(
        OwnedSkill{.skill_id = kThiefEvolved[index].skill_id});
  }
  CHECK_FALSE(Holds(RecordSurvivors(record, first), MinigameTrophy::kSurvivorsGrandThief));
  SurvivorsRun second = Fresh();
  PlayThief(second.field_for_tests());
  second.field_for_tests().hero.skills.push_back(
      OwnedSkill{.skill_id = kThiefEvolved.back().skill_id});
  const Trophies earned = RecordSurvivors(record, second);
  CHECK(Holds(earned, MinigameTrophy::kSurvivorsGrandThief));
  // Les évolués du Voleur n'entrent ni dans « Grand sorcier » ni dans « Grand chevalier ».
  CHECK_FALSE(Holds(earned, MinigameTrophy::kSurvivorsGrandWizard));
  CHECK_FALSE(Holds(earned, MinigameTrophy::kSurvivorsGrandKnight));
}

TEST_CASE("Trophées survivors : Grand voleur relit les évolués déjà mémorisés",
          "[sim][survivors]") {
  MinigameRecord record;
  for (const EvolvedOfClass& one : kThiefEvolved) {
    record.survivors_evolved.push_back(one.skill_id);
  }
  std::ranges::sort(record.survivors_evolved);
  SurvivorsRun run = Fresh();
  run.field_for_tests().rules = &ro::sim::ShippedSurvivorsRules();
  CHECK(RecordSurvivors(record, run) == Trophies{MinigameTrophy::kSurvivorsGrandThief});
}

TEST_CASE("Trophées survivors : les règles sans Voleur ne donnent pas Grand voleur",
          "[sim][survivors]") {
  MinigameRecord record;
  for (const EvolvedOfClass& one : kThiefEvolved) {
    record.survivors_evolved.push_back(one.skill_id);
  }
  std::ranges::sort(record.survivors_evolved);
  SurvivorsRun run = Fresh();
  CHECK(RecordSurvivors(record, run).empty());
}

TEST_CASE("Trophées survivors : le niveau 40 du Voleur n'est pas un Mage aguerri",
          "[sim][survivors]") {
  CHECK(EarnedBy([](SurvivorsField& field) {
          PlayThief(field);
          field.tally.level = kLevelForty;
        }).empty());
}

TEST_CASE("Trophées survivors : le niveau 40 du Chevalier n'est pas un Mage aguerri",
          "[sim][survivors]") {
  CHECK(EarnedBy([](SurvivorsField& field) {
          PlayKnight(field);
          field.tally.level = kLevelForty;
        }).empty());
}

TEST_CASE("Trophées survivors : Magnum Break seul ne fait pas un Pyromane", "[sim][survivors]") {
  const Trophies earned = EarnedBy([](SurvivorsField& field) {
    Win(field);
    PlayKnight(field);
    field.hero.skills = {OwnedSkill{.skill_id = kMagnumBreakSkillId}};
  });
  CHECK(earned == Trophies{MinigameTrophy::kSurvivorsVictory, MinigameTrophy::kSurvivorsPureMind});
}

TEST_CASE("Trophées survivors : le Pharaon vaincu ne fait pas tomber Baphomet",
          "[sim][survivors]") {
  const auto win_over_pharaoh = [](SurvivorsField& field) {
    Win(field);
    field.content.bosses = kPharaohFight;
  };
  // Pyromane et Esprit pur se gagnent sur toute victoire.
  CHECK(EarnedBy(win_over_pharaoh) ==
        Trophies{MinigameTrophy::kSurvivorsPyromaniac, MinigameTrophy::kSurvivorsPureMind});
}

TEST_CASE(
    "Trophées survivors : ce qui est acquis ne se retire pas, les conditions ont beau changer",
    "[sim][survivors]") {
  MinigameRecord record;
  record.unlocked[static_cast<std::size_t>(MinigameTrophy::kSurvivorsVictory)] = true;
  record.unlocked[static_cast<std::size_t>(MinigameTrophy::kSurvivorsLevel40)] = true;
  record.unlocked[static_cast<std::size_t>(MinigameTrophy::kSurvivorsPyromaniac)] = true;
  SurvivorsRun run = Fresh();
  PlayKnight(run.field_for_tests());
  run.field_for_tests().tally.outcome = SurvivorsOutcome::kVictory;
  run.field_for_tests().content.bosses = kPharaohFight;
  run.field_for_tests().tally.level = kLevelForty;
  CHECK_FALSE(Holds(RecordSurvivors(record, run), MinigameTrophy::kSurvivorsVictory));
  CHECK(record.Has(MinigameTrophy::kSurvivorsVictory));
  CHECK(record.Has(MinigameTrophy::kSurvivorsLevel40));
  CHECK(record.Has(MinigameTrophy::kSurvivorsPyromaniac));
}

TEST_CASE("Trophées survivors : mille gels, et la victoire sans minimap", "[sim][survivors]") {
  CHECK(EarnedBy([](SurvivorsField& field) { field.recap.freezes = kManyFreezes - 1; }).empty());
  CHECK(EarnedBy([](SurvivorsField& field) { field.recap.freezes = kManyFreezes; }) ==
        Trophies{MinigameTrophy::kSurvivorsDeepFreeze});
  // La minimap, `app` la dit : sans elle, la victoire seule gagne « À l'aveugle ».
  SurvivorsRun won = Fresh();
  Win(won.field_for_tests());
  MinigameRecord record;
  const Trophies blind =
      RecordSurvivors(record, won, ro::sim::SurvivorsTrophyFacts{.without_minimap = true});
  CHECK(std::ranges::find(blind, MinigameTrophy::kSurvivorsBlindVictory) != blind.end());
  MinigameRecord seeing;
  const Trophies seen = RecordSurvivors(seeing, won);
  CHECK(std::ranges::find(seen, MinigameTrophy::kSurvivorsBlindVictory) == seen.end());
}

TEST_CASE("Trophées survivors : le skill de départ est bien Fire", "[sim][survivors]") {
  // Le cas de la victoire ci-dessus en dépend.
  const SurvivorsRun run = Fresh();
  REQUIRE(run.field().hero.skills.size() == 1);
  CHECK(run.field().hero.skills.front().skill_id == kTinyStrikeId);
}

TEST_CASE("Trophées survivors : chaque évolué de l'Archer a le sien, et Grand archer les veut tous",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const auto archer = std::ranges::find_if(rules.classes, [](const auto& hero_class) {
    return std::string_view(hero_class.key) == "archer";
  });
  REQUIRE(archer != rules.classes.end());
  std::size_t evolved_count = 0;
  for (const ro::sim::EvolutionRule& evolution : rules.evolutions) {
    if (std::ranges::find(archer->skill_pool, evolution.skill_id) == archer->skill_pool.end()) {
      continue;
    }
    ++evolved_count;
    const auto listed =
        std::ranges::find(kArcherEvolved, evolution.evolved_skill_id, &EvolvedOfClass::skill_id);
    REQUIRE(listed != kArcherEvolved.end());
    CHECK(EarnedBy([&evolution](SurvivorsField& field) {
            field.hero.skills.push_back(OwnedSkill{.skill_id = evolution.evolved_skill_id});
          }) == Trophies{listed->trophy});
  }
  CHECK(evolved_count == kArcherEvolved.size());

  MinigameRecord record;
  SurvivorsRun first = Fresh();
  PlayArcher(first.field_for_tests());
  for (std::size_t index = 0; index + 1 < kArcherEvolved.size(); ++index) {
    first.field_for_tests().hero.skills.push_back(
        OwnedSkill{.skill_id = kArcherEvolved[index].skill_id});
  }
  CHECK_FALSE(Holds(RecordSurvivors(record, first), MinigameTrophy::kSurvivorsGrandArcher));
  SurvivorsRun second = Fresh();
  PlayArcher(second.field_for_tests());
  second.field_for_tests().hero.skills.push_back(
      OwnedSkill{.skill_id = kArcherEvolved.back().skill_id});
  const Trophies earned = RecordSurvivors(record, second);
  CHECK(Holds(earned, MinigameTrophy::kSurvivorsGrandArcher));
  CHECK_FALSE(Holds(earned, MinigameTrophy::kSurvivorsGrandThief));
}
