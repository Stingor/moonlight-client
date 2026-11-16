// LA FENÊTRE D'ÉQUIPEMENT DU JEU : sa largeur prise au fond, les zones de clic
// de ses places, les couleurs de ses noms, et ses gestes.
//
// 🔴 CE SONT LES NOMBRES DU JEU : une rangée décalée d'une place échange deux
// pièces sous la souris, et le joueur la verrait au premier survol.

#include "ro/ui/equip_window.h"

#include <cmath>
#include <cstddef>
#include <optional>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/ui_context.h"

using ro::render::SkinPiece;
using ro::ui::EquipWindowEntryAt;
using ro::ui::EquipWindowIconAt;
using ro::ui::EquipWindowLayoutFor;
using ro::ui::kEquipWindowAmmoPlace;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr int kSettleFrames = 4;
constexpr const char* kWindowName = "###equip";
constexpr float kGameWidth = 280.0F;
constexpr float kMoonlightWidth = 340.0F;
constexpr float kBackgroundHeight = 150.0F;
constexpr float kPlainPiecePixels = 8.0F;
constexpr float kTakeOffAllWidth = 40.0F;
constexpr float kTakeOffAllHeight = 20.0F;

// Le haut de chaque rangée de clic, puis un point au milieu de l'une.
constexpr float kFirstRowY = 40.0F;
constexpr float kSecondRowY = 70.0F;
constexpr float kLastRowY = 160.0F;
constexpr float kLeftIconX = 10.0F;
constexpr float kLeftNameX = 60.0F;
constexpr float kRightNameX = 200.0F;

[[nodiscard]] ro::ui::SkinArt EquipArt(float width) {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  art.Set(SkinPiece::kEquipBackground,
          ro::ui::SkinImage{.id = 1, .width = width, .height = kBackgroundHeight});
  for (const SkinPiece piece : {SkinPiece::kEquipTakeOffAllOut, SkinPiece::kEquipTakeOffAllOver,
                                SkinPiece::kEquipTakeOffAllOff}) {
    art.Set(piece,
            ro::ui::SkinImage{.id = 1, .width = kTakeOffAllWidth, .height = kTakeOffAllHeight});
  }
  return art;
}

struct EquipProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = EquipArt(kMoonlightWidth);
  ro::ui::EquipWindowState state;
  ro::ui::EquipWindowView view;

  EquipProbe() { state.open = true; }

  ro::ui::EquipWindowAction Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const ro::ui::EquipWindowAction action = ro::ui::DrawEquipWindow(state, view, art);
    ui.EndFrame();
    return action;
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      static_cast<void>(Frame());
    }
  }

  // Un point en pixels d'art du jeu : le corps se cale sous la barre réelle.
  [[nodiscard]] static ImVec2 At(float x, float y) {
    const ImGuiWindow* window = ImGui::FindWindowByName(kWindowName);
    REQUIRE(window != nullptr);
    const float scale = ImGui::GetStyle().FontScaleMain;
    return {window->Pos.x + x * scale, window->Pos.y + window->TitleBarHeight +
                                           (y - ro::ui::kEquipWindowTitleBarPixels) * scale};
  }

  // Ce que l'image de l'appui, puis celle du relâché, ont demandé : une case à
  // cocher répond à l'appui, un bouton peint au relâché.
  struct Clicked {
    ro::ui::EquipWindowAction pressed;
    ro::ui::EquipWindowAction released;
  };
  Clicked Click(ImVec2 at) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(at.x, at.y);
    static_cast<void>(Frame());
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    Clicked clicked;
    clicked.pressed = Frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    clicked.released = Frame();
    return clicked;
  }
};

}  // namespace

