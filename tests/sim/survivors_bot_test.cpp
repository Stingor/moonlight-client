#include "ro/sim/survivors_bot.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_play.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/walk_map.h"

#include "support/survivors_content_fixture.h"
#include "support/survivors_fixture.h"
#include "support/survivors_golden.h"

using ro::sim::AnswerKind;
using ro::sim::CellCenter;
using ro::sim::CellOf;
using ro::sim::Cells;
using ro::sim::FieldPoint;
using ro::sim::HazardKind;
using ro::sim::HeadingOf;
using ro::sim::kHeadingSteps;
using ro::sim::kNoHeading;
using ro::sim::MageBotProfile;
using ro::sim::OfferKind;
using ro::sim::OwnedCard;
using ro::sim::OwnedSkill;
using ro::sim::PickupKind;
using ro::sim::PlayBot;
using ro::sim::SurvivorsBot;
using ro::sim::SurvivorsBotProfile;
using ro::sim::SurvivorsField;
using ro::sim::SurvivorsHazard;
using ro::sim::SurvivorsInput;
using ro::sim::SurvivorsOffer;
using ro::sim::SurvivorsOutcome;
using ro::sim::SurvivorsPhase;
using ro::sim::SurvivorsPickup;
using ro::sim::SurvivorsPlay;
using ro::sim::SurvivorsRun;
using ro::sim::TurnVector;
using ro::test_support::ContentRules;
using ro::test_support::GoldenRules;
using ro::test_support::GoldenStart;
using ro::test_support::kGoldenArenaCells;
using ro::test_support::kTinyLunaticId;
using ro::test_support::PillarArena;
using ro::test_support::PlaceFoe;
using ro::test_support::QuietRules;
using ro::test_support::TinyStart;

namespace {

constexpr std::int32_t kArenaSideCells = 64;
constexpr std::uint64_t kSeed = 20261024;
// Le centre de l'arène de 64 cases, loin de ses bords et hors des piliers
// (ceux de 30-31 et de 42-43 sur chaque axe).
constexpr std::int32_t kCenterCell = 32;
// Une case à l'est d'un pilier (30-31 × 30-31) : le cap ouest y est muré.
constexpr std::int32_t kBesidePillarCellX = 32;
constexpr std::int32_t kBesidePillarCellY = 30;
// La fuite d'un monstre seul, au cap opposé, à un cap candidat près : l'orbite
// l'incline d'un quart de sa force.
constexpr std::uint8_t kFleeTolerance = 4;
constexpr std::uint8_t kWest = 16;
constexpr std::uint8_t kNorth = 0;
constexpr std::uint16_t kChestItemId = 603;
// Les parties jouées par le robot dans ces essais.
constexpr std::uint32_t kRobotSteps = 1500;
constexpr std::uint32_t kVersusSteps = 3000;

// La distance entre deux caps, sur le cercle des 64.
[[nodiscard]] std::uint8_t HeadingGap(std::uint8_t a, std::uint8_t b) {
  const auto gap = static_cast<std::uint8_t>((a - b + kHeadingSteps) % kHeadingSteps);
  return static_cast<std::uint8_t>(gap > kHeadingSteps / 2 ? kHeadingSteps - gap : gap);
}

// Une partie silencieuse sur l'arène de 64 cases, le héros posé au centre de
// la case (cell_x, cell_y).
[[nodiscard]] SurvivorsRun QuietRunAt(std::int32_t cell_x, std::int32_t cell_y) {
  auto run = SurvivorsRun::Start(QuietRules(), PillarArena(kArenaSideCells), TinyStart(kSeed));
  REQUIRE(run.has_value());
  SurvivorsField& field = run->field_for_tests();
  field.hero.position = CellCenter(cell_x, cell_y);
  field.hero.previous_position = field.hero.position;
  return std::move(*run);
}

// Vrai si le cap mène, à une et à deux cases, sur des cases praticables.
[[nodiscard]] bool OpenHeading(const SurvivorsField& field, std::uint8_t heading) {
  for (const std::int32_t cells : {1, 2}) {
    const FieldPoint probe = field.hero.position + TurnVector(heading, Cells(cells));
    if (!field.ground.Passable(CellOf(probe.x), CellOf(probe.y))) return false;
  }
  return true;
}

// Un champ de choix sur les règles livrées, les offres données.
[[nodiscard]] SurvivorsRun ChoiceRun(std::vector<SurvivorsOffer> offers) {
  auto run = SurvivorsRun::Start(GoldenRules(), PillarArena(kGoldenArenaCells), GoldenStart(kSeed));
  REQUIRE(run.has_value());
  SurvivorsField& field = run->field_for_tests();
  field.phase = SurvivorsPhase::kChoosing;
  field.offers = std::move(offers);
  return std::move(*run);
}

}  // namespace

