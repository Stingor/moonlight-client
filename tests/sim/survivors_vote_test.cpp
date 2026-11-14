#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_bot.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_team_play.h"

namespace ro::sim {
namespace {

constexpr std::uint64_t kSeed = 777;
constexpr std::uint16_t kMageClass = 1;
constexpr std::uint16_t kKnightClass = 2;

[[nodiscard]] SurvivorsStart TeamOf(const SurvivorsRules& rules, std::size_t allies) {
  SurvivorsStart start;
  start.seed = kSeed;
  start.class_id = kMageClass;
  start.first_skill_id = FindClass(rules, kMageClass)->starter_skills.front();
  for (const ArenaRule& arena : rules.arenas) {
    if (arena.terrain_id != 0) {
      start.arena_id = arena.arena_id;
      break;
    }
  }
  for (std::size_t ally = 0; ally < allies; ++ally) {
    const ClassRule* hero_class = FindClass(rules, kKnightClass);
    start.allies.push_back(SurvivorsAllyStart{.class_id = kKnightClass,
                                              .first_skill_id = hero_class->starter_skills.front(),
                                              .leaves = 1});
  }
  return start;
}

// Une équipe en plein vote : une montée de niveau due, et le tic qui l'ouvre.
[[nodiscard]] SurvivorsRun VotingTeam(const SurvivorsRules& rules, std::size_t allies) {
  auto run = SurvivorsRun::Start(rules, TeamOf(rules, allies));
  REQUIRE(run);
  run->field_for_tests().pending_level_ups = 1;
  const std::array<SurvivorsTeamInput, kMostHeroes> silent{};
  run->Tick(silent);
  REQUIRE(run->field().phase == SurvivorsPhase::kChoosing);
  return std::move(*run);
}

[[nodiscard]] SurvivorsAnswer Pick(std::uint8_t offer_index) {
  return SurvivorsAnswer{.kind = AnswerKind::kPick, .offer_index = offer_index};
}

TEST_CASE("le vote se clôt dès la majorité absolue", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsRun run = VotingTeam(rules, 3);
  std::array<SurvivorsTeamInput, kMostHeroes> inputs{};
  inputs[0].ballot = Pick(0);
  inputs[1].ballot = Pick(0);
  run.Tick(inputs);
  // Deux voix sur quatre ne font pas la majorité.
  CHECK(run.field().phase == SurvivorsPhase::kChoosing);
  inputs = {};
  inputs[2].ballot = Pick(0);
  run.Tick(inputs);
  CHECK(run.field().phase == SurvivorsPhase::kRunning);
  CHECK(run.field().ballots[0] == std::nullopt);
}

TEST_CASE("le vote attend la fin des tics, puis tire parmi les offres", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsRun run = VotingTeam(rules, 1);
  const std::array<SurvivorsTeamInput, kMostHeroes> silent{};
  for (std::uint32_t tick = 0; tick + 1 < kVoteTicks; ++tick) {
    run.Tick(silent);
    REQUIRE(run.field().phase == SurvivorsPhase::kChoosing);
  }
  run.Tick(silent);
  CHECK(run.field().phase == SurvivorsPhase::kRunning);
}

TEST_CASE("une égalité se tranche par un tirage, le même partout", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsRun first = VotingTeam(rules, 1);
  SurvivorsRun second = VotingTeam(rules, 1);
  REQUIRE(first.field().offers.size() >= 2);
  std::array<SurvivorsTeamInput, kMostHeroes> inputs{};
  inputs[0].ballot = Pick(0);
  inputs[1].ballot = Pick(1);
  first.Tick(inputs);
  second.Tick(inputs);
  CHECK(first.field().phase == SurvivorsPhase::kRunning);
  CHECK(SurvivorsDigest(first.field()) == SurvivorsDigest(second.field()));
}

TEST_CASE("un bulletin irrecevable ne compte pas", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsRun run = VotingTeam(rules, 1);
  CHECK_FALSE(run.BallotValid(Pick(200)));
  CHECK_FALSE(run.BallotValid(SurvivorsAnswer{.kind = AnswerKind::kReroll, .offer_index = 0}));
  // Personne n'est à terre : aucune relève.
  CHECK_FALSE(run.BallotValid(SurvivorsAnswer{.kind = AnswerKind::kRaise, .offer_index = 1}));
  std::array<SurvivorsTeamInput, kMostHeroes> inputs{};
  inputs[0].ballot = Pick(200);
  inputs[1].ballot = Pick(200);
  run.Tick(inputs);
  CHECK(run.field().phase == SurvivorsPhase::kChoosing);
}

TEST_CASE("relever un allié consomme une feuille et la montée de niveau", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsRun run = VotingTeam(rules, 1);
  SurvivorsField& field = run.field_for_tests();
  PutOnStage(field, 1);
  field.hero.hp = 0;
  field.hero.down = true;
  const std::size_t skills_before = field.hero.skills.size();
  PutOnStage(field, 0);
  const std::size_t first_skills_before = field.hero.skills.size();
  REQUIRE(field.leaves_left == 1);
  const SurvivorsAnswer raise{.kind = AnswerKind::kRaise, .offer_index = 1};
  REQUIRE(run.BallotValid(raise));
  std::array<SurvivorsTeamInput, kMostHeroes> inputs{};
  // Le héros à terre vote : deux voix sur deux.
  inputs[0].ballot = raise;
  inputs[1].ballot = raise;
  run.Tick(inputs);
  CHECK(field.phase == SurvivorsPhase::kRunning);
  CHECK(field.leaves_left == 0);
  CHECK_FALSE(HeroAt(field, 1).down);
  CHECK(HeroAt(field, 1).hp > 0);
  CHECK(HeroAt(field, 1).skills.size() == skills_before);
  CHECK(field.hero.skills.size() == first_skills_before);
  CHECK(field.pending_level_ups == 0);
  CHECK_FALSE(run.BallotValid(raise));
}

TEST_CASE("sans feuille, personne ne se relève", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsRun run = VotingTeam(rules, 1);
  SurvivorsField& field = run.field_for_tests();
  field.leaves_left = 0;
  PutOnStage(field, 1);
  field.hero.down = true;
  PutOnStage(field, 0);
  CHECK_FALSE(run.BallotValid(SurvivorsAnswer{.kind = AnswerKind::kRaise, .offer_index = 1}));
}

TEST_CASE("une relève refusée en solo", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  auto run = SurvivorsRun::Start(rules, TeamOf(rules, 0));
  REQUIRE(run);
  run->field_for_tests().pending_level_ups = 1;
  run->Step(SurvivorsInput{});
  REQUIRE(run->field().phase == SurvivorsPhase::kChoosing);
  CHECK_FALSE(run->Answer(SurvivorsAnswer{.kind = AnswerKind::kRaise, .offer_index = 0}));
}

// Fait jouer une équipe par des robots, tic par tic, sur `ticks` tics.
void PlayTeam(SurvivorsTeamPlay& play, std::uint32_t ticks) {
  const std::size_t players = HeroCount(play.run().field());
  std::vector<SurvivorsBot> bots(players, SurvivorsBot(MageBotProfile()));
  std::vector<SurvivorsTeamInput> inputs(players);
  for (std::uint32_t tick = 0; tick < ticks; ++tick) {
    if (play.run().field().phase == SurvivorsPhase::kOver) break;
    for (std::size_t player = 0; player < players; ++player) {
      play.mutable_run().PutHeroOnStage(player);
      const SurvivorsField& field = play.run().field();
      inputs[player].heading = bots[player].Steer(field).heading;
      inputs[player].ballot = field.phase == SurvivorsPhase::kChoosing
                                  ? std::optional(bots[player].Choose(field))
                                  : std::nullopt;
    }
    play.mutable_run().PutHeroOnStage(0);
    play.Tick(inputs);
  }
}

TEST_CASE("un journal coop se code, se relit et se rejoue à l'identique", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  auto run = SurvivorsRun::Start(rules, TeamOf(rules, 3));
  REQUIRE(run);
  SurvivorsTeamPlay play(std::move(*run));
  constexpr std::uint32_t kTicks = 3000;
  PlayTeam(play, kTicks);
  const SurvivorsTeamJournal& journal = play.journal();
  CHECK(journal.final_tick > 0);
  const std::vector<std::uint8_t> bytes = EncodeSurvivorsTeamJournal(journal);
  const core::Result<SurvivorsTeamJournal> decoded = DecodeSurvivorsTeamJournal(bytes);
  REQUIRE(decoded);
  CHECK(*decoded == journal);
  const core::Result<std::uint64_t> digest = ReplaySurvivorsTeam(rules, *decoded);
  REQUIRE(digest);
  CHECK(*digest == SurvivorsDigest(play.run().field()));
}

// LA PARTIE COOP EN OR : quatre robots, trois minutes. Cette empreinte est la même
// sous MSVC et sous g++-14. Elle change avec l'ordre du pas et avec l'échelle
// d'une équipe ; la corriger, c'est relire ce qui a changé.
TEST_CASE("la partie coop en or garde son empreinte", "[survivors]") {
  constexpr std::uint64_t kGoldenDigest = 14836191060566988434ULL;
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  auto run = SurvivorsRun::Start(rules, TeamOf(rules, 3));
  REQUIRE(run);
  SurvivorsTeamPlay play(std::move(*run));
  constexpr std::uint32_t kThreeMinutesTicks = 9000;
  PlayTeam(play, kThreeMinutesTicks);
  CHECK(SurvivorsDigest(play.run().field()) == kGoldenDigest);
}

TEST_CASE("un journal coop abîmé se refuse", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  auto run = SurvivorsRun::Start(rules, TeamOf(rules, 1));
  REQUIRE(run);
  SurvivorsTeamPlay play(std::move(*run));
  std::vector<std::uint8_t> bytes = EncodeSurvivorsTeamJournal(play.journal());
  CHECK_FALSE(DecodeSurvivorsTeamJournal(std::span(bytes).first(bytes.size() - 1)));
  bytes.push_back(0);
  CHECK_FALSE(DecodeSurvivorsTeamJournal(bytes));
  bytes.pop_back();
  bytes[0] = 'X';
  CHECK_FALSE(DecodeSurvivorsTeamJournal(bytes));
}

TEST_CASE("une empreinte change avec l'état", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  auto run = SurvivorsRun::Start(rules, TeamOf(rules, 1));
  REQUIRE(run);
  const std::uint64_t before = SurvivorsDigest(run->field());
  run->field_for_tests().hero.hp -= 1;
  CHECK(SurvivorsDigest(run->field()) != before);
}

TEST_CASE("l'échelle d'une équipe vaut pour l'EXP et la horde", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  auto duo = SurvivorsRun::Start(rules, TeamOf(rules, 1));
  REQUIRE(duo);
  const std::uint32_t needed = ExpToNext(rules, 1);
  CHECK(TeamExpToNext(duo->field()) < needed);
  auto solo = SurvivorsRun::Start(rules, TeamOf(rules, 0));
  REQUIRE(solo);
  CHECK(TeamExpToNext(solo->field()) == needed);
}

TEST_CASE("un coffre ouvert en coop pose une pause commune", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  auto run = SurvivorsRun::Start(rules, TeamOf(rules, 1));
  REQUIRE(run);
  SurvivorsField& field = run->field_for_tests();
  field.pickups.push_back(SurvivorsPickup{.serial = 1,
                                          .kind = PickupKind::kChest,
                                          .item_id = rules.chests.item_id,
                                          .position = field.hero.position});
  const std::array<SurvivorsTeamInput, kMostHeroes> silent{};
  run->Tick(silent);
  CHECK(field.tally.chests_opened == 1);
  REQUIRE(field.hold_ticks_left == kChestHoldTicks);
  const std::uint32_t step = field.step;
  run->Tick(silent);
  CHECK(field.step == step);
  CHECK(field.hold_ticks_left == kChestHoldTicks - 1);
}

}  // namespace
}  // namespace ro::sim
