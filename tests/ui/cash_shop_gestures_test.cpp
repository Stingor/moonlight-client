// LES GESTES D'UNE CARTE DE LA BOUTIQUE DE VOTE, comparés à ceux d'une ligne
// de la boutique d'un PNJ : le survol nomme l'item pour l'aperçu, le clic droit
// ouvre le menu des objets sans rien décrire, Maj + clic repose son lien comme
// une cellule d'objet — sur de vraies images ImGui, à la souris.

#include <array>
#include <cstdint>
#include <optional>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/cash_shop_window.h"
#include "ro/ui/chat_link.h"
#include "ro/ui/npc_shop_window.h"
#include "ro/ui/ui_context.h"

using ro::ui::CashShopCard;
using ro::ui::CashShopView;
using ro::ui::CashShopWindowAction;
using ro::ui::CashShopWindowState;
using ro::ui::ChatLinkAsk;
using ro::ui::ChatLinkGesture;
using ro::ui::NpcShopOfferView;
using ro::ui::NpcShopView;
using ro::ui::NpcShopWindowAction;
using ro::ui::NpcShopWindowState;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr int kSettleFrames = 4;
constexpr std::uint32_t kPotionId = 501;
constexpr std::int32_t kPotionPrice = 10;
constexpr std::uint32_t kVotePoints = 100;

// La zone balayée : le haut à gauche des deux fenêtres, là où leur premier
// item se dessine à leur première ouverture.
constexpr float kScanLeft = 80.0F;
constexpr float kScanRight = 480.0F;
constexpr float kScanTop = 60.0F;
constexpr float kScanBottom = 480.0F;
constexpr float kScanStepX = 8.0F;
constexpr float kScanStepY = 3.0F;

// CE QU'UNE FENÊTRE A DIT D'UN ITEM pendant une image, dans un vocabulaire
// commun aux deux boutiques.
struct Seen {
  std::uint32_t hovered = 0;
  std::uint32_t described = 0;
  std::uint32_t reposted = 0;
};

[[nodiscard]] Seen SeenOf(const NpcShopWindowAction& action) {
  return Seen{.hovered = action.hovered_id, .described = action.describe_id};
}

[[nodiscard]] Seen SeenOf(const CashShopWindowAction& action) {
  const ChatLinkAsk& item = action.item;
  return Seen{
      .hovered = item.hovered_id,
      .described = item.gesture == ChatLinkGesture::kDescribe ? item.item_id : 0,
      .reposted = item.gesture == ChatLinkGesture::kRepost ? item.item_id : 0,
  };
}

// UNE FENÊTRE DE BOUTIQUE ET SA SOURIS. `Draw` dessine une image et rend ce
// que la fenêtre en a dit.
template <typename Draw>
struct ShopProbe {
  ro::ui::UiContext ui;
  Draw draw;

  explicit ShopProbe(Draw drawer) : draw(drawer) {}

  Seen Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const Seen seen = draw();
    ui.EndFrame();
    return seen;
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      static_cast<void>(Frame());
    }
  }

  // Balaie la zone jusqu'à survoler un item ; rend le point.
  std::optional<ImVec2> Find() {
    for (float y = kScanTop; y < kScanBottom; y += kScanStepY) {
      for (float x = kScanLeft; x < kScanRight; x += kScanStepX) {
        ImGui::GetIO().AddMousePosEvent(x, y);
        if (Frame().hovered != 0) {
          return ImVec2(x, y);
        }
      }
    }
    return std::nullopt;
  }

  // Un clic complet ; rend la première image où il a dit quelque chose.
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
      if (kept.described == 0 && kept.reposted == 0) {
        kept = seen;
      }
    }
    return kept;
  }
};

// Ce que la boutique de PNJ dit de sa ligne : survol, puis clic droit, et si
// ce dernier a ouvert un menu.
struct Gestures {
  Seen hovered;
  Seen right_clicked;
  bool menu_opened = false;
};

Gestures NpcShopGestures() {
  const ro::ui::SkinArt art;
  const std::array<NpcShopOfferView, 1> offers = {NpcShopOfferView{
      .item = {.item_id = kPotionId, .name = "Red Potion"}, .price = kPotionPrice, .most = 1}};
  NpcShopView view;
  view.title = "Tool Dealer";
  view.unit = "z";
  view.offers = offers;
  view.offers_listed = true;
  NpcShopWindowState state;
  ro::ui::OpenNpcShopOn(state, 1, true);
  ShopProbe probe([&] { return SeenOf(ro::ui::DrawNpcShopWindow(state, view, art)); });
  probe.Settle();
  REQUIRE(probe.Find().has_value());
  Gestures gestures;
  gestures.hovered = probe.Frame();
  gestures.right_clicked = probe.Click(ImGuiMouseButton_Right);
  gestures.menu_opened = !GImGui->OpenPopupStack.empty();
  return gestures;
}

struct CashShopFixture {
  const ro::ui::SkinArt art;
  const std::array<CashShopCard, 1> cards = {
      CashShopCard{.item_id = kPotionId, .price = kPotionPrice, .name = "Red Potion"}};
  CashShopView view{.title = "Vote Shop", .currency = "Vote", .vote_points = kVotePoints};
  CashShopWindowState state;

  CashShopFixture() {
    view.tabs[0] = cards;
    ro::ui::OpenCashShopOn(state, 1, 0, 0);
  }

  [[nodiscard]] auto Drawer() {
    return [this] { return SeenOf(ro::ui::DrawCashShopWindow(state, view, art)); };
  }
};

}  // namespace

TEST_CASE("Boutique de vote : survol et clic droit disent ce que dit la boutique d'un PNJ",
          "[ui][cash_shop]") {
  const Gestures npc = NpcShopGestures();
  REQUIRE(npc.hovered.hovered == kPotionId);
  REQUIRE(npc.right_clicked.described == 0);
  REQUIRE(npc.menu_opened);

  CashShopFixture fixture;
  ShopProbe probe(fixture.Drawer());
  probe.Settle();
  REQUIRE(probe.Find().has_value());
  const Seen hovered = probe.Frame();
  CHECK(hovered.hovered == npc.hovered.hovered);
  CHECK(hovered.described == 0);
  const Seen right_clicked = probe.Click(ImGuiMouseButton_Right);
  CHECK(right_clicked.described == npc.right_clicked.described);
  CHECK(right_clicked.reposted == 0);
  CHECK(!GImGui->OpenPopupStack.empty() == npc.menu_opened);
  // Le clic droit n'est pas un achat.
  CHECK(fixture.state.cart.empty());
}

TEST_CASE("Boutique de vote : Maj + clic repose le lien, comme une cellule d'objet",
          "[ui][cash_shop]") {
  CashShopFixture fixture;
  ShopProbe probe(fixture.Drawer());
  probe.Settle();
  REQUIRE(probe.Find().has_value());
  ImGui::GetIO().AddKeyEvent(ImGuiMod_Shift, true);
  const Seen shifted = probe.Click(ImGuiMouseButton_Left);
  ImGui::GetIO().AddKeyEvent(ImGuiMod_Shift, false);
  CHECK(shifted.reposted == kPotionId);
  CHECK(shifted.described == 0);
  CHECK(fixture.state.cart.empty());

  SECTION("sans Maj, le clic gauche sur la carte ne fait rien : il reste aux boutons") {
    const Seen plain = probe.Click(ImGuiMouseButton_Left);
    CHECK(plain.reposted == 0);
    CHECK(plain.described == 0);
    CHECK(fixture.state.cart.empty());
  }
}