TEST_CASE("Robot survivors : il fuit un monstre seul, au cap opposé", "[sim][survivors]") {
  SurvivorsRun run = QuietRunAt(kCenterCell, kCenterCell);
  SurvivorsField& field = run.field_for_tests();
  // À quatre cases à l'est : la fuite pousse à l'ouest.
  REQUIRE(PlaceFoe(field, kTinyLunaticId, field.hero.position + FieldPoint{Cells(4), 0}) != 0);
  SurvivorsBot bot(MageBotProfile());
  const SurvivorsInput input = bot.Steer(field);
  REQUIRE(input.heading != kNoHeading);
  CHECK(HeadingGap(input.heading, kWest) <= kFleeTolerance);
}

TEST_CASE("Robot survivors : blessé, le Voleur lâche le monstre qu'il tenait", "[sim][survivors]") {
  CHECK(MageBotProfile().retreat_below_pct == 0);
  CHECK(ro::sim::KnightBotProfile().retreat_below_pct == 0);
  const SurvivorsBotProfile& profile = ro::sim::ThiefBotProfile();
  REQUIRE(profile.retreat_radius_units > profile.threat_radius_units);

  SurvivorsRun run = QuietRunAt(kCenterCell, kCenterCell);
  SurvivorsField& field = run.field_for_tests();
  // À l'est, entre ses deux distances : il ne le fuit que blessé.
  const std::int32_t between_units =
      (profile.threat_radius_units + profile.retreat_radius_units) / 2;
  REQUIRE(PlaceFoe(field, kTinyLunaticId, field.hero.position + FieldPoint{between_units, 0}) != 0);

  field.hero.hp = field.hero.max_hp;
  SurvivorsBot healthy(profile);
  CHECK(healthy.Steer(field).heading == kNoHeading);

  field.hero.hp = 1;
  SurvivorsBot hurt(profile);
  const SurvivorsInput input = hurt.Steer(field);
  REQUIRE(input.heading != kNoHeading);
  CHECK(HeadingGap(input.heading, kWest) <= kFleeTolerance);
}

TEST_CASE("Robot survivors : sans rien autour, il reste où il est", "[sim][survivors]") {
  SurvivorsRun run = QuietRunAt(kCenterCell, kCenterCell);
  SurvivorsBot bot(MageBotProfile());
  CHECK(bot.Steer(run.field()).heading == kNoHeading);
}

TEST_CASE("Robot survivors : il contourne un pilier au lieu de s'y jeter", "[sim][survivors]") {
  SurvivorsRun run = QuietRunAt(kBesidePillarCellX, kBesidePillarCellY);
  SurvivorsField& field = run.field_for_tests();
  // La fuite pousse droit à l'ouest, dans le pilier.
  REQUIRE(PlaceFoe(field, kTinyLunaticId, field.hero.position + FieldPoint{Cells(4), 0}) != 0);
  REQUIRE_FALSE(OpenHeading(field, kWest));
  SurvivorsBot bot(MageBotProfile());
  const SurvivorsInput input = bot.Steer(field);
  REQUIRE(input.heading != kNoHeading);
  CHECK(OpenHeading(field, input.heading));
  // Il s'écarte encore du monstre : son cap a une part vers l'ouest.
  CHECK(TurnVector(input.heading, Cells(1)).x < 0);
}

