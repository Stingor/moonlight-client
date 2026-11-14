// LA FENÊTRE DU TROC : ce qu'un clic demande au panier, un bouton que le verdict
// retient, et l'échange qui ne part qu'après une confirmation récapitulant ce
// qui part et ce qui arrive.
//
// Les gestes s'y jouent à la souris, sur la fenêtre dessinée, pas en appelant ses
// entrailles.

#include "ro/ui/barter_window.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/escape_stack.h"
#include "ro/ui/ui_context.h"

using ro::ui::BarterCartEditKind;
using ro::ui::BarterCartLineView;
using ro::ui::BarterLine;
using ro::ui::BarterOfferView;
using ro::ui::BarterPriceView;
using ro::ui::BarterRecap;
using ro::ui::BarterView;
using ro::ui::BarterWindowAction;
using ro::ui::BarterWindowState;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

// La boutique d'essai : une potion contre dix gelées, une épée contre deux
// dagues +4 et du zeny.
constexpr std::uint32_t kPotionIndex = 1;
constexpr std::uint32_t kSwordIndex = 2;
constexpr std::uint32_t kPotionId = 501;
constexpr std::uint32_t kSwordId = 1101;
constexpr std::uint32_t kJellopyId = 909;
constexpr std::uint32_t kKnifeId = 1201;
constexpr std::uint16_t kKnifeRefine = 4;
constexpr std::int64_t kSwordZeny = 1000;
constexpr int kPotionMost = 3;
constexpr std::uint32_t kCartSeq = 7;

// Le pas des balayages de la souris, en pixels.
constexpr float kSweepStepPixels = 3.0F;
// Les bords d'une fenêtre se saisissent pour la redimensionner : on les évite.
constexpr float kEdgeMarginPixels = 12.0F;

constexpr const char* kWindowName = ro::ui::kBarterWindowId;
constexpr const char* kConfirmName = "##barter_confirm";
constexpr const char* kRefusal = "Pas assez de Knife.";

struct Probe {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  BarterWindowState state;
  std::array<BarterPriceView, 1> potion_prices{
      BarterPriceView{.item_id = kJellopyId,
                      .amount = 10,
                      .held = 35,
                      .item = {.item_id = kJellopyId, .name = "Jellopy"}}};
  std::array<BarterPriceView, 2> sword_prices{
      BarterPriceView{.item_id = kKnifeId,
                      .refine_level = kKnifeRefine,
                      .amount = 2,
                      .held = 1,
                      .item = {.item_id = kKnifeId, .name = "+4 Knife"}},
      BarterPriceView{.item_id = kJellopyId,
                      .amount = 5,
                      .held = 35,
                      .item = {.item_id = kJellopyId, .name = "Jellopy"}}};
  std::array<BarterOfferView, 2> offers{
      BarterOfferView{.item = {.item_id = kPotionId, .name = "Red Potion"},
                      .shop_index = kPotionIndex,
                      .weight_tenths = 70,
                      .prices = potion_prices,
                      .most = kPotionMost},
      BarterOfferView{.item = {.item_id = kSwordId, .name = "Sword"},
                      .shop_index = kSwordIndex,
                      .stock = 4,
                      .weight_tenths = 500,
                      .zeny = kSwordZeny,
                      .prices = sword_prices,
                      .most = 0,
                      .refusal = kRefusal}};
  std::vector<BarterCartLineView> cart;
  std::vector<std::string_view> blockers;
  BarterView view;

  Probe() {
    ro::ui::OpenBarterOn(state, 1);
    view.expanded = true;
    view.offers = offers;
    view.cart_seq = kCartSeq;
  }

  BarterWindowAction Frame() {
    view.cart = cart;
    view.blockers = blockers;
    ui.BeginFrame(kViewport, kFrameSeconds);
    BarterWindowAction action = ro::ui::DrawBarterWindow(state, view, art);
    ui.EndFrame();
    return action;
  }

  void Settle() {
    for (int frame = 0; frame < 4; ++frame) {
      static_cast<void>(Frame());
    }
  }

