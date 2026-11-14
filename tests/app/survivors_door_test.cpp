// LA PORTE DU MODE SURVIVORS, sans fenêtre : la liaison perdue en pleine partie
// fige, écrit et fait sortir, et le bilan interrompu ne paraît qu'à la salle
// rouverte, une seule fois ; perdue sur le bilan, elle fait sortir sans rien
// réécrire ; « Rejouer » après une fin rejuge la garde ; les options du jeu
// s'ouvrent sur la pause, Échap les ferme d'abord, et la pause finie aussi.

#include "survivors/survivors_door.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_state.h"

#include "support/survivors_fixture.h"
#include "support/survivors_golden.h"
#include "support/temp_directory.h"
#include "survivors/survivors_file.h"
#include "survivors/survivors_stage.h"

using moonclient::FreezeAndSave;
using moonclient::HubDoor;
using moonclient::kArrivalMs;
using moonclient::kInterruptedEndingFileName;
using moonclient::LinkLossSteps;
using moonclient::LinkLossStepsOf;
using moonclient::SummaryStep;
using moonclient::SummaryStepOf;
using moonclient::SurvivorsEnding;
using moonclient::SurvivorsIntent;
using moonclient::SurvivorsSavePath;
using moonclient::SurvivorsScreen;
using moonclient::SurvivorsStage;
using ro::sim::kSurvivorsStepMs;
using ro::sim::SurvivorsOutcome;
using ro::test_support::GoldenRules;
using ro::test_support::kGoldenArenaCells;
using ro::test_support::kGoldenFirstSkillId;
using ro::test_support::kGoldenSeed;
using ro::test_support::PillarArena;
using ro::test_support::TempDirectory;

namespace {

constexpr std::uint32_t kFewSteps = 50;

// Un étage dont la partie en or court depuis quelques pas.
void BeginGolden(SurvivorsStage& stage) {
  stage.Open();
  const auto begun = stage.Begin(GoldenRules(), PillarArena(kGoldenArenaCells),
                                 ro::test_support::GoldenStart(ro::test_support::kGoldenSeed));
  REQUIRE(begun.has_value());
  stage.Loaded();
  stage.Advance(kArrivalMs, SurvivorsIntent{});
  for (std::uint32_t frame = 0; frame < kFewSteps; ++frame) {
    stage.Advance(kSurvivorsStepMs, SurvivorsIntent{});
  }
  REQUIRE(stage.screen() == SurvivorsScreen::kPlaying);
}

// Les pas que mène le mode sur une liaison perdue, comme `SurvivorsMode::Frame`.
void FollowLinkLoss(SurvivorsStage& stage, const std::filesystem::path& client_root) {
  const LinkLossSteps steps = LinkLossStepsOf(stage.screen(), false);
  if (steps.freeze) static_cast<void>(FreezeAndSave(stage, client_root));
  if (steps.exit) stage.Close();
}

[[nodiscard]] bool EndingWritten(const std::filesystem::path& client_root) {
  return std::filesystem::exists(SurvivorsSavePath(client_root, kInterruptedEndingFileName));
}

}  // namespace

TEST_CASE("Porte survivors : liaison ouverte ou mode fermé, rien à faire", "[app][survivors]") {
  CHECK(LinkLossStepsOf(SurvivorsScreen::kPlaying, true) == LinkLossSteps{});
  CHECK(LinkLossStepsOf(SurvivorsScreen::kClosed, false) == LinkLossSteps{});
  CHECK(LinkLossStepsOf(SurvivorsScreen::kPreparing, false) == LinkLossSteps{});
}

TEST_CASE("Porte survivors : une partie commencée se fige et le mode sort", "[app][survivors]") {
  for (const SurvivorsScreen screen :
       {SurvivorsScreen::kArriving, SurvivorsScreen::kPlaying, SurvivorsScreen::kPaused,
        SurvivorsScreen::kChoosing, SurvivorsScreen::kChest, SurvivorsScreen::kDying,
        SurvivorsScreen::kWon}) {
    INFO("écran " << static_cast<int>(screen));
    CHECK(LinkLossStepsOf(screen, false) == LinkLossSteps{.freeze = true, .exit = true});
  }
  // Rien ne s'est encore joué au chargement, et le bilan est déjà écrit.
  CHECK(LinkLossStepsOf(SurvivorsScreen::kLoading, false) == LinkLossSteps{.exit = true});
  CHECK(LinkLossStepsOf(SurvivorsScreen::kSummary, false) == LinkLossSteps{.exit = true});
}

TEST_CASE("Porte survivors : la liaison perdue en partie montre le bilan à la salle, une fois",
          "[app][survivors]") {
  const TempDirectory root;
  SurvivorsStage stage;
  HubDoor door;
  BeginGolden(stage);
  // La salle était ouverte au départ, et l'est restée.
  CHECK_FALSE(door.EndingToShow(true, stage.screen(), root.path()).has_value());

  FollowLinkLoss(stage, root.path());
  // Le mode a rendu l'écran : la boîte de liaison perdue paraît, pas le bilan.
  CHECK_FALSE(stage.owns_screen());
  CHECK(EndingWritten(root.path()));

  // La salle, fermée par la perte, se rouvre après la reconnexion.
  CHECK_FALSE(door.EndingToShow(false, stage.screen(), root.path()).has_value());
  std::optional<SurvivorsEnding> ending = door.EndingToShow(true, stage.screen(), root.path());
  REQUIRE(ending.has_value());
  CHECK(ending->tally.outcome == SurvivorsOutcome::kAbandoned);
  CHECK_FALSE(EndingWritten(root.path()));
  stage.ShowEnding(std::move(*ending));
  CHECK(stage.screen() == SurvivorsScreen::kSummary);

  // « Retour », puis la salle refermée et rouverte : plus rien à montrer.
  stage.Close();
  CHECK_FALSE(door.EndingToShow(false, stage.screen(), root.path()).has_value());
  CHECK_FALSE(door.EndingToShow(true, stage.screen(), root.path()).has_value());
}