TEST_CASE("Équipement : la fenêtre prend la largeur de son fond, 280 sans lui",
          "[ui][equip_window]") {
  EquipProbe probe;
  probe.Settle();
  const ImGuiWindow* window = ImGui::FindWindowByName(kWindowName);
  REQUIRE(window != nullptr);
  CHECK(window->Size.x == kMoonlightWidth);
  // LE CORPS DU JEU SOUS LA BARRE RÉELLE, que règlent l'art et la police.
  CHECK(window->Size.y == window->TitleBarHeight + ro::ui::kEquipWindowHeightPixels -
                              ro::ui::kEquipWindowTitleBarPixels);
  CHECK(probe.state.avatar_box.x == kMoonlightWidth);
  CHECK(probe.state.avatar_box.y == ro::ui::kEquipWindowAvatarFeetPixels);

  CHECK(ro::ui::EquipWindowWidth(ro::ui::SkinArt{}) == kGameWidth);
}

TEST_CASE("Équipement : élargie, la colonne de droite s'écarte et les noms respirent",
          "[ui][equip_window]") {
  const ro::ui::EquipWindowLayout game = EquipWindowLayoutFor(kGameWidth);
  CHECK(game.right_icon == 251.0F);
  CHECK(game.right_name == 172.0F);
  CHECK(game.name_room == 70.0F);

  const ro::ui::EquipWindowLayout wide = EquipWindowLayoutFor(kMoonlightWidth);
  CHECK(wide.right_icon == 311.0F);
  CHECK(wide.right_highlight == 308.0F);
  CHECK(wide.right_name == 205.0F);
  CHECK(wide.name_room == 127.0F);
}

TEST_CASE("Équipement : les rangées désignent les places dans l'ordre des bits",
          "[ui][equip_window]") {
  const ro::ui::EquipWindowLayout layout = EquipWindowLayoutFor(kMoonlightWidth);
  // À gauche : la coiffe haute en tête, puis les places 0 à 3.
  CHECK(EquipWindowEntryAt(layout, ImVec2(kLeftNameX, kFirstRowY)) ==
        std::optional<std::size_t>(8));
  CHECK(EquipWindowEntryAt(layout, ImVec2(kLeftNameX, kSecondRowY)) ==
        std::optional<std::size_t>(0));
  CHECK(EquipWindowEntryAt(layout, ImVec2(kLeftNameX, kLastRowY)) == std::optional<std::size_t>(3));
  // À droite : la coiffe du milieu en tête, puis les places 4 à 7.
  CHECK(EquipWindowEntryAt(layout, ImVec2(kRightNameX, kFirstRowY)) ==
        std::optional<std::size_t>(9));
  CHECK(EquipWindowEntryAt(layout, ImVec2(kRightNameX, kSecondRowY)) ==
        std::optional<std::size_t>(4));
  CHECK(EquipWindowEntryAt(layout, ImVec2(kRightNameX, kLastRowY)) ==
        std::optional<std::size_t>(7));
  // Hors de la bande, rien.
  CHECK_FALSE(EquipWindowEntryAt(layout, ImVec2(kLeftNameX, 20.0F)).has_value());
  CHECK_FALSE(EquipWindowEntryAt(layout, ImVec2(kLeftNameX, 170.0F)).has_value());
}

TEST_CASE("Équipement : seule l'icône se prend, et la munition sous le personnage",
          "[ui][equip_window]") {
  const ro::ui::EquipWindowLayout layout = EquipWindowLayoutFor(kMoonlightWidth);
  CHECK(EquipWindowIconAt(layout, ImVec2(kLeftIconX, kSecondRowY)) ==
        std::optional<std::size_t>(0));
  // Le nom n'est pas une prise.
  CHECK_FALSE(EquipWindowIconAt(layout, ImVec2(kLeftNameX, kSecondRowY)).has_value());
  // L'icône de droite a suivi l'élargissement.
  CHECK(EquipWindowIconAt(layout, ImVec2(320.0F, kSecondRowY)) == std::optional<std::size_t>(4));
  CHECK_FALSE(EquipWindowIconAt(layout, ImVec2(260.0F, kSecondRowY)).has_value());
  // La munition : vingt-quatre pixels sous le milieu de la fenêtre, à gauche.
  CHECK(EquipWindowIconAt(layout, ImVec2(160.0F, 75.0F)) ==
        std::optional<std::size_t>(kEquipWindowAmmoPlace));
}

