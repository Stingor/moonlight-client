// LA FENÊTRE D'ÉCHANGE ENTRE JOUEURS : le récapitulatif qui précède la
// validation, le verrou qui emporte le zeny tapé, la boîte qui se ferme sur une
// offre changée, et la proposition qu'on accepte ou refuse.
//
// Les gestes se jouent à la souris, sur la fenêtre dessinée.

#include "ro/ui/deal_window.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/shortcut_bar.h"
#include "ro/ui/ui_context.h"

using ro::ui::DealBagView;
using ro::ui::DealLineView;
using ro::ui::DealProposalAction;
using ro::ui::DealProposalState;
using ro::ui::DealProposalView;
using ro::ui::DealView;
using ro::ui::DealWindowAction;
using ro::ui::DealWindowState;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

constexpr std::uint32_t kOpening = 3;
constexpr std::uint32_t kRevision = 5;
constexpr std::uint16_t kPotionSlot = 2;
constexpr std::uint16_t kSwordSlot = 7;
constexpr std::uint16_t kArmorSlot = 9;
constexpr int kPotionStack = 12;
constexpr std::int64_t kMillion = 1'000'000;
constexpr const char* kWorn = "Un item porté ne s'échange pas.";

// Le pas des balayages de la souris, en pixels.
constexpr float kSweepStepPixels = 3.0F;
constexpr float kSweepColumnPixels = 20.0F;
// Les bords d'une fenêtre se saisissent pour la redimensionner : on les évite.
constexpr float kEdgeMarginPixels = 12.0F;
// Le seuil de glisser d'ImGui est de quelques pixels : on le franchit large.
constexpr float kDragPixels = 30.0F;

// La source d'un glisser, posée dans une fenêtre d'essai à part.
constexpr const char* kSourceWindow = "##deal_test_source";
constexpr ImVec2 kSourceAt(900.0F, 600.0F);

struct Probe {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  DealWindowState state;
  std::vector<DealLineView> mine;
  std::vector<DealLineView> partner;
  std::vector<DealBagView> bag{DealBagView{.client_slot = kPotionSlot,
                                           .item = {.item_id = 501, .name = "Red Potion"},
                                           .amount = kPotionStack,
                                           .most = kPotionStack},
                               DealBagView{.client_slot = kSwordSlot,
                                           .item = {.item_id = 1101, .name = "Sword"},
                                           .amount = 1,
                                           .most = 1},
                               DealBagView{.client_slot = kArmorSlot,
                                           .item = {.item_id = 2301, .name = "Cotton Shirt"},
                                           .amount = 1,
                                           .most = 0,
                                           .refusal = kWorn}};
  std::vector<std::string_view> lock_blockers;
  std::vector<std::string_view> execute_blockers;
  DealView view;
  // Ce que la source d'essai porte quand on la tire.
  std::optional<ro::ui::ItemDrag> carried;

  Probe() {
    view.partner_name = "Untel";
    view.opening_seq = kOpening;
    view.partner_revision = kRevision;
    ro::ui::OpenDealOn(state, kOpening);
  }