  // Un clic à l'endroit où la souris se trouve : ce que rendent l'enfoncement
  // et le relâchement, réunis.
  BarterWindowAction Click() {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    BarterWindowAction pressed = Frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    BarterWindowAction released = Frame();
    if (released.edit.kind == BarterCartEditKind::kNone) {
      released.edit = pressed.edit;
    }
    if (released.exchange.empty()) {
      released.exchange = std::move(pressed.exchange);
      released.immediate = pressed.immediate;
      released.cart_seq = pressed.cart_seq;
    }
    return released;
  }

  void MoveTo(ImVec2 at) {
    ImGui::GetIO().AddMousePosEvent(at.x, at.y);
    static_cast<void>(Frame());
  }
};

[[nodiscard]] ImGuiWindow* WindowNamed(std::string_view part, std::string_view but_not = {}) {
  for (ImGuiWindow* window : GImGui->Windows) {
    const std::string_view name(window->Name);
    if (window->WasActive && name.find(part) != std::string_view::npos &&
        (but_not.empty() || name.find(but_not) == std::string_view::npos)) {
      return window;
    }
  }
  return nullptr;
}

// Balaie la fenêtre jusqu'à survoler l'item voulu, et rend la hauteur de sa
// ligne.
[[nodiscard]] float RowOf(Probe& probe, std::uint32_t item_id) {
  const ImGuiWindow* window = ImGui::FindWindowByName(kWindowName);
  REQUIRE(window != nullptr);
  ImGuiIO& io = ImGui::GetIO();
  constexpr float kNameColumnPixels = 160.0F;
  constexpr float kNameStepPixels = 20.0F;
  for (float y = window->Pos.y; y < window->Pos.y + window->Size.y; y += kSweepStepPixels) {
    for (float x = window->Pos.x + kEdgeMarginPixels; x < window->Pos.x + kNameColumnPixels;
         x += kNameStepPixels) {
      io.AddMousePosEvent(x, y);
      if (probe.Frame().hovered_id == item_id) {
        return y;
      }
    }
  }
  return -1.0F;
}

// Le premier élément interactif rencontré de gauche à droite sur une hauteur.
[[nodiscard]] ImVec2 FirstControlOnRow(Probe& probe, float y) {
  const ImGuiWindow* window = ImGui::FindWindowByName(kWindowName);
  REQUIRE(window != nullptr);
  for (float x = window->Pos.x + kEdgeMarginPixels;
       x < window->Pos.x + window->Size.x - kEdgeMarginPixels; x += kSweepStepPixels) {
    probe.MoveTo(ImVec2(x, y));
    if (GImGui->HoveredId != 0) {
      return ImVec2(x, y);
    }
  }
  return ImVec2(-1.0F, -1.0F);
}

// Le plus bas des éléments interactifs d'une fenêtre, en son milieu.
[[nodiscard]] ImVec2 LowestControl(Probe& probe, const ImGuiWindow& window) {
  const float x = window.Pos.x + window.Size.x * 0.5F;
  for (float y = window.Pos.y + window.Size.y - 2.0F; y > window.Pos.y; y -= kSweepStepPixels) {
    probe.MoveTo(ImVec2(x, y));
    if (GImGui->HoveredId != 0) {
      return ImVec2(x, y);
    }
  }
  return ImVec2(-1.0F, -1.0F);
}

// Le bouton « oui » de la boîte : le premier rencontré de gauche à droite, sur
// la plus basse rangée qui en porte.
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

[[nodiscard]] bool ConfirmShown() {
  const ImGuiWindow* box = ImGui::FindWindowByName(kConfirmName);
  return box != nullptr && box->WasActive;
}

}  // namespace

// ── CE QUI SE CALCULE SANS DESSINER ─────────────────────────────────────────

