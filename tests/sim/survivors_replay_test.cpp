#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_journal.h"
#include "ro/sim/survivors_play.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

#include "support/survivors_fixture.h"
#include "support/survivors_golden.h"

using ro::sim::FieldPoint;
using ro::sim::JournalEntry;
using ro::sim::JournalKind;
using ro::sim::ReplaySurvivors;
using ro::sim::SurvivorsJournal;
using ro::sim::SurvivorsOutcome;
using ro::sim::SurvivorsPhase;
using ro::sim::SurvivorsPlay;
using ro::sim::SurvivorsRun;
using ro::sim::SurvivorsTally;
using ro::test_support::GoldenRules;
using ro::test_support::kGoldenArenaCells;
using ro::test_support::kGoldenSeed;
using ro::test_support::kGoldenSteps;
using ro::test_support::PillarArena;
using ro::test_support::PlayGolden;
using ro::test_support::QuietRules;
using ro::test_support::TinyRules;
using ro::test_support::TinyStart;

namespace {

constexpr std::int32_t kReplayArenaCells = 64;
constexpr std::uint64_t kReplaySeed = 20261023;
constexpr std::uint8_t kWestHeading = 16;
constexpr std::uint32_t kAbandonStep = 30;

// Le journal d'une partie silencieuse : le départ et les entrées données.
[[nodiscard]] SurvivorsJournal QuietJournal(std::vector<JournalEntry> entries,
                                            std::uint32_t final_step) {
  return SurvivorsJournal{.ruleset_version = QuietRules().ruleset_version,
                          .start = TinyStart(kReplaySeed),
                          .entries = std::move(entries),
                          .final_step = final_step};
}

// Le code du refus ; rien pour un journal rejoué.
[[nodiscard]] std::optional<ro::core::ErrorCode> ReplayError(const SurvivorsJournal& journal) {
  const auto replayed = ReplaySurvivors(QuietRules(), PillarArena(kReplayArenaCells), journal);
  if (replayed.has_value()) return std::nullopt;
  return replayed.error().code;
}

// Le journal écrit en octets, relu, puis rejoué contre les règles en or : le
// décodage rend le journal à l'identique.
[[nodiscard]] ro::core::Result<SurvivorsTally> RoundTrip(const SurvivorsJournal& journal) {
  const std::vector<std::uint8_t> bytes = ro::sim::EncodeSurvivorsJournal(journal);
  const ro::core::Result<SurvivorsJournal> decoded = ro::sim::DecodeSurvivorsJournal(bytes);
  REQUIRE(decoded.has_value());
  CHECK(*decoded == journal);
  return ReplaySurvivors(GoldenRules(), PillarArena(kGoldenArenaCells), *decoded);
}

// La partie en or et, pour chaque événement de l'arène, les monstres de son
// espèce nés aux pas où il a placé ses membres : un événement que le plafond
// arrête reprend son reste aux pas suivants.
struct GoldenWatch {
  std::optional<SurvivorsPlay> play;
  std::vector<std::uint32_t> event_births;
};

// La partie en or jouée une fois, pas à pas, pour les essais qui la lisent.
// Un pas à la fois ou les 15 000 d'un coup, PlayScripted fait les mêmes pas
// et répond les mêmes choix : le bilan épinglé le vérifie.
[[nodiscard]] const GoldenWatch& Golden() {
  static const GoldenWatch kWatch = [] {
    const std::span<const ro::sim::EventRule> events = GoldenRules().events;
    GoldenWatch watch{.play = PlayGolden(kGoldenSeed, 0),
                      .event_births = std::vector<std::uint32_t>(events.size(), 0)};
    REQUIRE(watch.play.has_value());
    const ro::sim::SurvivorsField& field = watch.play->run().field();
    while (field.step < kGoldenSteps && field.phase != SurvivorsPhase::kOver) {
      const std::uint32_t step_before = field.step;
      const std::size_t event_before = field.next_event;
      const std::uint32_t serial_before = field.next_serial;
      ro::test_support::PlayScripted(*watch.play, 1);
      if (field.step == step_before) break;
      // Les événements soldés à ce pas, et celui que le plafond a arrêté
      // après qu'il a placé des membres.
      const std::size_t touched_end =
          std::min(events.size(), field.next_event + (field.event_members_placed > 0 ? 1U : 0U));
      for (std::size_t event = event_before; event < touched_end; ++event) {
        watch.event_births[event] += static_cast<std::uint32_t>(
            std::ranges::count_if(field.foes, [&](const ro::sim::SurvivorsFoe& foe) {
              return foe.serial >= serial_before && foe.foe_id == events[event].foe_id;
            }));
      }
    }
    return watch;
  }();
  return kWatch;
}

[[nodiscard]] const SurvivorsPlay& GoldenPlay() { return *Golden().play; }

// La partie en or jouée jusqu'à son premier choix, sans y répondre.
[[nodiscard]] SurvivorsPlay PlayToFirstChoice() {
  auto run = SurvivorsRun::Start(GoldenRules(), PillarArena(kGoldenArenaCells),
                                 ro::test_support::GoldenStart(kGoldenSeed));
  REQUIRE(run.has_value());
  SurvivorsPlay play(std::move(*run));
  while (play.run().field().phase == SurvivorsPhase::kRunning &&
         play.run().field().step < kGoldenSteps) {
    (void)play.Advance(ro::sim::kSurvivorsStepMs,
                       ro::test_support::ScriptedInput(play.run().field()));
  }
  return play;
}

}  // namespace

