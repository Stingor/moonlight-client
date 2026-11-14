// LA FENÊTRE DU MARCHÉ, sur de vraies images ImGui : l'ouverture sur une
// saisie, « Y aller » et « Voir » qui rendent le rang de leur ligne, et le
// contenu d'une échoppe qui s'ouvre et se ferme avec elle.

#include "ro/ui/market_window.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/ui_context.h"

using ro::ui::MarketAction;
using ro::ui::MarketRowView;
using ro::ui::MarketView;
using ro::ui::MarketWindowState;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 900.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;
constexpr int kSettleFrames = 4;
constexpr float kSweepStepPixels = 3.0F;
constexpr float kEdgeMarginPixels = 6.0F;
constexpr ImTextureID kSkinTexture = 1;

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<ro::render::SkinPiece>(rank),
            ro::ui::SkinImage{
                .id = kSkinTexture, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

[[nodiscard]] MarketRowView Row() {
  MarketRowView row;
  row.item = ro::ui::TradeItemView{.item_id = 1201, .name = "Couteau"};
  row.price = "1,500 z";
  row.amount = "3";
  row.shop_name = "Chez Poring";
  row.place = "prontera 150,120";
  return row;
}

struct MarketProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  MarketWindowState state;
  std::array<MarketRowView, 1> rows = {Row()};
  MarketView view;

  MarketProbe() {
    view.rows = rows;
    view.page_count = 1;
    view.summary = "1 offre";
    view.store_title = "Chez Poring";
    view.store_rows = rows;
    ro::ui::OpenMarketOn(state, "1201");
  }

  MarketAction Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const MarketAction action = ro::ui::DrawMarketWindow(state, view, art);
    ui.EndFrame();
    return action;
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      static_cast<void>(Frame());
    }
  }

  // Un clic là où la souris se trouve : l'enfoncement et le relâchement réunis.
  MarketAction Click() {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    const MarketAction pressed = Frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    MarketAction released = Frame();
    if (released.go_to_row == ro::ui::kMarketNoRow) {
      released.go_to_row = pressed.go_to_row;
    }
    if (released.view_store_row == ro::ui::kMarketNoRow) {
      released.view_store_row = pressed.view_store_row;
    }
    return released;
  }
};

[[nodiscard]] ImGuiWindow* MarketWindow() {
  for (ImGuiWindow* window : GImGui->Windows) {
    const std::string_view name(window->Name);
    if (window->WasActive && name.find(ro::ui::kMarketWindowId) != std::string_view::npos) {
      return window;
    }
  }
  return nullptr;
}

// LA HAUTEUR DE LA PREMIÈRE LIGNE : balayée jusqu'à survoler son item.
[[nodiscard]] float FirstRowY(MarketProbe& probe) {
  const ImGuiWindow* window = MarketWindow();
  REQUIRE(window != nullptr);
  constexpr float kItemCellPixels = 60.0F;
  constexpr float kItemStepPixels = 15.0F;
  for (float y = window->Pos.y; y < window->Pos.y + window->Size.y; y += kSweepStepPixels) {
    for (float x = window->Pos.x + kEdgeMarginPixels;
         x < window->Pos.x + kEdgeMarginPixels + kItemCellPixels; x += kItemStepPixels) {
      ImGui::GetIO().AddMousePosEvent(x, y);
      if (probe.Frame().hovered_row == 0) {
        return y;
      }
    }
  }
  return -1.0F;
}

// Les contrôles d'une rangée, de droite à gauche : le n-ième distinct.
[[nodiscard]] ImVec2 ControlFromRight(MarketProbe& probe, float y, int rank) {
  const ImGuiWindow* window = MarketWindow();
  REQUIRE(window != nullptr);
  ImGuiID last = 0;
  int seen = 0;
  for (float x = window->Pos.x + window->Size.x - kEdgeMarginPixels; x > window->Pos.x;
       x -= kSweepStepPixels) {
    ImGui::GetIO().AddMousePosEvent(x, y);
    static_cast<void>(probe.Frame());
    const ImGuiID hovered = GImGui->HoveredId;
    if (hovered != 0 && hovered != last) {
      last = hovered;
      if (seen == rank) {
        return ImVec2(x, y);
      }
      ++seen;
    }
  }
  return ImVec2(-1.0F, -1.0F);
}

// LA TABLE DES OFFRES dessinée à la dernière image.
[[nodiscard]] ImGuiTable* ResultTable() {
  constexpr int kResultColumns = ro::ui::kMarketActionsColumn + 1;
  for (int index = 0; index < GImGui->Tables.GetMapSize(); ++index) {
    if (ImGuiTable* found = GImGui->Tables.TryGetMapData(index);
        found != nullptr && found->LastFrameActive == GImGui->FrameCount &&
        found->ColumnsCount == kResultColumns) {
      return found;
    }
  }
  return nullptr;
}

// La fenêtre du Marché à `width` pixels de large, posée puis laissée se faire.
void ResizeMarket(MarketProbe& probe, float width) {
  ImGuiWindow* window = MarketWindow();
  REQUIRE(window != nullptr);
  ImGui::SetWindowSize(window, ImVec2(width, window->Size.y), ImGuiCond_Always);
  probe.Settle();
}

