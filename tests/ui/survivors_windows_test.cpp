// Les fenêtres du mode « Survivants de Midgard » : chacune se dessine d'une vue
// minimale comme d'une vue pleine, et ne rend une intention que quand la vue
// la permet — touches du choix hors de la garde, clic qui passe la roulette,
// « Partir » seulement sans refus.

#include <array>
#include <cstdint>
#include <functional>
#include <optional>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/minigames_window.h"
#include "ro/ui/survivors_chest_window.h"
#include "ro/ui/survivors_offer_window.h"
#include "ro/ui/survivors_pause_window.h"
#include "ro/ui/survivors_prepare_window.h"
#include "ro/ui/survivors_summary_window.h"
#include "ro/ui/ui_context.h"

namespace {

using ro::ui::SkinArt;

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
// Une fenêtre auto-ajustée prend sa taille à la deuxième image.
constexpr int kSettleFrames = 3;
// Le pas du balayage des clics sur une fenêtre.
constexpr float kSweepStep = 10.0F;
// Plus long que le délai du double-clic d'ImGui (0,3 s).
constexpr float kBetweenClicksSeconds = 0.5F;
constexpr ImTextureID kSomeTexture = 7;
const SkinArt kNoArt;

// Les sommets d'une image où `draw` se dessine, moins ceux d'une image vide.
[[nodiscard]] int VerticesOf(const std::function<void()>& draw) {
  ro::ui::UiContext ui;
  int vertices = 0;
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    draw();
    ui.EndFrame();
    vertices = ImGui::GetDrawData() != nullptr ? ImGui::GetDrawData()->TotalVtxCount : 0;
  }
  return vertices;
}

// Une image ; `draw` rend vrai quand la fenêtre a rendu l'intention guettée.
[[nodiscard]] bool Frame(ro::ui::UiContext& ui, const std::function<bool()>& draw) {
  ui.BeginFrame(kViewport, kFrameSeconds);
  const bool asked = draw();
  ui.EndFrame();
  return asked;
}

// Un clic en `at` (survol, appui, relâche) : vrai si une des images a rendu
// l'intention.
[[nodiscard]] bool ClickAt(ro::ui::UiContext& ui, ImVec2 at, const std::function<bool()>& draw) {
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(at.x, at.y);
  bool asked = Frame(ui, draw);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  asked = Frame(ui, draw) || asked;
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  asked = Frame(ui, draw) || asked;
  return asked;
}

// Un clic sur chaque point d'une grille couvrant le corps de la fenêtre
// `window_id` : vrai si l'un d'eux a rendu l'intention. ⚠ LE CORPS SEUL : un
// clic sur la barre de titre la replierait.
[[nodiscard]] bool SweepClicks(ro::ui::UiContext& ui, const char* window_id,
                               const std::function<bool()>& draw) {
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    (void)Frame(ui, draw);
  }
  const ImGuiWindow* window = ImGui::FindWindowByName(window_id);
  REQUIRE(window != nullptr);
  const ImRect bounds = window->InnerRect;
  for (float y = bounds.Min.y; y < bounds.Max.y; y += kSweepStep) {
    for (float x = bounds.Min.x; x < bounds.Max.x; x += kSweepStep) {
      if (ClickAt(ui, ImVec2(x, y), draw)) return true;
      // ⚠ UNE IMAGE LONGUE ENTRE DEUX CLICS : deux clics proches dans le temps
      // font un double-clic.
      ui.BeginFrame(kViewport, kBetweenClicksSeconds);
      (void)draw();
      ui.EndFrame();
    }
  }
  return false;
}

const std::array<ro::ui::SurvivorsOfferCard, 4> kCards = {
    ro::ui::SurvivorsOfferCard{.icon = kSomeTexture,
                               .icon_size = ImVec2(24.0F, 24.0F),
                               .name = "Fire Bolt",
                               .description = "Des éclairs de feu",
                               .owned_level = 2,
                               .next_level = 3},
    ro::ui::SurvivorsOfferCard{.name = "Soul Strike", .owned_level = 0, .next_level = 1},
    ro::ui::SurvivorsOfferCard{.icon = kSomeTexture,
                               .icon_size = ImVec2(75.0F, 100.0F),
                               .name = "Poring Card",
                               .owned_level = 1,
                               .next_level = 2},
    ro::ui::SurvivorsOfferCard{
        .name = "Fire Wall", .owned_level = 4, .next_level = 5, .evolves = true},
};