TEST_CASE("Rejeu survivors : le journal naît avec la partie", "[sim][survivors]") {
  auto run =
      SurvivorsRun::Start(TinyRules(), PillarArena(kReplayArenaCells), TinyStart(kReplaySeed));
  REQUIRE(run.has_value());
  SurvivorsPlay play(std::move(*run));
  CHECK(play.journal().start == TinyStart(kReplaySeed));
  CHECK(play.journal().ruleset_version == ro::sim::kSurvivorsRulesetVersion);
  constexpr std::uint32_t kPlayedSteps = 50;
  ro::test_support::PlayScripted(play, kPlayedSteps);
  CHECK(play.run().field().step == kPlayedSteps);
  CHECK(play.journal().final_step == kPlayedSteps);
  // Le premier pas inscrit le cap du joueur scripté, la partie partant de
  // l'arrêt : au pas 0, sans gemme en vue, il tourne depuis le nord.
  REQUIRE_FALSE(play.journal().entries.empty());
  CHECK(play.journal().entries[0] ==
        JournalEntry{.step = 0, .kind = JournalKind::kHeading, .value = 0});
}

TEST_CASE("Rejeu survivors : un abandon se rejoue, entrées du pas final comprises",
          "[sim][survivors]") {
  // La partie jouée directement : trente pas vers l'ouest, puis l'abandon.
  auto run =
      SurvivorsRun::Start(QuietRules(), PillarArena(kReplayArenaCells), TinyStart(kReplaySeed));
  REQUIRE(run.has_value());
  for (std::uint32_t step = 0; step < kAbandonStep; ++step) run->Step({.heading = kWestHeading});
  run->Abandon();
  REQUIRE(run->tally().outcome == SurvivorsOutcome::kAbandoned);

  // Son journal : l'abandon s'inscrit au pas où la partie s'arrête, qui est aussi
  // son pas final.
  const SurvivorsJournal journal =
      QuietJournal({{.step = 0, .kind = JournalKind::kHeading, .value = kWestHeading},
                    {.step = kAbandonStep, .kind = JournalKind::kAbandon, .value = 0}},
                   kAbandonStep);
  const auto replayed = ReplaySurvivors(QuietRules(), PillarArena(kReplayArenaCells), journal);
  REQUIRE(replayed.has_value());
  CHECK(*replayed == run->tally());
  CHECK(replayed->outcome == SurvivorsOutcome::kAbandoned);
  CHECK(replayed->survived_steps == kAbandonStep);
}

TEST_CASE("Rejeu survivors : l'abandon tenu par SurvivorsPlay", "[sim][survivors]") {
  auto run =
      SurvivorsRun::Start(QuietRules(), PillarArena(kReplayArenaCells), TinyStart(kReplaySeed));
  REQUIRE(run.has_value());
  SurvivorsPlay play(std::move(*run));
  play.Abandon();
  REQUIRE(play.journal().entries.size() == 1U);
  CHECK(play.journal().entries[0] ==
        JournalEntry{.step = 0, .kind = JournalKind::kAbandon, .value = 0});
  CHECK(play.journal().final_step == play.run().field().step);
  // Une partie finie n'inscrit plus rien.
  play.Abandon();
  CHECK(play.journal().entries.size() == 1U);

  const auto replayed =
      ReplaySurvivors(QuietRules(), PillarArena(kReplayArenaCells), play.journal());
  REQUIRE(replayed.has_value());
  CHECK(*replayed == play.run().tally());
  CHECK(replayed->outcome == SurvivorsOutcome::kAbandoned);
}

