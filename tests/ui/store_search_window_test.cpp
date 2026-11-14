// LA FENÊTRE DE RECHERCHE D'ÉCHOPPES : ce qui se juge sans dessiner, puis sur
// de vraies images ImGui — sa taille d'ouverture, la main sur la colonne des
// échoppes, l'emblème du grade sur l'icône.
//
// 🔴 L'ENCRE D'UN PRIX change à chaque ordre de grandeur, et certains ordres se
// doublent d'une ombre : l'essai fige les seuils, qu'une erreur d'un zéro
// décalerait sans que rien ne casse à l'écran.

#include "ro/ui/store_search_window.h"

#include <array>
#include <cfloat>
#include <cstddef>
#include <optional>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/ui_context.h"

using ro::ui::StoreSearchPriceInkOf;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 900.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;
constexpr int kSettleFrames = 4;
constexpr float kScanStepX = 4.0F;
// Les textures du banc, distinctes pour que leurs commandes ne fusionnent pas.
constexpr ImTextureID kSkinTexture = 1;
constexpr ImTextureID kIconTexture = 2;
constexpr ImTextureID kBadgeTexture = 3;
constexpr float kIconPixels = 24.0F;
constexpr float kBadgePixels = 12.0F;
// La taille que l'installation du client d'origine donne à la fenêtre.
constexpr float kOriginWidthPixels = 445.0F;
constexpr float kOriginHeightPixels = 380.0F;

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<ro::render::SkinPiece>(rank),
            ro::ui::SkinImage{
                .id = kSkinTexture, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

[[nodiscard]] ro::ui::StoreSearchRowView Row(bool graded) {
  ro::ui::StoreSearchRowView row;
  row.item = ro::ui::TradeItemView{
      .item_id = 1201,
      .name = "Couteau",
      .icon = ro::ui::SkinImage{.id = kIconTexture, .width = kIconPixels, .height = kIconPixels}};
  if (graded) {
    row.grade_badge =
        ro::ui::SkinImage{.id = kBadgeTexture, .width = kBadgePixels, .height = kBadgePixels};
  }
  row.shop_name = "Chez Poring";
  row.amount = 3;
  row.price_zeny = 1'500;
  row.store_id = 7;
  row.owner_aid = 2'000'001;
  return row;
}

// Le coin haut gauche et le coin bas droit d'une image peinte.
struct PaintedBox {
  ImVec2 low;
  ImVec2 high;
};

struct SearchProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  ro::ui::StoreSearchWindowState state;
  std::array<ro::ui::StoreSearchRowView, 1> rows = {Row(true)};
  ro::ui::StoreSearchView view;

  SearchProbe() {
    view.words.title = "Recherche";
    view.words.shop_name = "Échoppe";
    view.words.item = "Objet";
    view.words.count = "Qté";
    view.words.price = "Prix";
    view.rows = rows;
    ro::ui::OpenStoreSearchOn(state, 1);
  }

  ro::ui::StoreSearchAction Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const ro::ui::StoreSearchAction action = ro::ui::DrawStoreSearchWindow(state, view, art);
    ui.EndFrame();
    return action;
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      static_cast<void>(Frame());
    }
  }

  [[nodiscard]] static ImGuiWindow* Window() {
    return ImGui::FindWindowByName(ro::ui::kStoreSearchWindowId);
  }

  // La boîte de la PREMIÈRE image peinte de cette texture, s'il y en a une.
  [[nodiscard]] static std::optional<PaintedBox> Painted(ImTextureID texture) {
    const ImGuiWindow* window = Window();
    REQUIRE(window != nullptr);
    const ImDrawList& list = *window->DrawList;
    for (const ImDrawCmd& command : list.CmdBuffer) {
      // ⚠ PAS `GetTexID()` : il assertionne sur la texture de la police, qu'aucun
      // moteur n'a chargée dans un banc.
      if (command.TexRef._TexData != nullptr || command.TexRef._TexID != texture ||
          command.ElemCount == 0) {
        continue;
      }
      PaintedBox box{.low = ImVec2(FLT_MAX, FLT_MAX), .high = ImVec2(-FLT_MAX, -FLT_MAX)};
      for (unsigned int rank = 0; rank < command.ElemCount; ++rank) {
        const ImDrawIdx index = list.IdxBuffer[static_cast<int>(command.IdxOffset + rank)];
        const ImVec2 at = list.VtxBuffer[static_cast<int>(command.VtxOffset + index)].pos;
        box.low = ImVec2(ImMin(box.low.x, at.x), ImMin(box.low.y, at.y));
        box.high = ImVec2(ImMax(box.high.x, at.x), ImMax(box.high.y, at.y));
      }
      return box;
    }
    return std::nullopt;
  }
};

}  // namespace