TEST_CASE("Troc : le récapitulatif cumule chaque devise sur tout le panier", "[ui][barter]") {
  Probe probe;
  const std::array<BarterLine, 3> lines = {BarterLine{.shop_index = kPotionIndex, .amount = 2},
                                           BarterLine{.shop_index = kSwordIndex, .amount = 1},
                                           BarterLine{.shop_index = 99, .amount = 5}};
  const BarterRecap recap = ro::ui::RecapBarterLines(probe.offers, lines);

  // La ligne dont l'article n'existe plus ne compte pas.
  REQUIRE(recap.arrivals.size() == 2);
  // Les gelées des deux lignes s'additionnent ; les dagues +4 sont à part.
  REQUIRE(recap.costs.size() == 2);
  CHECK(recap.costs[0].price.item_id == kJellopyId);
  CHECK(recap.costs[0].total == 2 * 10 + 5);
  CHECK(recap.costs[0].enough());
  CHECK(recap.costs[1].price.item_id == kKnifeId);
  CHECK(recap.costs[1].total == 2);
  CHECK_FALSE(recap.costs[1].enough());
  CHECK(recap.zeny == kSwordZeny);
  // 🔴 UNE DEVISE RAFFINÉE PART : la confirmation doit le dire.
  CHECK(recap.refined_leaves);
}

TEST_CASE("Troc : la confirmation dit ce qui part, ce qui arrive, et le raffiné", "[ui][barter]") {
  Probe probe;
  const std::array<BarterLine, 1> sword = {BarterLine{.shop_index = kSwordIndex, .amount = 1}};
  const std::string refined =
      ro::ui::BarterConfirmationText(probe.offers, ro::ui::RecapBarterLines(probe.offers, sword));
  CHECK(refined.find("Part :") != std::string::npos);
  CHECK(refined.find("x2 +4 Knife") != std::string::npos);
  CHECK(refined.find("x5 Jellopy") != std::string::npos);
  CHECK(refined.find("1,000z") != std::string::npos);
  CHECK(refined.find("Arrive :") != std::string::npos);
  CHECK(refined.find("x1 Sword") != std::string::npos);
  CHECK(refined.find("(!)") != std::string::npos);

  // Des gelées seules : aucune mise en garde, aucun zeny.
  const std::array<BarterLine, 1> potion = {BarterLine{.shop_index = kPotionIndex, .amount = 3}};
  const std::string plain =
      ro::ui::BarterConfirmationText(probe.offers, ro::ui::RecapBarterLines(probe.offers, potion));
  CHECK(plain.find("x30 Jellopy") != std::string::npos);
  CHECK(plain.find("x3 Red Potion") != std::string::npos);
  CHECK(plain.find("(!)") == std::string::npos);
  CHECK(plain.find('z') == std::string::npos);
}

TEST_CASE("Troc : une devise tenue plus raffinée que l'exigence se signale", "[ui][barter]") {
  Probe probe;
  probe.potion_prices[0].may_take_higher_refine = true;
  const std::array<BarterLine, 1> potion = {BarterLine{.shop_index = kPotionIndex, .amount = 1}};
  CHECK(ro::ui::RecapBarterLines(probe.offers, potion).refined_leaves);
}

// ── LA FENÊTRE DESSINÉE ─────────────────────────────────────────────────────

TEST_CASE("Troc : fermée, la fenêtre ne dessine rien", "[ui][barter]") {
  Probe probe;
  probe.state.open = false;
  const BarterWindowAction action = probe.Frame();
  CHECK_FALSE(action.close);
  CHECK(action.edit.kind == BarterCartEditKind::kNone);
  CHECK(ImGui::FindWindowByName(kWindowName) == nullptr);
}

TEST_CASE("Troc : un palier demande l'ajout au panier, et le panier paraît", "[ui][barter]") {
  Probe probe;
  probe.Settle();
  const float row = RowOf(probe, kPotionId);
  REQUIRE(row >= 0.0F);
  const ImVec2 step = FirstControlOnRow(probe, row);
  REQUIRE(step.x >= 0.0F);
  CHECK_FALSE(GImGui->HoveredIdIsDisabled);

  const BarterWindowAction clicked = probe.Click();
  REQUIRE(clicked.edit.kind == BarterCartEditKind::kAdd);
  CHECK(clicked.edit.line.shop_index == kPotionIndex);
  CHECK(clicked.edit.line.amount == 1);
  // ⚠ LA FENÊTRE NE REMPLIT RIEN ELLE-MÊME : c'est le troc qui tient le panier.
  CHECK(WindowNamed("##barter_cart") == nullptr);
  CHECK(clicked.exchange.empty());

  // `app` a accepté : le panier est dans la vue, et son volet paraît.
  probe.cart = {BarterCartLineView{.shop_index = kPotionIndex, .amount = 1, .most = kPotionMost}};
  probe.Settle();
  CHECK(WindowNamed("##barter_cart", "lines") != nullptr);
}

