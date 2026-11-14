// L'ÉTAGE DU MODE SURVIVORS SANS ÉCRAN : la pause, le choix et le coffre
// cessent d'avancer la partie sans toucher au journal, la reprise ne rattrape
// pas le temps, et une partie jouée par le robot à travers l'étage est celle
// que PlayBot joue seul.

#include "survivors/survivors_stage.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_bot.h"
#include "ro/sim/survivors_journal.h"
#include "ro/sim/survivors_meta.h"
#include "ro/sim/survivors_play.h"
#include "ro/sim/survivors_records.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

#include "support/survivors_fixture.h"
#include "support/survivors_golden.h"
#include "support/survivors_golden_bot.h"
#include "survivors/survivors_book.h"
#include "survivors/survivors_codex_keys.h"
#include "survivors/survivors_save.h"
#include "survivors/survivors_sightings.h"

using moonclient::CodexCategory;
using moonclient::kArrivalMs;
using moonclient::kDeathVeilMs;
using moonclient::kInputGuardMs;
using moonclient::kVictoryBannerMs;
using moonclient::PauseCause;
using moonclient::SurvivorsIntent;
using moonclient::SurvivorsScreen;
using moonclient::SurvivorsStage;
using ro::sim::kSurvivorsStepMs;
using ro::sim::SurvivorsBot;
using ro::sim::SurvivorsField;
using ro::sim::SurvivorsOutcome;
using ro::test_support::GoldenBotMostSteps;
using ro::test_support::GoldenRules;
using ro::test_support::kGoldenArenaCells;
using ro::test_support::kGoldenBotTally;
using ro::test_support::kGoldenFirstSkillId;
using ro::test_support::kGoldenSeed;
using ro::test_support::PillarArena;

namespace {

constexpr std::uint8_t kNorthHeading = 0;
// Une minute de partie : assez pour voir plusieurs choix.
constexpr std::uint32_t kEquivalenceSteps = 3000;
// L'élite rapide de l'essai du coffre : à 5 s, aux PV de son espèce.
constexpr std::uint32_t kQuickEliteMs = 5000;
constexpr std::int32_t kPlainHpPct = 100;
// Des PV de monstre hors de portée des skills d'une minute de partie.
constexpr std::int32_t kSturdyFoeHp = 1000000;
// Une pause, un choix, un coffre tenus longtemps : dix secondes d'écran.
constexpr std::uint32_t kLongHoldFrames = 500;
constexpr std::uint32_t kFewSteps = 50;
// L'image du chargement de la carte, en multiples de l'arrivée.
constexpr std::uint32_t kLongArrivalFrameShare = 4;
// Une borne contre une boucle d'essai qui ne finirait pas.
constexpr std::uint32_t kMostFrames = 200000;
// L'arène des règles minuscules, et deux plantes du jeu (Red et Yellow Plant).
constexpr std::int32_t kTinyArenaSide = 64;
constexpr std::uint16_t kOutsidePlantId = 1078;
constexpr std::uint16_t kInsidePlantId = 1081;

// Un étage dont la partie en or vient d'arriver : la descente est finie.
void BeginGolden(SurvivorsStage& stage) {
  stage.Open();
  const auto begun = stage.Begin(GoldenRules(), PillarArena(kGoldenArenaCells),
                                 ro::test_support::GoldenStart(ro::test_support::kGoldenSeed));
  REQUIRE(begun.has_value());
  stage.Loaded();
  stage.Advance(kArrivalMs, SurvivorsIntent{});
  REQUIRE(stage.screen() == SurvivorsScreen::kPlaying);
}

[[nodiscard]] const SurvivorsField& FieldOf(const SurvivorsStage& stage) {
  return stage.play()->run().field();
}

// Une image de 20 ms, le robot aux commandes : il marche en kPlaying, choisit
// dès que la garde le permet, passe roulette, liste et écrans de fin d'un
// clic. Le robot n'est consulté qu'aux images où la partie avance, comme
// PlayBot le fait.
void DriveFrame(SurvivorsStage& stage, SurvivorsBot& bot) {
  SurvivorsIntent intent;
  switch (stage.screen()) {
    case SurvivorsScreen::kPlaying:
      intent.heading = bot.Steer(FieldOf(stage)).heading;
      break;
    case SurvivorsScreen::kChoosing:
      if (stage.accepts_input()) intent.answer = bot.Choose(FieldOf(stage));
      break;
    case SurvivorsScreen::kChest:
    case SurvivorsScreen::kDying:
    case SurvivorsScreen::kWon:
      intent.proceed = stage.accepts_input();
      break;
    default:
      break;
  }
  stage.Advance(kSurvivorsStepMs, intent);
}

// Joue jusqu'à `steps` pas de partie ou jusqu'au bilan.
void DriveSteps(SurvivorsStage& stage, SurvivorsBot& bot, std::uint32_t steps) {
  for (std::uint32_t frame = 0; frame < kMostFrames; ++frame) {
    if (stage.screen() == SurvivorsScreen::kSummary || FieldOf(stage).step >= steps) return;
    DriveFrame(stage, bot);
  }
  FAIL("l'étage n'a pas atteint le pas voulu");
}

// Joue jusqu'à l'écran `screen`.
void DriveUntil(SurvivorsStage& stage, SurvivorsBot& bot, SurvivorsScreen screen) {
  for (std::uint32_t frame = 0; frame < kMostFrames; ++frame) {
    if (stage.screen() == screen) return;
    DriveFrame(stage, bot);
  }
  FAIL("l'écran attendu n'est jamais venu");
}

[[nodiscard]] SurvivorsIntent Escape() { return SurvivorsIntent{.escape = true}; }

}  // namespace

