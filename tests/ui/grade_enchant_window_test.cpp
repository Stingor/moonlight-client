// LA FENÊTRE DES GRADES : fermée, rien ; ouverte, elle se dessine, et la mise ou
// les doses hors de portée se ramènent dans les bornes de la vue.

#include "ro/ui/grade_enchant_window.h"

#include <array>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/ui_context.h"

using ro::ui::GradeEnchantPieceView;
using ro::ui::GradeEnchantStakeView;
using ro::ui::GradeEnchantView;
using ro::ui::GradeEnchantWindowAction;
using ro::ui::GradeEnchantWindowState;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr std::uint16_t kSwordSlot = 12;

struct Probe {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  GradeEnchantWindowState state;
  std::array<GradeEnchantPieceView, 1> candidates{GradeEnchantPieceView{
      .client_slot = kSwordSlot, .item = {.item_id = 1116, .name = "+8 Sword"}, .chosen = true}};
  std::array<GradeEnchantStakeView, 2> stakes{
      GradeEnchantStakeView{.item = {.item_id = 1000326, .name = "Topaz"},
                            .owned = 1,
                            .needed = 1,
                            .zeny = 175000,
                            .risk = "risque",
                            .risky = true},
      GradeEnchantStakeView{.item = {.item_id = 1000326, .name = "Topaz"},
                            .owned = 1,
                            .needed = 5,
                            .zeny = 875000,
                            .risk = "sans risque"}};
  GradeEnchantView view;

  Probe() {
    ro::ui::OpenGradeEnchantOn(state, 1);
    view.instruction = "Choisissez une arme.";
    view.candidates = candidates;
    view.chosen = candidates[0].item;
    view.step = "Grade : D, puis C";
    view.offer_ready = true;
    view.chance_percent = 20;
    view.blessing = {.item_id = 1000337, .name = "Dust"};
    view.blessing_owned = 6;
    view.blessing_per_step = 3;
    view.blessing_most_steps = 10;
    view.blessing_bonus_percent = 1;
    view.stakes = stakes;
    view.confirmation = "Améliorer ?";
  }

  GradeEnchantWindowAction Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    GradeEnchantWindowAction action = ro::ui::DrawGradeEnchantWindow(state, view, art);
    ui.EndFrame();
    return action;
  }
};

[[nodiscard]] bool WindowShown() {
  for (ImGuiWindow* window : GImGui->Windows) {
    const std::string_view name(window->Name);
    if (window->WasActive && name.find("###grade_enchant") != std::string_view::npos &&
        name.find('/') == std::string_view::npos) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("Fenêtre des grades : fermée, elle ne dessine rien", "[ui][grade_enchant]") {
  Probe probe;
  probe.state.open = false;
  const GradeEnchantWindowAction action = probe.Frame();
  CHECK_FALSE(WindowShown());
  CHECK_FALSE(action.close);
}

TEST_CASE("Fenêtre des grades : la mise et les doses restent dans les bornes",
          "[ui][grade_enchant]") {
  Probe probe;
  probe.state.stake = 7;
  probe.state.blessing_steps = 99;
  // Quelques images : une fenêtre neuve ne se dit active qu'une fois posée.
  for (int frame = 0; frame < 3; ++frame) {
    static_cast<void>(probe.Frame());
  }
  CHECK(WindowShown());
  CHECK(probe.state.stake == 1);
  CHECK(probe.state.blessing_steps == 10);
}

TEST_CASE("Fenêtre des grades : sans poussière, aucune dose", "[ui][grade_enchant]") {
  Probe probe;
  probe.view.blessing = {};
  probe.state.blessing_steps = 4;
  static_cast<void>(probe.Frame());
  CHECK(probe.state.blessing_steps == 0);
}
