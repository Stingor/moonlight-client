// LA SECTION « STAFF » DE LA FICHE DU FAMILIER, DESSINÉE : absente sans le droit,
// sans conflit d'identifiants une fois dépliée, et ses curseurs n'envoient
// jamais l'intimité zéro, qui détruirait l'œuf. La fiche colle à son contenu :
// ses curseurs ne doivent pas la faire grandir d'une image à l'autre.

#include <algorithm>
#include <cstdlib>
#include <optional>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/pet_window.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 900.0F};
constexpr float kFrameSeconds = 1.0F / 30.0F;
constexpr float kSweepStepPixels = 6.0F;
constexpr int kSettleFrames = 8;
constexpr const char* kWindowId = "###pet_window";

// LES ERREURS SE COMPTENT, ELLES N'INTERROMPENT PAS : une boîte d'assertion
// attendrait un clic sur l'écran du joueur.
struct ErrorCount {
  int errors = 0;
  explicit ErrorCount(ImGuiIO& io) {
    io.ConfigErrorRecoveryEnableAssert = false;
    io.ConfigErrorRecoveryEnableDebugLog = false;
    io.ConfigErrorRecoveryEnableTooltip = false;
    io.ConfigDebugHighlightIdConflicts = true;
    GImGui->ErrorCallback = [](ImGuiContext*, void* counted, const char*) {
      ++*static_cast<int*>(counted);
    };
    GImGui->ErrorCallbackUserData = &errors;
  }
};

[[nodiscard]] ro::ui::PetWindowView StaffView() {
  ro::ui::PetWindowView view;
  view.present = true;
  view.name = "Hodremlin";
  view.species = "Hodremlin";
  view.level = 199;
  view.hunger = 50;
  view.hunger_most = 100;
  view.intimacy = 250;
  view.intimacy_most = 1000;
  view.staff_allowed = true;
  return view;
}

// Une image de la fiche, sous les événements déjà posés.
ro::ui::PetWindowAction PetFrame(ro::ui::UiContext& ui, ro::ui::PetWindowState& state,
                                 const ro::ui::PetWindowView& view) {
  const ro::ui::SkinArt art;
  ui.BeginFrame(kViewport, kFrameSeconds);
  // Un clic du balayage sur la croix ne met pas fin à l'essai.
  state.open = true;
  const ro::ui::PetWindowAction action = ro::ui::DrawPetWindow(state, view, art);
  ui.EndFrame();
  return action;
}

}  // namespace

TEST_CASE("Fiche du pet : les curseurs du staff n'envoient jamais l'intimité zéro",
          "[ui][pet_staff]") {
  ro::ui::UiContext ui;
  const ErrorCount counted(ImGui::GetIO());
  ImGuiIO& io = ImGui::GetIO();
  const ro::ui::PetWindowView view = StaffView();
  ro::ui::PetWindowState state;

  static_cast<void>(PetFrame(ui, state, view));
  const ImGuiWindow* const window = ImGui::FindWindowByName(kWindowId);
  REQUIRE(window != nullptr);

  int hunger_orders = 0;
  int intimacy_orders = 0;
  int lowest_intimacy = view.intimacy_most;
  int conflicts = 0;
  std::optional<ImVec2> on_intimacy_track;
  const ImVec2 low = window->Pos;
  // La largeur et la hauteur se relisent après dépliage : la section allonge la
  // fiche.
  state.staff_open = true;
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    static_cast<void>(PetFrame(ui, state, view));
  }
  const ImVec2 size = window->SizeFull;
  for (float y = low.y; y < low.y + size.y; y += kSweepStepPixels) {
    for (float x = low.x; x < low.x + size.x; x += kSweepStepPixels) {
      io.AddMousePosEvent(x, y);
      io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
      state.staff_open = true;
      const ro::ui::PetWindowAction pressed = PetFrame(ui, state, view);
      // 🔴 RIEN NE PART BOUTON TENU.
      CHECK_FALSE(pressed.staff_hunger.has_value());
      CHECK_FALSE(pressed.staff_intimacy.has_value());
      conflicts += GImGui->DebugDrawIdConflictsId != 0 ? 1 : 0;
      io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
      state.staff_open = true;
      const ro::ui::PetWindowAction released = PetFrame(ui, state, view);
      if (released.staff_hunger.has_value()) {
        ++hunger_orders;
      }
      if (released.staff_intimacy.has_value()) {
        ++intimacy_orders;
        lowest_intimacy = std::min(lowest_intimacy, *released.staff_intimacy);
        // Une flèche fait un pas ; la piste, elle, fait un saut et saisit la poignée.
        if (std::abs(*released.staff_intimacy - view.intimacy) > 1) {
          on_intimacy_track = ImVec2(x, y);
        }
      }
      // Le serveur ne répond pas sur ce banc : chaque clic part de la valeur servie.
      state.hunger_pending.reset();
      state.intimacy_pending.reset();
    }
  }
  CHECK(counted.errors == 0);
  CHECK(conflicts == 0);
  CHECK(hunger_orders > 0);
  CHECK(intimacy_orders > 0);
  CHECK(lowest_intimacy >= ro::ui::kPetStaffIntimacyLeast);

  // LE GESTE LE PLUS DANGEREUX : saisir la poignée et la traîner bien au-delà du
  // bord gauche. Elle s'arrête à la borne, et le serveur garde l'œuf.
  REQUIRE(on_intimacy_track.has_value());
  io.AddMousePosEvent(on_intimacy_track->x, on_intimacy_track->y);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  static_cast<void>(PetFrame(ui, state, view));
  io.AddMousePosEvent(0.0F, on_intimacy_track->y);
  static_cast<void>(PetFrame(ui, state, view));
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  const ro::ui::PetWindowAction dragged = PetFrame(ui, state, view);
  REQUIRE(dragged.staff_intimacy.has_value());
  CHECK(*dragged.staff_intimacy == ro::ui::kPetStaffIntimacyLeast);
}

