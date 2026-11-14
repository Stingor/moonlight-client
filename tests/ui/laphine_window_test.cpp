// LA FENÊTRE DU LAPHINE : ce qu'un clic demande, un bouton que le verdict retient, et
// la transformation qui ne part qu'après sa confirmation. Les gestes se jouent à la
// souris, sur la fenêtre dessinée.

#include "ro/ui/laphine_window.h"

#include <array>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/ui_context.h"

using ro::ui::LaphineAcceptedView;
using ro::ui::LaphineItemView;
using ro::ui::LaphineView;
using ro::ui::LaphineWindowAction;
using ro::ui::LaphineWindowState;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kSweepStepPixels = 3.0F;
constexpr float kEdgeMarginPixels = 12.0F;
constexpr std::uint16_t kSwordSlot = 12;
constexpr std::uint16_t kAxeSlot = 13;
constexpr std::uint32_t kPilesSeq = 9;
constexpr const char* kConfirmName = "##laphine_confirm";

struct Probe {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  LaphineWindowState state;
  std::array<LaphineItemView, 2> candidates{
      LaphineItemView{.client_slot = kSwordSlot, .item = {.item_id = 1101, .name = "Sword"}},
      LaphineItemView{.client_slot = kAxeSlot,
                      .item = {.item_id = 1301, .name = "Axe"},
                      .refusal = "Cette pièce est portée."}};
  std::array<LaphineAcceptedView, 1> accepted{
      LaphineAcceptedView{.item = {.item_id = 1101, .name = "Sword"}}};
  std::vector<LaphineItemView> deposited;
  std::vector<std::string_view> blockers;
  LaphineView view;

  Probe() {
    ro::ui::OpenLaphineOn(state, 1);
    view.title = "Pierre";
    view.instruction = "Déposez la pièce.";
    view.make_label = "Améliorer";
    view.candidates = candidates;
    view.accepted = accepted;
    view.piles_seq = kPilesSeq;
    view.confirmation = "Améliorer ?";
  }

  LaphineWindowAction Frame() {
    view.deposited = deposited;
    view.blockers = blockers;
    ui.BeginFrame(kViewport, kFrameSeconds);
    LaphineWindowAction action = ro::ui::DrawLaphineWindow(state, view, art);
    ui.EndFrame();
    return action;
  }

  void Settle() {
    for (int frame = 0; frame < 4; ++frame) {
      static_cast<void>(Frame());
    }
  }

  LaphineWindowAction Click() {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    LaphineWindowAction pressed = Frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    LaphineWindowAction released = Frame();
    if (released.deposit_slot == 0) {
      released.deposit_slot = pressed.deposit_slot;
    }
    if (released.withdraw_slot == 0) {
      released.withdraw_slot = pressed.withdraw_slot;
    }
    released.make = released.make || pressed.make;
    if (released.piles_seq == 0) {
      released.piles_seq = pressed.piles_seq;
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
    if (window->WasActive && name.find("###laphine") != std::string_view::npos &&
        name.find('/') == std::string_view::npos) {
      return window;
    }
  }
  return nullptr;
}

// La hauteur de la ligne de cet objet du sac.
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

// Le dernier élément interactif d'une ligne, en partant de la droite du volet du sac.
[[nodiscard]] ImVec2 ButtonOnRow(Probe& probe, float y) {
  const ImGuiWindow* window = MainWindow();
  REQUIRE(window != nullptr);
  const float left = window->Pos.x + kEdgeMarginPixels;
  // Le sac prend la part gauche : le volet du dépôt commence avant le milieu passé.
  const float right = window->Pos.x + window->Size.x * 0.55F;
  for (float x = right; x > left; x -= kSweepStepPixels) {
    probe.MoveTo(ImVec2(x, y));
    if (GImGui->HoveredId != 0) {
      return ImVec2(x, y);
    }
  }
  return ImVec2(-1.0F, -1.0F);
}

// Le plus bas des boutons de pleine largeur : celui qui transforme, au-dessus de
// « Fermer ». On le cherche à droite, où « Fermer » n'arrive pas.
[[nodiscard]] ImVec2 MakeButton(Probe& probe) {
  const ImGuiWindow* window = MainWindow();
  REQUIRE(window != nullptr);
  const float x = window->Pos.x + window->Size.x * 0.75F;
  // ⚠ LE BORD DE LA FENÊTRE SE SAISIT AUSSI : on part au-dessus.
  for (float y = window->Pos.y + window->Size.y - kEdgeMarginPixels; y > window->Pos.y;
       y -= kSweepStepPixels) {
    probe.MoveTo(ImVec2(x, y));
    if (GImGui->HoveredId != 0) {
      return ImVec2(x, y);
    }
  }
  return ImVec2(-1.0F, -1.0F);
}

[[nodiscard]] ImVec2 YesButton(Probe& probe) {
  const ImGuiWindow* box = ImGui::FindWindowByName(kConfirmName);
  REQUIRE(box != nullptr);
  for (float y = box->Pos.y + box->Size.y - 2.0F; y > box->Pos.y; y -= kSweepStepPixels) {
    for (float x = box->Pos.x + 2.0F; x < box->Pos.x + box->Size.x; x += kSweepStepPixels) {
      probe.MoveTo(ImVec2(x, y));
      if (GImGui->HoveredId != 0) {
        return ImVec2(x, y);
      }
    }
  }
  return ImVec2(-1.0F, -1.0F);
}

}  // namespace