TEST_CASE("Recherche d'échoppes : l'encre d'un prix suit son ordre de grandeur",
          "[ui][store_search]") {
  constexpr ImU32 kRed = IM_COL32(255, 0, 0, 255);
  constexpr ImU32 kBlack = IM_COL32(0, 0, 0, 255);
  constexpr ImU32 kBlue = IM_COL32(0, 0, 255, 255);
  CHECK(StoreSearchPriceInkOf(1'000'000'000).text == kRed);
  CHECK(StoreSearchPriceInkOf(1'000'000'000).shadow == IM_COL32(255, 0, 127, 255));
  CHECK(StoreSearchPriceInkOf(999'999'999).text == kBlack);
  CHECK(StoreSearchPriceInkOf(10'000'000).text == kRed);
  CHECK((StoreSearchPriceInkOf(10'000'000).shadow & IM_COL32_A_MASK) == 0);
  CHECK(StoreSearchPriceInkOf(1'000'000).shadow == IM_COL32(0, 255, 0, 255));
  CHECK(StoreSearchPriceInkOf(100'000).text == kBlue);
  CHECK(StoreSearchPriceInkOf(10'000).text == IM_COL32(255, 30, 255, 255));
  CHECK(StoreSearchPriceInkOf(1'000).shadow == IM_COL32(255, 255, 0, 255));
  CHECK(StoreSearchPriceInkOf(999).text == kBlue);
  CHECK(StoreSearchPriceInkOf(10).shadow == IM_COL32(200, 0, 200, 255));
  CHECK(StoreSearchPriceInkOf(9).text == kBlack);
  CHECK(StoreSearchPriceInkOf(0).shadow == IM_COL32(0, 255, 255, 255));
}

TEST_CASE("Recherche d'échoppes : une ouverture remet la fenêtre à neuf", "[ui][store_search]") {
  ro::ui::StoreSearchWindowState state;
  state.page = 4;
  state.kind = 1;
  state.similar = false;
  state.item_name[0] = 'A';
  state.pending = {.row = 2, .store_id = 7, .owner_aid = 9};
  state.first_page_seen = 9;
  ro::ui::OpenStoreSearchOn(state, 3);
  CHECK(state.open);
  CHECK(state.opening_seen == 3);
  CHECK(state.page == 1);
  CHECK(state.kind == 0);
  CHECK(state.similar);
  CHECK(state.item_name[0] == '\0');
  CHECK_FALSE(state.pending.chosen());
  CHECK(state.first_page_seen == 9);
}

TEST_CASE("Recherche d'échoppes : la fenêtre s'ouvre à la taille du client d'origine",
          "[ui][store_search]") {
  SearchProbe probe;
  probe.Settle();
  const ImGuiWindow* window = SearchProbe::Window();
  REQUIRE(window != nullptr);
  const float scale = ImGui::GetStyle().FontScaleMain;
  CHECK(window->SizeFull.x == kOriginWidthPixels * scale);
  CHECK(window->SizeFull.y == kOriginHeightPixels * scale);
}

TEST_CASE("Recherche d'échoppes : la main ne paraît que sur la colonne des échoppes",
          "[ui][store_search]") {
  SearchProbe probe;
  probe.Settle();
  const std::optional<PaintedBox> icon = SearchProbe::Painted(kIconTexture);
  REQUIRE(icon.has_value());
  // LA LIGNE SE BALAIE À MI-HAUTEUR DE L'ICÔNE, de bord à bord de la fenêtre.
  const ImGuiWindow* window = SearchProbe::Window();
  REQUIRE(window != nullptr);
  const float row_y = (icon->low.y + icon->high.y) / 2.0F;
  std::optional<float> hand_from;
  std::optional<float> hand_to;
  std::optional<float> plain_first;
  std::optional<float> plain_last;
  for (float x = window->Pos.x + 1.0F; x < window->Pos.x + window->Size.x; x += kScanStepX) {
    ImGui::GetIO().AddMousePosEvent(x, row_y);
    if (probe.Frame().hovered_row != 0) {
      continue;
    }
    if (ImGui::GetMouseCursor() == ImGuiMouseCursor_Hand) {
      hand_from = hand_from.value_or(x);
      hand_to = x;
    } else {
      plain_first = plain_first.value_or(x);
      plain_last = x;
    }
  }
  REQUIRE(hand_from.has_value());
  REQUIRE(plain_first.has_value());
  // L'icône, à gauche, et l'objet, à droite, gardent la flèche : la main se
  // tient entre les deux.
  CHECK(*plain_first < *hand_from);
  CHECK(*hand_to < *plain_last);
}

TEST_CASE("Recherche d'échoppes : l'emblème du grade se pose dans le coin bas gauche de l'icône",
          "[ui][store_search]") {
  SearchProbe probe;
  probe.Settle();
  const std::optional<PaintedBox> icon = SearchProbe::Painted(kIconTexture);
  const std::optional<PaintedBox> badge = SearchProbe::Painted(kBadgeTexture);
  REQUIRE(icon.has_value());
  REQUIRE(badge.has_value());
  const float scale = ImGui::GetStyle().FontScaleMain;
  CHECK(badge->low.x == icon->low.x);
  CHECK(badge->high.y == icon->high.y);
  CHECK(badge->high.x - badge->low.x == kBadgePixels * scale);
  CHECK(badge->high.y - badge->low.y == kBadgePixels * scale);

  SECTION("sans grade, aucun emblème") {
    probe.rows[0] = Row(false);
    probe.Settle();
    CHECK(SearchProbe::Painted(kIconTexture).has_value());
    CHECK_FALSE(SearchProbe::Painted(kBadgeTexture).has_value());
  }
}