TEST_CASE("Fiche du pet : sans le droit, la section Staff n'existe pas, et n'allonge rien",
          "[ui][pet_staff]") {
  ro::ui::UiContext ui;
  const ErrorCount counted(ImGui::GetIO());
  ro::ui::PetWindowView view = StaffView();
  ro::ui::PetWindowState state;

  view.staff_allowed = false;
  // Même dépliée dans son état, elle ne paraît pas.
  state.staff_open = true;
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    static_cast<void>(PetFrame(ui, state, view));
  }
  const ImGuiWindow* const window = ImGui::FindWindowByName(kWindowId);
  REQUIRE(window != nullptr);
  const float without = window->SizeFull.y;

  view.staff_allowed = true;
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    static_cast<void>(PetFrame(ui, state, view));
  }
  CHECK(window->SizeFull.y > without);
  CHECK(counted.errors == 0);
}

TEST_CASE("Fiche du pet : les curseurs ne font pas grandir la fenêtre", "[ui][pet_staff]") {
  ro::ui::UiContext ui;
  const ErrorCount counted(ImGui::GetIO());
  const ro::ui::PetWindowView view = StaffView();
  ro::ui::PetWindowState state;
  state.staff_open = true;
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    static_cast<void>(PetFrame(ui, state, view));
  }
  const ImGuiWindow* const window = ImGui::FindWindowByName(kWindowId);
  REQUIRE(window != nullptr);
  const ImVec2 settled = window->SizeFull;
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    static_cast<void>(PetFrame(ui, state, view));
  }
  CHECK(window->SizeFull.x == settled.x);
  CHECK(window->SizeFull.y == settled.y);
  CHECK(counted.errors == 0);
}

TEST_CASE("Fiche du pet : une valeur attendue est montrée, et ne repart pas", "[ui][pet_staff]") {
  ro::ui::UiContext ui;
  const ErrorCount counted(ImGui::GetIO());
  ro::ui::PetWindowView view = StaffView();
  ro::ui::PetWindowState state;
  state.staff_open = true;
  constexpr int kAwaitedHunger = 80;

  // Le serveur la renvoie : l'attente s'éteint d'elle-même.
  state.hunger_pending = ro::ui::StaffSliderPending{.value = kAwaitedHunger, .sent_at_s = 0.0};
  static_cast<void>(PetFrame(ui, state, view));
  CHECK(state.hunger_pending.has_value());
  view.hunger = kAwaitedHunger;
  const ro::ui::PetWindowAction action = PetFrame(ui, state, view);
  CHECK_FALSE(state.hunger_pending.has_value());
  CHECK_FALSE(action.staff_hunger.has_value());
  CHECK(counted.errors == 0);
}
