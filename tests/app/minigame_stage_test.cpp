// L'ÉTAGE DES MINI-JEUX : l'horloge qui ne court que fenêtre ouverte,
// Rojeweled qui se fige fenêtre fermée, et Entrée réclamée par la méta
// survivors qui a le focus.

#include "game/minigame_stage.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <imgui.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/match3_game.h"
#include "ro/sim/minigame_trophies.h"
#include "ro/sim/survivors_run.h"
#include "ro/ui/escape_stack.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/ui_context.h"

#include "game/minigame_file.h"
#include "support/survivors_fixture.h"
#include "support/temp_directory.h"

using moonclient::kMostWatchedStepMs;
using moonclient::MinigameStage;
using moonclient::MinigameWindows;
using moonclient::SurvivorsClaimsEnter;
using moonclient::WatchedClock;
using ro::sim::MinigameTrophy;
using ro::test_support::TempDirectory;

namespace {

// L'instant d'une ouverture, et la durée d'une image.
constexpr std::uint32_t kOpenedAtMs = 1000;
constexpr std::uint32_t kFrameMs = 16;
// Une fermeture plus longue que le silence qui fait paraître l'indice.
constexpr std::uint32_t kClosedForMs = 2 * ro::sim::kMatch3HintAfterMs;
// Une partie de survivors d'essai.
constexpr std::uint64_t kSurvivorsSeed = 5;
constexpr std::int32_t kArenaSideCells = 48;
// Le banc d'interface : un écran, une image, et les images qui posent les
// fenêtres avant qu'on leur donne le focus.
constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr int kSettleFrames = 3;

}  // namespace

TEST_CASE("WatchedClock : le temps ne court que fenêtre ouverte", "[app][minigames]") {
  WatchedClock clock;
  CHECK(clock.Follow(true, kOpenedAtMs) == 0);
  CHECK(clock.Follow(true, kOpenedAtMs + kFrameMs) == kFrameMs);
  CHECK(clock.watched_ms() == kFrameMs);
  CHECK(clock.Follow(false, kOpenedAtMs + 2 * kFrameMs) == 0);
  CHECK(clock.watched_ms() == kFrameMs);
  // ⚠ LA RÉOUVERTURE NE RATTRAPE PAS LA FERMETURE.
  const std::uint32_t reopened_ms = kOpenedAtMs + kClosedForMs;
  CHECK(clock.Follow(true, reopened_ms) == 0);
  CHECK(clock.Follow(true, reopened_ms + kFrameMs) == kFrameMs);
  CHECK(clock.watched_ms() == 2 * kFrameMs);
}

TEST_CASE("WatchedClock : un gel ne se rattrape qu'à hauteur d'un pas", "[app][minigames]") {
  WatchedClock clock;
  clock.Follow(true, kOpenedAtMs);
  CHECK(clock.Follow(true, kOpenedAtMs + kClosedForMs) == kMostWatchedStepMs);
}

TEST_CASE("WatchedClock : l'heure du client peut repasser par zéro", "[app][minigames]") {
  // Un pas qui enjambe le passage de 2^32 à 0.
  constexpr std::uint32_t kWrapStepMs = 32;
  constexpr std::uint32_t kBeforeWrapMs = 0U - kWrapStepMs / 2;
  constexpr std::uint32_t kAfterWrapMs = kWrapStepMs / 2;
  WatchedClock clock;
  clock.Follow(true, kBeforeWrapMs);
  CHECK(clock.Follow(true, kAfterWrapMs) == kWrapStepMs);
}

TEST_CASE("Rojeweled se fige fenêtre fermée : l'indice attend un silence regardé",
          "[app][minigames]") {
  const TempDirectory root;
  MinigameStage stage;
  MinigameWindows windows;
  windows.rojeweled.open = true;
  stage.Advance(windows, {}, root.path(), kOpenedAtMs);
  windows.rojeweled.open = false;
  stage.Advance(windows, {}, root.path(), kOpenedAtMs + kFrameMs);
  windows.rojeweled.open = true;
  std::uint32_t now_ms = kOpenedAtMs + kClosedForMs;
  stage.Advance(windows, {}, root.path(), now_ms);
  CHECK_FALSE(windows.rojeweled_view.hint.has_value());
  // Puis le silence, regardé image par image.
  const std::uint32_t hinted_ms = now_ms + ro::sim::kMatch3HintAfterMs + kFrameMs;
  while (now_ms < hinted_ms) {
    now_ms += kFrameMs;
    stage.Advance(windows, {}, root.path(), now_ms);
  }
  CHECK(windows.rojeweled_view.hint.has_value());
}

TEST_CASE("La demande d'une icône du menu ouvre son jeu, et survit au dessin de la salle",
          "[app][minigames]") {
  const TempDirectory root;
  MinigameStage stage;
  MinigameWindows windows;
  windows.hub.open = true;
  stage.Advance(windows, {}, root.path(), kOpenedAtMs);
  windows.icon_answer.open_roggle = true;
  // La salle redessinée réécrit sa propre réponse, pas celle des icônes.
  windows.hub_answer = {};
  stage.Advance(windows, {}, root.path(), kOpenedAtMs + kFrameMs);
  CHECK(windows.roggle.open);
  CHECK_FALSE(windows.rojeweled.open);
  CHECK_FALSE(windows.icon_answer.open_roggle);
}

