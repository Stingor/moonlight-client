// LES OUTILS DU STAFF SUR L'HOMONCULE, DESSINÉS : la section « Staff » de
// l'onglet Homoncule, dépliée, et la création d'un homoncule dans la ligne du
// staff du grimoire. La pile d'identifiants d'ImGui reste équilibrée, et aucun
// identifiant survolé n'est porté par deux éléments. Les curseurs de la section
// ne prennent pas la molette, n'envoient qu'au terme du geste, et un niveau
// attendu bloque un second `@homlevel`. Puis la borne des curseurs de distance
// de l'IA, qui suit le rayon de vue du serveur.

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/companion_tactic_window.h"
#include "ro/ui/homunculus_tab.h"
#include "ro/ui/skill_window.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/staff_homunculus.h"
#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 900.0F};
constexpr float kFrameSeconds = 1.0F / 30.0F;
constexpr float kSweepStepPixels = 14.0F;
constexpr float kSweepRightPixels = 1280.0F;
constexpr float kSweepBottomPixels = 900.0F;
constexpr int kServerLevelMost = 350;
constexpr int kHomunculusLevel = 99;

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

// Dessine `draw` dans une fenêtre, la souris promenée sur tout l'écran. Rend
// le nombre d'images où ImGui a vu un conflit d'identifiants.
template <typename Draw>
[[nodiscard]] int SweptConflicts(ro::ui::UiContext& ui, Draw draw) {
  ImGuiIO& io = ImGui::GetIO();
  int conflicts = 0;
  for (float y = 0.0F; y < kSweepBottomPixels; y += kSweepStepPixels) {
    for (float x = 0.0F; x < kSweepRightPixels; x += kSweepStepPixels * 4.0F) {
      io.AddMousePosEvent(x, y);
      ui.BeginFrame(kViewport, kFrameSeconds);
      ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
      ImGui::SetNextWindowSize(ImVec2(kSweepRightPixels / 2.0F, kSweepBottomPixels));
      ImGui::Begin("banc");
      draw();
      ImGui::End();
      ui.EndFrame();
      conflicts += GImGui->DebugDrawIdConflictsId != 0 ? 1 : 0;
    }
  }
  return conflicts;
}

[[nodiscard]] std::vector<ro::ui::StaffHomunculusChoice> Forms() {
  return {{.job = 6048, .label = "6048 Eira"}, {.job = 6049, .label = "6049 Bayeri"}};
}

// Une image du banc : `draw` dans une fenêtre, sous les événements déjà posés.
//
// ⚠ SANS BARRE DE TITRE : un clic du balayage sur sa flèche replierait la
// fenêtre pour de bon, et plus aucun clic n'atteindrait l'onglet.
template <typename Draw>
void BenchFrame(ro::ui::UiContext& ui, Draw draw) {
  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
  ImGui::SetNextWindowSize(ImVec2(kSweepRightPixels / 2.0F, kSweepBottomPixels));
  ImGui::Begin("banc", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);
  draw();
  ImGui::End();
  ui.EndFrame();
}

// Un clic gauche à chaque point de la grille : une image bouton enfoncé, une
// image bouton relâché.
template <typename Draw>
void SweptClicks(ro::ui::UiContext& ui, Draw draw) {
  ImGuiIO& io = ImGui::GetIO();
  for (float y = 0.0F; y < kSweepBottomPixels; y += kSweepStepPixels) {
    for (float x = 0.0F; x < kSweepRightPixels / 2.0F; x += kSweepStepPixels) {
      io.AddMousePosEvent(x, y);
      io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
      BenchFrame(ui, draw);
      io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
      BenchFrame(ui, draw);
    }
  }
}

// La fiche d'un homoncule évolué de niveau 99, la section Staff permise.
[[nodiscard]] ro::ui::HomunculusTabView StaffView(
    std::span<const ro::ui::StaffHomunculusChoice> forms) {
  ro::ui::HomunculusTabView view;
  view.present = true;
  view.sheet_known = true;
  view.alive = true;
  view.name = "Lif";
  view.level = kHomunculusLevel;
  view.hunger = 50;
  view.intimacy = 910;
  view.staff_allowed = true;
  view.evolved = true;
  view.mutations = forms;
  view.staff_level_most = kServerLevelMost;
  return view;
}

}  // namespace

