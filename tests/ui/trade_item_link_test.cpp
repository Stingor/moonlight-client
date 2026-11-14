// UN ITEM DE COMMERCE SE MANIE COMME UNE CELLULE D'OBJET : Maj + clic repose
// son lien, le clic droit ouvre LE menu des objets — mêmes entrées que celui
// de la cellule —, Ctrl + clic droit la description sans menu, et le clic
// simple ne change pas de sens. Éprouvé sur la
// boutique d'un PNJ, l'échange, la boutique de vote et la recherche
// d'échoppes, sur de vraies images ImGui, à la souris.

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/cash_shop_window.h"
#include "ro/ui/chat_link.h"
#include "ro/ui/deal_window.h"
#include "ro/ui/item_link_cell.h"
#include "ro/ui/npc_shop_window.h"
#include "ro/ui/store_search_window.h"
#include "ro/ui/ui_context.h"

using ro::ui::ChatLinkAsk;
using ro::ui::ChatLinkGesture;
using ro::ui::VoteShopOffer;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr int kSettleFrames = 4;
constexpr float kScanStepX = 8.0F;
constexpr float kScanStepY = 3.0F;
constexpr float kMenuStepPixels = 3.0F;
constexpr float kMenuInsetPixels = 30.0F;
constexpr std::uint32_t kPotionId = 501;
constexpr std::int32_t kPotionPrice = 10;
constexpr std::int32_t kPotionVotePrice = 5;
constexpr std::uint32_t kVotePoints = 100;
constexpr const char* kPotionName = "Red Potion";
// Le second résultat de la recherche : un autre objet, que le geste ne doit pas
// viser à la place du premier.
constexpr std::uint32_t kSwordId = 1101;
constexpr const char* kSwordName = "+7 Sword [Poring]";
constexpr std::int64_t kSwordPrice = 2'000;
constexpr std::uint32_t kStoreId = 7;
constexpr std::uint32_t kOwnerAid = 2'000'001;
constexpr const char* kCellMenuId = "##trade_item_link_test_cell_menu";

// LES ENTRÉES DU MENU D'UN OBJET, celles que la cellule offre.
constexpr std::array<std::string_view, 5> kItemEntries = {
    "Description", "Base de données du site", "Lien dans le chat", "@iteminfo", "@whodrops"};
constexpr std::string_view kVoteShopEntry = "Vote Shop : ajouter au panier";

// CE QU'UNE FENÊTRE A DIT pendant une image, dans un vocabulaire commun : le
// survol, puis le geste et l'item qu'il vise.
struct Seen {
  bool hovering = false;
  ChatLinkGesture gesture = ChatLinkGesture::kNothing;
  std::uint32_t aimed_id = 0;
};

// UNE FENÊTRE ET SA SOURIS. `draw` dessine dans une image ouverte et rend ce
// que la fenêtre en a dit ; ce que l'image a écrit se relit dans `logged`.
template <typename Draw>
struct Probe {
  ro::ui::UiContext ui;
  Draw draw;
  std::string logged;

  explicit Probe(Draw drawer) : draw(drawer) {}