TEST_CASE("SurvivorsStage : la pause n'avance aucun pas et ne touche pas le journal",
          "[app][survivors]") {
  SurvivorsStage stage;
  BeginGolden(stage);
  for (std::uint32_t frame = 0; frame < kFewSteps; ++frame) {
    stage.Advance(kSurvivorsStepMs, SurvivorsIntent{.heading = kNorthHeading});
  }
  stage.Advance(kSurvivorsStepMs, Escape());
  REQUIRE(stage.screen() == SurvivorsScreen::kPaused);
  CHECK(stage.pause_cause() == PauseCause::kPlayer);
  const std::uint32_t paused_step = FieldOf(stage).step;
  const ro::sim::SurvivorsJournal journal = stage.play()->journal();
  const std::uint64_t presentation_ms = stage.presentation_ms();
  for (std::uint32_t frame = 0; frame < kLongHoldFrames; ++frame) {
    stage.Advance(kSurvivorsStepMs, SurvivorsIntent{.heading = kNorthHeading});
    CHECK(stage.news().hits.empty());
  }
  CHECK(FieldOf(stage).step == paused_step);
  CHECK(stage.play()->journal() == journal);
  CHECK(stage.presentation_ms() == presentation_ms);
}

TEST_CASE("SurvivorsStage : la reprise attend la garde et ne rattrape pas le temps",
          "[app][survivors]") {
  SurvivorsStage stage;
  BeginGolden(stage);
  stage.Advance(kSurvivorsStepMs, SurvivorsIntent{.focus_lost = true});
  REQUIRE(stage.screen() == SurvivorsScreen::kPaused);
  CHECK(stage.pause_cause() == PauseCause::kFocusLost);
  // Le retour du focus ne reprend pas : seul « Reprendre » ou Échap le font,
  // et pas dans la garde.
  stage.Advance(kInputGuardMs - kSurvivorsStepMs - 1, SurvivorsIntent{.resume = true});
  CHECK(stage.screen() == SurvivorsScreen::kPaused);
  for (std::uint32_t frame = 0; frame < kLongHoldFrames; ++frame) {
    stage.Advance(kSurvivorsStepMs, SurvivorsIntent{});
  }
  CHECK(stage.screen() == SurvivorsScreen::kPaused);
  const std::uint32_t paused_step = FieldOf(stage).step;
  stage.Advance(kSurvivorsStepMs, SurvivorsIntent{.resume = true});
  REQUIRE(stage.screen() == SurvivorsScreen::kPlaying);
  CHECK(FieldOf(stage).step == paused_step);
  stage.Advance(kSurvivorsStepMs, SurvivorsIntent{});
  CHECK(FieldOf(stage).step == paused_step + 1);
}

TEST_CASE("SurvivorsStage : la saisie met en pause, et la partie reprend quand elle se referme",
          "[app][survivors]") {
  SurvivorsStage stage;
  BeginGolden(stage);
  stage.Advance(kSurvivorsStepMs, SurvivorsIntent{});
  stage.Advance(kSurvivorsStepMs, SurvivorsIntent{.typing = true});
  REQUIRE(stage.screen() == SurvivorsScreen::kPaused);
  CHECK(stage.pause_cause() == PauseCause::kTyping);
  const std::uint32_t paused_step = FieldOf(stage).step;
  for (std::uint32_t frame = 0; frame < kLongHoldFrames; ++frame) {
    stage.Advance(kSurvivorsStepMs, SurvivorsIntent{.typing = true});
  }
  CHECK(FieldOf(stage).step == paused_step);
  // La saisie se referme : la partie reprend sans attendre la garde.
  stage.Advance(kSurvivorsStepMs, SurvivorsIntent{});
  REQUIRE(stage.screen() == SurvivorsScreen::kPlaying);
  stage.Advance(kSurvivorsStepMs, SurvivorsIntent{});
  CHECK(FieldOf(stage).step == paused_step + 1);
}

