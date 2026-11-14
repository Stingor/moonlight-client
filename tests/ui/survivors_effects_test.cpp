// Les effets d'une partie de « Survivants de Midgard » vus par le joueur : les
// secondes, le clignotement et le voile d'un effet en cours, l'avis d'un objet
// ramassé, et la liste des effets permanents de la pause.

#include "ro/ui/survivors_effects.h"

#include <array>
#include <functional>
#include <string_view>

#include <imgui.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/ui/survivors_hud.h"
#include "ro/ui/survivors_pause_window.h"
#include "ro/ui/ui_context.h"

namespace {

using ro::ui::SurvivorsEffectFigure;
using ro::ui::SurvivorsHudEffect;
using ro::ui::SurvivorsHudNotice;
using ro::ui::SurvivorsHudView;

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
// Une fenêtre auto-ajustée prend sa taille à la deuxième image.
constexpr int kSettleFrames = 3;
constexpr ImTextureID kSomeTexture = 7;
constexpr std::uint32_t kManualMs = 30'000;
const ro::ui::SkinArt kNoArt;

// Les sommets que le HUD ajoute à la liste de fond.
[[nodiscard]] int HudVerticesOf(const SurvivorsHudView& view) {
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  const ImDrawList* list = ImGui::GetBackgroundDrawList();
  const int before = list->VtxBuffer.Size;
  ro::ui::DrawSurvivorsHud(view);
  const int after = list->VtxBuffer.Size;
  ui.EndFrame();
  return after - before;
}

// Les sommets d'une image où `draw` se dessine, une fois la fenêtre posée.
[[nodiscard]] int WindowVerticesOf(const std::function<void()>& draw) {
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

}  // namespace

TEST_CASE("Effets survivors : les secondes restantes, arrondies au-dessus", "[ui][survivors]") {
  CHECK(ro::ui::SurvivorsEffectSecondsLeft(0) == 0U);
  CHECK(ro::ui::SurvivorsEffectSecondsLeft(1) == 1U);
  CHECK(ro::ui::SurvivorsEffectSecondsLeft(1'000) == 1U);
  CHECK(ro::ui::SurvivorsEffectSecondsLeft(1'001) == 2U);
  CHECK(ro::ui::SurvivorsEffectSecondsLeft(kManualMs) == 30U);
}

TEST_CASE("Effets survivors : l'icône clignote les dernières secondes", "[ui][survivors]") {
  // Au-dessus du seuil, toujours allumée.
  CHECK(ro::ui::SurvivorsEffectIconLit(kManualMs));
  CHECK(ro::ui::SurvivorsEffectIconLit(ro::ui::kSurvivorsEffectBlinkMs + 1));
  // En dessous, une demi-période sur deux.
  constexpr std::uint32_t kHalf = ro::ui::kSurvivorsEffectBlinkHalfPeriodMs;
  CHECK(ro::ui::SurvivorsEffectIconLit(ro::ui::kSurvivorsEffectBlinkMs));
  CHECK_FALSE(ro::ui::SurvivorsEffectIconLit(ro::ui::kSurvivorsEffectBlinkMs - 1));
  CHECK_FALSE(ro::ui::SurvivorsEffectIconLit(ro::ui::kSurvivorsEffectBlinkMs - kHalf));
  CHECK(ro::ui::SurvivorsEffectIconLit(ro::ui::kSurvivorsEffectBlinkMs - kHalf - 1));
  int lit = 0;
  int dark = 0;
  for (std::uint32_t left_ms = 1; left_ms <= ro::ui::kSurvivorsEffectBlinkMs; ++left_ms) {
    (ro::ui::SurvivorsEffectIconLit(left_ms) ? lit : dark) += 1;
  }
  CHECK(lit > 0);
  CHECK(dark > 0);
}

TEST_CASE("Effets survivors : le voile couvre la part écoulée", "[ui][survivors]") {
  CHECK(ro::ui::SurvivorsEffectVeil(kManualMs, kManualMs) == Catch::Approx(0.0F));
  CHECK(ro::ui::SurvivorsEffectVeil(kManualMs / 2, kManualMs) == Catch::Approx(0.5F));
  CHECK(ro::ui::SurvivorsEffectVeil(0, kManualMs) == Catch::Approx(1.0F));
  // Un reste au-delà de la durée pleine ne dévoile pas plus que l'icône.
  CHECK(ro::ui::SurvivorsEffectVeil(kManualMs * 2, kManualMs) == Catch::Approx(0.0F));
  CHECK(ro::ui::SurvivorsEffectVeil(0, 0) == Catch::Approx(1.0F));
}

TEST_CASE("Effets survivors : l'avis d'un objet dit son nom et son effet", "[ui][survivors]") {
  const SurvivorsHudNotice manual{.name = "Battle Manual",
                                  .figure = SurvivorsEffectFigure::kExpPct,
                                  .value = 100,
                                  .duration_ms = kManualMs};
  CHECK(std::string_view(ro::ui::SurvivorsNoticeTextOf(manual).data()) ==
        "Battle Manual : Expérience +100 %, 30 s");
  const SurvivorsHudNotice water{
      .name = "Holy Water", .figure = SurvivorsEffectFigure::kPurifyCells, .value = 12};
  CHECK(std::string_view(ro::ui::SurvivorsNoticeTextOf(water).data()) ==
        "Holy Water : Purifie les monstres à 12 cases");
  CHECK(std::string_view(
            ro::ui::SurvivorsEffectFigureText(SurvivorsEffectFigure::kLivesLeft, 1).data()) ==
        "Vies restantes : 1");

  // Entier, puis il s'efface pendant son dernier quart.
  CHECK(ro::ui::SurvivorsNoticeAlpha(0.5F) == Catch::Approx(1.0F));
  CHECK(ro::ui::SurvivorsNoticeAlpha(0.875F) == Catch::Approx(0.5F));
  CHECK(ro::ui::SurvivorsNoticeAlpha(1.0F) == Catch::Approx(0.0F));
}

TEST_CASE("Effets survivors : le HUD peint la colonne et les avis", "[ui][survivors]") {
  const int bare = HudVerticesOf({});
  const std::array<SurvivorsHudEffect, 1> running = {
      SurvivorsHudEffect{.icon = kSomeTexture, .remaining_ms = 12'000, .full_ms = kManualMs}};
  CHECK(HudVerticesOf({.effects = running}) > bare);

  // Éteinte dans son clignotement, l'icône ne se peint pas ; la case, le voile
  // et les secondes (un chiffre de part et d'autre) restent.
  const std::array<SurvivorsHudEffect, 1> lit = {
      SurvivorsHudEffect{.icon = kSomeTexture, .remaining_ms = 3'100, .full_ms = kManualMs}};
  const std::array<SurvivorsHudEffect, 1> dark = {
      SurvivorsHudEffect{.icon = kSomeTexture, .remaining_ms = 2'900, .full_ms = kManualMs}};
  CHECK(HudVerticesOf({.effects = dark}) < HudVerticesOf({.effects = lit}));

  const std::array<SurvivorsHudNotice, 1> fresh = {
      SurvivorsHudNotice{.name = "Speed Potion",
                         .figure = SurvivorsEffectFigure::kWalkPct,
                         .value = 50,
                         .duration_ms = 10'000,
                         .fraction = 0.5F}};
  CHECK(HudVerticesOf({.notices = fresh}) > bare);
  // Au bout de sa vie, l'avis ne peint plus rien.
  std::array<SurvivorsHudNotice, 1> over = fresh;
  over[0].fraction = 1.0F;
  CHECK(HudVerticesOf({.notices = over}) == bare);
}

TEST_CASE("Effets survivors : la pause liste les effets permanents", "[ui][survivors]") {
  const std::array<ro::ui::SurvivorsPauseEffectRow, 3> rows = {
      ro::ui::SurvivorsPauseEffectRow{.icon = kSomeTexture,
                                      .name = "Hornet Card",
                                      .level = 3,
                                      .figure = SurvivorsEffectFigure::kDamagePct,
                                      .value = 18},
      ro::ui::SurvivorsPauseEffectRow{
          .name = "Magnifier", .figure = SurvivorsEffectFigure::kRerollsLeft, .value = 2},
      ro::ui::SurvivorsPauseEffectRow{
          .name = "Resurrection", .figure = SurvivorsEffectFigure::kLivesLeft, .value = 1}};
  const ro::ui::SurvivorsPauseView bare{.accepts_input = true};
  ro::ui::SurvivorsPauseView listed = bare;
  listed.effects = rows;
  ro::ui::SurvivorsPauseState state;
  const int without =
      WindowVerticesOf([&] { (void)ro::ui::DrawSurvivorsPauseWindow(state, bare, kNoArt); });
  const int with =
      WindowVerticesOf([&] { (void)ro::ui::DrawSurvivorsPauseWindow(state, listed, kNoArt); });
  CHECK(with > without);
}
