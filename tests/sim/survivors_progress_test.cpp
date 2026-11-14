#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/pcg32.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_journal.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_play.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_score.h"
#include "ro/sim/survivors_state.h"

#include "support/survivors_fixture.h"

using ro::sim::AnswerKind;
using ro::sim::CastKind;
using ro::sim::Cells;
using ro::sim::FieldPoint;
using ro::sim::OfferKind;
using ro::sim::SurvivorsAnswer;
using ro::sim::SurvivorsField;
using ro::sim::SurvivorsOffer;
using ro::sim::SurvivorsPhase;
using ro::sim::SurvivorsRun;
using ro::sim::SurvivorsStart;
using ro::sim::SurvivorsStat;
using ro::test_support::kTinyCardId;
using ro::test_support::kTinyHomingId;
using ro::test_support::kTinyLunaticId;
using ro::test_support::kTinyOrbitId;
using ro::test_support::kTinyStrikeId;
using ro::test_support::PillarArena;
using ro::test_support::PlaceFoe;
using ro::test_support::QuietRules;
using ro::test_support::SetBonus;
using ro::test_support::StepStill;
using ro::test_support::TinyRules;
using ro::test_support::TinyStart;

namespace {

constexpr std::int32_t kArenaSideCells = 64;
// Assez de relances pour qu'une offre voulue finisse par sortir du tirage.
constexpr std::uint8_t kPlentyOfRerolls = 60;

void DropGem(SurvivorsField& field, FieldPoint offset, std::uint32_t exp) {
  const FieldPoint at = field.hero.position + offset;
  field.gems.push_back(
      ro::sim::SurvivorsGem{.position = at, .previous_position = at, .exp = exp, .pull_units = 0});
}

// Une gemme aux pieds du héros, de quoi monter d'un niveau : le pas la ramasse
// et ouvre le choix.
void LevelUp(SurvivorsRun& run) {
  SurvivorsField& field = run.field_for_tests();
  DropGem(field, FieldPoint{}, ro::sim::ExpToNext(*field.rules, field.hero.level) - field.hero.exp);
  StepStill(run);
}

[[nodiscard]] std::optional<std::uint8_t> IndexOf(const SurvivorsField& field, OfferKind kind,
                                                  std::uint16_t id) {
  for (std::size_t index = 0; index < field.offers.size(); ++index) {
    if (field.offers[index].kind == kind && field.offers[index].id == id) {
      return static_cast<std::uint8_t>(index);
    }
  }
  return std::nullopt;
}

// L'index de l'offre voulue, en relançant le tirage tant qu'il le faut et
// qu'une relance reste.
[[nodiscard]] std::optional<std::uint8_t> RerollFor(SurvivorsRun& run, OfferKind kind,
                                                    std::uint16_t id) {
  for (;;) {
    if (const auto index = IndexOf(run.field(), kind, id)) return index;
    if (!run.Answer(SurvivorsAnswer{.kind = AnswerKind::kReroll, .offer_index = 0})) {
      return std::nullopt;
    }
  }
}

[[nodiscard]] SurvivorsAnswer Pick(std::uint8_t index) {
  return SurvivorsAnswer{.kind = AnswerKind::kPick, .offer_index = index};
}

[[nodiscard]] bool Distinct(const std::vector<SurvivorsOffer>& offers) {
  for (std::size_t a = 0; a < offers.size(); ++a) {
    for (std::size_t b = a + 1; b < offers.size(); ++b) {
      if (offers[a].kind == offers[b].kind && offers[a].id == offers[b].id) return false;
    }
  }
  return true;
}

// Les règles silencieuses, avec une seconde carte (des PV max) dans la réserve
// de la classe : cinq offres possibles au premier niveau. Les spans pointent
// dans l'objet : il ne se copie pas, et survit à sa partie.
constexpr std::uint16_t kHpCardId = 4003;  // Pupa Card
constexpr std::array<std::int32_t, 2> kHpCardLevels{20, 40};

struct RichRules {
  std::array<ro::sim::CardRule, 2> cards{{
      ro::test_support::kTinyCards[0],
      {.card_item_id = kHpCardId,
       .key = "hp_card",
       .description = "Carte de PV d'essai.",
       .stat = SurvivorsStat::kMaxHp,
       .per_level = kHpCardLevels},
  }};
  std::array<std::uint16_t, 2> card_pool{kTinyCardId, kHpCardId};
  std::array<ro::sim::ClassRule, 1> classes = ro::test_support::kTinyClasses;
  ro::sim::SurvivorsRules rules = QuietRules();