TEST_CASE("SurvivorsStage : la pause du joueur ne se lève pas quand une saisie se referme",
          "[app][survivors]") {
  SurvivorsStage stage;
  BeginGolden(stage);
  stage.Advance(kSurvivorsStepMs, Escape());
  REQUIRE(stage.screen() == SurvivorsScreen::kPaused);
  stage.Advance(kSurvivorsStepMs, SurvivorsIntent{.typing = true});
  stage.Advance(kSurvivorsStepMs, SurvivorsIntent{});
  CHECK(stage.screen() == SurvivorsScreen::kPaused);
  CHECK(stage.pause_cause() == PauseCause::kPlayer);
}

TEST_CASE("SurvivorsStage : une alerte du monde réel met en pause, même à l'arrivée",
          "[app][survivors]") {
  SurvivorsStage stage;
  stage.Open();
  REQUIRE(stage
              .Begin(GoldenRules(), PillarArena(kGoldenArenaCells),
                     ro::test_support::GoldenStart(ro::test_support::kGoldenSeed))
              .has_value());
  CHECK(stage.owns_screen());
  stage.Loaded();
  REQUIRE(stage.screen() == SurvivorsScreen::kArriving);
  stage.Advance(kSurvivorsStepMs, SurvivorsIntent{});
  stage.Advance(kSurvivorsStepMs, SurvivorsIntent{.world_alert = true});
  REQUIRE(stage.screen() == SurvivorsScreen::kPaused);
  CHECK(stage.pause_cause() == PauseCause::kWorldAlert);
  // La descente s'arrête avec la pause et reprend où elle l'a laissée : la
  // horde ne part pas sous une caméra encore haute.
  for (std::uint32_t frame = 0; frame < kLongHoldFrames; ++frame) {
    stage.Advance(kSurvivorsStepMs, SurvivorsIntent{});
  }
  CHECK(stage.arrival_ms() == kSurvivorsStepMs);
  stage.Advance(kSurvivorsStepMs, Escape());
  REQUIRE(stage.screen() == SurvivorsScreen::kArriving);
  stage.Advance(kArrivalMs - kSurvivorsStepMs - 1, SurvivorsIntent{});
  CHECK(stage.screen() == SurvivorsScreen::kArriving);
  CHECK(FieldOf(stage).step == 0);
  stage.Advance(1, SurvivorsIntent{});
  CHECK(stage.screen() == SurvivorsScreen::kPlaying);
  CHECK(stage.arrival_ms() == kArrivalMs);
}

TEST_CASE("SurvivorsStage : une longue image d'arrivée pose la caméra avant que la horde parte",
          "[app][survivors]") {
  SurvivorsStage stage;
  stage.Open();
  REQUIRE(stage
              .Begin(GoldenRules(), PillarArena(kGoldenArenaCells),
                     ro::test_support::GoldenStart(ro::test_support::kGoldenSeed))
              .has_value());
  stage.Loaded();
  // L'image du chargement synchrone de la carte : plusieurs secondes d'un coup.
  stage.Advance(kArrivalMs * kLongArrivalFrameShare, SurvivorsIntent{});
  CHECK(stage.screen() == SurvivorsScreen::kPlaying);
  CHECK(stage.arrival_ms() == kArrivalMs);
  CHECK(FieldOf(stage).step == 0);
}

TEST_CASE("SurvivorsStage : une alerte pendant un choix met en pause au retour au jeu",
          "[app][survivors]") {
  SurvivorsStage stage;
  BeginGolden(stage);
  SurvivorsBot bot(ro::sim::MageBotProfile());
  DriveUntil(stage, bot, SurvivorsScreen::kChoosing);
  const std::uint32_t step = FieldOf(stage).step;
  stage.Advance(kSurvivorsStepMs, SurvivorsIntent{.world_alert = true});
  CHECK(stage.screen() == SurvivorsScreen::kChoosing);
  // Le choix, puis ceux et les coffres qu'il enchaîne, se finissent ; la
  // pause vient ensuite, avec la cause de l'alerte.
  for (std::uint32_t frame = 0; frame < kMostFrames; ++frame) {
    const SurvivorsScreen screen = stage.screen();
    if (screen != SurvivorsScreen::kChoosing && screen != SurvivorsScreen::kChest) break;
    DriveFrame(stage, bot);
  }
  REQUIRE(stage.screen() == SurvivorsScreen::kPaused);
  CHECK(stage.pause_cause() == PauseCause::kWorldAlert);
  CHECK(FieldOf(stage).step == step);
  stage.Advance(kInputGuardMs, SurvivorsIntent{.resume = true});
  CHECK(stage.screen() == SurvivorsScreen::kPlaying);
}