  Seen Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::LogToBuffer();
    const Seen seen = draw();
    logged = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    ui.EndFrame();
    return seen;
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      static_cast<void>(Frame());
    }
  }

  // Balaie la fenêtre nommée jusqu'à survoler un item ; rend le point.
  std::optional<ImVec2> Find(const char* window_name) {
    const ImGuiWindow* window = ImGui::FindWindowByName(window_name);
    REQUIRE(window != nullptr);
    const ImVec2 least = window->Pos;
    const ImVec2 most(window->Pos.x + window->Size.x, window->Pos.y + window->Size.y);
    for (float y = least.y; y < most.y; y += kScanStepY) {
      for (float x = least.x; x < most.x; x += kScanStepX) {
        ImGui::GetIO().AddMousePosEvent(x, y);
        if (Frame().hovering) {
          return ImVec2(x, y);
        }
      }
    }
    return std::nullopt;
  }

  // Un clic complet ; rend le premier geste qu'il a fait naître.
  Seen Click(ImGuiMouseButton button) {
    ImGuiIO& io = ImGui::GetIO();
    Seen kept;
    for (int step = 0; step < 3; ++step) {
      if (step == 1) {
        io.AddMouseButtonEvent(button, true);
      } else if (step == 2) {
        io.AddMouseButtonEvent(button, false);
      }
      const Seen seen = Frame();
      if (kept.gesture == ChatLinkGesture::kNothing) {
        kept = seen;
      }
    }
    return kept;
  }

  Seen ShiftClick() {
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Shift, true);
    const Seen seen = Click(ImGuiMouseButton_Left);
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Shift, false);
    return seen;
  }

  Seen CtrlRightClick() {
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, true);
    const Seen seen = Click(ImGuiMouseButton_Right);
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, false);
    return seen;
  }

  // LE MENU OUVERT : survole son entrée, la clique, et rend le geste rendu.
  Seen Choose(std::string_view entry) {
    REQUIRE_FALSE(GImGui->OpenPopupStack.empty());
    ImGuiWindow* menu = GImGui->OpenPopupStack.back().Window;
    REQUIRE(menu != nullptr);
    const ImGuiID wanted = menu->GetID(entry.data(), entry.data() + entry.size());
    const float x = menu->Pos.x + kMenuInsetPixels;
    bool found = false;
    for (float y = menu->Pos.y; y < menu->Pos.y + menu->Size.y && !found; y += kMenuStepPixels) {
      ImGui::GetIO().AddMousePosEvent(x, y);
      static_cast<void>(Frame());
      static_cast<void>(Frame());
      found = GImGui->HoveredId == wanted;
    }
    REQUIRE(found);
    return Click(ImGuiMouseButton_Left);
  }

  [[nodiscard]] bool Shows(std::string_view text) const {
    return logged.find(text) != std::string::npos;
  }
};

