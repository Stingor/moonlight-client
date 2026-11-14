// LA FENÊTRE DE RÉFORME : ce qu'un clic demande, et un bouton que le verdict retient.
// Les gestes se jouent à la souris, sur la fenêtre dessinée.

#include "ro/ui/item_reform_window.h"

#include <array>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/ui_context.h"

using ro::ui::ItemReformPieceView;
using ro::ui::ItemReformView;
using ro::ui::ItemReformWindowAction;
using ro::ui::ItemReformWindowState;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kSweepStepPixels = 3.0F;
constexpr float kEdgeMarginPixels = 12.0F;
constexpr std::uint16_t kSwordSlot = 12;
constexpr std::uint16_t kLowSwordSlot = 13;

struct Probe {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  ItemReformWindowState state;
  std::array<ItemReformPieceView, 2> candidates{
      ItemReformPieceView{.client_slot = kSwordSlot, .item = {.item_id = 1499, .name = "+9 Sword"}},
      ItemReformPieceView{.client_slot = kLowSwordSlot,
                          .item = {.item_id = 1499, .name = "+4 Sword"},
                          .refusal = "Raffinement trop bas."}};
  std::array<std::string_view, 1> blockers{"Choisissez la pièce."};
  ItemReformView view;

  Probe() {
    ro::ui::OpenItemReformOn(state, 1);
    view.title = "Rituel";
    view.instruction = "Choisissez une pièce.";
    view.candidates = candidates;
    view.blockers = blockers;
    view.confirmation = "Réformer ?";
  }

  ItemReformWindowAction Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ItemReformWindowAction action = ro::ui::DrawItemReformWindow(state, view, art);
    ui.EndFrame();
    return action;
  }

  void Settle() {
    for (int frame = 0; frame < 4; ++frame) {
      static_cast<void>(Frame());
    }
  }

  ItemReformWindowAction Click() {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    const ItemReformWindowAction pressed = Frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    ItemReformWindowAction released = Frame();
    if (released.choose_slot == 0) {
      released.choose_slot = pressed.choose_slot;
    }
    return released;
  }

  void MoveTo(ImVec2 at) {
    ImGui::GetIO().AddMousePosEvent(at.x, at.y);
    static_cast<void>(Frame());
  }
};

[[nodiscard]] ImGuiWindow* MainWindow() {
  for (ImGuiWindow* window : GImGui->Windows) {
    const std::string_view name(window->Name);
    if (window->WasActive && name.find("###item_reform") != std::string_view::npos &&
        name.find('/') == std::string_view::npos) {
      return window;
    }
  }
  return nullptr;
}

// La hauteur de la ligne de cette pièce du sac.
[[nodiscard]] float RowOf(Probe& probe, std::uint16_t slot) {
  const ImGuiWindow* window = MainWindow();
  REQUIRE(window != nullptr);
  ImGuiIO& io = ImGui::GetIO();
  const float x = window->Pos.x + window->Size.x * 0.1F;
  for (float y = window->Pos.y; y < window->Pos.y + window->Size.y; y += kSweepStepPixels) {
    io.AddMousePosEvent(x, y);
    if (probe.Frame().hovered_slot == slot) {
      return y;
    }
  }
  return -1.0F;
}

// Le dernier élément interactif d'une ligne, en partant de la droite de la liste.
[[nodiscard]] ImVec2 ButtonOnRow(Probe& probe, float y) {
  const ImGuiWindow* window = MainWindow();
  REQUIRE(window != nullptr);
  const float left = window->Pos.x + kEdgeMarginPixels;
  const float right = window->Pos.x + window->Size.x * 0.5F;
  for (float x = right; x > left; x -= kSweepStepPixels) {
    probe.MoveTo(ImVec2(x, y));
    if (GImGui->HoveredId != 0) {
      return ImVec2(x, y);
    }
  }
  return ImVec2(-1.0F, -1.0F);
}

}  // namespace

TEST_CASE("Fenêtre de réforme : fermée, elle ne dessine rien", "[ui][item_reform]") {
  Probe probe;
  probe.state.open = false;
  const ItemReformWindowAction action = probe.Frame();
  CHECK(MainWindow() == nullptr);
  CHECK_FALSE(action.close);
}

TEST_CASE("Fenêtre de réforme : une pièce admise se choisit", "[ui][item_reform]") {
  Probe probe;
  probe.Settle();
  const float y = RowOf(probe, kSwordSlot);
  REQUIRE(y > 0.0F);
  const ImVec2 button = ButtonOnRow(probe, y);
  REQUIRE(button.x > 0.0F);
  probe.MoveTo(button);
  CHECK(probe.Click().choose_slot == kSwordSlot);
}

TEST_CASE("Fenêtre de réforme : une pièce refusée ne se choisit pas", "[ui][item_reform]") {
  Probe probe;
  probe.Settle();
  const float y = RowOf(probe, kLowSwordSlot);
  REQUIRE(y > 0.0F);
  const ImVec2 end = ButtonOnRow(probe, y);
  if (end.x > 0.0F) {
    probe.MoveTo(end);
  }
  CHECK(probe.Click().choose_slot == 0);
}