TEST_CASE("SurvivorsStage : le choix fige l'horloge de présentation et garde 400 ms",
          "[app][survivors]") {
  SurvivorsStage stage;
  BeginGolden(stage);
  SurvivorsBot bot(ro::sim::MageBotProfile());
  for (std::uint32_t frame = 0; frame < kMostFrames && stage.screen() != SurvivorsScreen::kChoosing;
       ++frame) {
    stage.Advance(kSurvivorsStepMs, SurvivorsIntent{.heading = bot.Steer(FieldOf(stage)).heading});
  }
  REQUIRE(stage.screen() == SurvivorsScreen::kChoosing);
  const std::uint64_t presentation_ms = stage.presentation_ms();
  const std::uint32_t step = FieldOf(stage).step;
  const ro::sim::SurvivorsAnswer answer = bot.Choose(FieldOf(stage));
  // Dans la garde, la réponse est refusée.
  stage.Advance(kInputGuardMs - 1, SurvivorsIntent{.answer = answer});
  CHECK(stage.screen() == SurvivorsScreen::kChoosing);
  CHECK(stage.presentation_ms() == presentation_ms);
  CHECK(FieldOf(stage).step == step);
  stage.Advance(1, SurvivorsIntent{.answer = answer});
  CHECK(stage.screen() != SurvivorsScreen::kChoosing);
  CHECK(stage.presentation_ms() == presentation_ms);
  CHECK(FieldOf(stage).step == step);
}

TEST_CASE("SurvivorsStage : le coffre fige le monde jusqu'au clic qui ferme sa liste",
          "[app][survivors]") {
  // Une élite dès les premières secondes, sans PV majorés : son coffre tombe
  // vite, sans jouer deux minutes de partie.
  ro::sim::SurvivorsRules rules = GoldenRules();
  rules.elites.first_ms = kQuickEliteMs;
  rules.elites.hp_pct = kPlainHpPct;
  SurvivorsStage stage;
  stage.Open();
  REQUIRE(stage
              .Begin(rules, PillarArena(kGoldenArenaCells),
                     ro::test_support::GoldenStart(ro::test_support::kGoldenSeed))
              .has_value());
  stage.Loaded();
  stage.Advance(kArrivalMs, SurvivorsIntent{});
  SurvivorsBot bot(ro::sim::MageBotProfile());
  DriveUntil(stage, bot, SurvivorsScreen::kChest);
  REQUIRE_FALSE(stage.chests().empty());
  const std::uint32_t step = FieldOf(stage).step;
  // La roulette se passe d'elle-même ; la liste reste jusqu'au clic.
  for (std::uint32_t frame = 0; frame < kLongHoldFrames; ++frame) {
    stage.Advance(kSurvivorsStepMs, SurvivorsIntent{});
  }
  CHECK(stage.screen() == SurvivorsScreen::kChest);
  CHECK(stage.chest_list_shown());
  CHECK(FieldOf(stage).step == step);
  stage.Advance(kSurvivorsStepMs, SurvivorsIntent{.proceed = true});
  CHECK(stage.screen() != SurvivorsScreen::kChest);
  CHECK(stage.chests().empty());
  CHECK(FieldOf(stage).step == step);
}

TEST_CASE("SurvivorsStage : le robot à travers l'étage joue la partie de PlayBot",
          "[app][survivors]") {
  SurvivorsStage stage;
  BeginGolden(stage);
  SurvivorsBot staged_bot(ro::sim::MageBotProfile());
  DriveSteps(stage, staged_bot, kEquivalenceSteps);

  auto run =
      ro::sim::SurvivorsRun::Start(GoldenRules(), PillarArena(kGoldenArenaCells),
                                   ro::test_support::GoldenStart(ro::test_support::kGoldenSeed));
  REQUIRE(run.has_value());
  ro::sim::SurvivorsPlay alone(std::move(*run));
  SurvivorsBot lone_bot(ro::sim::MageBotProfile());
  ro::sim::PlayBot(alone, lone_bot, kEquivalenceSteps, nullptr);
  // Les deux s'arrêtent au même pas, un choix ouvert par ce pas restant sans
  // réponse ; le journal est le même, entrée pour entrée.
  CHECK(FieldOf(stage).step == alone.run().field().step);
  CHECK(stage.play()->journal() == alone.journal());
  CHECK(FieldOf(stage).tally == alone.run().field().tally);
  CHECK(FieldOf(stage).recap == alone.run().field().recap);
}

