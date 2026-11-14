// LES TROPHÉES D'UNE PARTIE DE SURVIVORS, comptés comme le mode les compte :
// la liaison perdue fige la partie et l'inscrit dans `minigames.yml`, une
// partie abandonnée compte à son entrée dans le bilan, une seule fois, et une
// partie rejouée compte de nouveau. Sans étage des mini-jeux, la partie reste
// à compter.

#include "game/survivors_trophy_count.h"

#include <cstdint>
#include <filesystem>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/minigame_trophies.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"

#include "game/minigame_file.h"
#include "game/minigame_stage.h"
#include "support/survivors_fixture.h"
#include "support/temp_directory.h"
#include "survivors/survivors_door.h"
#include "survivors/survivors_stage.h"

using moonclient::kArrivalMs;
using moonclient::LinkLossSteps;
using moonclient::LinkLossStepsOf;
using moonclient::LoadMinigameRecord;
using moonclient::MinigameStage;
using moonclient::SurvivorsIntent;
using moonclient::SurvivorsScreen;
using moonclient::SurvivorsStage;
using moonclient::SurvivorsTrophyCount;
using ro::sim::kSurvivorsStepMs;
using ro::sim::MinigameTrophy;
using ro::test_support::TempDirectory;

namespace {

constexpr std::uint64_t kSeed = 5;
constexpr std::int32_t kArenaSideCells = 48;
// Les cinq minutes de partie de « Tenir bon », en millisecondes.
constexpr std::uint32_t kHoldOutMs = 300000;

// Les règles sans monstre, la partie durant deux fois « Tenir bon » : la
// victoire ne la close pas avant. La partie garde leur adresse.
[[nodiscard]] const ro::sim::SurvivorsRules& LongQuietRules() {
  static const ro::sim::SurvivorsRules kRules = [] {
    ro::sim::SurvivorsRules rules = ro::test_support::QuietRules();
    rules.run_ms = 2 * kHoldOutMs;
    return rules;
  }();
  return kRules;
}

// Un étage dont la partie, sans monstre, a tenu cinq minutes : elle gagne
// « Tenir bon » à son premier compte.
void BeginHeldOut(SurvivorsStage& stage) {
  stage.Open();
  const auto begun = stage.Begin(LongQuietRules(), ro::test_support::PillarArena(kArenaSideCells),
                                 ro::test_support::TinyStart(kSeed));
  REQUIRE(begun.has_value());
  stage.Loaded();
  stage.Advance(kArrivalMs, SurvivorsIntent{});
  REQUIRE(stage.screen() == SurvivorsScreen::kPlaying);
  while (stage.screen() == SurvivorsScreen::kPlaying &&
         stage.play()->run().elapsed_ms() < kHoldOutMs) {
    stage.Advance(kSurvivorsStepMs, SurvivorsIntent{});
  }
  REQUIRE(stage.screen() == SurvivorsScreen::kPlaying);
}

[[nodiscard]] bool HeldOutWritten(const std::filesystem::path& client_root) {
  return LoadMinigameRecord(client_root).record.Has(MinigameTrophy::kSurvivorsFiveMinutes);
}

// Abandonne la partie depuis la pause, et avance jusqu'à l'image qui entre
// dans le bilan, en la suivant comme le mode : chaque image compte si elle y
// entre.
void AbandonToSummary(SurvivorsStage& stage, SurvivorsTrophyCount& count, MinigameStage* minigames,
                      const std::filesystem::path& client_root) {
  stage.Advance(kSurvivorsStepMs, SurvivorsIntent{.escape = true});
  REQUIRE(stage.screen() == SurvivorsScreen::kPaused);
  stage.Advance(kSurvivorsStepMs, SurvivorsIntent{.abandon = true});
  count.FollowEntered(stage, minigames, client_root);
  while (stage.entered() != SurvivorsScreen::kSummary) {
    REQUIRE(stage.owns_screen());
    stage.Advance(kSurvivorsStepMs, SurvivorsIntent{.proceed = true});
    count.FollowEntered(stage, minigames, client_root);
  }
}

}  // namespace

TEST_CASE("Trophées survivors : la liaison perdue fige la partie et l'inscrit",
          "[app][survivors][minigames]") {
  const TempDirectory root;
  SurvivorsStage stage;
  MinigameStage minigames;
  SurvivorsTrophyCount count;
  BeginHeldOut(stage);
  const LinkLossSteps steps = LinkLossStepsOf(stage.screen(), false);
  REQUIRE(steps.freeze);
  CHECK(count.Freeze(stage, &minigames, root.path()));
  CHECK(count.counted());
  CHECK(HeldOutWritten(root.path()));
  // Une partie déjà close ne se fige pas deux fois.
  CHECK_FALSE(count.Freeze(stage, &minigames, root.path()));
}

TEST_CASE("Trophées survivors : la partie qui se ferme sans étage reste à compter",
          "[app][survivors][minigames]") {
  const TempDirectory root;
  SurvivorsStage stage;
  SurvivorsTrophyCount count;
  BeginHeldOut(stage);
  CHECK(count.Freeze(stage, nullptr, root.path()));
  CHECK_FALSE(count.counted());
  CHECK_FALSE(HeldOutWritten(root.path()));
  // L'étage des mini-jeux revenu, l'entrée au bilan la compte.
  MinigameStage minigames;
  count.FollowEntered(stage, &minigames, root.path());
  CHECK(count.counted());
  CHECK(HeldOutWritten(root.path()));
}

TEST_CASE("Trophées survivors : une partie abandonnée compte au bilan, une fois",
          "[app][survivors][minigames]") {
  const TempDirectory root;
  const TempDirectory other_root;
  SurvivorsStage stage;
  MinigameStage minigames;
  SurvivorsTrophyCount count;
  BeginHeldOut(stage);
  AbandonToSummary(stage, count, &minigames, root.path());
  CHECK(count.counted());
  CHECK(HeldOutWritten(root.path()));

  // UNE SECONDE OCCASION NE RECOMPTE PAS : la même partie, figée ou suivie
  // au bilan, n'écrit rien dans une autre installation.
  MinigameStage other_minigames;
  count.FollowEntered(stage, &other_minigames, other_root.path());
  CHECK_FALSE(count.Freeze(stage, &other_minigames, other_root.path()));
  CHECK_FALSE(HeldOutWritten(other_root.path()));

  // « REJOUER » : la partie neuve compte de nouveau.
  count.Restart();
  CHECK_FALSE(count.counted());
  BeginHeldOut(stage);
  AbandonToSummary(stage, count, &other_minigames, other_root.path());
  CHECK(count.counted());
  CHECK(HeldOutWritten(other_root.path()));
}