  DealWindowAction Frame() {
    view.mine = mine;
    view.partner = partner;
    view.bag = bag;
    view.lock_blockers = lock_blockers;
    view.execute_blockers = execute_blockers;
    ui.BeginFrame(kViewport, kFrameSeconds);
    DealWindowAction action = ro::ui::DrawDealWindow(state, view, art);
    if (carried.has_value()) {
      ImGui::SetNextWindowPos(kSourceAt);
      ImGui::Begin(kSourceWindow, nullptr, ImGuiWindowFlags_AlwaysAutoResize);
      ImGui::Button("source");
      if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload(ro::ui::kItemDragPayload, &*carried, sizeof(*carried));
        ImGui::EndDragDropSource();
      }
      ImGui::End();
    }
    ui.EndFrame();
    return action;
  }

  void Settle() {
    for (int frame = 0; frame < 4; ++frame) {
      static_cast<void>(Frame());
    }
  }

  // Un clic à l'endroit où la souris se trouve : l'enfoncement et le
  // relâchement, réunis.
  DealWindowAction Click() {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    DealWindowAction pressed = Frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    DealWindowAction released = Frame();
    released.lock |= pressed.lock;
    released.execute |= pressed.execute;
    released.cancel |= pressed.cancel;
    if (!released.zeny.has_value()) {
      released.zeny = pressed.zeny;
    }
    if (!released.offer.has_value()) {
      released.offer = pressed.offer;
    }
    if (pressed.execute) {
      released.execute_revision = pressed.execute_revision;
    }
    return released;
  }

  void MoveTo(ImVec2 at) {
    ImGui::GetIO().AddMousePosEvent(at.x, at.y);
    static_cast<void>(Frame());
  }

  // Tire la source d'essai jusqu'au point voulu et l'y lâche.
  DealWindowAction DragTo(ImVec2 target) {
    const ImGuiWindow* source = WindowNamed(kSourceWindow);
    REQUIRE(source != nullptr);
    const ImVec2 grip(source->Pos.x + source->Size.x * 0.5F, source->Pos.y + source->Size.y * 0.5F);
    MoveTo(grip);
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    static_cast<void>(Frame());
    MoveTo(ImVec2(grip.x - kDragPixels, grip.y - kDragPixels));
    MoveTo(target);
    MoveTo(target);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    DealWindowAction dropped = Frame();
    static_cast<void>(Frame());
    return dropped;
  }

  static ImGuiWindow* WindowNamed(std::string_view part) {
    for (ImGuiWindow* window : GImGui->Windows) {
      if (window->WasActive &&
          std::string_view(window->Name).find(part) != std::string_view::npos) {
        return window;
      }
    }
    return nullptr;
  }
};

// Balaie une fenêtre jusqu'à survoler le bouton de ce libellé, posé à son
// niveau ; rend sa position, ou (-1, -1).
[[nodiscard]] ImVec2 FindButton(Probe& probe, std::string_view window_part, const char* label) {
  ImGuiWindow* window = Probe::WindowNamed(window_part);
  REQUIRE(window != nullptr);
  const ImGuiID wanted = window->GetID(label);
  for (float y = window->Pos.y + window->Size.y - kEdgeMarginPixels; y > window->Pos.y;
       y -= kSweepStepPixels) {
    for (float x = window->Pos.x + kEdgeMarginPixels;
         x < window->Pos.x + window->Size.x - kEdgeMarginPixels; x += kSweepColumnPixels) {
      probe.MoveTo(ImVec2(x, y));
      if (GImGui->HoveredId == wanted) {
        return ImVec2(x, y);
      }
    }
  }
  return ImVec2(-1.0F, -1.0F);
}

[[nodiscard]] bool ConfirmShown() {
  const ImGuiWindow* box = Probe::WindowNamed("##deal_confirm");
  return box != nullptr;
}

// Le bouton « oui » de la boîte : le premier rencontré de gauche à droite, sur
// la plus basse rangée qui en porte.
[[nodiscard]] ImVec2 YesButton(Probe& probe) {
  const ImGuiWindow* box = Probe::WindowNamed("##deal_confirm");
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

[[nodiscard]] bool Has(const std::string& text, std::string_view part) {
  return text.find(part) != std::string::npos;
}

}  // namespace

// ── CE QUI SE CALCULE SANS DESSINER ─────────────────────────────────────────