TEST_CASE("SurvivorsStage : un journal joué par l'étage se rejoue au même bilan",
          "[app][survivors]") {
  SurvivorsStage stage;
  BeginGolden(stage);
  SurvivorsBot bot(ro::sim::MageBotProfile());
  DriveSteps(stage, bot, kEquivalenceSteps / 3);
  // Une pause puis l'abandon, entre deux choix : la partie se clôt au bilan.
  while (stage.screen() != SurvivorsScreen::kPlaying) DriveFrame(stage, bot);
  stage.Advance(kSurvivorsStepMs, Escape());
  stage.Advance(kSurvivorsStepMs, SurvivorsIntent{.abandon = true});
  REQUIRE(stage.screen() == SurvivorsScreen::kSummary);
  REQUIRE(stage.ending() != nullptr);
  CHECK(stage.ending()->tally.outcome == SurvivorsOutcome::kAbandoned);
  const auto replayed = ro::sim::ReplaySurvivors(GoldenRules(), PillarArena(kGoldenArenaCells),
                                                 stage.play()->journal());
  REQUIRE(replayed.has_value());
  CHECK(*replayed == FieldOf(stage).tally);
  CHECK(stage.ending()->tally == FieldOf(stage).tally);
}

TEST_CASE("SurvivorsStage : la liaison perdue clôt la partie en cours, une seule fois",
          "[app][survivors]") {
  SurvivorsStage stage;
  CHECK_FALSE(stage.Freeze());
  // Une partie ne commence que depuis la préparation ou le bilan.
  CHECK_FALSE(stage
                  .Begin(GoldenRules(), PillarArena(kGoldenArenaCells),
                         ro::test_support::GoldenStart(ro::test_support::kGoldenSeed))
                  .has_value());
  BeginGolden(stage);
  for (std::uint32_t frame = 0; frame < kFewSteps; ++frame) {
    stage.Advance(kSurvivorsStepMs, SurvivorsIntent{.heading = kNorthHeading});
  }
  const std::uint32_t cut_step = FieldOf(stage).step;
  CHECK(stage.Freeze());
  CHECK(stage.screen() == SurvivorsScreen::kSummary);
  REQUIRE(stage.ending() != nullptr);
  CHECK(stage.ending()->tally.outcome == SurvivorsOutcome::kAbandoned);
  CHECK(stage.ending()->tally.survived_steps == cut_step);
  CHECK(stage.play()->journal().entries.back().kind == ro::sim::JournalKind::kAbandon);
  CHECK_FALSE(stage.Freeze());
  // Le bilan relu d'une partie interrompue se montre sans partie.
  moonclient::SurvivorsEnding ending = *stage.ending();
  stage.Close();
  CHECK(stage.screen() == SurvivorsScreen::kClosed);
  stage.ShowEnding(ending);
  CHECK(stage.screen() == SurvivorsScreen::kSummary);
  CHECK(stage.play() == nullptr);
  CHECK(*stage.ending() == ending);
}