TEST_CASE("Une partie de survivors finie inscrit ses trophées et les rend à l'écran de fin",
          "[app][minigames][survivors]") {
  const TempDirectory root;
  auto run = ro::sim::SurvivorsRun::Start(ro::test_support::QuietRules(),
                                          ro::test_support::PillarArena(kArenaSideCells),
                                          ro::test_support::TinyStart(kSurvivorsSeed));
  REQUIRE(run.has_value());
  run->field_for_tests().tally.evolutions = 1;
  MinigameStage stage;
  const std::vector<MinigameTrophy> won = stage.RecordSurvivorsRun(*run, root.path());
  REQUIRE(won.size() == 1);
  CHECK(won.front() == MinigameTrophy::kSurvivorsFirstEvolution);

  // ÉCRIT AUSSITÔT, fenêtres fermées.
  const moonclient::MinigameRecordHeld saved = moonclient::LoadMinigameRecord(root.path());
  CHECK(saved.record.Has(MinigameTrophy::kSurvivorsFirstEvolution));
  // LA SALLE LE MONTRE GAGNÉ ; son avis est celui de l'écran de fin du mode
  // (Q13), et la fenêtre d'un autre jeu ne le redit pas.
  MinigameWindows windows;
  windows.hub.open = true;
  windows.rojeweled.open = true;
  stage.Advance(windows, {}, root.path(), kOpenedAtMs);
  const auto& row =
      windows.hub_view.trophies[static_cast<std::size_t>(MinigameTrophy::kSurvivorsFirstEvolution)];
  CHECK(row.unlocked);
  CHECK(windows.rojeweled_view.toast == nullptr);
  // UNE SECONDE FOIS, rien de neuf.
  CHECK(stage.RecordSurvivorsRun(*run, root.path()).empty());
}

TEST_CASE("Ouvrir l'accueil de survivors ferme la salle des mini-jeux",
          "[app][minigames][survivors]") {
  const TempDirectory root;
  MinigameStage stage;
  MinigameWindows windows;
  windows.hub.open = true;
  stage.Advance(windows, {}, root.path(), kOpenedAtMs);
  REQUIRE(windows.composed);
  windows.hub_answer.open_survivors = true;
  stage.Advance(windows, {}, root.path(), kOpenedAtMs + kFrameMs);
  CHECK(windows.survivors_prepare.open);
  CHECK_FALSE(windows.hub.open);
  CHECK_FALSE(windows.composed);
}

TEST_CASE("La salle montre survivors à tous les joueurs", "[app][minigames][survivors]") {
  const TempDirectory root;
  MinigameStage stage;
  MinigameWindows windows;
  windows.hub.open = true;
  stage.Advance(windows, {}, root.path(), kOpenedAtMs);
  REQUIRE(windows.composed);
  CHECK(windows.hub_view.survivors_offered);
}

TEST_CASE("La méta survivors qui a le focus réclame Entrée, la barre de conversation ne l'a pas",
          "[app][minigames][survivors]") {
  ro::ui::UiContext ui;
  MinigameWindows windows;
  const ro::ui::SkinArt art;
  ro::ui::EscapeStack escape;
  // Une image ; `focus` désigne la fenêtre à mettre au premier plan, rien
  // pour retirer le focus à toutes.
  const auto frame = [&](std::optional<const char*> focus) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    if (focus.has_value()) ImGui::SetWindowFocus(*focus);
    DrawMinigameWindows(windows, art, escape);
    escape.Process();
    ui.EndFrame();
  };
  CHECK_FALSE(SurvivorsClaimsEnter(windows));

  windows.survivors_prepare.open = true;
  windows.survivors_prepare_composed = true;
  windows.survivors_meta.church.open = true;
  for (int settle = 0; settle < kSettleFrames; ++settle) frame(std::nullopt);
  // L'Église au premier plan : Entrée achète, la barre ne se déplie pas.
  frame("###survivors_church");
  frame(std::nullopt);
  CHECK(SurvivorsClaimsEnter(windows));
  // L'accueil au premier plan : Entrée part.
  frame("###survivors_prepare");
  frame(std::nullopt);
  CHECK(SurvivorsClaimsEnter(windows));
  // Le joueur a cliqué dans le monde : la touche revient à la conversation.
  frame(nullptr);
  frame(std::nullopt);
  CHECK_FALSE(SurvivorsClaimsEnter(windows));
  // L'accueil fermé, plus rien ne réclame, même avec un focus resté sur lui.
  frame("###survivors_prepare");
  windows.survivors_prepare.open = false;
  CHECK_FALSE(SurvivorsClaimsEnter(windows));
}