TEST_CASE("Échange : le récapitulatif dit ce qui part et ce qui arrive", "[ui][deal]") {
  Probe probe;
  const std::array<DealLineView, 2> mine = {
      DealLineView{.item = {.name = "Red Potion"}, .amount = 3},
      DealLineView{.item = {.name = "Sword"}, .amount = 1}};
  probe.view.mine = mine;
  probe.view.my_zeny = kMillion;
  const std::string text = ro::ui::DealConfirmationText(probe.view);
  CHECK(Has(text, "Vous donnez :"));
  CHECK(Has(text, "Red Potion x 3"));
  CHECK(Has(text, "Sword x 1"));
  CHECK(Has(text, "1,000,000z (1 million)"));
  CHECK(Has(text, "Vous recevez :"));
  // Rien en face : le récapitulatif le dit plutôt que de laisser un vide.
  CHECK(Has(text.substr(text.find("Vous recevez :")), "rien"));
  CHECK_FALSE(Has(text, "(!)"));
  CHECK(text.ends_with("Échanger ?"));
}

TEST_CASE("Échange : le récapitulatif met en garde contre une offre changée", "[ui][deal]") {
  Probe probe;
  const std::array<DealLineView, 1> partner = {
      DealLineView{.item = {.name = "Jellopy"}, .amount = 40, .changed_after_my_lock = true}};
  probe.view.partner = partner;
  probe.view.partner_zeny = 2 * kMillion + 5;
  probe.view.partner_changed_after_my_lock = true;
  const std::string text = ro::ui::DealConfirmationText(probe.view);
  CHECK(Has(text, "Jellopy x 40"));
  CHECK(Has(text, "2,000,005z (2 millions 5)"));
  CHECK(Has(text, "(!) Untel a changé son offre"));
  // Mon côté est vide : « rien », avant « Vous recevez ».
  CHECK(text.find("rien") < text.find("Vous recevez :"));
}

TEST_CASE("Échange : une ouverture neuve remet la fenêtre à zéro", "[ui][deal]") {
  DealWindowState state;
  state.zeny_text = "500";
  state.asking_amount = true;
  state.asking_value = 4;
  state.picking = true;
  static_cast<void>(std::string_view("pot").copy(state.filter.InputBuf, 3));
  state.filter.Build();
  state.flash = "refus";
  state.confirming = true;
  state.confirming_revision = 9;
  ro::ui::OpenDealOn(state, 4);
  CHECK(state.open);
  CHECK(state.opening_seen == 4);
  CHECK(state.zeny_text.empty());
  CHECK_FALSE(state.asking_amount);
  CHECK_FALSE(state.picking);
  CHECK_FALSE(state.filter.IsActive());
  CHECK(state.flash.empty());
  CHECK_FALSE(state.confirming);
}

TEST_CASE("Échange : le montant tapé se juge avant le clic", "[ui][deal]") {
  CHECK(ro::ui::ReadDealZeny("", 0).blank());
  CHECK(ro::ui::ReadDealZeny("   ", 0).blank());
  CHECK(ro::ui::ReadDealZeny("1,500,000", ro::ui::kTradeNoPurse).amount == 1'500'000);
  CHECK(ro::ui::ReadDealZeny(" 1 500 000 ", 2 * kMillion).amount == 1'500'000);
  // Illisible, négatif, au-delà de la bourse : refusé, et la raison se dit.
  for (const char* typed : {"1,5", "-10", "abc"}) {
    const ro::ui::DealZenyEntry entry = ro::ui::ReadDealZeny(typed, ro::ui::kTradeNoPurse);
    CHECK_FALSE(entry.amount.has_value());
    CHECK_FALSE(entry.refusal.empty());
  }
  const ro::ui::DealZenyEntry too_much = ro::ui::ReadDealZeny("2,000,001", 2 * kMillion);
  CHECK_FALSE(too_much.amount.has_value());
  CHECK_FALSE(too_much.refusal.empty());
}