  RichRules() {
    classes[0].card_pool = card_pool;
    rules.cards = cards;
    rules.classes = classes;
  }
  RichRules(const RichRules&) = delete;
  RichRules& operator=(const RichRules&) = delete;
};

}  // namespace

TEST_CASE("Progression survivors : le loadout entre dans les statistiques", "[sim][survivors]") {
  ro::sim::SurvivorsStart start = TinyStart(1);
  start.loadout.bonus.values[static_cast<std::size_t>(SurvivorsStat::kMaxHp)] = 20;
  start.loadout.bonus.values[static_cast<std::size_t>(SurvivorsStat::kWalkPct)] = 50;
  start.loadout.bonus.values[static_cast<std::size_t>(SurvivorsStat::kMagnetPct)] = 25;
  start.loadout.rerolls = 2;
  start.loadout.skips = 1;
  start.loadout.banishes = 3;
  const auto run = SurvivorsRun::Start(TinyRules(), PillarArena(64), start);
  REQUIRE(run.has_value());
  const ro::sim::SurvivorsHero& hero = run->field().hero;
  CHECK(hero.stats == start.loadout.bonus);
  CHECK(hero.max_hp == 120);
  CHECK(hero.hp == 120);
  CHECK(hero.units_per_step == 204);
  CHECK(hero.magnet_units == ro::sim::Cells(2) * 125 / 100);
  CHECK(run->field().rerolls_left == 2);
  CHECK(run->field().skips_left == 1);
  CHECK(run->field().banishes_left == 3);
}

TEST_CASE("Progression survivors : un monstre tombé laisse sa gemme", "[sim][survivors]") {
  auto run = SurvivorsRun::Start(QuietRules(), PillarArena(kArenaSideCells), TinyStart(1));
  REQUIRE(run.has_value());
  SurvivorsField& field = run->field_for_tests();
  const FieldPoint at = field.hero.position + FieldPoint{Cells(6), 0};
  const std::uint32_t serial = PlaceFoe(field, kTinyLunaticId, at);
  field.foes.back().units_per_step = 0;
  field.foes.back().hp = 0;
  PlaceFoe(field, kTinyLunaticId, field.hero.position + FieldPoint{0, Cells(12)});
  field.foes.back().units_per_step = 0;

  StepStill(*run);
  REQUIRE(field.foes.size() == 1);
  CHECK(field.foes[0].serial == serial + 1);
  CHECK(field.tally.kills == 1U);
  CHECK(field.tally.kill_points ==
        ro::sim::KillPoints(*ro::sim::FindFoe(*field.rules, kTinyLunaticId), false));
  CHECK(field.tally.boss_kills == 0);
  REQUIRE(field.gems.size() == 1);
  CHECK(field.gems[0].position == at);
  CHECK(field.gems[0].exp == ro::sim::FindFoe(*field.rules, kTinyLunaticId)->exp);
  const ro::sim::SurvivorsNews news = run->TakeNews();
  REQUIRE(news.falls.size() == 1);
  CHECK(news.falls[0].foe_serial == serial);
  CHECK(news.falls[0].at == at);
}