TEST_CASE("Rejeu survivors : un journal sans entrée court jusqu'à son pas final",
          "[sim][survivors]") {
  constexpr std::uint32_t kFinalStep = 20;
  const auto replayed =
      ReplaySurvivors(QuietRules(), PillarArena(kReplayArenaCells), QuietJournal({}, kFinalStep));
  REQUIRE(replayed.has_value());
  CHECK(replayed->survived_steps == kFinalStep);
  CHECK(replayed->outcome == SurvivorsOutcome::kRunning);
}

TEST_CASE("Rejeu survivors : les journaux incohérents sont refusés", "[sim][survivors]") {
  using ro::core::ErrorCode;
  const JournalEntry abandon{.step = kAbandonStep, .kind = JournalKind::kAbandon, .value = 0};

  SurvivorsJournal other_rules = QuietJournal({abandon}, kAbandonStep);
  other_rules.ruleset_version = static_cast<std::uint16_t>(other_rules.ruleset_version + 1);
  CHECK(ReplayError(other_rules) == ErrorCode::kUnsupportedVersion);

  // Une entrée au pas de l'abandon, mais après lui.
  CHECK(ReplayError(QuietJournal(
            {abandon, {.step = kAbandonStep, .kind = JournalKind::kHeading, .value = 0}},
            kAbandonStep)) == ErrorCode::kCorruptData);
  // Une partie abandonnée avant son pas final, ou un pas final jamais atteint.
  CHECK(ReplayError(QuietJournal({abandon}, kAbandonStep + 1)) == ErrorCode::kCorruptData);
  // Une entrée après le pas final.
  CHECK(ReplayError(QuietJournal({abandon}, kAbandonStep - 1)) == ErrorCode::kCorruptData);
  // Des entrées dans le désordre.
  CHECK(ReplayError(QuietJournal({{.step = 5, .kind = JournalKind::kHeading, .value = 3},
                                  {.step = 4, .kind = JournalKind::kHeading, .value = 2}},
                                 10)) == ErrorCode::kCorruptData);
  // Un cap hors de [0, 64) qui n'est pas « à l'arrêt ».
  CHECK(ReplayError(QuietJournal(
            {{.step = 0, .kind = JournalKind::kHeading, .value = ro::sim::kHeadingSteps}}, 10)) ==
        ErrorCode::kCorruptData);
  CHECK_FALSE(
      ReplayError(
          QuietJournal({{.step = 0, .kind = JournalKind::kHeading, .value = ro::sim::kNoHeading}},
                       10))
          .has_value());
  // Une réponse sans choix ouvert.
  CHECK(ReplayError(QuietJournal({{.step = 2, .kind = JournalKind::kAnswer, .value = 0}}, 10)) ==
        ErrorCode::kCorruptData);
}

TEST_CASE("Rejeu survivors : un choix resté sans réponse est refusé", "[sim][survivors]") {
  // Un pas ne fait rien pendant un choix : sans ce refus, le rejeu d'un journal
  // privé de sa réponse ne finirait jamais.
  SurvivorsPlay play = PlayToFirstChoice();
  REQUIRE(play.run().field().phase == SurvivorsPhase::kChoosing);
  const std::uint32_t choice_step = play.run().field().step;
  SurvivorsJournal journal = play.journal();
  // Au-delà du pas du choix : c'est le refus du choix qui arrête le rejeu, pas
  // le pas final.
  journal.final_step = choice_step + 10;
  const auto replayed = ReplaySurvivors(GoldenRules(), PillarArena(kGoldenArenaCells), journal);
  REQUIRE_FALSE(replayed.has_value());
  CHECK(replayed.error().code == ro::core::ErrorCode::kCorruptData);
}

// ── La partie en or : les règles livrées, cinq minutes ─────────────────────