[[nodiscard]] bool TooltipShown() {
  for (const ImGuiWindow* window : GImGui->Windows) {
    if ((window->Flags & ImGuiWindowFlags_Tooltip) != 0 && window->Active) {
      return true;
    }
  }
  return false;
}

// LE SURVOL DU NOM de la première ligne : une bulle s'ouvre-t-elle ?
[[nodiscard]] bool ShopNameTooltip(MarketProbe& probe) {
  const float row = FirstRowY(probe);
  REQUIRE(row >= 0.0F);
  const ImGuiTable* table = ResultTable();
  REQUIRE(table != nullptr);
  const ImGuiTableColumn& shop = table->Columns[ro::ui::kMarketShopColumn];
  ImGui::GetIO().AddMousePosEvent((shop.MinX + shop.MaxX) * 0.5F, row);
  static_cast<void>(probe.Frame());
  static_cast<void>(probe.Frame());
  return TooltipShown();
}

// LE BORD DROIT DE LA COLONNE DES ITEMS, tiré de `offset` pixels à la souris.
void DragItemBorder(MarketProbe& probe, float offset) {
  const ImGuiTable* table = ResultTable();
  REQUIRE(table != nullptr);
  const float x = table->Columns[ro::ui::kMarketItemColumn].MaxX;
  const float y = table->OuterRect.Min.y + kEdgeMarginPixels;
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(x, y);
  probe.Settle();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  static_cast<void>(probe.Frame());
  io.AddMousePosEvent(x + offset, y);
  probe.Settle();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  probe.Settle();
}

}  // namespace

TEST_CASE("Marché : l'ouverture pose la saisie et garde le tri", "[ui][market]") {
  MarketWindowState state;
  state.sort_column = ro::ui::kMarketShopColumn;
  state.sort_descending = true;
  state.page = 4;
  state.query[0] = 'x';
  ro::ui::OpenMarketOn(state, "4001");
  CHECK(state.open);
  CHECK(state.raise);
  CHECK(std::string(state.query.data()) == "4001");
  CHECK(state.page == 1);
  CHECK(state.sort_column == ro::ui::kMarketShopColumn);
  CHECK(state.sort_descending);
  CHECK_FALSE(state.store_open);
  ro::ui::OpenMarketOn(state, "");
  CHECK(state.query[0] == '\0');
}

TEST_CASE("Marché : rouvert sur l'onglet PNJ, il y reste sans rien redemander", "[ui][market]") {
  MarketProbe probe;
  probe.state.tab = ro::ui::kMarketNpcTab;
  ro::ui::OpenMarketOn(probe.state, "");
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    CHECK_FALSE(probe.Frame().search);
  }
  CHECK(probe.state.tab == ro::ui::kMarketNpcTab);
  CHECK_FALSE(probe.state.select_tab);
}

TEST_CASE("Marché : fermée, la fenêtre ne dessine rien et ne demande rien", "[ui][market]") {
  MarketProbe probe;
  probe.state.open = false;
  const MarketAction action = probe.Frame();
  CHECK_FALSE(action.search);
  CHECK(MarketWindow() == nullptr);
}

TEST_CASE("Marché : « Voir » puis « Y aller » rendent le rang de leur ligne", "[ui][market]") {
  MarketProbe probe;
  probe.Settle();
  const float row = FirstRowY(probe);
  REQUIRE(row >= 0.0F);
  // « Voir » est le dernier bouton de la rangée, « Y aller » le précédent.
  const ImVec2 view_button = ControlFromRight(probe, row, 0);
  REQUIRE(view_button.x >= 0.0F);
  const MarketAction viewed = probe.Click();
  CHECK(viewed.view_store_row == 0);
  CHECK(viewed.go_to_row == ro::ui::kMarketNoRow);
  const ImVec2 go_button = ControlFromRight(probe, row, 1);
  REQUIRE(go_button.x >= 0.0F);
  REQUIRE(go_button.x < view_button.x);
  const MarketAction went = probe.Click();
  CHECK(went.go_to_row == 0);
  CHECK(went.view_store_row == ro::ui::kMarketNoRow);
}

TEST_CASE("Marché : le contenu d'une échoppe se ferme avec le Marché", "[ui][market]") {
  MarketProbe probe;
  probe.state.store_open = true;
  probe.Settle();
  const ImGuiWindow* store = ImGui::FindWindowByName(ro::ui::kMarketStoreWindowId);
  REQUIRE(store != nullptr);
  CHECK(store->WasActive);
  // Échap a baissé le Marché : son échoppe ne se dessine plus.
  probe.state.open = false;
  probe.Settle();
  CHECK_FALSE(store->WasActive);
}