TEST_CASE("Échange : une case refusée se dit, une seule s'offre, une pile se demande",
          "[ui][deal]") {
  Probe probe;
  DealWindowAction action;
  ro::ui::ChooseDealBagSlot(probe.state, probe.bag[2], action);
  CHECK(probe.state.flash == kWorn);
  CHECK_FALSE(action.offer.has_value());

  ro::ui::ChooseDealBagSlot(probe.state, probe.bag[1], action);
  REQUIRE(action.offer.has_value());
  CHECK(action.offer->client_slot == kSwordSlot);
  CHECK(action.offer->amount == 1);
  CHECK(probe.state.flash.empty());

  action = {};
  ro::ui::ChooseDealBagSlot(probe.state, probe.bag[0], action);
  CHECK_FALSE(action.offer.has_value());
  CHECK(probe.state.asking_amount);
  CHECK(probe.state.asking_slot == kPotionSlot);
  CHECK(probe.state.asking_value == kPotionStack);
}

TEST_CASE("Échange : la ligne d'état dit toujours la prochaine étape", "[ui][deal]") {
  DealView view;
  view.partner_name = "Untel";
  CHECK(Has(ro::ui::DealStatusText(view), "Verrouillez votre offre"));
  view.lock_sent = true;
  CHECK(Has(ro::ui::DealStatusText(view), "En attente du verrou de Untel"));
  view.my_locked = true;
  view.partner_locked = true;
  CHECK(Has(ro::ui::DealStatusText(view), "cliquez « Échanger »"));
  view.execute_sent = true;
  CHECK(Has(ro::ui::DealStatusText(view), "en attente de Untel"));
}

// ── LA FENÊTRE DESSINÉE ─────────────────────────────────────────────────────

TEST_CASE("Échange : fermée, la fenêtre ne dessine rien et n'annule rien", "[ui][deal]") {
  Probe probe;
  probe.state.open = false;
  const DealWindowAction action = probe.Frame();
  CHECK_FALSE(action.cancel);
  CHECK(Probe::WindowNamed(ro::ui::kDealWindowId) == nullptr);
}

TEST_CASE("Échange : une ouverture neuve vue par la fenêtre la remet à zéro", "[ui][deal]") {
  Probe probe;
  probe.Settle();
  probe.state.zeny_text = "123";
  probe.view.opening_seq = kOpening + 1;
  static_cast<void>(probe.Frame());
  CHECK(probe.state.opening_seen == kOpening + 1);
  CHECK(probe.state.zeny_text.empty());
}

TEST_CASE("Échange : « Annuler » ferme, et c'est une annulation", "[ui][deal]") {
  Probe probe;
  probe.Settle();
  REQUIRE(FindButton(probe, ro::ui::kDealWindowId, "Annuler").x >= 0.0F);
  const DealWindowAction action = probe.Click();
  CHECK(action.cancel);
  CHECK_FALSE(probe.state.open);
}

TEST_CASE("Échange : le verrou emporte le zeny tapé, qui part d'abord", "[ui][deal]") {
  Probe probe;
  probe.state.zeny_text = "1,500,000";
  probe.Settle();
  REQUIRE(FindButton(probe, ro::ui::kDealWindowId, "Verrouiller").x >= 0.0F);
  CHECK_FALSE(GImGui->HoveredIdIsDisabled);
  const DealWindowAction action = probe.Click();
  CHECK(action.lock);
  REQUIRE(action.zeny.has_value());
  CHECK(*action.zeny == 1'500'000);

  SECTION("le montant déjà proposé ne se renvoie pas") {
    probe.view.my_zeny = 1'500'000;
    const DealWindowAction again = probe.Click();
    CHECK(again.lock);
    CHECK_FALSE(again.zeny.has_value());
  }
}

TEST_CASE("Échange : un montant illisible ou une raison retient le verrou", "[ui][deal]") {
  Probe probe;
  SECTION("montant illisible") { probe.state.zeny_text = "1,5"; }
  SECTION("raison donnée par l'échange") { probe.lock_blockers = {"Offre pleine."}; }
  probe.Settle();
  REQUIRE(FindButton(probe, ro::ui::kDealWindowId, "Verrouiller").x >= 0.0F);
  CHECK(GImGui->HoveredIdIsDisabled);
  const DealWindowAction action = probe.Click();
  CHECK_FALSE(action.lock);
  CHECK_FALSE(action.zeny.has_value());
}