namespace {

constexpr std::int32_t kTinyArenaCells = 64;
// Un héros que rien n'abat en une partie courte.
constexpr std::int32_t kSturdyHeroHp = 1000000;
// Une partie des règles minuscules tenue en deux secondes.
constexpr std::uint32_t kShortRunMs = 2000;

// Les règles minuscules, au héros de `hero_hp` PV devant des monstres que ses
// skills n'abattent pas, sur des tables que l'appelant garde en vie avec la
// partie.
[[nodiscard]] ro::sim::SurvivorsRules SturdyFoesRules(std::array<ro::sim::ClassRule, 1>& classes,
                                                      std::array<ro::sim::FoeRule, 2>& foes,
                                                      std::int32_t hero_hp) {
  classes = {ro::test_support::kTinyClasses[0]};
  classes[0].max_hp = hero_hp;
  foes = ro::test_support::kTinyFoes;
  for (ro::sim::FoeRule& foe : foes) foe.hp = kSturdyFoeHp;
  ro::sim::SurvivorsRules rules = ro::test_support::TinyRules();
  rules.classes = classes;
  rules.foes = foes;
  return rules;
}

// La partie des règles minuscules, le héros immobile jusqu'à la fin : il
// prend la première offre de chaque choix et passe chaque coffre, dès la
// garde passée.
void PlayStillUntilOver(SurvivorsStage& stage, const ro::sim::SurvivorsRules& rules) {
  stage.Open();
  REQUIRE(stage.Begin(rules, PillarArena(kTinyArenaCells), ro::test_support::TinyStart(kGoldenSeed))
              .has_value());
  stage.Loaded();
  stage.Advance(kArrivalMs, SurvivorsIntent{});
  for (std::uint32_t frame = 0; frame < kMostFrames; ++frame) {
    const SurvivorsScreen screen = stage.screen();
    SurvivorsIntent intent;
    if (screen == SurvivorsScreen::kChoosing) {
      intent.answer = ro::sim::SurvivorsAnswer{};
    } else if (screen == SurvivorsScreen::kChest) {
      intent.proceed = stage.accepts_input();
    } else if (screen != SurvivorsScreen::kPlaying) {
      return;
    }
    stage.Advance(kSurvivorsStepMs, intent);
  }
  FAIL("la partie n'a pas fini");
}

}  // namespace

TEST_CASE("SurvivorsStage : la mort passe par son voile avant le bilan", "[app][survivors]") {
  // Un héros d'un PV, immobile : le premier contact le tue.
  std::array<ro::sim::ClassRule, 1> classes{};
  std::array<ro::sim::FoeRule, 2> foes{};
  const ro::sim::SurvivorsRules rules = SturdyFoesRules(classes, foes, 1);
  SurvivorsStage stage;
  PlayStillUntilOver(stage, rules);
  REQUIRE(stage.screen() == SurvivorsScreen::kDying);
  CHECK(stage.entered() == SurvivorsScreen::kDying);
  REQUIRE(stage.ending() != nullptr);
  CHECK(stage.ending()->tally.outcome == SurvivorsOutcome::kFell);
  // L'épilogue fait courir l'horloge de présentation : la chute se joue.
  const std::uint64_t fallen_ms = stage.presentation_ms();
  stage.Advance(kSurvivorsStepMs, SurvivorsIntent{.proceed = true});
  CHECK(stage.screen() == SurvivorsScreen::kDying);
  CHECK(stage.presentation_ms() == fallen_ms + kSurvivorsStepMs);
  stage.Advance(kDeathVeilMs, SurvivorsIntent{});
  CHECK(stage.screen() == SurvivorsScreen::kSummary);
}

TEST_CASE("SurvivorsStage : une partie jouée par l'étage crédite le carnet en mémoire",
          "[app][survivors]") {
  std::array<ro::sim::ClassRule, 1> classes{};
  std::array<ro::sim::FoeRule, 2> foes{};
  const ro::sim::SurvivorsRules rules = SturdyFoesRules(classes, foes, 1);
  SurvivorsStage stage;
  PlayStillUntilOver(stage, rules);
  stage.Advance(kDeathVeilMs, SurvivorsIntent{});
  REQUIRE(stage.screen() == SurvivorsScreen::kSummary);
  const moonclient::SurvivorsEnding& ending = *stage.ending();
  CHECK(ending.board == moonclient::SurvivorsRunBoard{});
  moonclient::SurvivorsSave save;
  constexpr std::int64_t kNowUtcS = 1'793'000'000;
  const moonclient::SurvivorsCredit credit =
      moonclient::CreditRun(rules, save, ending, FieldOf(stage).start, kNowUtcS);
  CHECK(credit.zeny == ro::sim::RunZeny(ending.tally, ending.zeny, SurvivorsOutcome::kFell));
  CHECK(save.meta.zeny == credit.zeny);
  CHECK(save.album.counters.runs == 1);
  CHECK(credit.record == moonclient::RecordVerdict::kPlaced);
  REQUIRE(save.records.size() == 1);
  REQUIRE(save.records.front().runs.size() == 1);
  CHECK(save.records.front().runs.front().played_at_utc_s == kNowUtcS);
}