TEST_CASE("Fenêtre du Laphine : fermée, elle ne dessine rien", "[ui][laphine]") {
  Probe probe;
  probe.state.open = false;
  const LaphineWindowAction action = probe.Frame();
  CHECK(MainWindow() == nullptr);
  CHECK_FALSE(action.close);
}

TEST_CASE("Fenêtre du Laphine : un objet du sac se survole et se dépose", "[ui][laphine]") {
  Probe probe;
  probe.Settle();
  const float y = RowOf(probe, kSwordSlot);
  REQUIRE(y > 0.0F);
  const ImVec2 button = ButtonOnRow(probe, y);
  REQUIRE(button.x > 0.0F);
  probe.MoveTo(button);
  CHECK(probe.Click().deposit_slot == kSwordSlot);
}

TEST_CASE("Fenêtre du Laphine : un objet refusé ne se dépose pas", "[ui][laphine]") {
  Probe probe;
  probe.Settle();
  const float y = RowOf(probe, kAxeSlot);
  REQUIRE(y > 0.0F);
  // Le bouton est grisé : rien ne le survole, et un clic au bout de la ligne ne donne
  // rien.
  const ImGuiWindow* window = MainWindow();
  REQUIRE(window != nullptr);
  bool deposited = false;
  for (float x = window->Pos.x + kEdgeMarginPixels; x < window->Pos.x + window->Size.x * 0.55F;
       x += kSweepStepPixels * 10.0F) {
    probe.MoveTo(ImVec2(x, y));
    deposited = deposited || probe.Click().deposit_slot != 0;
  }
  CHECK_FALSE(deposited);
}

TEST_CASE("Fenêtre du Laphine : le bouton retenu ne demande rien", "[ui][laphine]") {
  Probe probe;
  probe.blockers.emplace_back("Il faut déposer 1 objet(s).");
  probe.Settle();
  const ImGuiWindow* window = MainWindow();
  REQUIRE(window != nullptr);
  const float x = window->Pos.x + window->Size.x * 0.75F;
  for (float y = window->Pos.y + window->Size.y * 0.5F; y < window->Pos.y + window->Size.y;
       y += kSweepStepPixels * 4.0F) {
    probe.MoveTo(ImVec2(x, y));
    static_cast<void>(probe.Click());
  }
  CHECK_FALSE(probe.state.asking);
}

TEST_CASE("Fenêtre du Laphine : la transformation part après sa confirmation", "[ui][laphine]") {
  Probe probe;
  probe.deposited.push_back(probe.candidates[0]);
  probe.Settle();
  const ImVec2 make = MakeButton(probe);
  REQUIRE(make.x > 0.0F);
  probe.MoveTo(make);
  const LaphineWindowAction clicked = probe.Click();
  // 🔴 LE CLIC SEUL NE TRANSFORME RIEN.
  CHECK_FALSE(clicked.make);
  CHECK(probe.state.asking);
  probe.Settle();

  const ImVec2 yes = YesButton(probe);
  REQUIRE(yes.x > 0.0F);
  probe.MoveTo(yes);
  const LaphineWindowAction confirmed = probe.Click();
  CHECK(confirmed.make);
  CHECK(confirmed.piles_seq == kPilesSeq);
  CHECK_FALSE(probe.state.asking);
}

TEST_CASE("Fenêtre du Laphine : un objet déposé se retire", "[ui][laphine]") {
  Probe probe;
  probe.deposited.push_back(probe.candidates[0]);
  probe.Settle();
  const ImGuiWindow* window = MainWindow();
  REQUIRE(window != nullptr);
  // Sa croix est au bout de sa ligne, dans le volet de droite.
  bool withdrew = false;
  for (float y = window->Pos.y + kEdgeMarginPixels;
       y < window->Pos.y + window->Size.y * 0.6F && !withdrew; y += kSweepStepPixels) {
    const ImVec2 at(window->Pos.x + window->Size.x - 2.0F * kEdgeMarginPixels - kSweepStepPixels,
                    y);
    probe.MoveTo(at);
    if (GImGui->HoveredId != 0) {
      withdrew = probe.Click().withdraw_slot == kSwordSlot;
    }
  }
  CHECK(withdrew);
}

TEST_CASE("Fenêtre du Laphine : fermer le dit", "[ui][laphine]") {
  Probe probe;
  probe.Settle();
  probe.state.open = false;
  // Baissée entre deux images, comme Échap le fait : `app` lit l'interrupteur.
  CHECK_FALSE(probe.Frame().close);
  ro::ui::OpenLaphineOn(probe.state, 2);
  CHECK(probe.state.open);
  CHECK(probe.state.opening_seen == 2);
}
