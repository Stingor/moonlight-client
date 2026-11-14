// LES COÉQUIPIERS DU CHAMP DE BATAILLE SUR LA MINIMAP : leur marque, leur nom
// au survol, à part des membres du groupe.

#include <array>
#include <cmath>
#include <cstddef>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/minimap_window.h"
#include "ro/ui/ui_context.h"

using ro::ui::MinimapBattleMate;
using ro::ui::MinimapCell;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;
constexpr int kSettleFrames = 4;
constexpr int kMapCells = 100;
constexpr float kPlayerCell = 50.0F;
constexpr float kSidePixels = 220.0F;
// L'anticrénelage rentre les sommets d'un demi-pixel : le centre n'en bouge pas.
constexpr float kCenterSlackPixels = 0.75F;

constexpr MinimapCell kMateCell{.x = 30, .y = 70};
constexpr MinimapCell kOtherCell{.x = 70, .y = 30};
// Hors de la carte : rien ne se dessine.
constexpr MinimapCell kStrayCell{.x = 150, .y = 50};
// Des teintes que rien d'autre ne peint.
constexpr ImU32 kLionColor = IM_COL32(200, 40, 40, 255);
constexpr ImU32 kEagleColor = IM_COL32(40, 60, 210, 255);
constexpr ImU32 kPartyColor = IM_COL32(30, 200, 30, 255);

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<ro::render::SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

struct MatesProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  ro::ui::MinimapSettings settings;
  ro::ui::MinimapWindowState state;
  ro::ui::MinimapView view{.map_name = "bat_a01",
                           .cells_width = kMapCells,
                           .cells_height = kMapCells,
                           .player_cell_x = kPlayerCell,
                           .player_cell_y = kPlayerCell};
  std::string logged;

  MatesProbe() { settings.side_px = static_cast<int>(kSidePixels); }

  void Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::LogToBuffer();
    static_cast<void>(ro::ui::DrawMinimapWindow(settings, state, view, art));
    logged = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    ui.EndFrame();
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      Frame();
    }
  }

  [[nodiscard]] static const ImGuiWindow& Window() {
    const ImGuiWindow* window = ImGui::FindWindowByName("###minimap");
    REQUIRE(window != nullptr);
    return *window;
  }

  // Le point de l'écran d'une cellule, au zoom 1 et sans image.
  [[nodiscard]] static ImVec2 CellPoint(MinimapCell cell) {
    const ImVec2 origin = Window().DC.CursorStartPos;
    const auto cells = static_cast<float>(kMapCells);
    return {origin.x + static_cast<float>(cell.x) / cells * kSidePixels,
            origin.y + (cells - static_cast<float>(cell.y)) / cells * kSidePixels};
  }

  void Hover(ImVec2 at) {
    ImGui::GetIO().AddMousePosEvent(at.x, at.y);
    Frame();
    Frame();
  }
};

// Le centre des sommets PEINTS de cette teinte dans la fenêtre, et leur nombre.
struct TintCenter {
  ImVec2 center{0.0F, 0.0F};
  int count = 0;
};

[[nodiscard]] TintCenter CenterOf(const ImGuiWindow& window, ImU32 tint) {
  TintCenter found;
  float sum_x = 0.0F;
  float sum_y = 0.0F;
  for (const ImDrawVert& vertex : window.DrawList->VtxBuffer) {
    if (vertex.col == tint) {
      sum_x += vertex.pos.x;
      sum_y += vertex.pos.y;
      ++found.count;
    }
  }
  if (found.count > 0) {
    found.center =
        ImVec2(sum_x / static_cast<float>(found.count), sum_y / static_cast<float>(found.count));
  }
  return found;
}

}  // namespace

TEST_CASE("Minimap : une vue neuve n'a aucun coéquipier de champ", "[ui][battleground]") {
  ro::ui::MinimapView view;
  CHECK(view.battle_mates.empty());
}