TEST_CASE("Progression survivors : au plafond, l'expérience rejoint la première gemme",
          "[sim][survivors]") {
  auto run = SurvivorsRun::Start(QuietRules(), PillarArena(kArenaSideCells), TinyStart(1));
  REQUIRE(run.has_value());
  SurvivorsField& field = run->field_for_tests();
  const std::size_t most_gems = field.rules->caps.most_gems;
  for (std::size_t gem = 0; gem < most_gems; ++gem) DropGem(field, FieldPoint{Cells(6), 0}, 1);
  PlaceFoe(field, kTinyLunaticId, field.hero.position + FieldPoint{0, Cells(6)});
  field.foes.back().units_per_step = 0;
  field.foes.back().hp = 0;

  StepStill(*run);
  CHECK(field.gems.size() == most_gems);
  CHECK(field.gems[0].exp == 2U);
  CHECK(field.gems[1].exp == 1U);
}

TEST_CASE("Progression survivors : l'aimant saisit, la gemme accélère", "[sim][survivors]") {
  auto run = SurvivorsRun::Start(QuietRules(), PillarArena(kArenaSideCells), TinyStart(1));
  REQUIRE(run.has_value());
  SurvivorsField& field = run->field_for_tests();
  const FieldPoint hero = field.hero.position;
  const std::int32_t pull_start = field.rules->tuning.gem_pull_start_units;
  const std::int32_t pull_accel = field.rules->tuning.gem_pull_accel_units;
  // L'aimant porte à deux cases : la première gemme est saisie, l'autre non.
  DropGem(field, FieldPoint{Cells(3) / 2, 0}, 1);
  DropGem(field, FieldPoint{Cells(3), 0}, 1);

  StepStill(*run);
  REQUIRE(field.gems.size() == 2);
  CHECK(field.gems[0].previous_position == hero + FieldPoint{Cells(3) / 2, 0});
  CHECK(field.gems[0].position == hero + FieldPoint{Cells(3) / 2 - pull_start, 0});
  CHECK(field.gems[0].pull_units == pull_start + pull_accel);
  CHECK(field.gems[1].position == hero + FieldPoint{Cells(3), 0});
  CHECK(field.gems[1].pull_units == 0);

  StepStill(*run);
  CHECK(field.gems[0].position == hero + FieldPoint{Cells(3) / 2 - 2 * pull_start - pull_accel, 0});

  StepStill(*run, 20);
  REQUIRE(field.gems.size() == 1);
  CHECK(field.gems[0].pull_units == 0);
  CHECK(field.hero.exp == 1U);
  CHECK(field.tally.gems_gathered == 1U);
  CHECK(field.tally.exp_total == 1U);
  CHECK(run->TakeNews().gems_gathered == 1U);
}

TEST_CASE("Progression survivors : l'expérience majorée par la statistique", "[sim][survivors]") {
  SurvivorsStart start = TinyStart(1);
  SetBonus(start, SurvivorsStat::kExpPct, 50);
  auto run = SurvivorsRun::Start(QuietRules(), PillarArena(kArenaSideCells), start);
  REQUIRE(run.has_value());
  SurvivorsField& field = run->field_for_tests();
  // Une gemme de 2 sur le héros : 2 × 150 / 100, sous les 5 du premier palier.
  REQUIRE(ro::sim::ExpToNext(*field.rules, 1) == 5U);
  DropGem(field, FieldPoint{}, 2);
  StepStill(*run);
  CHECK(field.gems.empty());
  CHECK(field.hero.level == 1);
  CHECK(field.hero.exp == 3U);
  CHECK(field.tally.exp_total == 3U);
}

TEST_CASE("Progression survivors : une gemme peut valoir plusieurs niveaux", "[sim][survivors]") {
  auto run = SurvivorsRun::Start(QuietRules(), PillarArena(kArenaSideCells), TinyStart(1));
  REQUIRE(run.has_value());
  SurvivorsField& field = run->field_for_tests();
  const ro::sim::SurvivorsRules& rules = *field.rules;
  const std::uint32_t three_levels =
      ro::sim::ExpToNext(rules, 1) + ro::sim::ExpToNext(rules, 2) + ro::sim::ExpToNext(rules, 3);
  DropGem(field, FieldPoint{}, three_levels + 2);

  StepStill(*run);
  CHECK(field.hero.level == 4);
  CHECK(field.hero.exp == 2U);
  CHECK(field.pending_level_ups == 3);
  CHECK(field.phase == SurvivorsPhase::kChoosing);
  CHECK(run->TakeNews().level_ups == 3);
  for (int answered = 0; answered < 2; ++answered) {
    REQUIRE(run->Answer(Pick(0)));
    CHECK(field.phase == SurvivorsPhase::kChoosing);
    CHECK_FALSE(field.offers.empty());
  }
  REQUIRE(run->Answer(Pick(0)));
  CHECK(field.pending_level_ups == 0);
  CHECK(field.phase == SurvivorsPhase::kRunning);
  CHECK(field.offers.empty());
}