TEST_CASE("Marché : à 830 pixels, le nom de l'échoppe prend la place qui reste", "[ui][market]") {
  constexpr float kPlayerWindowPixels = 830.0F;
  constexpr float kWiderPixels = 200.0F;
  constexpr float kRoundingPixels = 2.0F;
  MarketProbe probe;
  probe.Settle();
  ResizeMarket(probe, kPlayerWindowPixels);
  const ImGuiTable* table = ResultTable();
  REQUIRE(table != nullptr);
  CHECK(table->Columns[ro::ui::kMarketShopColumn].WidthGiven >=
        ro::ui::kMarketShopColumnLeastPixels);
  // La carte tient un nom de carte et sa case sans être coupée.
  CHECK(table->Columns[ro::ui::kMarketMapColumn].WidthGiven >=
        ImGui::CalcTextSize("moc_ruins 118,170").x);
  // La table tient dans la fenêtre : rien ne déborde à droite.
  CHECK(table->Columns[ro::ui::kMarketActionsColumn].MaxX <= table->OuterRect.Max.x);

  // ÉLARGIE, la fenêtre ne donne sa place qu'au nom.
  const float shop_before = table->Columns[ro::ui::kMarketShopColumn].WidthGiven;
  const float item_before = table->Columns[ro::ui::kMarketItemColumn].WidthGiven;
  const float map_before = table->Columns[ro::ui::kMarketMapColumn].WidthGiven;
  ResizeMarket(probe, kPlayerWindowPixels + kWiderPixels);
  table = ResultTable();
  REQUIRE(table != nullptr);
  CHECK(table->Columns[ro::ui::kMarketShopColumn].WidthGiven >=
        shop_before + kWiderPixels - kRoundingPixels);
  CHECK(table->Columns[ro::ui::kMarketItemColumn].WidthGiven == item_before);
  CHECK(table->Columns[ro::ui::kMarketMapColumn].WidthGiven == map_before);
}

TEST_CASE("Marché : la fenêtre ne se rétrécit pas sous la largeur minimale du nom",
          "[ui][market]") {
  constexpr float kNarrowPixels = 300.0F;
  MarketProbe probe;
  probe.Settle();
  ResizeMarket(probe, kNarrowPixels);
  const ImGuiTable* table = ResultTable();
  REQUIRE(table != nullptr);
  CHECK(MarketWindow()->Size.x > kNarrowPixels);
  CHECK(table->Columns[ro::ui::kMarketShopColumn].WidthGiven >=
        ro::ui::kMarketShopColumnLeastPixels);
}

TEST_CASE("Marché : chez des PNJ au stock illimité, la colonne des quantités se masque",
          "[ui][market]") {
  MarketProbe probe;
  probe.Settle();
  REQUIRE(ResultTable() != nullptr);
  CHECK(ResultTable()->Columns[ro::ui::kMarketAmountColumn].IsEnabled);
  probe.view.amount_shown = false;
  probe.Settle();
  REQUIRE(ResultTable() != nullptr);
  CHECK_FALSE(ResultTable()->Columns[ro::ui::kMarketAmountColumn].IsEnabled);
}

TEST_CASE("Marché : un nom de PNJ coupé se lit entier au survol, un nom entier n'a pas de bulle",
          "[ui][market]") {
  MarketProbe probe;
  probe.state.tab = ro::ui::kMarketNpcTab;
  ro::ui::OpenMarketOn(probe.state, "");
  probe.Settle();
  CHECK_FALSE(ShopNameTooltip(probe));
  probe.rows[0].shop_name =
      "Marchande ambulante des remparts de Prontera, fournisseuse attitrée des chevaliers";
  probe.Settle();
  CHECK(ShopNameTooltip(probe));
}

TEST_CASE("Marché : la largeur réglée à la souris survit à la fermeture et au fichier de réglages",
          "[ui][market]") {
  constexpr float kDragPixels = 40.0F;
  constexpr float kRoundingPixels = 2.0F;
  std::string settings;
  float dragged = 0.0F;
  {
    MarketProbe probe;
    probe.Settle();
    REQUIRE(ResultTable() != nullptr);
    const float initial = ResultTable()->Columns[ro::ui::kMarketItemColumn].WidthGiven;
    DragItemBorder(probe, kDragPixels);
    REQUIRE(ResultTable() != nullptr);
    dragged = ResultTable()->Columns[ro::ui::kMarketItemColumn].WidthGiven;
    CHECK(dragged >= initial + kDragPixels - kRoundingPixels);
    // Fermée puis rouverte : la même largeur.
    probe.state.open = false;
    probe.Settle();
    ro::ui::OpenMarketOn(probe.state, "");
    probe.Settle();
    REQUIRE(ResultTable() != nullptr);
    CHECK(ResultTable()->Columns[ro::ui::kMarketItemColumn].WidthGiven == dragged);
    settings = ImGui::SaveIniSettingsToMemory();
  }
  // UNE AUTRE SESSION, qui relit le fichier de réglages.
  MarketProbe probe;
  ImGui::LoadIniSettingsFromMemory(settings.c_str(), settings.size());
  probe.Settle();
  REQUIRE(ResultTable() != nullptr);
  CHECK(ResultTable()->Columns[ro::ui::kMarketItemColumn].WidthGiven == dragged);
}