TEST_CASE("Rejeu survivors : la partie en or se rejoue depuis ses octets", "[sim][survivors]") {
  const SurvivorsPlay* play = &GoldenPlay();
  const SurvivorsTally& tally = play->run().tally();
  const ro::sim::SurvivorsField& field = play->run().field();

  // La couverture : l'essaim de 1:30 et l'anneau de 2:30, la première élite
  // tuée et son coffre ouvert, Mastering né, l'essaim de 4:30, un skill à
  // éclats, tir droit, mur, nova ou anneau, un skill monté. Un événement est
  // couvert quand il a fait naître au moins un monstre, pas quand son heure
  // est passée. La partie tient ses cinq minutes, ou tombe après l'essaim de
  // 4:30. Sinon on change la graine, jamais les règles.
  constexpr std::size_t kEarlySwarm = 0;  // l'essaim de 1:30
  constexpr std::size_t kFirstRing = 1;   // l'anneau de 2:30
  constexpr std::size_t kLateSwarm = 2;   // l'essaim de 4:30
  REQUIRE(GoldenRules().events.size() > kLateSwarm);
  REQUIRE(GoldenRules().events[kLateSwarm].kind == ro::sim::EventKind::kSwarm);
  REQUIRE(GoldenRules().events[kLateSwarm].at_ms <=
          std::uint64_t{kGoldenSteps} * ro::sim::kSurvivorsStepMs);
  CHECK((tally.outcome == SurvivorsOutcome::kRunning || tally.outcome == SurvivorsOutcome::kFell));
  CHECK(field.next_event > kLateSwarm);
  const std::vector<std::uint32_t>& births = Golden().event_births;
  CHECK(births[kEarlySwarm] >= 1U);
  CHECK(births[kFirstRing] >= 1U);
  CHECK(births[kLateSwarm] >= 1U);
  CHECK(field.next_boss >= 1U);
  CHECK(tally.elite_kills >= 1U);
  CHECK(tally.chests_opened >= 1U);
  CHECK(tally.level >= 5);
  CHECK(tally.kills >= 100U);
  CHECK(play->journal().final_step == tally.survived_steps);
  bool blast_or_later_archetype = false;
  bool raised = false;
  for (const ro::sim::OwnedSkill& owned : field.hero.skills) {
    const ro::sim::SurvivorsSkillRule* rule = ro::sim::FindSkill(GoldenRules(), owned.skill_id);
    REQUIRE(rule != nullptr);
    blast_or_later_archetype =
        blast_or_later_archetype || rule->archetype >= ro::sim::SkillArchetype::kClusterBlast;
    raised = raised || owned.level >= 2;
  }
  CHECK(blast_or_later_archetype);
  CHECK(raised);

  const auto replayed = RoundTrip(play->journal());
  REQUIRE(replayed.has_value());
  CHECK(*replayed == tally);

  // Le bilan en or, champ par champ (voir survivors_golden.h), épinglé avec sa
  // version de règles : si cet essai change, montez kSurvivorsRulesetVersion et
  // kGoldenRulesetVersion ensemble.
  CHECK(ro::sim::kSurvivorsRulesetVersion == ro::test_support::kGoldenRulesetVersion);
  using ro::test_support::kGoldenTally;
  CHECK(tally.survived_steps == kGoldenTally.survived_steps);
  CHECK(tally.kills == kGoldenTally.kills);
  CHECK(tally.boss_kills == kGoldenTally.boss_kills);
  CHECK(tally.kill_points == kGoldenTally.kill_points);
  CHECK(tally.level == kGoldenTally.level);
  CHECK(tally.exp_total == kGoldenTally.exp_total);
  CHECK(tally.hp_left == kGoldenTally.hp_left);
  CHECK(tally.damage_dealt == kGoldenTally.damage_dealt);
  CHECK(tally.gems_gathered == kGoldenTally.gems_gathered);
  CHECK(tally.foes_alive == kGoldenTally.foes_alive);
  CHECK(tally.outcome == kGoldenTally.outcome);
  CHECK(tally.elite_kills == kGoldenTally.elite_kills);
  CHECK(tally.chests_opened == kGoldenTally.chests_opened);
  CHECK(tally.evolutions == kGoldenTally.evolutions);
  CHECK(tally == kGoldenTally);

  REQUIRE(field.hero.skills.size() == ro::test_support::kGoldenSkills.size());
  for (std::size_t slot = 0; slot < field.hero.skills.size(); ++slot) {
    CHECK(field.hero.skills[slot].skill_id == ro::test_support::kGoldenSkills[slot].id);
    CHECK(field.hero.skills[slot].level == ro::test_support::kGoldenSkills[slot].level);
  }
  REQUIRE(field.hero.cards.size() == ro::test_support::kGoldenCards.size());
  for (std::size_t slot = 0; slot < field.hero.cards.size(); ++slot) {
    CHECK(field.hero.cards[slot].card_item_id == ro::test_support::kGoldenCards[slot].id);
    CHECK(field.hero.cards[slot].level == ro::test_support::kGoldenCards[slot].level);
  }
  CHECK(field.hero.position.x == ro::test_support::kGoldenHeroPosition.x);
  CHECK(field.hero.position.y == ro::test_support::kGoldenHeroPosition.y);
  const auto foes = ro::test_support::FirstFoes(field);
  REQUIRE(field.foes.size() >= foes.size());
  for (std::size_t index = 0; index < foes.size(); ++index) {
    const ro::test_support::GoldenFoe& pinned = ro::test_support::kGoldenFoes[index];
    CHECK(foes[index].serial == pinned.serial);
    CHECK(foes[index].foe_id == pinned.foe_id);
    CHECK(foes[index].position.x == pinned.position.x);
    CHECK(foes[index].position.y == pinned.position.y);
    CHECK(foes[index].hp == pinned.hp);
  }
}