TEST_CASE("Échange : « Échanger » ouvre le récapitulatif avant de valider", "[ui][deal]") {
  Probe probe;
  probe.view.lock_sent = true;
  probe.view.my_locked = true;
  probe.view.partner_locked = true;
  probe.Settle();
  REQUIRE(FindButton(probe, ro::ui::kDealWindowId, "Échanger").x >= 0.0F);
  CHECK_FALSE(GImGui->HoveredIdIsDisabled);
  const DealWindowAction clicked = probe.Click();
  // 🔴 LE CLIC NE VALIDE PAS.
  CHECK_FALSE(clicked.execute);
  CHECK(probe.state.confirming);
  CHECK(probe.state.confirming_revision == kRevision);
  static_cast<void>(probe.Frame());
  REQUIRE(ConfirmShown());

  SECTION("oui : la validation part, au rang que le joueur a lu") {
    REQUIRE(YesButton(probe).x >= 0.0F);
    const DealWindowAction confirmed = probe.Click();
    CHECK(confirmed.execute);
    CHECK(confirmed.execute_revision == kRevision);
    CHECK_FALSE(probe.state.confirming);
  }

  SECTION("une offre changée sous la boîte la ferme sans valider") {
    probe.view.partner_revision = kRevision + 1;
    const DealWindowAction changed = probe.Frame();
    CHECK_FALSE(changed.execute);
    CHECK_FALSE(probe.state.confirming);
    static_cast<void>(probe.Frame());
    CHECK_FALSE(ConfirmShown());
  }
}

TEST_CASE("Échange : une validation envoyée grise le bouton", "[ui][deal]") {
  Probe probe;
  probe.view.lock_sent = true;
  probe.view.my_locked = true;
  probe.view.partner_locked = true;
  probe.view.execute_sent = true;
  probe.Settle();
  REQUIRE(FindButton(probe, ro::ui::kDealWindowId, "En attente…").x >= 0.0F);
  CHECK(GImGui->HoveredIdIsDisabled);
  static_cast<void>(probe.Click());
  CHECK_FALSE(probe.state.confirming);
}

TEST_CASE("Échange : un item du sac lâché sur mon offre s'offre", "[ui][deal]") {
  Probe probe;
  probe.carried = ro::ui::ItemDrag{.item_id = 1101, .client_slot = kSwordSlot};
  probe.Settle();
  const ImGuiWindow* column = Probe::WindowNamed("##deal_mine");
  REQUIRE(column != nullptr);
  const ImVec2 target(column->Pos.x + column->Size.x * 0.5F, column->Pos.y + column->Size.y * 0.5F);

  SECTION("un item seul part tout de suite") {
    const DealWindowAction dropped = probe.DragTo(target);
    REQUIRE(dropped.offer.has_value());
    CHECK(dropped.offer->client_slot == kSwordSlot);
    CHECK(dropped.offer->amount == 1);
  }

  SECTION("une pile demande sa quantité") {
    probe.carried->client_slot = kPotionSlot;
    const DealWindowAction dropped = probe.DragTo(target);
    CHECK_FALSE(dropped.offer.has_value());
    CHECK(probe.state.asking_amount);
    CHECK(probe.state.asking_value == kPotionStack);
    // Lâchée, la pile se demande par la modale du sac et du storage.
    CHECK(probe.state.asking_by_drop);
  }

  SECTION("une pièce portée se refuse, et le refus se dit") {
    probe.carried->worn = true;
    const DealWindowAction dropped = probe.DragTo(target);
    CHECK_FALSE(dropped.offer.has_value());
    CHECK_FALSE(probe.state.flash.empty());
  }
}

// ── LA PROPOSITION ──────────────────────────────────────────────────────────