TEST_CASE("Progression survivors : les offres, distinctes et pondérées", "[sim][survivors]") {
  constexpr std::uint64_t kSeed = 77;
  auto run = SurvivorsRun::Start(QuietRules(), PillarArena(kArenaSideCells), TinyStart(kSeed));
  REQUIRE(run.has_value());
  LevelUp(*run);
  const SurvivorsField& field = run->field();
  REQUIRE(field.phase == SurvivorsPhase::kChoosing);

  // Le même tirage, refait : la chance d'abord (toujours tirée), puis trois
  // tirages pondérés sans remise. Le skill possédé pèse offer_weight_owned, les
  // nouveautés offer_weight_fresh, dans l'ordre des réserves.
  const std::array<SurvivorsOffer, 4> candidates{{
      {OfferKind::kSkillLevel, kTinyStrikeId, 2},
      {OfferKind::kNewSkill, kTinyHomingId, 1},
      {OfferKind::kNewSkill, kTinyOrbitId, 1},
      {OfferKind::kNewCard, kTinyCardId, 1},
  }};
  std::array<std::uint32_t, 4> weights{100, 60, 60, 60};
  ro::core::Pcg32 offer_random(kSeed, ro::sim::kOfferStream);
  CHECK_FALSE(offer_random.Chance(0, 100));
  std::vector<SurvivorsOffer> expected;
  for (int drawn = 0; drawn < 3; ++drawn) {
    const std::size_t picked = offer_random.PickWeighted(weights);
    REQUIRE(picked < weights.size());
    expected.push_back(candidates[picked]);
    weights[picked] = 0;
  }
  CHECK(field.offers == expected);
  CHECK(field.offer_random == offer_random);
  CHECK(Distinct(field.offers));
}

TEST_CASE("Progression survivors : la chance ajoute une offre, jamais au-delà de kMostOffers",
          "[sim][survivors]") {
  RichRules rich;
  SECTION("une quatrième offre") {
    SurvivorsStart start = TinyStart(5);
    SetBonus(start, SurvivorsStat::kLuckPct, 100);
    auto run = SurvivorsRun::Start(rich.rules, PillarArena(kArenaSideCells), start);
    REQUIRE(run.has_value());
    LevelUp(*run);
    CHECK(run->field().offers.size() == 4);
    CHECK(Distinct(run->field().offers));
  }
  SECTION("sans chance, le compte des règles") {
    auto run = SurvivorsRun::Start(rich.rules, PillarArena(kArenaSideCells), TinyStart(5));
    REQUIRE(run.has_value());
    LevelUp(*run);
    CHECK(run->field().offers.size() == 3);
  }
  SECTION("au plafond, la chance n'ajoute rien") {
    rich.rules.caps.offers = ro::sim::kMostOffers;
    SurvivorsStart start = TinyStart(5);
    SetBonus(start, SurvivorsStat::kLuckPct, 100);
    start.loadout.rerolls = 20;
    auto run = SurvivorsRun::Start(rich.rules, PillarArena(kArenaSideCells), start);
    REQUIRE(run.has_value());
    LevelUp(*run);
    for (int reroll = 0; reroll <= 20; ++reroll) {
      CHECK(run->field().offers.size() == ro::sim::kMostOffers);
      CHECK(Distinct(run->field().offers));
      if (!run->Answer(SurvivorsAnswer{.kind = AnswerKind::kReroll, .offer_index = 0})) break;
    }
  }
}