TEST_CASE("Robot survivors : il ne choisit jamais un cap muré", "[sim][survivors]") {
  auto run = SurvivorsRun::Start(ContentRules(), PillarArena(kArenaSideCells), TinyStart(kSeed));
  REQUIRE(run.has_value());
  SurvivorsPlay play(std::move(*run));
  SurvivorsBot bot(MageBotProfile());
  std::uint32_t walled = 0;
  std::uint32_t moving = 0;
  for (std::uint32_t step = 0; step < kRobotSteps; ++step) {
    const SurvivorsField& field = play.run().field();
    if (field.phase == SurvivorsPhase::kOver) break;
    if (field.phase == SurvivorsPhase::kChoosing) {
      REQUIRE(play.Answer(bot.Choose(field)));
      continue;
    }
    const SurvivorsInput input = bot.Steer(field);
    if (input.heading != kNoHeading) {
      ++moving;
      if (!OpenHeading(field, input.heading)) ++walled;
    }
    REQUIRE(play.Advance(ro::sim::kSurvivorsStepMs, input) == 1);
  }
  CHECK(moving > 0);
  CHECK(walled == 0);
}

TEST_CASE("Robot survivors : il sort d'un cercle de séisme annoncé", "[sim][survivors]") {
  SurvivorsRun run = QuietRunAt(kCenterCell, kCenterCell);
  SurvivorsField& field = run.field_for_tests();
  const FieldPoint center = field.hero.position;
  constexpr std::uint32_t kStrikeInSteps = 50;
  field.hazards.push_back(SurvivorsHazard{.kind = HazardKind::kQuake,
                                          .center = center,
                                          .radius_units = Cells(1),
                                          .damage = 40,
                                          .armed_step = field.step + kStrikeInSteps,
                                          .until_step = field.step + kStrikeInSteps + 1});
  SurvivorsBot bot(MageBotProfile());
  for (std::uint32_t step = 0; step < kStrikeInSteps; ++step) run.Step(bot.Steer(run.field()));
  const std::int64_t clear = Cells(1) + std::int64_t{QuietRules().tuning.hero_radius_units};
  CHECK(ro::sim::DistanceSquared(run.field().hero.position, center) > clear * clear);
}

TEST_CASE("Robot survivors : il va au coffre sans danger, pas avec un monstre à deux cases",
          "[sim][survivors]") {
  SurvivorsRun run = QuietRunAt(kCenterCell, kCenterCell);
  SurvivorsField& field = run.field_for_tests();
  // Un coffre à quatre cases au nord, dans le cadre d'apparition.
  field.pickups.push_back(
      SurvivorsPickup{.serial = field.next_serial++,
                      .kind = PickupKind::kChest,
                      .item_id = kChestItemId,
                      .position = field.hero.position + FieldPoint{0, Cells(4)}});
  SurvivorsBot calm(MageBotProfile());
  CHECK(calm.Steer(field).heading == kNorth);

  REQUIRE(PlaceFoe(field, kTinyLunaticId, field.hero.position + FieldPoint{Cells(2), 0}) != 0);
  SurvivorsBot scared(MageBotProfile());
  const SurvivorsInput input = scared.Steer(field);
  REQUIRE(input.heading != kNoHeading);
  CHECK(HeadingGap(input.heading, kWest) <= kFleeTolerance);
}

TEST_CASE("Robot survivors : il garde son cap steady_steps pas", "[sim][survivors]") {
  SurvivorsRun run = QuietRunAt(kCenterCell, kCenterCell);
  SurvivorsField& field = run.field_for_tests();
  const FieldPoint hero = field.hero.position;
  REQUIRE(PlaceFoe(field, kTinyLunaticId, hero + FieldPoint{Cells(4), 0}) != 0);
  const SurvivorsBotProfile& profile = MageBotProfile();
  REQUIRE(profile.steady_steps > 1);
  SurvivorsBot bot(profile);
  const std::uint8_t first = bot.Steer(field).heading;
  REQUIRE(first != kNoHeading);
  // Le monstre passe à l'ouest : la fuite pousse désormais à l'est.
  field.foes.back().position = hero - FieldPoint{Cells(4), 0};
  const std::uint32_t since = field.step;
  for (std::uint32_t held = 1; held < profile.steady_steps; ++held) {
    field.step = since + held;
    CHECK(bot.Steer(field).heading == first);
  }
  field.step = since + profile.steady_steps;
  const std::uint8_t turned = bot.Steer(field).heading;
  CHECK(turned != first);
  CHECK(HeadingGap(turned, static_cast<std::uint8_t>(kWest + kHeadingSteps / 2)) <= kFleeTolerance);
}