namespace {

struct ProposalProbe {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  DealProposalState state;
  DealProposalView view{
      .text = "Untel (Niv. 99) vous propose un échange.", .incoming = true, .seq = 1};

  DealProposalAction Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const DealProposalAction action = ro::ui::DrawDealProposal(state, view, art);
    ui.EndFrame();
    return action;
  }

  DealProposalAction Click() {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    const DealProposalAction pressed = Frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    DealProposalAction released = Frame();
    released.accept |= pressed.accept;
    released.refuse |= pressed.refuse;
    return released;
  }

  [[nodiscard]] bool Hover(const char* label) {
    ImGuiWindow* window = Probe::WindowNamed(ro::ui::kDealProposalWindowId);
    REQUIRE(window != nullptr);
    const ImGuiID wanted = window->GetID(label);
    for (float y = window->Pos.y + window->Size.y - 2.0F; y > window->Pos.y;
         y -= kSweepStepPixels) {
      for (float x = window->Pos.x + 2.0F; x < window->Pos.x + window->Size.x;
           x += kSweepStepPixels) {
        ImGui::GetIO().AddMousePosEvent(x, y);
        static_cast<void>(Frame());
        if (GImGui->HoveredId == wanted) {
          return true;
        }
      }
    }
    return false;
  }
};

}  // namespace

TEST_CASE("Échange : une proposition neuve ouvre la boîte, au centre", "[ui][deal]") {
  ProposalProbe probe;
  static_cast<void>(probe.Frame());
  static_cast<void>(probe.Frame());
  CHECK(probe.state.open);
  CHECK(probe.state.seen == 1);
  const ImGuiWindow* box = Probe::WindowNamed(ro::ui::kDealProposalWindowId);
  REQUIRE(box != nullptr);
  const float middle = box->Pos.x + box->Size.x * 0.5F;
  CHECK(middle > kViewport.width_px * 0.4F);
  CHECK(middle < kViewport.width_px * 0.6F);

  // Refermée, elle se rouvre sur la suivante.
  probe.state.open = false;
  static_cast<void>(probe.Frame());
  probe.view.seq = 2;
  static_cast<void>(probe.Frame());
  CHECK(probe.state.open);
}

TEST_CASE("Échange : une proposition reçue s'accepte ou se refuse", "[ui][deal]") {
  ProposalProbe probe;
  static_cast<void>(probe.Frame());
  static_cast<void>(probe.Frame());

  SECTION("Accepter") {
    REQUIRE(probe.Hover("Accepter"));
    const DealProposalAction action = probe.Click();
    CHECK(action.accept);
    CHECK_FALSE(action.refuse);
  }

  SECTION("Refuser") {
    REQUIRE(probe.Hover("Refuser"));
    const DealProposalAction action = probe.Click();
    CHECK(action.refuse);
    CHECK_FALSE(action.accept);
  }

  SECTION("répondue, les boutons se retiennent") {
    probe.view.answered = true;
    REQUIRE(probe.Hover("Accepter"));
    CHECK(GImGui->HoveredIdIsDisabled);
    const DealProposalAction action = probe.Click();
    CHECK_FALSE(action.accept);
  }
}

TEST_CASE("Échange : ma proposition envoyée se retire par « Annuler »", "[ui][deal]") {
  ProposalProbe probe;
  probe.view.incoming = false;
  static_cast<void>(probe.Frame());
  static_cast<void>(probe.Frame());
  REQUIRE(probe.Hover("Annuler"));
  const DealProposalAction action = probe.Click();
  CHECK(action.refuse);
}

TEST_CASE("Échange : la croix de la proposition refuse", "[ui][deal]") {
  ProposalProbe probe;
  static_cast<void>(probe.Frame());
  static_cast<void>(probe.Frame());
  REQUIRE(probe.Hover("#CLOSE"));
  const DealProposalAction action = probe.Click();
  CHECK(action.refuse);
  CHECK_FALSE(probe.state.open);
}