TEST_CASE("Progression survivors : un Id banni n'est plus offert, même en montée",
          "[sim][survivors]") {
  SurvivorsStart start = TinyStart(3);
  start.loadout.rerolls = kPlentyOfRerolls;
  start.loadout.banishes = 1;
  auto run = SurvivorsRun::Start(QuietRules(), PillarArena(kArenaSideCells), start);
  REQUIRE(run.has_value());
  LevelUp(*run);
  const SurvivorsField& field = run->field();
  // La montée de niveau du skill possédé.
  const auto level_up = RerollFor(*run, OfferKind::kSkillLevel, kTinyStrikeId);
  REQUIRE(level_up.has_value());
  REQUIRE(run->Answer(SurvivorsAnswer{.kind = AnswerKind::kBanish, .offer_index = *level_up}));
  CHECK(field.banishes_left == 0);
  CHECK(field.banished == std::vector<std::uint16_t>{kTinyStrikeId});
  CHECK(field.phase == SurvivorsPhase::kChoosing);
  // Plus de charge : un second bannissement est refusé.
  CHECK_FALSE(run->Answer(SurvivorsAnswer{.kind = AnswerKind::kBanish, .offer_index = 0}));
  for (;;) {
    for (const SurvivorsOffer& offer : field.offers) CHECK(offer.id != kTinyStrikeId);
    if (!run->Answer(SurvivorsAnswer{.kind = AnswerKind::kReroll, .offer_index = 0})) break;
  }
  CHECK(field.rerolls_left == 0);
  // Le skill banni reste possédé, à son niveau.
  CHECK(field.hero.skills[0].skill_id == kTinyStrikeId);
  CHECK(field.hero.skills[0].level == 1);
}

TEST_CASE("Progression survivors : une orbite acquise ou montée part sur kOrbitOn",
          "[sim][survivors]") {
  SurvivorsStart start = TinyStart(11);
  start.loadout.rerolls = kPlentyOfRerolls;
  auto run = SurvivorsRun::Start(QuietRules(), PillarArena(kArenaSideCells), start);
  REQUIRE(run.has_value());
  const SurvivorsField& field = run->field();
  const auto orbit_on = [&] {
    const ro::sim::SurvivorsNews news = run->TakeNews();
    return std::count_if(news.casts.begin(), news.casts.end(),
                         [](const ro::sim::SurvivorsCast& cast) {
                           return cast.skill_id == kTinyOrbitId && cast.kind == CastKind::kOrbitOn;
                         });
  };

  StepStill(*run, 3);
  LevelUp(*run);
  const auto fresh = RerollFor(*run, OfferKind::kNewSkill, kTinyOrbitId);
  REQUIRE(fresh.has_value());
  const std::uint32_t acquired_step = field.step;
  REQUIRE(run->Answer(Pick(*fresh)));
  REQUIRE(field.hero.skills.size() == 2);
  CHECK(field.hero.skills[1].skill_id == kTinyOrbitId);
  CHECK(field.hero.skills[1].ready_step == acquired_step);
  CHECK(field.hero.skills[1].phase_step == acquired_step);
  // Acquise pendant le choix : aucune orbe avant le pas suivant.
  std::vector<ro::sim::SurvivorsOrb> orbs;
  run->Orbs(orbs);
  CHECK(orbs.empty());
  (void)run->TakeNews();
  StepStill(*run);
  CHECK(orbit_on() == 1);
  run->Orbs(orbs);
  CHECK(orbs.size() == 1);

  // Au milieu de sa phase active, une montée la fait repartir.
  StepStill(*run, 10);
  CHECK(orbit_on() == 0);
  LevelUp(*run);
  run->Orbs(orbs);
  CHECK(orbs.size() == 1);
  const auto raised = RerollFor(*run, OfferKind::kSkillLevel, kTinyOrbitId);
  REQUIRE(raised.has_value());
  const std::uint32_t raised_step = field.step;
  REQUIRE(run->Answer(Pick(*raised)));
  CHECK(field.hero.skills[1].level == 2);
  CHECK(field.hero.skills[1].phase_step == raised_step);
  // Montée pendant le choix : l'orbite repart, et ne montre rien avant le pas
  // suivant.
  run->Orbs(orbs);
  CHECK(orbs.empty());
  (void)run->TakeNews();
  StepStill(*run);
  CHECK(orbit_on() == 1);
  run->Orbs(orbs);
  CHECK(orbs.size() == 2);
}