TEST_CASE("Robot survivors : un cap tenu menacé se quitte sans attendre", "[sim][survivors]") {
  SurvivorsRun run = QuietRunAt(kCenterCell, kCenterCell);
  SurvivorsField& field = run.field_for_tests();
  const FieldPoint hero = field.hero.position;
  REQUIRE(PlaceFoe(field, kTinyLunaticId, hero + FieldPoint{Cells(4), 0}) != 0);
  SurvivorsBot bot(MageBotProfile());
  const std::uint8_t first = bot.Steer(field).heading;
  REQUIRE(first != kNoHeading);
  // Une flaque tombe sur le point à deux cases du cap tenu.
  field.hazards.push_back(SurvivorsHazard{.kind = HazardKind::kPuddle,
                                          .center = hero + TurnVector(first, Cells(2)),
                                          .radius_units = Cells(1),
                                          .damage = 5,
                                          .armed_step = field.step,
                                          .until_step = field.step + 100,
                                          .period_steps = 10});
  field.step += 1;
  CHECK(bot.Steer(field).heading != first);
}

TEST_CASE("Robot survivors : la table de priorité des offres", "[sim][survivors]") {
  const SurvivorsBot bot(MageBotProfile());
  constexpr std::uint16_t kFireBolt = 19;
  constexpr std::uint16_t kSoulStrike = 13;
  // Le second skill d'une union : il n'évolue pas lui-même.
  constexpr std::uint16_t kNapalmBeat = 11;
  constexpr std::uint16_t kSight = 10;
  constexpr std::uint16_t kColdBolt = 14;
  constexpr std::uint16_t kFireWall = 18;
  constexpr std::uint16_t kHornetCard = 4019;
  constexpr std::uint16_t kGolemCard = 4072;
  constexpr std::uint16_t kPoringCard = 4001;
  const SurvivorsOffer heal{.kind = OfferKind::kHeal};
  const SurvivorsOffer zeny{.kind = OfferKind::kZeny};

  SECTION("la carte qui complète une évolution passe devant tout") {
    SurvivorsRun run =
        ChoiceRun({zeny,
                   {.kind = OfferKind::kNewSkill, .id = kSoulStrike, .next_level = 1},
                   {.kind = OfferKind::kSkillLevel, .id = kFireBolt, .next_level = 2},
                   {.kind = OfferKind::kNewCard, .id = kHornetCard, .next_level = 1}});
    CHECK(bot.Choose(run.field()).offer_index == 3);
  }
  SECTION("une carte qui n'évolue rien de possédé reste une carte") {
    SurvivorsRun run =
        ChoiceRun({{.kind = OfferKind::kNewSkill, .id = kSoulStrike, .next_level = 1},
                   {.kind = OfferKind::kNewCard, .id = kGolemCard, .next_level = 1}});
    CHECK(bot.Choose(run.field()).offer_index == 0);
  }
  SECTION("la montée d'un skill qui évolue, le plus avancé d'abord") {
    SurvivorsRun run =
        ChoiceRun({{.kind = OfferKind::kNewSkill, .id = kSoulStrike, .next_level = 1},
                   {.kind = OfferKind::kSkillLevel, .id = kFireBolt, .next_level = 2},
                   {.kind = OfferKind::kSkillLevel, .id = kSight, .next_level = 4}});
    run.field_for_tests().hero.skills.push_back(OwnedSkill{.skill_id = kSight, .level = 3});
    CHECK(bot.Choose(run.field()).offer_index == 2);
  }
  SECTION("un skill neuf, dans l'ordre du profil") {
    SurvivorsRun run =
        ChoiceRun({{.kind = OfferKind::kNewSkill, .id = kColdBolt, .next_level = 1},
                   {.kind = OfferKind::kNewCard, .id = kGolemCard, .next_level = 1},
                   {.kind = OfferKind::kNewSkill, .id = kFireWall, .next_level = 1}});
    CHECK(bot.Choose(run.field()).offer_index == 2);
  }
  SECTION("sans emplacement libre, un skill neuf ne se prend plus") {
    SurvivorsRun run =
        ChoiceRun({{.kind = OfferKind::kNewSkill, .id = kColdBolt, .next_level = 1},
                   {.kind = OfferKind::kNewCard, .id = kPoringCard, .next_level = 1}});
    SurvivorsField& field = run.field_for_tests();
    while (field.hero.skills.size() < GoldenRules().caps.skill_slots) {
      field.hero.skills.push_back(OwnedSkill{.skill_id = kSoulStrike, .level = 1});
    }
    CHECK(bot.Choose(field).offer_index == 1);
  }
  SECTION("une carte, dans l'ordre du profil, neuve ou montée") {
    SurvivorsRun run =
        ChoiceRun({{.kind = OfferKind::kNewCard, .id = kPoringCard, .next_level = 1},
                   {.kind = OfferKind::kCardLevel, .id = kGolemCard, .next_level = 2},
                   {.kind = OfferKind::kSkillLevel, .id = kNapalmBeat, .next_level = 2}});
    run.field_for_tests().hero.cards.push_back(OwnedCard{.card_item_id = kGolemCard, .level = 1});
    CHECK(bot.Choose(run.field()).offer_index == 1);
  }
  SECTION("la montée d'un autre skill passe devant les zénys") {
    SurvivorsRun run = ChoiceRun(
        {zeny, heal, {.kind = OfferKind::kSkillLevel, .id = kNapalmBeat, .next_level = 2}});
    CHECK(bot.Choose(run.field()).offer_index == 2);
  }
  SECTION("le soin sous le seuil, sinon les zénys") {
    SurvivorsRun run = ChoiceRun({heal, zeny});
    CHECK(bot.Choose(run.field()).offer_index == 1);
    SurvivorsField& field = run.field_for_tests();
    field.hero.hp = field.hero.max_hp / 4;
    CHECK(bot.Choose(field).offer_index == 0);
  }
  SECTION("à égalité, la première offre") {
    SurvivorsRun run =
        ChoiceRun({{.kind = OfferKind::kSkillLevel, .id = kSoulStrike, .next_level = 2},
                   {.kind = OfferKind::kSkillLevel, .id = kColdBolt, .next_level = 2}});
    const ro::sim::SurvivorsAnswer answer = bot.Choose(run.field());
    CHECK(answer.kind == AnswerKind::kPick);
    CHECK(answer.offer_index == 0);
  }
}

