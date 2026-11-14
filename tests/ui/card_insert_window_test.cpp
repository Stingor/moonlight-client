// LA FENÊTRE DE SERTISSAGE : ce qu'elle rend au clic, et ce qu'elle oublie.
//
// 🔴 LA SÉLECTION SE TIENT PAR LA PLACE, et elle TOMBE deux fois : quand une
// nouvelle liste arrive — celle redemandée après un sertissage ressemble à la
// précédente, mais n'est plus la même —, et quand la pièce choisie n'y est plus.
// Gardée, elle ferait sertir d'un clic sur « Sertir » une pièce que le joueur
// ne voit plus choisie.

#include "ro/ui/card_insert_window.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/ui_context.h"

using ro::render::SkinPiece;
using ro::ui::CardInsertRowView;
using ro::ui::CardInsertVerb;
using ro::ui::CardInsertView;
using ro::ui::CardInsertWindowAction;
using ro::ui::CardInsertWindowState;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;
constexpr std::uint16_t kSwordSlot = 33;
constexpr std::uint16_t kShieldSlot = 40;
constexpr std::uint32_t kSwordId = 1101;
constexpr std::uint32_t kShieldId = 2101;

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

// Une session ouverte sur deux pièces, dessinée image par image.
struct InsertProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  CardInsertWindowState state;
  std::array<CardInsertRowView, 2> rows = {
      CardInsertRowView{
          .equip_slot = kSwordSlot, .item_id = kSwordId, .name = "Sword [3]", .total_slots = 3},
      CardInsertRowView{
          .equip_slot = kShieldSlot, .item_id = kShieldId, .name = "Guard [1]", .total_slots = 1}};
  CardInsertView view{.card_found = true,
                      .card_name = "Poring Card",
                      .stock = 5,
                      .rows = rows,
                      .listed = 2,
                      .offer_seq = 1};

  InsertProbe() { state.open = true; }

  CardInsertWindowAction Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    CardInsertWindowAction action = ro::ui::DrawCardInsertWindow(state, view, art);
    ui.EndFrame();
    return action;
  }

  void Settle() {
    for (int frame = 0; frame < 4; ++frame) {
      static_cast<void>(Frame());
    }
  }
};

// Balaie la fenêtre jusqu'à survoler la pièce voulue.
[[nodiscard]] ImVec2 FindRow(InsertProbe& probe, std::uint16_t equip_slot) {
  const ImGuiWindow* window = ImGui::FindWindowByName("###card_insert");
  REQUIRE(window != nullptr);
  ImGuiIO& io = ImGui::GetIO();
  for (float y = window->Pos.y; y < window->Pos.y + window->Size.y; y += 3.0F) {
    for (float x = window->Pos.x; x < window->Pos.x + window->Size.x; x += 15.0F) {
      io.AddMousePosEvent(x, y);
      if (probe.Frame().hovered_slot == equip_slot) {
        return ImVec2(x, y);
      }
    }
  }
  return ImVec2(-1.0F, -1.0F);
}

void Click(InsertProbe& probe, CardInsertWindowAction* released) {
  ImGuiIO& io = ImGui::GetIO();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  static_cast<void>(probe.Frame());
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  *released = probe.Frame();
}

}  // namespace

TEST_CASE("Sertissage : fermée, la fenêtre ne dessine rien", "[ui][sertissage]") {
  InsertProbe probe;
  probe.state.open = false;
  CHECK(probe.Frame().verb == CardInsertVerb::kNothing);
  CHECK(ImGui::FindWindowByName("###card_insert") == nullptr);
}

TEST_CASE("Sertissage : survoler une pièce la rapporte, avec son objet", "[ui][sertissage]") {
  InsertProbe probe;
  probe.Settle();
  const ImVec2 at = FindRow(probe, kShieldSlot);
  REQUIRE(at.x >= 0.0F);
  const CardInsertWindowAction action = probe.Frame();
  CHECK(action.hovered_slot == kShieldSlot);
  CHECK(action.hovered_id == kShieldId);
}

TEST_CASE("Sertissage : un clic choisit, un double-clic sertit une fois", "[ui][sertissage]") {
  InsertProbe probe;
  probe.Settle();
  REQUIRE(FindRow(probe, kSwordSlot).x >= 0.0F);
  CardInsertWindowAction released;
  Click(probe, &released);
  CHECK(probe.state.selected == kSwordSlot);
  CHECK(released.verb == CardInsertVerb::kNothing);

  // LE SECOND CLIC, dans le délai : ImGui rend le double-clic à l'ENFONCEMENT.
  ImGuiIO& io = ImGui::GetIO();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  const CardInsertWindowAction pressed = probe.Frame();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  static_cast<void>(probe.Frame());
  REQUIRE(pressed.verb == CardInsertVerb::kInsert);
  CHECK(pressed.equip_slot == kSwordSlot);
  CHECK(pressed.times == 1);
}

TEST_CASE("Sertissage : une liste nouvelle efface le choix", "[ui][sertissage]") {
  InsertProbe probe;
  probe.Settle();
  probe.state.selected = kSwordSlot;
  static_cast<void>(probe.Frame());
  CHECK(probe.state.selected == kSwordSlot);
  probe.view.offer_seq = 2;
  static_cast<void>(probe.Frame());
  CHECK(probe.state.selected == 0);
}

TEST_CASE("Sertissage : une pièce sortie de la liste n'est plus choisie", "[ui][sertissage]") {
  InsertProbe probe;
  probe.Settle();
  probe.state.selected = kShieldSlot;
  static_cast<void>(probe.Frame());
  probe.view.rows = std::span<const CardInsertRowView>(probe.rows.data(), 1);
  static_cast<void>(probe.Frame());
  CHECK(probe.state.selected == 0);
}