TEST_CASE("Progression survivors : tout au maximum, un soin et des zenys", "[sim][survivors]") {
  SurvivorsStart start = TinyStart(1);
  start.loadout.banishes = 1;
  auto run = SurvivorsRun::Start(QuietRules(), PillarArena(kArenaSideCells), start);
  REQUIRE(run.has_value());
  SurvivorsField& field = run->field_for_tests();
  field.hero.skills = {{.skill_id = kTinyStrikeId, .level = 2},
                       {.skill_id = kTinyHomingId, .level = 2},
                       {.skill_id = kTinyOrbitId, .level = 2}};
  field.hero.cards = {{.card_item_id = kTinyCardId, .level = 2}};
  field.hero.hp = 50;

  LevelUp(*run);
  const std::vector<SurvivorsOffer> heal_and_zeny{
      {.kind = OfferKind::kHeal, .id = 0, .next_level = 0},
      {.kind = OfferKind::kZeny, .id = 0, .next_level = 0}};
  CHECK(field.offers == heal_and_zeny);
  // Ni le soin ni les zenys ne se bannissent.
  CHECK_FALSE(run->Answer(SurvivorsAnswer{.kind = AnswerKind::kBanish, .offer_index = 0}));
  CHECK(field.banishes_left == 1);
  REQUIRE(run->Answer(Pick(0)));
  CHECK(field.hero.hp == 50 + field.rules->tuning.heal_offer_hp);

  LevelUp(*run);
  CHECK(field.offers == heal_and_zeny);
  REQUIRE(run->Answer(Pick(1)));
  CHECK(field.hero.zeny == field.rules->tuning.zeny_offer);
}

TEST_CASE("Progression survivors : les charges se décomptent", "[sim][survivors]") {
  SurvivorsStart start = TinyStart(4);
  start.loadout.rerolls = 1;
  start.loadout.skips = 1;
  auto run = SurvivorsRun::Start(QuietRules(), PillarArena(kArenaSideCells), start);
  REQUIRE(run.has_value());
  SurvivorsField& field = run->field_for_tests();
  CHECK_FALSE(run->Answer(Pick(0)));  // aucun choix ouvert

  const std::uint32_t two_levels =
      ro::sim::ExpToNext(*field.rules, 1) + ro::sim::ExpToNext(*field.rules, 2);
  DropGem(field, FieldPoint{}, two_levels);
  StepStill(*run);
  REQUIRE(field.pending_level_ups == 2);

  // Un index hors des offres est refusé pour toute sorte de réponse, même
  // celles qui n'en désignent aucune : la charge reste, les offres aussi.
  const std::vector<SurvivorsOffer> offered = field.offers;
  const auto beyond = static_cast<std::uint8_t>(field.offers.size());
  CHECK_FALSE(run->Answer(SurvivorsAnswer{.kind = AnswerKind::kSkip, .offer_index = beyond}));
  CHECK_FALSE(run->Answer(SurvivorsAnswer{.kind = AnswerKind::kReroll, .offer_index = beyond}));
  CHECK(field.skips_left == 1);
  CHECK(field.rerolls_left == 1);
  CHECK(field.pending_level_ups == 2);
  CHECK(field.offers == offered);

  REQUIRE(run->Answer(SurvivorsAnswer{.kind = AnswerKind::kReroll, .offer_index = 0}));
  CHECK(field.rerolls_left == 0);
  CHECK(field.pending_level_ups == 2);
  CHECK_FALSE(run->Answer(SurvivorsAnswer{.kind = AnswerKind::kReroll, .offer_index = 0}));

  REQUIRE(run->Answer(SurvivorsAnswer{.kind = AnswerKind::kSkip, .offer_index = 0}));
  CHECK(field.skips_left == 0);
  CHECK(field.pending_level_ups == 1);
  CHECK(field.phase == SurvivorsPhase::kChoosing);
  CHECK_FALSE(run->Answer(SurvivorsAnswer{.kind = AnswerKind::kSkip, .offer_index = 0}));
  CHECK_FALSE(run->Answer(SurvivorsAnswer{.kind = AnswerKind::kBanish, .offer_index = 0}));
  CHECK_FALSE(run->Answer(Pick(static_cast<std::uint8_t>(field.offers.size()))));

  REQUIRE(run->Answer(Pick(0)));
  CHECK(field.phase == SurvivorsPhase::kRunning);
  CHECK(field.pending_level_ups == 0);
  CHECK_FALSE(run->Answer(Pick(0)));
}