TEST_CASE("Porte survivors : la liaison perdue sur le bilan fait sortir sans réécrire",
          "[app][survivors]") {
  const TempDirectory root;
  SurvivorsStage stage;
  BeginGolden(stage);
  REQUIRE(stage.Freeze());
  REQUIRE(stage.screen() == SurvivorsScreen::kSummary);
  FollowLinkLoss(stage, root.path());
  CHECK_FALSE(stage.owns_screen());
  CHECK_FALSE(EndingWritten(root.path()));
}

TEST_CASE("Porte survivors : le bilan qui paraît ferme la porte aux autres", "[app][survivors]") {
  const TempDirectory root;
  SurvivorsStage stage;
  BeginGolden(stage);
  REQUIRE(FreezeAndSave(stage, root.path()));
  // L'étage tient encore l'écran : la salle qui s'ouvre ne lit rien.
  HubDoor door;
  CHECK_FALSE(door.EndingToShow(true, stage.screen(), root.path()).has_value());
  CHECK(EndingWritten(root.path()));
  // Une partie déjà close ne se fige pas deux fois.
  CHECK_FALSE(FreezeAndSave(stage, root.path()));
}

TEST_CASE("Porte survivors : « Rejouer » après une fin rejuge la garde", "[app][survivors]") {
  CHECK(SummaryStepOf(false, false, false) == SummaryStep::kStay);
  CHECK(SummaryStepOf(true, false, false) == SummaryStep::kBack);
  CHECK(SummaryStepOf(true, true, true) == SummaryStep::kBack);
  CHECK(SummaryStepOf(false, true, false) == SummaryStep::kReplay);
  CHECK(SummaryStepOf(false, true, true) == SummaryStep::kBackToPrepare);

  // Une partie finie se relance sur le même étage, depuis son bilan.
  SurvivorsStage stage;
  BeginGolden(stage);
  REQUIRE(stage.Freeze());
  const auto replayed = stage.Begin(GoldenRules(), PillarArena(kGoldenArenaCells),
                                    ro::test_support::GoldenStart(ro::test_support::kGoldenSeed));
  CHECK(replayed.has_value());
  CHECK(stage.screen() == SurvivorsScreen::kLoading);
}

TEST_CASE("Porte survivors : les options ouvertes depuis la pause", "[app][survivors]") {
  using moonclient::PauseOptionsDoor;
  constexpr SurvivorsScreen kPaused = SurvivorsScreen::kPaused;

  SECTION("« Options » ouvre sur la pause, et seulement là") {
    PauseOptionsDoor door;
    bool open = false;
    CHECK_FALSE(door.Follow(open, true, false, SurvivorsScreen::kPlaying));
    CHECK_FALSE(open);
    CHECK_FALSE(door.Follow(open, true, false, kPaused));
    CHECK(open);
    CHECK(door.shown(open, kPaused));
  }
  SECTION("Échap ferme d'abord les options, puis va à la partie") {
    PauseOptionsDoor door;
    bool open = false;
    static_cast<void>(door.Follow(open, true, false, kPaused));
    CHECK(door.Follow(open, false, true, kPaused));
    CHECK_FALSE(open);
    CHECK_FALSE(door.shown(open, kPaused));
    CHECK_FALSE(door.Follow(open, false, true, kPaused));
  }
  SECTION("la pause finie — reprise, abandon, fin — les ferme") {
    for (const SurvivorsScreen after :
         {SurvivorsScreen::kPlaying, SurvivorsScreen::kSummary, SurvivorsScreen::kClosed}) {
      PauseOptionsDoor door;
      bool open = false;
      static_cast<void>(door.Follow(open, true, false, kPaused));
      door.Settle(open, kPaused);
      CHECK(open);
      door.Settle(open, after);
      CHECK_FALSE(open);
    }
  }
  SECTION("des options ouvertes dans le jeu avant la partie restent au jeu") {
    PauseOptionsDoor door;
    bool open = true;
    CHECK_FALSE(door.Follow(open, false, true, kPaused));
    CHECK(open);
    CHECK_FALSE(door.shown(open, kPaused));
    door.Settle(open, SurvivorsScreen::kPlaying);
    CHECK(open);
  }
  SECTION("ouvertes dans le jeu, puis montrées par « Options », elles restent au jeu") {
    for (const bool by_escape : {false, true}) {
      PauseOptionsDoor door;
      bool open = true;
      CHECK_FALSE(door.Follow(open, true, false, kPaused));
      CHECK(door.shown(open, kPaused));
      if (by_escape) {
        // Échap cesse de les montrer sans les fermer au jeu.
        CHECK(door.Follow(open, false, true, kPaused));
        CHECK_FALSE(door.shown(open, kPaused));
      }
      door.Settle(open, SurvivorsScreen::kPlaying);
      CHECK(open);
    }
  }
  SECTION("fermées par leur croix, Échap revient à la partie") {
    PauseOptionsDoor door;
    bool open = false;
    static_cast<void>(door.Follow(open, true, false, kPaused));
    open = false;
    CHECK_FALSE(door.Follow(open, false, true, kPaused));
  }
}