TEST_CASE("Troc : l'ajout que le verdict refuse reste gris", "[ui][barter]") {
  Probe probe;
  probe.Settle();
  const float potion = RowOf(probe, kPotionId);
  const float sword = RowOf(probe, kSwordId);
  REQUIRE(potion >= 0.0F);
  REQUIRE(sword >= 0.0F);
  const ImVec2 step = FirstControlOnRow(probe, potion);
  REQUIRE(step.x >= 0.0F);

  // Le « +1 » de l'épée, à la même place que celui de la potion : retenu.
  probe.MoveTo(ImVec2(step.x, sword));
  REQUIRE(GImGui->HoveredId != 0);
  CHECK(GImGui->HoveredIdIsDisabled);
  const BarterWindowAction clicked = probe.Click();
  CHECK(clicked.edit.kind == BarterCartEditKind::kNone);
  CHECK(probe.state.pending.empty());
}

TEST_CASE("Troc : le bouton d'échange se retient tant que le panier serait refusé",
          "[ui][barter]") {
  Probe probe;
  probe.cart = {BarterCartLineView{.shop_index = kSwordIndex, .amount = 1, .most = 1}};
  probe.blockers = {kRefusal};
  probe.Settle();
  const ImGuiWindow* cart = WindowNamed("##barter_cart", "lines");
  REQUIRE(cart != nullptr);
  const ImVec2 button = LowestControl(probe, *cart);
  REQUIRE(button.x >= 0.0F);
  CHECK(GImGui->HoveredIdIsDisabled);

  static_cast<void>(probe.Click());
  static_cast<void>(probe.Frame());
  CHECK(probe.state.pending.empty());
  CHECK_FALSE(ConfirmShown());
}

TEST_CASE("Troc : l'échange du panier attend sa confirmation", "[ui][barter]") {
  Probe probe;
  probe.cart = {BarterCartLineView{.shop_index = kPotionIndex, .amount = 2, .most = kPotionMost}};
  probe.Settle();
  const ImGuiWindow* cart = WindowNamed("##barter_cart", "lines");
  REQUIRE(cart != nullptr);
  const ImVec2 button = LowestControl(probe, *cart);
  REQUIRE(button.x >= 0.0F);
  CHECK_FALSE(GImGui->HoveredIdIsDisabled);

  // 🔴 LE CLIC NE FAIT RIEN PARTIR : il pose la question.
  const BarterWindowAction clicked = probe.Click();
  CHECK(clicked.exchange.empty());
  REQUIRE(probe.state.pending.size() == 1);
  CHECK(probe.state.pending[0].amount == 2);
  CHECK_FALSE(probe.state.pending_immediate);
  static_cast<void>(probe.Frame());
  REQUIRE(ConfirmShown());

  SECTION("oui : le panier part, au rang que le joueur a vu") {
    const ImVec2 yes = YesButton(probe);
    REQUIRE(yes.x >= 0.0F);
    const BarterWindowAction confirmed = probe.Click();
    REQUIRE(confirmed.exchange.size() == 1);
    CHECK(confirmed.exchange[0].shop_index == kPotionIndex);
    CHECK(confirmed.exchange[0].amount == 2);
    CHECK_FALSE(confirmed.immediate);
    CHECK(confirmed.cart_seq == kCartSeq);
    CHECK(probe.state.pending.empty());
  }

  SECTION("Échap : rien ne part, et la question ne se repose pas") {
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
    const BarterWindowAction refused = probe.Frame();
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
    CHECK(refused.exchange.empty());
    CHECK(probe.state.pending.empty());
    static_cast<void>(probe.Frame());
    static_cast<void>(probe.Frame());
    CHECK_FALSE(ConfirmShown());
  }
}