TEST_CASE("Robot survivors : deux robots sur la même partie rendent les mêmes entrées",
          "[sim][survivors]") {
  const auto play_once = [] {
    auto run = SurvivorsRun::Start(ContentRules(), PillarArena(kArenaSideCells), TinyStart(kSeed));
    REQUIRE(run.has_value());
    SurvivorsPlay play(std::move(*run));
    SurvivorsBot bot(MageBotProfile());
    std::uint32_t seen = 0;
    const std::uint32_t played =
        PlayBot(play, bot, kRobotSteps, [&seen](const SurvivorsRun&) { ++seen; });
    CHECK(seen == played);
    return play.journal();
  };
  const ro::sim::SurvivorsJournal first = play_once();
  const ro::sim::SurvivorsJournal second = play_once();
  CHECK(first == second);
  CHECK(first.entries.size() > 1);
}

TEST_CASE("Robot survivors : il survit plus longtemps qu'un héros immobile", "[sim][survivors]") {
  const auto survived = [](bool robot) {
    auto run = SurvivorsRun::Start(ContentRules(), PillarArena(kArenaSideCells), TinyStart(kSeed));
    REQUIRE(run.has_value());
    SurvivorsPlay play(std::move(*run));
    SurvivorsBot bot(MageBotProfile());
    if (robot) {
      PlayBot(play, bot, kVersusSteps, nullptr);
    } else {
      for (std::uint32_t step = 0; step < kVersusSteps; ++step) {
        const SurvivorsField& field = play.run().field();
        if (field.phase == SurvivorsPhase::kOver) break;
        if (field.phase == SurvivorsPhase::kChoosing) {
          REQUIRE(play.Answer(bot.Choose(field)));
          continue;
        }
        play.Advance(ro::sim::kSurvivorsStepMs, SurvivorsInput{});
      }
    }
    return play.run().tally();
  };
  const ro::sim::SurvivorsTally still = survived(false);
  const ro::sim::SurvivorsTally moving = survived(true);
  UNSCOPED_INFO("immobile : " << still.survived_steps << " pas ; robot : " << moving.survived_steps
                              << " pas");
  REQUIRE(still.outcome == SurvivorsOutcome::kFell);
  CHECK(moving.survived_steps > still.survived_steps);
}