// Une touche tenue le temps d'une image, la fenêtre de choix dessinée.
[[nodiscard]] std::optional<ro::ui::SurvivorsOfferAsk> PressInOffer(
    const ro::ui::SurvivorsOfferView& view, ImGuiKey key) {
  ro::ui::UiContext ui;
  ro::ui::SurvivorsOfferState state;
  std::optional<ro::ui::SurvivorsOfferAsk> asked;
  const auto draw = [&]() {
    const auto answer = ro::ui::DrawSurvivorsOfferWindow(state, view, kNoArt);
    if (answer.has_value()) asked = answer;
    return answer.has_value();
  };
  (void)Frame(ui, draw);
  if (key != ImGuiKey_None) ImGui::GetIO().AddKeyEvent(key, true);
  (void)Frame(ui, draw);
  if (key != ImGuiKey_None) ImGui::GetIO().AddKeyEvent(key, false);
  (void)Frame(ui, draw);
  return asked;
}

}  // namespace

TEST_CASE("Survivors : chaque fenêtre se dessine, vue minimale ou pleine", "[ui][survivors]") {
  const int empty = VerticesOf([] {});

  ro::ui::SurvivorsOfferState offer_state;
  const ro::ui::SurvivorsOfferView offer_minimal;
  const ro::ui::SurvivorsOfferView offer_full{
      .cards = kCards, .accepts_input = true, .rerolls = 1, .skips = 2, .banishes = 1};
  CHECK(VerticesOf([&] { (void)DrawSurvivorsOfferWindow(offer_state, offer_minimal, kNoArt); }) >
        empty);
  CHECK(VerticesOf([&] { (void)DrawSurvivorsOfferWindow(offer_state, offer_full, kNoArt); }) >
        VerticesOf([&] { (void)DrawSurvivorsOfferWindow(offer_state, offer_minimal, kNoArt); }));

  const std::array<ro::ui::SurvivorsChestGainRow, 3> gains = {
      ro::ui::SurvivorsChestGainRow{.icon = kSomeTexture,
                                    .name = "Meteor Storm",
                                    .level = 1,
                                    .look = ro::ui::SurvivorsChestGainLook::kEvolution},
      ro::ui::SurvivorsChestGainRow{.name = "Cold Bolt", .level = 5},
      ro::ui::SurvivorsChestGainRow{.look = ro::ui::SurvivorsChestGainLook::kZeny},
  };
  const std::array<ImTextureID, 2> roulette = {kSomeTexture, kSomeTexture};
  CHECK(VerticesOf([] { (void)DrawSurvivorsChestWindow({}, kNoArt); }) > empty);
  CHECK(VerticesOf([&] {
          (void)DrawSurvivorsChestWindow(
              {.gains = gains, .roulette_icons = roulette, .list_shown = true}, kNoArt);
        }) > empty);

  ro::ui::SurvivorsPauseState pause_state;
  const std::array<ro::ui::SurvivorsMeasureRow, 2> measures = {
      ro::ui::SurvivorsMeasureRow{.label = "Simulation", .p50_ms = 0.4, .p95_ms = 0.9},
      ro::ui::SurvivorsMeasureRow{.label = "Scène", .p50_ms = 2.0, .max_ms = 7.5},
  };
  const std::array<ro::ui::SurvivorsCountRow, 1> counts = {
      ro::ui::SurvivorsCountRow{.label = "Monstres", .value = 312}};
  const ro::ui::SurvivorsPauseView pause_full{.reason = ro::ui::SurvivorsPauseReason::kWorldAlert,
                                              .alert_banner = "Votre personnage est attaqué !",
                                              .accepts_input = true,
                                              .measures_offered = true,
                                              .show_measures = true,
                                              .measures = measures,
                                              .counts = counts};
  const int pause_minimal =
      VerticesOf([&] { (void)DrawSurvivorsPauseWindow(pause_state, {}, kNoArt); });
  CHECK(pause_minimal > empty);
  const int pause_drawn =
      VerticesOf([&] { (void)DrawSurvivorsPauseWindow(pause_state, pause_full, kNoArt); });
  CHECK(pause_drawn > pause_minimal);
  // HORS DU CLIENT DE DÉVELOPPEMENT, ni la case ni les mesures ne se dessinent,
  // même rangées « montrées ».
  ro::ui::SurvivorsPauseView pause_player = pause_full;
  pause_player.measures_offered = false;
  CHECK(VerticesOf([&] { (void)DrawSurvivorsPauseWindow(pause_state, pause_player, kNoArt); }) <
        pause_drawn);

  const std::array<ro::ui::SurvivorsFoeRow, 2> foes = {
      ro::ui::SurvivorsFoeRow{.name = "Poring", .kills = 140},
      ro::ui::SurvivorsFoeRow{.name = "Lunatic", .kills = 52}};
  const std::array<ro::ui::SurvivorsSkillDamageRow, 2> damage = {
      ro::ui::SurvivorsSkillDamageRow{.icon = kSomeTexture, .name = "Fire Bolt", .damage = 90'000},
      ro::ui::SurvivorsSkillDamageRow{.name = "Napalm Beat", .damage = 12'000}};
  const std::array<ro::ui::SurvivorsCardRow, 1> cards = {
      ro::ui::SurvivorsCardRow{.illustration = kSomeTexture,
                               .illustration_size = ImVec2(75.0F, 100.0F),
                               .name = "Poring Card"}};
  for (const ro::ui::SurvivorsEndPhase phase :
       {ro::ui::SurvivorsEndPhase::kDeathVeil, ro::ui::SurvivorsEndPhase::kVictoryBanner,
        ro::ui::SurvivorsEndPhase::kSummary}) {
    CHECK(VerticesOf([&] { (void)DrawSurvivorsSummaryWindow({.phase = phase}, kNoArt); }) > empty);
  }
  CHECK(VerticesOf([&] {
          (void)DrawSurvivorsSummaryWindow({.outcome = ro::ui::SurvivorsEndOutcome::kVictory,
                                            .survived_ms = 900'000,
                                            .level = 31,
                                            .job_name = "Wizard",
                                            .foes = foes,
                                            .skills = damage,
                                            .cards = cards,
                                            .zeny = 1'200},
                                           kNoArt);
        }) > VerticesOf([&] { (void)DrawSurvivorsSummaryWindow({}, kNoArt); }));

  ro::ui::SurvivorsPrepareState prepare_state{.open = true};
  const int prepare_minimal =
      VerticesOf([&] { (void)DrawSurvivorsPrepareWindow(prepare_state, {}, kNoArt); });
  CHECK(prepare_minimal > empty);
  CHECK(VerticesOf([&] {
          (void)DrawSurvivorsPrepareWindow(prepare_state, {.refusal = "Hors d'une ville."}, kNoArt);
        }) > prepare_minimal);
}

TEST_CASE("Survivors : une touche 1 à 4 rend la carte dite, sauf dans la garde",
          "[ui][survivors]") {
  ro::ui::SurvivorsOfferView view{.cards = kCards, .accepts_input = true};
  const auto second = PressInOffer(view, ImGuiKey_2);
  REQUIRE(second.has_value());
  CHECK(*second ==
        ro::ui::SurvivorsOfferAsk{.verb = ro::ui::SurvivorsOfferVerb::kPick, .card_index = 1});
  const auto fourth = PressInOffer(view, ImGuiKey_Keypad4);
  REQUIRE(fourth.has_value());
  CHECK(fourth->card_index == 3);

  // La touche par sa place, que pose `app` depuis le scancode (AZERTY).
  view.pressed_card_key = 3;
  const auto placed = PressInOffer(view, ImGuiKey_None);
  REQUIRE(placed.has_value());
  CHECK(placed->card_index == 2);

  view.accepts_input = false;
  CHECK_FALSE(PressInOffer(view, ImGuiKey_2).has_value());
  view.pressed_card_key = 0;
  CHECK_FALSE(PressInOffer(view, ImGuiKey_1).has_value());
}

TEST_CASE("Survivors : une touche au-delà des cartes offertes ne rend rien", "[ui][survivors]") {
  const ro::ui::SurvivorsOfferView view{.cards = std::span(kCards).first(3), .accepts_input = true};
  CHECK_FALSE(PressInOffer(view, ImGuiKey_4).has_value());
}

TEST_CASE("Survivors : B arme le bannissement, la touche suivante bannit", "[ui][survivors]") {
  const ro::ui::SurvivorsOfferView view{.cards = kCards, .accepts_input = true, .banishes = 1};
  ro::ui::UiContext ui;
  ro::ui::SurvivorsOfferState state;
  std::optional<ro::ui::SurvivorsOfferAsk> asked;
  const auto draw = [&]() {
    asked = ro::ui::DrawSurvivorsOfferWindow(state, view, kNoArt);
    return asked.has_value();
  };
  (void)Frame(ui, draw);
  ImGui::GetIO().AddKeyEvent(ImGuiKey_B, true);
  (void)Frame(ui, draw);
  ImGui::GetIO().AddKeyEvent(ImGuiKey_B, false);
  CHECK(state.banishing);
  CHECK_FALSE(asked.has_value());
  ImGui::GetIO().AddKeyEvent(ImGuiKey_1, true);
  REQUIRE(Frame(ui, draw));
  CHECK(*asked ==
        ro::ui::SurvivorsOfferAsk{.verb = ro::ui::SurvivorsOfferVerb::kBanish, .card_index = 0});
  CHECK_FALSE(state.banishing);
}

TEST_CASE("Survivors : un clic sur la roulette la passe", "[ui][survivors]") {
  const std::array<ImTextureID, 1> roulette = {kSomeTexture};
  ro::ui::SurvivorsChestView view{.roulette_icons = roulette, .accepts_input = true};
  ro::ui::UiContext ui;
  const auto draw = [&]() { return ro::ui::DrawSurvivorsChestWindow(view, kNoArt); };
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    CHECK_FALSE(Frame(ui, draw));
  }
  const ImGuiWindow* window = ImGui::FindWindowByName("###survivors_chest");
  REQUIRE(window != nullptr);
  const ImVec2 inside(window->ContentRegionRect.Min.x + kSweepStep,
                      window->ContentRegionRect.Min.y + kSweepStep);
  CHECK(ClickAt(ui, inside, draw));
  // Dans la garde, le même clic ne passe rien.
  view.accepts_input = false;
  CHECK_FALSE(ClickAt(ui, inside, draw));
}

TEST_CASE("Survivors : « Partir » grisé ne rend aucune intention", "[ui][survivors]") {
  ro::ui::SurvivorsPrepareState state{.open = true};
  ro::ui::SurvivorsPrepareView view;
  const auto draw = [&]() {
    state.open = true;
    return ro::ui::DrawSurvivorsPrepareWindow(state, view, kNoArt).start;
  };
  view.refusal = "Un monstre est trop près.";
  {
    ro::ui::UiContext ui;
    CHECK_FALSE(SweepClicks(ui, "###survivors_prepare", draw));
  }
  // Sans refus, le même balayage trouve le bouton.
  view.refusal = nullptr;
  {
    ro::ui::UiContext ui;
    CHECK(SweepClicks(ui, "###survivors_prepare", draw));
  }
}

TEST_CASE("Survivors : « Codex » de la pause s'ouvre, sauf dans la garde",
          "[ui][survivors][codex]") {
  ro::ui::SurvivorsPauseView view{.accepts_input = true};
  const auto draw = [&]() {
    // Un clic sur « Abandonner » cacherait la rangée : chaque image repart neuve.
    ro::ui::SurvivorsPauseState state;
    return ro::ui::DrawSurvivorsPauseWindow(state, view, kNoArt).open_codex;
  };
  {
    ro::ui::UiContext ui;
    CHECK(SweepClicks(ui, "###survivors_pause", draw));
  }
  // Dans la garde de la reprise, le bouton est grisé : aucun clic ne l'ouvre.
  view.accepts_input = false;
  {
    ro::ui::UiContext ui;
    CHECK_FALSE(SweepClicks(ui, "###survivors_pause", draw));
  }
}

TEST_CASE("Mini-jeux : la ligne des survivants ne paraît qu'offerte", "[ui][survivors]") {
  ro::ui::MinigamesState state{.open = true};
  const int hidden = VerticesOf([&] { (void)DrawMinigamesWindow(state, {}, kNoArt); });
  const int offered =
      VerticesOf([&] { (void)DrawMinigamesWindow(state, {.survivors_offered = true}, kNoArt); });
  CHECK(offered > hidden);
}

TEST_CASE("Survivors : pendant une saisie, les touches de l'offre se taisent", "[ui][survivors]") {
  ro::ui::SurvivorsOfferView view{.cards = kCards, .accepts_input = true};
  view.rerolls = 1;
  view.skips = 1;
  view.banishes = 1;
  view.typing = true;
  CHECK_FALSE(PressInOffer(view, ImGuiKey_2).has_value());
  CHECK_FALSE(PressInOffer(view, ImGuiKey_Keypad4).has_value());
  CHECK_FALSE(PressInOffer(view, ImGuiKey_R).has_value());
  CHECK_FALSE(PressInOffer(view, ImGuiKey_P).has_value());
  view.pressed_card_key = 3;
  CHECK_FALSE(PressInOffer(view, ImGuiKey_None).has_value());

  // La saisie refermée, les mêmes touches répondent.
  view.pressed_card_key = 0;
  view.typing = false;
  CHECK(PressInOffer(view, ImGuiKey_R).has_value());
}