TEST_CASE("Onglet Homoncule : la section Staff dépliée se dessine sans conflit",
          "[ui][homunculus_staff]") {
  ro::ui::UiContext ui;
  const ErrorCount counted(ImGui::GetIO());
  const ro::ui::SkinArt art;
  const std::vector<ro::ui::StaffHomunculusChoice> forms = Forms();
  ro::ui::HomunculusTabView view;
  view.present = true;
  view.sheet_known = true;
  view.alive = true;
  view.name = "Lif";
  view.level = kHomunculusLevel;
  view.hunger = 50;
  view.intimacy = 910;
  view.staff_allowed = true;
  view.evolved = true;
  view.mutations = forms;
  view.staff_level_most = kServerLevelMost;
  ro::ui::HomunculusTabState state;
  state.staff_open = true;
  ro::ui::HomunculusTabAction action;
  const int conflicts =
      SweptConflicts(ui, [&]() { action = ro::ui::DrawHomunculusTab(state, view, art); });
  CHECK(state.staff_open);
  CHECK(counted.errors == 0);
  CHECK(conflicts == 0);
  // ⚠ RIEN NE PART AU SIMPLE SURVOL : aucun brouillon, aucune commande.
  CHECK(action.verb == ro::ui::HomunculusVerb::kNothing);
  CHECK_FALSE(state.hunger_draft.has_value());
}

TEST_CASE("Onglet Homoncule : la molette sur la section Staff ne règle rien",
          "[ui][homunculus_staff]") {
  ro::ui::UiContext ui;
  const ErrorCount counted(ImGui::GetIO());
  const ro::ui::SkinArt art;
  const std::vector<ro::ui::StaffHomunculusChoice> forms = Forms();
  const ro::ui::HomunculusTabView view = StaffView(forms);
  ro::ui::HomunculusTabState state;
  ImGuiIO& io = ImGui::GetIO();
  int staff_orders = 0;
  // Un cran à chaque point, dans les deux sens : la fiche défile, et aucun
  // curseur ne prend le cran au passage.
  for (const float notch : {-1.0F, 1.0F}) {
    for (float y = 0.0F; y < kSweepBottomPixels; y += kSweepStepPixels / 2.0F) {
      for (float x = 0.0F; x < kSweepRightPixels / 2.0F; x += kSweepStepPixels) {
        io.AddMousePosEvent(x, y);
        io.AddMouseWheelEvent(0.0F, notch);
        BenchFrame(ui, [&]() {
          state.staff_open = true;
          const ro::ui::HomunculusTabAction action = ro::ui::DrawHomunculusTab(state, view, art);
          staff_orders += action.verb == ro::ui::HomunculusVerb::kStaff ? 1 : 0;
        });
      }
    }
  }
  CHECK(counted.errors == 0);
  CHECK(staff_orders == 0);
  CHECK_FALSE(state.hunger_draft.has_value());
  CHECK_FALSE(state.intimacy_draft.has_value());
  CHECK_FALSE(state.level_draft.has_value());
}