TEST_CASE("SurvivorsStage : la liaison perdue sous le voile de la mort garde la partie finie",
          "[app][survivors]") {
  std::array<ro::sim::ClassRule, 1> classes{};
  std::array<ro::sim::FoeRule, 2> foes{};
  const ro::sim::SurvivorsRules rules = SturdyFoesRules(classes, foes, 1);
  SurvivorsStage stage;
  PlayStillUntilOver(stage, rules);
  REQUIRE(stage.screen() == SurvivorsScreen::kDying);
  const ro::sim::SurvivorsJournal journal = stage.play()->journal();
  // Le bilan n'a pas paru : app doit écrire le journal et la fin, telle
  // quelle, sans abandon ajouté.
  CHECK(stage.Freeze());
  CHECK(stage.screen() == SurvivorsScreen::kSummary);
  REQUIRE(stage.ending() != nullptr);
  CHECK(stage.ending()->tally.outcome == SurvivorsOutcome::kFell);
  CHECK(stage.play()->journal() == journal);
  // Le bilan montré, il n'y a plus rien à sauver.
  CHECK_FALSE(stage.Freeze());
}

TEST_CASE("SurvivorsStage : une partie tenue jusqu'au bout passe par le bandeau de victoire",
          "[app][survivors]") {
  std::array<ro::sim::ClassRule, 1> classes{};
  std::array<ro::sim::FoeRule, 2> foes{};
  ro::sim::SurvivorsRules rules = SturdyFoesRules(classes, foes, kSturdyHeroHp);
  rules.run_ms = kShortRunMs;
  SurvivorsStage stage;
  PlayStillUntilOver(stage, rules);
  REQUIRE(stage.screen() == SurvivorsScreen::kWon);
  CHECK(stage.entered() == SurvivorsScreen::kWon);
  REQUIRE(stage.ending() != nullptr);
  CHECK(stage.ending()->tally.outcome == SurvivorsOutcome::kSurvived);
  // Le clic dans la garde est refusé ; l'épilogue fait courir l'horloge.
  const std::uint64_t won_ms = stage.presentation_ms();
  stage.Advance(kSurvivorsStepMs, SurvivorsIntent{.proceed = true});
  CHECK(stage.screen() == SurvivorsScreen::kWon);
  CHECK(stage.presentation_ms() == won_ms + kSurvivorsStepMs);
  // Le bandeau tient kVictoryBannerMs, pas le voile de la mort.
  stage.Advance(kVictoryBannerMs - kSurvivorsStepMs - 1, SurvivorsIntent{});
  CHECK(stage.screen() == SurvivorsScreen::kWon);
  stage.Advance(1, SurvivorsIntent{});
  CHECK(stage.screen() == SurvivorsScreen::kSummary);
}

// ── LE RELEVÉ DU CODEX ─────────────────────────────────────────────────────

TEST_CASE("SurvivorsStage : un monstre né est vu, et le relevé repart vide au départ suivant",
          "[app][survivors][codex]") {
  SurvivorsStage stage;
  BeginGolden(stage);
  const auto& first_skill = *ro::sim::FindSkill(GoldenRules(), kGoldenFirstSkillId);
  CHECK(stage.sightings().Has(CodexCategory::kSkills, moonclient::CodexSkillKey(first_skill)));
  SurvivorsBot bot(ro::sim::MageBotProfile());
  DriveSteps(stage, bot, kEquivalenceSteps);
  REQUIRE_FALSE(FieldOf(stage).foes.empty());
  const std::uint16_t born = FieldOf(stage).foes.front().foe_id;
  CHECK(stage.sightings().Has(CodexCategory::kFoes, moonclient::CodexFoeKey(born)));
  // Une partie neuve part d'un relevé vierge : aucun monstre n'y est encore né.
  stage.Advance(kSurvivorsStepMs, Escape());
  for (std::uint32_t frame = 0; frame < kLongHoldFrames; ++frame) {
    stage.Advance(kSurvivorsStepMs, SurvivorsIntent{});
  }
  stage.Advance(kSurvivorsStepMs, SurvivorsIntent{.abandon = true});
  REQUIRE(stage.screen() == SurvivorsScreen::kSummary);
  REQUIRE(stage
              .Begin(GoldenRules(), PillarArena(kGoldenArenaCells),
                     ro::test_support::GoldenStart(ro::test_support::kGoldenSeed))
              .has_value());
  CHECK_FALSE(stage.sightings().Has(CodexCategory::kFoes, moonclient::CodexFoeKey(born)));
  CHECK(stage.sightings().lists[static_cast<std::size_t>(CodexCategory::kFoes)].empty());
}