TEST_CASE("Rejeu survivors : même graine, même partie ; graine voisine, autre partie",
          "[sim][survivors]") {
  // Une minute suffit : la partie entière est déjà épinglée par l'essai en or,
  // et rejouée depuis ses octets dans une autre simulation.
  constexpr std::uint32_t kMinuteSteps = ro::sim::kStepsPerMinute;
  std::optional<SurvivorsPlay> first = PlayGolden(kGoldenSeed, kMinuteSteps);
  std::optional<SurvivorsPlay> second = PlayGolden(kGoldenSeed, kMinuteSteps);
  REQUIRE(first.has_value());
  REQUIRE(second.has_value());
  CHECK(first->run().tally() == second->run().tally());
  CHECK(first->journal() == second->journal());
  CHECK(first->run().field().hero.position == second->run().field().hero.position);
  CHECK(ro::test_support::FirstFoes(first->run().field()) ==
        ro::test_support::FirstFoes(second->run().field()));

  // La minute est le début de la partie en or : même départ, et ses entrées
  // sont celles de la partie entière jusqu'à ce pas.
  const SurvivorsJournal& whole = GoldenPlay().journal();
  CHECK(first->journal().start == whole.start);
  const std::vector<JournalEntry>& minute = first->journal().entries;
  REQUIRE(minute.size() <= whole.entries.size());
  CHECK(std::equal(minute.begin(), minute.end(), whole.entries.begin()));
  if (minute.size() < whole.entries.size()) {
    CHECK(whole.entries[minute.size()].step >= kMinuteSteps);
  }

  // La graine voisine diverge dès la première minute.
  std::optional<SurvivorsPlay> other = PlayGolden(kGoldenSeed + 1, kMinuteSteps);
  REQUIRE(other.has_value());
  CHECK(first->run().tally().survived_steps == other->run().tally().survived_steps);
  CHECK_FALSE(first->run().tally() == other->run().tally());
}

TEST_CASE("Rejeu survivors : rejoué jusqu'à un pas, la partie de ce pas", "[sim][survivors]") {
  constexpr std::uint32_t kMinuteSteps = ro::sim::kStepsPerMinute;
  constexpr std::uint32_t kHalfMinuteSteps = kMinuteSteps / 2;
  std::optional<SurvivorsPlay> whole = PlayGolden(kGoldenSeed, kMinuteSteps);
  std::optional<SurvivorsPlay> part = PlayGolden(kGoldenSeed, kHalfMinuteSteps);
  REQUIRE(whole.has_value());
  REQUIRE(part.has_value());
  const ro::sim::SurvivorsField& expected = part->run().field();

  const auto replayed = ro::sim::ReplaySurvivorsTo(GoldenRules(), PillarArena(kGoldenArenaCells),
                                                   whole->journal(), expected.step);
  REQUIRE(replayed.has_value());
  CHECK(replayed->field().step == expected.step);
  CHECK(replayed->field().hero.position == expected.hero.position);
  CHECK(replayed->field().foes.size() == expected.foes.size());
  CHECK(replayed->tally().kills == part->run().tally().kills);

  // Au-delà du pas final, la partie s'arrête au pas final.
  const std::uint32_t final_step = whole->journal().final_step;
  const auto beyond = ro::sim::ReplaySurvivorsTo(GoldenRules(), PillarArena(kGoldenArenaCells),
                                                 whole->journal(), final_step + kMinuteSteps);
  REQUIRE(beyond.has_value());
  CHECK(beyond->field().step == final_step);
}

TEST_CASE("Rejeu survivors : un abandon pendant un choix se rejoue", "[sim][survivors]") {
  SurvivorsPlay play = PlayToFirstChoice();
  REQUIRE(play.run().field().phase == SurvivorsPhase::kChoosing);
  const std::uint32_t choice_step = play.run().field().step;
  play.Abandon();
  REQUIRE(play.run().tally().outcome == SurvivorsOutcome::kAbandoned);
  REQUIRE_FALSE(play.journal().entries.empty());
  CHECK(play.journal().entries.back() ==
        JournalEntry{.step = choice_step, .kind = JournalKind::kAbandon, .value = 0});
  CHECK(play.journal().final_step == choice_step);

  const auto replayed = RoundTrip(play.journal());
  REQUIRE(replayed.has_value());
  CHECK(*replayed == play.run().tally());
  CHECK(replayed->outcome == SurvivorsOutcome::kAbandoned);
}