TEST_CASE("Onglet Homoncule : un niveau attendu bloque un second @homlevel",
          "[ui][homunculus_staff]") {
  const ro::ui::SkinArt art;
  const std::vector<ro::ui::StaffHomunculusChoice> forms = Forms();
  const ro::ui::HomunculusTabView view = StaffView(forms);
  constexpr int kAwaitedLevel = kHomunculusLevel + 10;

  // Témoin : sans attente, les clics de la grille atteignent bien le curseur
  // du niveau, et en font partir au moins un.
  int free_level_orders = 0;
  {
    ro::ui::UiContext ui;
    const ErrorCount counted(ImGui::GetIO());
    ro::ui::HomunculusTabState state;
    SweptClicks(ui, [&]() {
      state.staff_open = true;
      const ro::ui::HomunculusTabAction action = ro::ui::DrawHomunculusTab(state, view, art);
      free_level_orders += action.staff_order == ro::ui::HomunculusStaffOrder::kAddLevels ? 1 : 0;
    });
    CHECK(counted.errors == 0);
  }
  CHECK(free_level_orders > 0);

  // Le même balayage, un niveau attendu à chaque image : rien ne part.
  int awaited_level_orders = 0;
  {
    ro::ui::UiContext ui;
    const ErrorCount counted(ImGui::GetIO());
    ro::ui::HomunculusTabState state;
    SweptClicks(ui, [&]() {
      state.staff_open = true;
      state.level_pending =
          ro::ui::StaffSliderPending{.value = kAwaitedLevel, .sent_at_s = ImGui::GetTime()};
      const ro::ui::HomunculusTabAction action = ro::ui::DrawHomunculusTab(state, view, art);
      awaited_level_orders +=
          action.staff_order == ro::ui::HomunculusStaffOrder::kAddLevels ? 1 : 0;
      CHECK_FALSE(state.level_draft.has_value());
    });
    CHECK(counted.errors == 0);
  }
  CHECK(awaited_level_orders == 0);
}

TEST_CASE("Onglet Homoncule : l'attente tombe quand le serveur répond, ou au délai",
          "[ui][homunculus_staff]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  const std::vector<ro::ui::StaffHomunculusChoice> forms = Forms();
  ro::ui::HomunculusTabView view = StaffView(forms);
  ro::ui::HomunculusTabState state;
  state.staff_open = true;
  const auto draw = [&]() { static_cast<void>(ro::ui::DrawHomunculusTab(state, view, art)); };
  constexpr int kAwaitedLevel = kHomunculusLevel + 10;
  constexpr int kFewFrames = 3;

  state.level_pending =
      ro::ui::StaffSliderPending{.value = kAwaitedLevel, .sent_at_s = ImGui::GetTime()};
  for (int frame = 0; frame < kFewFrames; ++frame) {
    BenchFrame(ui, draw);
  }
  CHECK(state.level_pending.has_value());
  view.level = kAwaitedLevel;
  BenchFrame(ui, draw);
  CHECK_FALSE(state.level_pending.has_value());

  // Un refus ne renvoie rien : le délai seul rend le curseur.
  state.level_pending =
      ro::ui::StaffSliderPending{.value = kAwaitedLevel + 1, .sent_at_s = ImGui::GetTime()};
  const auto frames_to_expire =
      static_cast<int>(ro::ui::kStaffSliderAwaitSeconds / static_cast<double>(kFrameSeconds)) +
      kFewFrames;
  for (int frame = 0; frame < frames_to_expire; ++frame) {
    BenchFrame(ui, draw);
  }
  CHECK_FALSE(state.level_pending.has_value());
}

TEST_CASE("Curseurs du Staff : la valeur ne part qu'au terme du geste", "[ui][homunculus_staff]") {
  using ro::ui::StaffSliderGesture;
  using ro::ui::StaffSliderSettle;
  constexpr int kValue = 42;
  std::optional<int> draft;

  // Une flèche tenue répète sans saisir le curseur : rien tant que le bouton
  // est enfoncé, la dernière valeur au relâchement.
  CHECK_FALSE(StaffSliderSettle(kValue, {.changed = true, .mouse_held = true}, draft));
  CHECK_FALSE(StaffSliderSettle(kValue + 1, {.changed = true, .mouse_held = true}, draft));
  CHECK_FALSE(StaffSliderSettle(kValue + 1, {.mouse_held = true}, draft));
  CHECK(draft == kValue + 1);
  CHECK(StaffSliderSettle(kValue + 1, {}, draft) == kValue + 1);
  CHECK_FALSE(draft.has_value());

  // Le glisser : un brouillon, puis la valeur au lâcher.
  CHECK_FALSE(
      StaffSliderSettle(kValue, {.changed = true, .active = true, .mouse_held = true}, draft));
  CHECK(draft == kValue);
  CHECK(StaffSliderSettle(kValue, {.released_after_edit = true}, draft) == kValue);
  CHECK_FALSE(draft.has_value());

  // Un clic qui saisit sans rien changer n'envoie rien.
  CHECK_FALSE(StaffSliderSettle(kValue, {.active = true, .mouse_held = true}, draft));
  CHECK_FALSE(StaffSliderSettle(kValue, {}, draft));
  CHECK_FALSE(draft.has_value());
}