// LE MENU DE LA CELLULE D'OBJET, ouvert sur le même item : la mesure de ce que
// le commerce doit montrer. Rend sa taille, que des entrées identiques sous
// une même tête reproduisent au pixel.
//
// ⚠ À MESURER AVANT D'OUVRIR LE COMMERCE : son contexte ImGui, détruit au
// retour, laisse le contexte courant vide.
ImVec2 CellMenuSize(ro::ui::ItemLinkFacts link_facts) {
  ro::ui::ItemLinkMenu menu;
  const ro::ui::ItemLinkCell cell{.item_id = kPotionId, .text = kPotionName};
  Probe probe([&] {
    ChatLinkAsk asked;
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::Begin("##trade_item_link_test_cell", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    static_cast<void>(ro::ui::DrawItemLinkCell(cell, menu, asked));
    ro::ui::DrawItemLinkMenu(kCellMenuId, menu, asked, link_facts);
    ImGui::End();
    return Seen{.hovering = asked.hovering, .gesture = asked.gesture, .aimed_id = asked.item_id};
  });
  probe.Settle();
  REQUIRE(probe.Find("##trade_item_link_test_cell").has_value());
  static_cast<void>(probe.Click(ImGuiMouseButton_Right));
  probe.Settle();
  REQUIRE(GImGui->OpenPopupStack.Size == 1);
  for (const std::string_view entry : kItemEntries) {
    CHECK(probe.Shows(entry));
  }
  return GImGui->OpenPopupStack.back().Window->Size;
}

// Le clic droit a ouvert un menu : ses entrées sont celles de la cellule, à la
// même taille.
template <typename Draw>
void CheckMenuLikeCell(Probe<Draw>& probe, bool vote_shop, ImVec2 cell_menu) {
  probe.Settle();
  REQUIRE(GImGui->OpenPopupStack.Size == 1);
  for (const std::string_view entry : kItemEntries) {
    CHECK(probe.Shows(entry));
  }
  CHECK(probe.Shows(kVoteShopEntry) == vote_shop);
  const ImVec2 size = GImGui->OpenPopupStack.back().Window->Size;
  CHECK(size.x == cell_menu.x);
  CHECK(size.y == cell_menu.y);
}

const std::array<VoteShopOffer, 1> kVoteShop = {
    VoteShopOffer{.item_id = kPotionId, .price = kPotionVotePrice}};
const ro::ui::ItemLinkFacts kVoteShopFacts{.vote_shop = kVoteShop};

// ── LA BOUTIQUE D'UN PNJ ────────────────────────────────────────────────────

struct NpcShopFixture {
  const ro::ui::SkinArt art;
  const std::array<ro::ui::NpcShopOfferView, 1> offers = {ro::ui::NpcShopOfferView{
      .item = {.item_id = kPotionId, .name = kPotionName}, .price = kPotionPrice, .most = 1}};
  ro::ui::NpcShopView view;
  ro::ui::NpcShopWindowState state;

  NpcShopFixture() {
    view.title = "Tool Dealer";
    view.unit = "z";
    view.offers = offers;
    view.offers_listed = true;
    ro::ui::OpenNpcShopOn(state, 1, true);
  }

  [[nodiscard]] auto Drawer() {
    return [this] {
      const ro::ui::NpcShopWindowAction action =
          ro::ui::DrawNpcShopWindow(state, view, art, kVoteShopFacts);
      return Seen{
          .hovering = action.hovered_id != 0,
          .gesture = action.describe_id != 0 ? action.item_gesture : ChatLinkGesture::kNothing,
          .aimed_id = action.describe_id};
    };
  }
};

// ── L'ÉCHANGE ───────────────────────────────────────────────────────────────

struct DealFixture {
  const ro::ui::SkinArt art;
  const std::array<ro::ui::DealLineView, 1> partner = {
      ro::ui::DealLineView{.item = {.item_id = kPotionId, .name = kPotionName}, .amount = 1}};
  ro::ui::DealView view;
  ro::ui::DealWindowState state;
  ro::ui::DealLineRef described;

  DealFixture() {
    view.title = "Échange avec Untel";
    view.partner_name = "Untel";
    view.opening_seq = 1;
    view.partner = partner;
    ro::ui::OpenDealOn(state, 1);
  }

  [[nodiscard]] auto Drawer() {
    return [this] {
      const ro::ui::DealWindowAction action =
          ro::ui::DrawDealWindow(state, view, art, kVoteShopFacts);
      if (action.described.valid()) {
        described = action.described;
      }
      const bool aimed = action.described.valid() && action.described.partner;
      return Seen{
          .hovering = action.hovered.valid(),
          .gesture = aimed ? action.item_gesture : ChatLinkGesture::kNothing,
          .aimed_id =
              aimed ? partner[static_cast<std::size_t>(action.described.index)].item.item_id : 0};
    };
  }
};

// ── LA BOUTIQUE DE VOTE ─────────────────────────────────────────────────────

struct CashShopFixture {
  const ro::ui::SkinArt art;
  const std::array<ro::ui::CashShopCard, 1> cards = {
      ro::ui::CashShopCard{.item_id = kPotionId, .price = kPotionPrice, .name = kPotionName}};
  ro::ui::CashShopView view{.title = "Vote Shop", .currency = "Vote", .vote_points = kVotePoints};
  ro::ui::CashShopWindowState state;

  CashShopFixture() {
    view.tabs[0] = cards;
    ro::ui::OpenCashShopOn(state, 1, 0, 0);
  }

  [[nodiscard]] auto Drawer() {
    return [this] {
      const ChatLinkAsk item = ro::ui::DrawCashShopWindow(state, view, art).item;
      return Seen{
          .hovering = item.hovered_id != 0, .gesture = item.gesture, .aimed_id = item.item_id};
    };
  }
};

// ── LA RECHERCHE D'ÉCHOPPES ─────────────────────────────────────────────────

struct StoreSearchFixture {
  const ro::ui::SkinArt art;
  const std::array<ro::ui::StoreSearchRowView, 2> rows = {
      ro::ui::StoreSearchRowView{.item = {.item_id = kPotionId, .name = kPotionName},
                                 .shop_name = "Potions",
                                 .amount = 1,
                                 .price_zeny = kPotionPrice,
                                 .store_id = kStoreId,
                                 .owner_aid = kOwnerAid},
      ro::ui::StoreSearchRowView{.item = {.item_id = kSwordId, .name = kSwordName},
                                 .shop_name = "Armes",
                                 .amount = 1,
                                 .price_zeny = kSwordPrice,
                                 .store_id = kStoreId + 1,
                                 .owner_aid = kOwnerAid + 1}};
  ro::ui::StoreSearchView view;
  ro::ui::StoreSearchWindowState state;
  int described_row = ro::ui::kStoreSearchNoRow;
  bool picked = false;

  StoreSearchFixture() {
    view.words.title = "Recherche";
    view.rows = rows;
    ro::ui::OpenStoreSearchOn(state, 1);
  }

  [[nodiscard]] auto Drawer() {
    return [this] {
      const ro::ui::StoreSearchAction action =
          ro::ui::DrawStoreSearchWindow(state, view, art, kVoteShopFacts);
      picked = picked || action.clicked.chosen() || state.pending.chosen();
      const bool aimed = action.describe_row != ro::ui::kStoreSearchNoRow;
      if (aimed) {
        described_row = action.describe_row;
      }
      return Seen{
          .hovering = action.hovered_row != ro::ui::kStoreSearchNoRow,
          .gesture = aimed ? action.item_gesture : ChatLinkGesture::kNothing,
          .aimed_id = aimed ? rows[static_cast<std::size_t>(action.describe_row)].item.item_id : 0};
    };
  }
};

// LES TROIS RÈGLES, pour une fenêtre déjà posée sous la souris.
template <typename Draw>
void CheckItemLink(Probe<Draw>& probe, const char* window_name, bool vote_shop, ImVec2 cell_menu) {
  probe.Settle();
  REQUIRE(probe.Find(window_name).has_value());

  const Seen plain = probe.Click(ImGuiMouseButton_Left);
  CHECK(plain.gesture == ChatLinkGesture::kNothing);

  const Seen shifted = probe.ShiftClick();
  CHECK(shifted.gesture == ChatLinkGesture::kRepost);
  CHECK(shifted.aimed_id == kPotionId);

  // Ctrl + clic droit : la description tout de suite, sans menu.
  const Seen described = probe.CtrlRightClick();
  CHECK(described.gesture == ChatLinkGesture::kDescribe);
  CHECK(described.aimed_id == kPotionId);
  probe.Settle();
  CHECK(GImGui->OpenPopupStack.empty());

  const Seen right = probe.Click(ImGuiMouseButton_Right);
  // Le clic droit n'est pas un geste à jouer : il ouvre le menu, rien d'autre.
  CHECK(right.gesture == ChatLinkGesture::kNothing);
  CheckMenuLikeCell(probe, vote_shop, cell_menu);

  const Seen chosen = probe.Choose("Description");
  CHECK(chosen.gesture == ChatLinkGesture::kDescribe);
  CHECK(chosen.aimed_id == kPotionId);
}

}  // namespace

