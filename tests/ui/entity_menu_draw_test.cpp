// LE MENU D'ENTITÉ DESSINÉ : un sous-menu ouvert ne déséquilibre pas la pile
// d'identifiants d'ImGui.

#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/entity_menu.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/ui_context.h"

using ro::ui::EntityMenuItem;
using ro::ui::EntityMenuState;
using ro::ui::EntityMenuView;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
// Des images longues : un sous-menu ne s'ouvre au survol qu'après un instant.
constexpr float kFrameSeconds = 1.0F / 20.0F;
// Où le menu s'ouvre, puis le balayage qui cherche la ligne du sous-menu.
constexpr float kOpenX = 100.0F;
constexpr float kOpenY = 100.0F;
constexpr float kSweepRightPixels = 20.0F;
constexpr float kSweepDownPixels = 160.0F;
constexpr float kSweepStepPixels = 3.0F;
constexpr int kFramesPerStep = 4;

[[nodiscard]] EntityMenuItem Line(const char* label, int verb, const std::string& submenu) {
  EntityMenuItem item;
  item.label = label;
  item.verb = verb;
  item.submenu = !submenu.empty();
  item.submenu_label = submenu;
  return item;
}

}  // namespace

TEST_CASE("Menu d'entité : un sous-menu ouvert laisse la pile d'identifiants intacte",
          "[ui][entity_menu]") {
  ro::ui::UiContext ui;
  ImGuiIO& io = ImGui::GetIO();
  // ⚠ LES ERREURS SE COMPTENT, ELLES N'INTERROMPENT PAS : une boîte d'assertion
  // attendrait un clic sur l'écran du joueur. Le rappel les voit toutes.
  int errors = 0;
  io.ConfigErrorRecoveryEnableAssert = false;
  io.ConfigErrorRecoveryEnableDebugLog = false;
  io.ConfigErrorRecoveryEnableTooltip = false;
  GImGui->ErrorCallback = [](ImGuiContext*, void* counted, const char*) {
    ++*static_cast<int*>(counted);
  };
  GImGui->ErrorCallbackUserData = &errors;
  const ro::ui::SkinArt art;
  EntityMenuView view;
  view.subject = 1;
  view.name = "Poring";
  view.family = "Monstre";
  view.items = {Line("Attaquer", 1, ""), Line("Haut", 2, "Tactique du compagnon"),
                Line("Bas", 3, "Tactique du compagnon")};
  EntityMenuState state;
  state.open_requested = true;

  const auto frame = [&ui, &state, &view, &art]() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    static_cast<void>(ro::ui::DrawEntityMenu(state, &view, art));
    ui.EndFrame();
  };
  // Le menu s'ouvre sous la souris : son coin est là.
  io.AddMousePosEvent(kOpenX, kOpenY);
  frame();
  REQUIRE(GImGui->OpenPopupStack.Size == 1);

  bool submenu_seen = false;
  for (float down = 0.0F; down < kSweepDownPixels && !submenu_seen; down += kSweepStepPixels) {
    io.AddMousePosEvent(kOpenX + kSweepRightPixels, kOpenY + down);
    for (int step = 0; step < kFramesPerStep; ++step) {
      frame();
      // Le surgissant du menu, et par-dessus le sous-menu.
      submenu_seen = submenu_seen || GImGui->OpenPopupStack.Size >= 2;
    }
  }
  // Quelques images de plus, sous-menu ouvert.
  for (int step = 0; step < kFramesPerStep; ++step) {
    frame();
  }
  CHECK(submenu_seen);
  CHECK(errors == 0);
}