TEST_CASE("@homlevel : la différence se calcule sur un niveau servi à jour",
          "[ui][homunculus_staff]") {
  using ro::ui::HomunculusStaffLevelsToAdd;
  using ro::ui::StaffSliderAwaited;
  using ro::ui::StaffSliderPending;
  constexpr int kServed = 50;
  constexpr int kTarget = 99;
  constexpr double kSentAt = 1.0;
  constexpr double kSoonAfter = 1.5;

  // Niveau 50, glissé à 99 : 49 niveaux, et 99 devient attendu.
  CHECK(HomunculusStaffLevelsToAdd(kTarget, kServed, false) == kTarget - kServed);
  const std::optional<StaffSliderPending> pending =
      StaffSliderPending{.value = kTarget, .sent_at_s = kSentAt};
  // Reglissé avant l'écho : le niveau servi est encore 50, et rien ne part.
  REQUIRE(StaffSliderAwaited(pending, kServed, kSoonAfter));
  CHECK_FALSE(HomunculusStaffLevelsToAdd(kTarget, kServed, true).has_value());
  // L'écho arrive : l'attente tombe.
  CHECK_FALSE(StaffSliderAwaited(pending, kTarget, kSoonAfter));
  // Pas d'écho : le délai la fait tomber.
  CHECK_FALSE(StaffSliderAwaited(pending, kServed, kSentAt + ro::ui::kStaffSliderAwaitSeconds));
  CHECK_FALSE(StaffSliderAwaited(std::nullopt, kServed, kSoonAfter));
  // Rien à ajouter sous le niveau servi.
  CHECK_FALSE(HomunculusStaffLevelsToAdd(kServed, kServed, false).has_value());
}

TEST_CASE("Grimoire : la création d'un homoncule se dessine sans conflit",
          "[ui][homunculus_staff]") {
  ro::ui::UiContext ui;
  const ErrorCount counted(ImGui::GetIO());
  const ro::ui::SkinArt art;
  const std::vector<ro::ui::StaffHomunculusChoice> forms = Forms();
  ro::ui::SkillWindowState state;
  for (const bool has_homunculus : {false, true}) {
    const ro::ui::GrimoireStaffView staff{.homunculi = forms, .has_homunculus = has_homunculus};
    const int conflicts = SweptConflicts(ui, [&]() {
      static_cast<void>(ro::ui::DrawGrimoireTab(state, {}, {}, /*staff_allowed=*/true, art, staff));
    });
    CHECK(conflicts == 0);
  }
  CHECK(counted.errors == 0);
}

TEST_CASE("Liste des homoncules : le rang se ramène dans la liste", "[ui][homunculus_staff]") {
  const std::vector<ro::ui::StaffHomunculusChoice> forms = Forms();
  CHECK(ro::ui::StaffHomunculusJobAt(forms, 1) == 6049);
  CHECK(ro::ui::StaffHomunculusJobAt(forms, 2) == 0);
  CHECK(ro::ui::StaffHomunculusJobAt(forms, -1) == 0);
  CHECK(ro::ui::StaffHomunculusJobAt({}, 0) == 0);
}

TEST_CASE("Curseurs de distance de l'IA : le rayon de vue du serveur les borne",
          "[ui][homunculus_staff]") {
  constexpr int kMoonlightViewCells = 18;
  constexpr int kAbsoluteCells = 50;
  constexpr int kTinyCap = 10;
  CHECK(ro::ui::CompanionAiCellsShown(kMoonlightViewCells, kAbsoluteCells) == kMoonlightViewCells);
  // Le plafond absolu reste le plafond, même sous un rayon plus large.
  CHECK(ro::ui::CompanionAiCellsShown(kMoonlightViewCells, kTinyCap) == kTinyCap);
  // Rayon inconnu : le plafond seul.
  CHECK(ro::ui::CompanionAiCellsShown(0, kAbsoluteCells) == kAbsoluteCells);
}