TEST_CASE("Troc : Ctrl + clic échange la ligne seule, et se confirme aussi", "[ui][barter]") {
  Probe probe;
  probe.Settle();
  const float row = RowOf(probe, kPotionId);
  REQUIRE(row >= 0.0F);
  REQUIRE(FirstControlOnRow(probe, row).x >= 0.0F);

  ImGuiIO& io = ImGui::GetIO();
  io.AddKeyEvent(ImGuiMod_Ctrl, true);
  const BarterWindowAction clicked = probe.Click();
  io.AddKeyEvent(ImGuiMod_Ctrl, false);
  CHECK(clicked.edit.kind == BarterCartEditKind::kNone);
  CHECK(clicked.exchange.empty());
  REQUIRE(probe.state.pending.size() == 1);
  CHECK(probe.state.pending[0].shop_index == kPotionIndex);
  CHECK(probe.state.pending_immediate);
  static_cast<void>(probe.Frame());
  REQUIRE(ConfirmShown());

  REQUIRE(YesButton(probe).x >= 0.0F);
  const BarterWindowAction confirmed = probe.Click();
  REQUIRE(confirmed.exchange.size() == 1);
  CHECK(confirmed.immediate);
}

TEST_CASE("Troc : Échap la retrouve et la baisse", "[ui][barter]") {
  // ⚠ LA PILE CHERCHE LA FENÊTRE PAR L'IDENTITÉ QUE `app` LUI DÉCLARE : elle
  // doit être celle que la fenêtre porte, sans quoi Échap passerait à côté du
  // troc — et le serveur n'apprendrait jamais la fermeture.
  Probe probe;
  probe.Settle();
  ro::ui::EscapeStack escape;
  ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
  probe.ui.BeginFrame(kViewport, kFrameSeconds);
  static_cast<void>(ro::ui::DrawBarterWindow(probe.state, probe.view, probe.art));
  escape.Register(ro::ui::kBarterWindowId, &probe.state.open);
  escape.Process();
  probe.ui.EndFrame();
  ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
  CHECK_FALSE(probe.state.open);
}

TEST_CASE("Troc : baissée, la fenêtre oublie sa question et n'échange rien", "[ui][barter]") {
  Probe probe;
  probe.Settle();
  probe.state.pending = {BarterLine{.shop_index = kPotionIndex, .amount = 1}};
  probe.state.open = false;
  const BarterWindowAction action = probe.Frame();
  CHECK(action.exchange.empty());
  CHECK(probe.state.pending.empty());
}

TEST_CASE("Troc : la colonne de coût tient la ligne la plus longue", "[ui][barter]") {
  Probe probe;
  probe.Settle();

  // Un nom de devise comme ceux du jeu, et ce que le joueur en tient.
  constexpr std::string_view kLongName = "Decayed Deep Sea Fish of the Illusion Dungeon";
  constexpr std::int64_t kAmount = 150;
  constexpr std::int64_t kHeld = 12345;
  const std::array<BarterPriceView, 1> long_prices{
      BarterPriceView{.item_id = kJellopyId,
                      .amount = kAmount,
                      .held = kHeld,
                      .may_take_higher_refine = true,
                      .item = {.item_id = kJellopyId, .name = kLongName}}};
  const std::array<BarterOfferView, 1> offers{
      BarterOfferView{.item = {.item_id = kPotionId, .name = "Red Potion"}, .prices = long_prices}};

  probe.ui.BeginFrame(kViewport, kFrameSeconds);
  const float column = ro::ui::BarterCostColumnWidth(offers);
  const float parts = ImGui::CalcTextSize(std::string(kLongName).c_str()).x +
                      ImGui::CalcTextSize("x150").x + ImGui::CalcTextSize("possédé 12,345").x +
                      ImGui::CalcTextSize("(!)").x;
  const float short_line = ro::ui::BarterCostColumnWidth(probe.offers);
  probe.ui.EndFrame();

  // 🔴 LE TEXTE ENTIER TIENT : c'est « possédé » qui se coupait sur une largeur fixe.
  CHECK(column > parts);
  CHECK(column > short_line);
}