TEST_CASE("Boutique d'un PNJ : Maj + clic, menu des objets, clic simple inchangé",
          "[ui][trade][item_link]") {
  const ImVec2 cell_menu = CellMenuSize(kVoteShopFacts);
  NpcShopFixture fixture;
  Probe probe(fixture.Drawer());
  CheckItemLink(probe, "###npc_shop", true, cell_menu);
  // Le clic simple sur l'item ne remplit pas le panier : ce sont les paliers.
  CHECK(fixture.state.cart.empty());
}

TEST_CASE("Échange : Maj + clic, menu des objets, clic simple inchangé", "[ui][trade][item_link]") {
  const ImVec2 cell_menu = CellMenuSize(kVoteShopFacts);
  DealFixture fixture;
  Probe probe(fixture.Drawer());
  CheckItemLink(probe, "###deal", true, cell_menu);
  CHECK(fixture.described.partner);
  CHECK(fixture.described.index == 0);
}

TEST_CASE("Boutique de vote : Maj + clic, menu des objets, clic simple inchangé",
          "[ui][trade][item_link][cash_shop]") {
  const ImVec2 cell_menu = CellMenuSize({});
  CashShopFixture fixture;
  Probe probe(fixture.Drawer());
  CheckItemLink(probe, "###cash_shop", false, cell_menu);
  // Aucun geste sur la carte n'est un achat.
  CHECK(fixture.state.cart.empty());
}

TEST_CASE("Recherche d'échoppes : Maj + clic, menu des objets, clic simple inchangé",
          "[ui][trade][item_link][store_search]") {
  const ImVec2 cell_menu = CellMenuSize(kVoteShopFacts);
  StoreSearchFixture fixture;
  Probe probe(fixture.Drawer());
  CheckItemLink(probe, "###store_search", true, cell_menu);
  // Le geste vise l'objet de la ligne survolée, la première, et non l'autre.
  CHECK(fixture.described_row == 0);
  // Aucun geste sur l'objet ne désigne l'échoppe : c'est le double-clic sur son
  // nom.
  CHECK_FALSE(fixture.picked);
}