TEST_CASE("Progression survivors : une carte de PV max monte aussi les PV", "[sim][survivors]") {
  RichRules rich;
  SurvivorsStart start = TinyStart(9);
  start.loadout.rerolls = kPlentyOfRerolls;
  auto run = SurvivorsRun::Start(rich.rules, PillarArena(kArenaSideCells), start);
  REQUIRE(run.has_value());
  SurvivorsField& field = run->field_for_tests();
  field.hero.hp = 70;

  LevelUp(*run);
  const auto card = RerollFor(*run, OfferKind::kNewCard, kHpCardId);
  REQUIRE(card.has_value());
  REQUIRE(run->Answer(Pick(*card)));
  CHECK(field.hero.stats.Of(SurvivorsStat::kMaxHp) == 20);
  CHECK(field.hero.max_hp == 120);
  CHECK(field.hero.hp == 90);

  LevelUp(*run);
  const auto raised = RerollFor(*run, OfferKind::kCardLevel, kHpCardId);
  REQUIRE(raised.has_value());
  REQUIRE(run->Answer(Pick(*raised)));
  CHECK(field.hero.cards.back().level == 2);
  CHECK(field.hero.max_hp == 140);
  CHECK(field.hero.hp == 110);
}

TEST_CASE("Progression survivors : la régénération du loadout", "[sim][survivors]") {
  // 60 PV par minute : la réserve gagne 60 par pas, un PV tous les 50 pas.
  constexpr std::uint32_t kRegenPerMinute = 60;
  constexpr std::uint32_t kStepsPerHp = ro::sim::kStepsPerMinute / kRegenPerMinute;
  SurvivorsStart start = TinyStart(5);
  SetBonus(start, SurvivorsStat::kRegenPerMinute, static_cast<std::int32_t>(kRegenPerMinute));
  auto run = SurvivorsRun::Start(QuietRules(), PillarArena(kArenaSideCells), start);
  REQUIRE(run.has_value());
  SurvivorsField& field = run->field_for_tests();
  const std::int32_t max_hp = field.hero.max_hp;
  field.hero.hp = max_hp - 2;

  StepStill(*run, kStepsPerHp - 1);
  CHECK(field.hero.hp == max_hp - 2);
  CHECK(field.hero.regen_carry == (kStepsPerHp - 1) * kRegenPerMinute);
  StepStill(*run);
  CHECK(field.hero.hp == max_hp - 1);
  CHECK(field.hero.regen_carry == 0U);

  // Le reste de la réserve se reporte d'un pas à l'autre.
  StepStill(*run, kStepsPerHp / 2);
  CHECK(field.hero.regen_carry == kStepsPerHp / 2 * kRegenPerMinute);
  StepStill(*run, kStepsPerHp - kStepsPerHp / 2);
  CHECK(field.hero.hp == max_hp);

  // Plein, le héros ne dépasse pas son maximum ; la réserve tourne toujours.
  StepStill(*run, 3 * kStepsPerHp + 1);
  CHECK(field.hero.hp == max_hp);
  CHECK(field.hero.regen_carry == kRegenPerMinute);
}