TEST_CASE("SurvivorsStage : un sort offert à une montée de niveau est vu",
          "[app][survivors][codex]") {
  SurvivorsStage stage;
  BeginGolden(stage);
  SurvivorsBot bot(ro::sim::MageBotProfile());
  DriveUntil(stage, bot, SurvivorsScreen::kChoosing);
  std::size_t skill_offers = 0;
  for (const ro::sim::SurvivorsOffer& offer : FieldOf(stage).offers) {
    if (offer.kind != ro::sim::OfferKind::kNewSkill &&
        offer.kind != ro::sim::OfferKind::kSkillLevel) {
      continue;
    }
    ++skill_offers;
    const ro::sim::SurvivorsSkillRule* skill = ro::sim::FindSkill(GoldenRules(), offer.id);
    REQUIRE(skill != nullptr);
    CHECK(stage.sightings().Has(CodexCategory::kSkills, moonclient::CodexSkillKey(*skill)));
  }
  CHECK(skill_offers > 0);
}

TEST_CASE("Relevé du Codex : une plante n'est vue que dans le cadre autour du héros",
          "[app][survivors][codex]") {
  auto run =
      ro::sim::SurvivorsRun::Start(ro::test_support::TinyRules(), PillarArena(kTinyArenaSide),
                                   ro::test_support::TinyStart(kGoldenSeed));
  REQUIRE(run.has_value());
  SurvivorsField& field = run->field_for_tests();
  const ro::sim::SpawnFrame& frame = field.rules->spawn_frame;
  const ro::sim::FieldPoint hero = field.hero.position;
  field.plants.clear();
  field.plants.push_back(ro::sim::SurvivorsPlant{
      .foe_id = kOutsidePlantId,
      .position = hero + ro::sim::FieldPoint{frame.half_width_units + 1, 0}});
  field.plants.push_back(ro::sim::SurvivorsPlant{
      .foe_id = kInsidePlantId, .position = hero + ro::sim::FieldPoint{0, frame.north_units}});
  moonclient::SurvivorsCodexSeen seen;
  moonclient::NoteSightings(seen, field, ro::sim::SurvivorsNews{}, 0);
  CHECK(seen.Has(CodexCategory::kPlants, moonclient::CodexPlantKey(kInsidePlantId)));
  CHECK_FALSE(seen.Has(CodexCategory::kPlants, moonclient::CodexPlantKey(kOutsidePlantId)));
  // Le héros s'approche : la plante entre dans le cadre, et se voit.
  field.hero.position = field.plants.front().position;
  moonclient::NoteSightings(seen, field, ro::sim::SurvivorsNews{}, 0);
  CHECK(seen.Has(CodexCategory::kPlants, moonclient::CodexPlantKey(kOutsidePlantId)));
}

// La partie entière, cachée de ctest par son coût comme l'or long : le
// critère de fin du lot 5c, le bilan que `roc-cli survivors simulate` rend
// sur la même graine.
TEST_CASE("SurvivorsStage : la partie entière du robot à travers l'étage donne l'or long",
          "[app][survivors][.golden]") {
  SurvivorsStage stage;
  BeginGolden(stage);
  SurvivorsBot bot(ro::sim::MageBotProfile());
  DriveSteps(stage, bot, GoldenBotMostSteps());
  REQUIRE(stage.screen() == SurvivorsScreen::kSummary);
  CHECK(FieldOf(stage).tally == kGoldenBotTally);
}

TEST_CASE("survivors_stage : le bilan montre le corps de la fin, monture comprise",
          "[app][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const auto knight_class = std::ranges::find_if(
      rules.classes, [](const ro::sim::ClassRule& rule) { return rule.mount_card_item_id != 0; });
  REQUIRE(knight_class != rules.classes.end());
  const ro::sim::ClassRule& knight = *knight_class;

  moonclient::SurvivorsEnding ending;
  CHECK(moonclient::EndingJobOf(knight, ending) == knight.job_id);
  ending.tally.evolutions = 1;
  CHECK(moonclient::EndingJobOf(knight, ending) == knight.evolved_job_id);
  ending.cards.push_back(ro::sim::OwnedCard{.card_item_id = knight.mount_card_item_id});
  CHECK(moonclient::EndingJobOf(knight, ending) == knight.mounted_job_id);
  ending.tally.level = knight.reborn_level;
  CHECK(moonclient::EndingJobOf(knight, ending) == knight.reborn_mounted_job_id);
  ending.cards.clear();
  CHECK(moonclient::EndingJobOf(knight, ending) == knight.reborn_job_id);

  // En coop, le compte des évolutions est celui de l'équipe : seul celui du héros vaut.
  moonclient::SurvivorsEnding team_ending;
  team_ending.board.coop = true;
  team_ending.tally.evolutions = 1;
  CHECK(moonclient::EndingJobOf(knight, team_ending) == knight.job_id);
  team_ending.own_evolved = true;
  CHECK(moonclient::EndingJobOf(knight, team_ending) == knight.evolved_job_id);
}