TEST_CASE("Minimap : un coéquipier de champ, dans la couleur de son camp, à sa cellule",
          "[ui][battleground]") {
  MatesProbe probe;
  const std::array<MinimapBattleMate, 3> mates = {
      MinimapBattleMate{.cell = kMateCell, .color = kLionColor, .name = "Ayla"},
      MinimapBattleMate{.cell = kOtherCell, .color = kEagleColor, .name = "Bran"},
      MinimapBattleMate{.cell = kStrayCell, .color = kEagleColor, .name = "Hors carte"}};
  probe.view.battle_mates = mates;
  probe.Settle();

  const TintCenter lion = CenterOf(MatesProbe::Window(), kLionColor);
  REQUIRE(lion.count > 0);
  const ImVec2 wanted = MatesProbe::CellPoint(kMateCell);
  CHECK(lion.center.x == Catch::Approx(wanted.x).margin(kCenterSlackPixels));
  CHECK(lion.center.y == Catch::Approx(wanted.y).margin(kCenterSlackPixels));

  // 🔴 LE COÉQUIPIER HORS DU CADRAGE NE S'ÉCRASE PAS CONTRE LE BORD : seul
  // celui qui est sur la carte peint du bleu.
  const TintCenter eagle = CenterOf(MatesProbe::Window(), kEagleColor);
  REQUIRE(eagle.count > 0);
  const ImVec2 other = MatesProbe::CellPoint(kOtherCell);
  CHECK(eagle.center.x == Catch::Approx(other.x).margin(kCenterSlackPixels));
  CHECK(eagle.center.y == Catch::Approx(other.y).margin(kCenterSlackPixels));
}

TEST_CASE("Minimap : le survol d'un coéquipier de champ dit son nom", "[ui][battleground]") {
  MatesProbe probe;
  const std::array<MinimapBattleMate, 2> mates = {
      MinimapBattleMate{.cell = kMateCell, .color = kLionColor, .name = "Ayla"},
      MinimapBattleMate{.cell = kOtherCell, .color = kLionColor, .name = "Bran"}};
  probe.view.battle_mates = mates;
  probe.Settle();

  probe.Hover(MatesProbe::CellPoint(kMateCell));
  INFO(probe.logged);
  CHECK(probe.logged.find("Ayla") != std::string::npos);
  // ⚠ SEUL CELUI QUI EST SOUS LA SOURIS.
  CHECK(probe.logged.find("Bran") == std::string::npos);

  // Loin de tout coéquipier, aucune bulle.
  probe.Hover(MatesProbe::CellPoint(MinimapCell{.x = 50, .y = 10}));
  CHECK(probe.logged.find("Ayla") == std::string::npos);
}

TEST_CASE("Minimap : un coéquipier de champ ne se confond pas avec un membre du groupe",
          "[ui][battleground]") {
  MatesProbe probe;
  const std::array<MinimapBattleMate, 1> mates = {
      MinimapBattleMate{.cell = kMateCell, .color = kLionColor, .name = "Ayla"}};
  const std::array<ro::ui::MinimapMate, 1> party = {
      ro::ui::MinimapMate{.cell = kMateCell, .color = kPartyColor}};
  probe.view.battle_mates = mates;
  probe.view.party = party;
  probe.Settle();
  // Un carré de groupe a quatre sommets pleins ; le losange en a autant, mais
  // pas aux mêmes places : ses pointes sont sur les axes de la cellule.
  const ImVec2 at = MatesProbe::CellPoint(kMateCell);
  bool tip_on_axis = false;
  for (const ImDrawVert& vertex : MatesProbe::Window().DrawList->VtxBuffer) {
    if (vertex.col == kLionColor && std::abs(vertex.pos.x - at.x) < kCenterSlackPixels &&
        std::abs(vertex.pos.y - at.y) > kCenterSlackPixels) {
      tip_on_axis = true;
    }
  }
  CHECK(tip_on_axis);
  // Et le losange se peint APRÈS le carré : à la même case, c'est lui qu'on voit.
  int party_last = -1;
  int mate_first = -1;
  const ImVector<ImDrawVert>& vertices = MatesProbe::Window().DrawList->VtxBuffer;
  for (int rank = 0; rank < vertices.Size; ++rank) {
    if (vertices[rank].col == kPartyColor) {
      party_last = rank;
    }
    if (vertices[rank].col == kLionColor && mate_first < 0) {
      mate_first = rank;
    }
  }
  REQUIRE(party_last >= 0);
  REQUIRE(mate_first >= 0);
  CHECK(mate_first > party_last);
}