TEST_CASE("Progression survivors : le journal n'inscrit que les réponses acceptées",
          "[sim][survivors]") {
  SurvivorsStart start = TinyStart(6);
  start.loadout.rerolls = 1;
  start.loadout.skips = 1;
  auto run = SurvivorsRun::Start(QuietRules(), PillarArena(kArenaSideCells), start);
  REQUIRE(run.has_value());
  SurvivorsField& field = run->field_for_tests();
  DropGem(field, FieldPoint{},
          ro::sim::ExpToNext(*field.rules, 1) + ro::sim::ExpToNext(*field.rules, 2));
  ro::sim::SurvivorsPlay play(std::move(*run));
  REQUIRE(play.Advance(ro::sim::kSurvivorsStepMs, ro::sim::SurvivorsInput{}) == 1U);
  REQUIRE(play.run().field().phase == SurvivorsPhase::kChoosing);
  const std::uint32_t step = play.run().field().step;
  const auto beyond = static_cast<std::uint8_t>(play.run().field().offers.size());

  using ro::sim::JournalEntry;
  using ro::sim::JournalKind;
  const auto entry = [&](SurvivorsAnswer answer) {
    return JournalEntry{
        .step = step, .kind = JournalKind::kAnswer, .value = ro::sim::PackAnswer(answer)};
  };
  const SurvivorsAnswer reroll{.kind = AnswerKind::kReroll, .offer_index = 0};
  const SurvivorsAnswer skip{.kind = AnswerKind::kSkip, .offer_index = 0};
  // Refusées : rien ne s'inscrit, pas même un index que le masque tronquerait.
  CHECK_FALSE(play.Answer(Pick(beyond)));
  CHECK_FALSE(play.Answer(SurvivorsAnswer{.kind = AnswerKind::kBanish, .offer_index = 0}));
  CHECK(play.journal().entries.empty());
  // Acceptées : chacune au pas du choix, dans l'ordre, empaquetée.
  REQUIRE(play.Answer(reroll));
  CHECK_FALSE(play.Answer(reroll));
  REQUIRE(play.Answer(skip));
  REQUIRE(play.Answer(Pick(0)));
  CHECK(play.run().field().phase == SurvivorsPhase::kRunning);
  CHECK(play.journal().entries ==
        std::vector<JournalEntry>{entry(reroll), entry(skip), entry(Pick(0))});
  CHECK(play.journal().final_step == step);
  CHECK_FALSE(play.Answer(Pick(0)));
  CHECK(play.journal().entries.size() == 3U);
}

TEST_CASE("Progression survivors : un Id verrouillé n'est jamais offert, sur mille offres",
          "[sim][survivors]") {
  constexpr std::size_t kThousandOffers = 1000;
  // Les offres tirées d'affilée par des relances, jusqu'à mille.
  const auto offers_with = [](std::vector<std::uint16_t> locked_ids) {
    SurvivorsStart start = TinyStart(5);
    start.locked_ids = std::move(locked_ids);
    auto run = SurvivorsRun::Start(QuietRules(), PillarArena(kArenaSideCells), start);
    REQUIRE(run.has_value());
    LevelUp(*run);
    std::vector<SurvivorsOffer> seen;
    while (seen.size() < kThousandOffers) {
      const std::vector<SurvivorsOffer>& offers = run->field().offers;
      seen.insert(seen.end(), offers.begin(), offers.end());
      run->field_for_tests().rerolls_left = 1;
      REQUIRE(run->Answer(SurvivorsAnswer{.kind = AnswerKind::kReroll, .offer_index = 0}));
    }
    return seen;
  };
  const auto offers_card = [](const std::vector<SurvivorsOffer>& offers) {
    return std::any_of(offers.begin(), offers.end(),
                       [](const SurvivorsOffer& offer) { return offer.id == kTinyCardId; });
  };
  CHECK(offers_card(offers_with({})));
  const std::vector<SurvivorsOffer> locked = offers_with({kTinyCardId});
  CHECK_FALSE(offers_card(locked));
}
