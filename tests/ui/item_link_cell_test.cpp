// LA CELLULE D'UN OBJET CLIQUABLE : ce qu'un survol, un clic, un Maj + clic et
// un clic droit écrivent, pour un objet tenu (jeton) comme pour un monstre —
// sur de vraies images ImGui, à la souris.

#include "ro/ui/item_link_cell.h"

#include <optional>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/ui_context.h"

using ro::ui::ChatLinkAsk;
using ro::ui::ChatLinkGesture;
using ro::ui::ChatLinkKind;
using ro::ui::ItemLinkCell;
using ro::ui::ItemLinkMenu;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 640.0F, .height_px = 480.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr int kSettleFrames = 4;
constexpr float kScanStep = 4.0F;
constexpr std::uint32_t kSwordId = 1101;
constexpr std::uint32_t kJellopyId = 909;
constexpr std::uint32_t kPoringId = 1002;
constexpr std::uint32_t kPieceToken = 7;
constexpr const char* kMenuId = "##item_link_cell_test_menu";

// UNE FENÊTRE, une cellule, son menu.
struct CellProbe {
  ro::ui::UiContext ui;
  ItemLinkCell cell;
  ItemLinkMenu menu;
  ChatLinkGesture played = ChatLinkGesture::kNothing;

  ChatLinkAsk Frame() {
    ChatLinkAsk asked;
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::Begin("##item_link_cell_test", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    played = DrawItemLinkCell(cell, menu, asked);
    DrawItemLinkMenu(kMenuId, menu, asked);
    ImGui::End();
    ui.EndFrame();
    return asked;
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      static_cast<void>(Frame());
    }
  }

  // Balaie la fenêtre jusqu'à survoler la cellule ; rend le point.
  std::optional<ImVec2> Find() {
    for (float y = 1.0F; y < kViewport.height_px; y += kScanStep) {
      for (float x = 1.0F; x < kViewport.width_px / 2.0F; x += kScanStep) {
        ImGui::GetIO().AddMousePosEvent(x, y);
        if (Frame().hovering) {
          return ImVec2(x, y);
        }
      }
    }
    return std::nullopt;
  }

  // Un clic complet ; rend la première demande qu'il a fait naître.
  ChatLinkAsk Click(ImGuiMouseButton button) {
    ImGuiIO& io = ImGui::GetIO();
    ChatLinkAsk kept;
    for (int step = 0; step < 3; ++step) {
      if (step == 1) {
        io.AddMouseButtonEvent(button, true);
      } else if (step == 2) {
        io.AddMouseButtonEvent(button, false);
      }
      const ChatLinkAsk asked = Frame();
      if (kept.gesture == ChatLinkGesture::kNothing) {
        kept = asked;
      }
    }
    return kept;
  }
};

}  // namespace

TEST_CASE("Cellule d'objet : le survol nomme l'objet tenu, jeton compris, et le clic le décrit",
          "[ui][item_link]") {
  CellProbe probe;
  probe.cell =
      ItemLinkCell{.item_id = kSwordId, .token = kPieceToken, .text = "Épée +7", .damaged = true};
  probe.Settle();
  REQUIRE(probe.Find().has_value());
  const ChatLinkAsk hovered = probe.Frame();
  CHECK(hovered.hovered_id == kSwordId);
  CHECK(hovered.hovered_token == kPieceToken);
  CHECK(hovered.hovered_kind == ChatLinkKind::kItem);
  CHECK(ImGui::GetMouseCursor() == ImGuiMouseCursor_Hand);

  const ChatLinkAsk clicked = probe.Click(ImGuiMouseButton_Left);
  CHECK(clicked.gesture == ChatLinkGesture::kDescribe);
  CHECK(clicked.item_id == kSwordId);
  CHECK(clicked.token == kPieceToken);
}

TEST_CASE("Cellule d'objet : Maj + clic repose le lien", "[ui][item_link]") {
  CellProbe probe;
  probe.cell = ItemLinkCell{.item_id = kJellopyId, .text = "Jellopy"};
  probe.Settle();
  REQUIRE(probe.Find().has_value());
  ImGui::GetIO().AddKeyEvent(ImGuiMod_Shift, true);
  const ChatLinkAsk clicked = probe.Click(ImGuiMouseButton_Left);
  ImGui::GetIO().AddKeyEvent(ImGuiMod_Shift, false);
  CHECK(clicked.gesture == ChatLinkGesture::kRepost);
  CHECK(clicked.item_id == kJellopyId);
}

TEST_CASE("Cellule d'objet : le clic droit d'un monstre ouvre son menu sous son seul nom",
          "[ui][item_link]") {
  CellProbe probe;
  probe.cell = ItemLinkCell{.item_id = kPoringId,
                            .kind = ChatLinkKind::kMonster,
                            .text = "Poring — 12,5 %",
                            .menu_label = "Poring"};
  probe.Settle();
  REQUIRE(probe.Find().has_value());
  const ChatLinkAsk clicked = probe.Click(ImGuiMouseButton_Right);
  // Le clic droit n'est pas un geste à jouer : il arme le menu, rien d'autre.
  CHECK(clicked.gesture == ChatLinkGesture::kNothing);
  CHECK(probe.menu.kind == ChatLinkKind::kMonster);
  CHECK(probe.menu.item_id == kPoringId);
  CHECK(probe.menu.label == "Poring");
  CHECK_FALSE(probe.menu.armed);
  CHECK(GImGui->OpenPopupStack.Size == 1);
}