TEST_CASE("Équipement : un nom prend la couleur de ses cartes au-delà des emplacements",
          "[ui][equip_window]") {
  CHECK(ro::ui::EquipWindowNameColor(0) == IM_COL32(0x00, 0x00, 0x00, 0xFF));
  CHECK(ro::ui::EquipWindowNameColor(1) == IM_COL32(0x66, 0x99, 0x00, 0xFF));
  CHECK(ro::ui::EquipWindowNameColor(4) == IM_COL32(0x94, 0x00, 0xD3, 0xFF));
  // Au-delà de quatre, la dernière.
  CHECK(ro::ui::EquipWindowNameColor(9) == ro::ui::EquipWindowNameColor(4));
}

TEST_CASE("Équipement : « tout retirer » demande l'onglet montré, et s'éteint sans rien à ôter",
          "[ui][equip_window][equip_take_off_all]") {
  EquipProbe probe;
  probe.Settle();
  const ImVec2 button = EquipProbe::At(255.0F, 177.0F);
  // Rien de porté : le bouton est éteint.
  CHECK_FALSE(probe.Click(button).released.take_off_all_costume.has_value());

  probe.view.general[1].piece.client_slot = 7;
  probe.Settle();
  CHECK(probe.Click(button).released.take_off_all_costume == std::optional<bool>(false));

  // En attente de l'accusé, il se tait.
  probe.view.take_off_all_pending = true;
  probe.Settle();
  CHECK_FALSE(probe.Click(button).released.take_off_all_costume.has_value());
}

TEST_CASE("Équipement : la case à cocher demande l'autre état que le serveur a dit",
          "[ui][equip_window][equip_setting_box]") {
  EquipProbe probe;
  probe.view.equipment_shown = false;
  probe.Settle();
  const ro::ui::EquipWindowAction action = probe.Click(EquipProbe::At(14.0F, 177.0F)).pressed;
  CHECK(action.sheet.show_equipment == std::optional<bool>(true));
}

TEST_CASE("Équipement : réduite, elle ne garde que sa barre", "[ui][equip_window]") {
  EquipProbe probe;
  probe.state.minimized = true;
  probe.Settle();
  const ImGuiWindow* window = ImGui::FindWindowByName(kWindowName);
  REQUIRE(window != nullptr);
  CHECK(window->Collapsed);
  CHECK(window->Size.y == window->TitleBarHeight);
  CHECK(probe.state.minimized);

  // Son bouton la rouvre, et l'état retenu suit.
  const float scale = ImGui::GetStyle().FontScaleMain;
  // Le bouton de réduction : à gauche de la croix, depuis le bord droit.
  const ImVec2 minimize(window->Pos.x + window->Size.x -
                            (ro::ui::kSystemEdgePixels + kPlainPiecePixels +
                             ro::ui::kSystemButtonGapPixels + kPlainPiecePixels / 2.0F) *
                                scale,
                        window->Pos.y + window->TitleBarHeight / 2.0F);
  static_cast<void>(probe.Click(minimize));
  probe.Settle();
  CHECK_FALSE(probe.state.minimized);
  CHECK_FALSE(window->Collapsed);
}

TEST_CASE("Équipement : la fenêtre se traîne par ImGui, donc par l'aimant", "[ui][equip_window]") {
  EquipProbe probe;
  probe.Settle();
  const ImGuiWindow* window = ImGui::FindWindowByName(kWindowName);
  REQUIRE(window != nullptr);
  CHECK((window->Flags & ImGuiWindowFlags_NoMove) == 0);
}
