// L'échoppe de vente dans la boucle : la préparation, mon échoppe et son
// journal. La visite est dans `vending_visit_stage.cpp`, les vues dans
// `vending_views.cpp`. Voir l'en-tête.

#include "game/vending_stage.h"

#include <algorithm>
#include <cstring>
#include <optional>
#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/vending.h"
#include "ro/sim/session_settings.h"
#include "ro/sim/world.h"
#include "ro/ui/inventory_window.h"

#include "chat/banned_input.h"
#include "game/trade_recap.h"
#include "game/vending_said.h"
#include "items/inventory_view.h"

namespace moonclient {
namespace {

const ro::sim::InventoryItem* CartItemAt(const ro::sim::World& world, std::uint16_t cart_slot) {
  if (cart_slot == 0) {
    return nullptr;
  }
  for (const ro::sim::InventoryItem& item : world.cart().items()) {
    if (item.client_slot == cart_slot && item.item_id != 0) {
      return &item;
    }
  }
  return nullptr;
}

const ro::sim::InventoryItem* ShopItemAt(const ro::sim::Vending& vending, std::uint16_t cart_slot) {
  for (const ro::sim::VendingLine& line : vending.shop_lines()) {
    if (line.item.client_slot == cart_slot) {
      return &line.item;
    }
  }
  return nullptr;
}

}  // namespace

void VendingStage::SeeSaveRoot(std::filesystem::path client_root) {
  client_root_ = std::move(client_root);
  memory_ = VendingMemory{};
  LoadVendingMemory(client_root_, memory_);
}

void VendingStage::StartSession() {
  vending_.CloseSilently();
  static_cast<void>(vending_.TakeFarewell());
  opening_result_said_ = vending_.opening_result_seq();
  purchase_result_said_ = vending_.purchase_result_seq();
  journal_said_ = vending_.journal_seq();
  sales_said_ = vending_.journal().size();
  visit_list_seen_ = vending_.visit_seq();
  shop_remembered_ = vending_.shop_opening_seq();
  sold_out_closed_ = vending_.shop_opening_seq();
  shop_was_open_ = false;
  purchase_in_flight_ = false;
  visit_outcome_.clear();
  hovered_ = {};
  described_.reset();
}

bool VendingStage::HoldsWorld() const noexcept {
  return vending_.preparing() || vending_.shop_open() || vending_.visiting();
}

std::optional<TradeItemFocus> VendingStage::TakeDescribed() noexcept {
  return std::exchange(described_, std::nullopt);
}

void VendingStage::Note(const ro::sim::InventoryItem* item, bool hovered, bool described,
                        ro::ui::ChatLinkGesture gesture) {
  if (item == nullptr || item->item_id == 0) {
    return;
  }
  // L'OBJET SE DÉCRIT PAR CE QU'IL PORTE : il n'est pas dans le sac, et sa case
  // n'y dirait rien.
  const TradeItemFocus focus{
      .item_id = item->item_id, .link = ItemLinkOf(*item), .gesture = gesture};
  if (hovered) {
    hovered_ = focus;
  }
  if (described) {
    described_ = focus;
  }
}

void VendingStage::SaveMemory() const {
  if (!client_root_.empty()) {
    static_cast<void>(SaveVendingMemory(client_root_, memory_));
  }
}

void VendingStage::Advance(VendingWindows& windows, const TradeContext& context) {
  hovered_ = {};
  FollowSetup(windows, context);
  FollowShop(windows, context);
  FollowLog(windows);
  FollowVisit(windows, context);

  // L'ABANDON SE DIT : sans lui, le serveur garderait le cart figé.
  if (vending_.TakeFarewell()) {
    if (auto said = ro::net::SayVendingPreparationCancelled(context.link); !said) {
      ro::core::LogError("échoppe : {}", ro::core::Describe(said.error()));
    }
  }
  if (vending_.opening_pending() && context.now_ms - opening_sent_ms_ >= kVendingOpeningSilenceMs) {
    vending_.ForgetPendingOpening();
  }

  SayOwnNews(windows, context);
  RememberOpenedShop(context);
  SayVisitNews(windows, context);
  OpenWindows(windows);

  ComposeSetup(windows.setup, context);
  ComposeShop(context);
  ComposeLog(context);
  ComposeVisit(windows.visit, context);
  windows.setup_view = vending_.preparing() && windows.setup.open ? &setup_view_ : nullptr;
  windows.shop_view = vending_.shop_open() ? &shop_view_ : nullptr;
  windows.log_view = windows.log.open ? &log_view_ : nullptr;
  windows.visit_view = vending_.visiting() && windows.visit.open ? &visit_view_ : nullptr;
}

void VendingStage::FollowSetup(VendingWindows& windows, const TradeContext& context) {
  ro::ui::VendingSetupWindowState& state = windows.setup;
  const ro::ui::VendingSetupAction answer = std::exchange(windows.setup_answer, {});
  Note(CartItemAt(context.world, answer.hovered_slot), true, false);
  Note(CartItemAt(context.world, answer.describe_slot), false, true, answer.item_gesture);
  if (answer.settings_changed) {
    memory_.grid = state.grid;
    memory_.confirm_high_prices = state.confirm_high_prices;
    SaveMemory();
  }
  if (!vending_.preparing()) {
    return;
  }
  // 🔴 UNE FENÊTRE BAISSÉE EST UN ABANDON — la croix, « Annuler », Échap. ⚠
  // MAIS SEULEMENT CELLE QUI MONTRAIT DÉJÀ CETTE PRÉPARATION : à l'image où
  // elle s'annonce, la fenêtre est encore baissée.
  const bool shown = state.preparation_seen == vending_.preparation_seq();
  if (answer.cancel || (!state.open && shown)) {
    vending_.CancelPreparation();
    return;
  }
  if (!shown) {
    return;
  }
  if (answer.resume) {
    Resume(state, context);
  }
  if (!answer.open_shop || !vending_.CanSendOpening()) {
    return;
  }
  // ⚠ REJUGÉ ICI : la fenêtre a pu juger d'une image de retard. Ce qui
  // échouerait ne part pas, et la raison se dit dans la conversation.
  std::vector<std::size_t> unreadable;
  const std::vector<ro::sim::VendingSetupLine> lines = SetupLinesOf(state, unreadable);
  const std::string typed(state.title.data());
  const std::span<const ro::sim::InventoryItem> cart = context.world.cart().items();
  const std::int64_t zeny = ZenyOf(context.world);
  if (!unreadable.empty()) {
    Say(context, ro::core::Tr("Un prix ne se lit pas."), kVendingFailedColor);
    return;
  }
  // 🔴 UN TITRE QUI PORTE UN MOT INTERDIT NE PART PAS, avant même les lignes.
  if (context.banned != nullptr) {
    if (const std::optional<std::string> refused =
            BannedInputRefusal(*context.banned, typed, BannedInput::kSentence);
        refused.has_value()) {
      Say(context, *refused, BannedInputColor(BannedInput::kSentence));
      return;
    }
  }
  if (const std::vector<ro::sim::VendingSetupRefusal> refusals = ro::sim::CheckVendingSetup(
          ro::sim::FitVendingTitle(typed), lines, vending_.permitted_lines(), cart, zeny);
      !refusals.empty()) {
    Say(context, VendingSetupFaultText(refusals.front().fault), kVendingFailedColor);
    return;
  }
  if (auto sent = ro::net::RequestVendingOpen(context.link, vending_, typed, lines, cart, zeny);
      !sent) {
    ro::core::LogError("échoppe : {}", ro::core::Describe(sent.error()));
    return;
  }
  opening_sent_ms_ = context.now_ms;
}

// « REPRENDRE LA DERNIÈRE ÉCHOPPE » : son nom, et ses lignes reposées sur ce que
// le cart contient AUJOURD'HUI. Rien ne se pose qui n'y soit.
void VendingStage::Resume(ro::ui::VendingSetupWindowState& state, const TradeContext& context) {
  const RememberedVendingShop* last = memory_.LastOf(context.world.player_name());
  if (last == nullptr) {
    return;
  }
  const std::size_t kept = std::min(last->title.size(), state.title.size() - 1);
  std::memcpy(state.title.data(), last->title.data(), kept);
  state.title[kept] = '\0';
  const std::vector<ResumedVendingLine> resumed =
      ResumeVendingLines(*last, context.world.cart().items(), vending_.permitted_lines());
  if (resumed.empty()) {
    Say(context, ro::core::Tr("Aucun item de la dernière échoppe n'est dans le cart."),
        kVendingFailedColor);
    return;
  }
  state.lines.clear();
  for (const ResumedVendingLine& line : resumed) {
    ro::ui::VendingSetupLineState placed;
    placed.cart_slot = line.cart_slot;
    placed.amount = line.amount;
    const std::string price = ro::ui::GroupThousands(line.price_zeny);
    const std::size_t length = std::min(price.size(), placed.price_text.size() - 1);
    std::memcpy(placed.price_text.data(), price.data(), length);
    state.lines.push_back(placed);
  }
}

void VendingStage::FollowShop(VendingWindows& windows, const TradeContext& context) {
  const ro::ui::VendingShopAction answer = std::exchange(windows.shop_answer, {});
  Note(ShopItemAt(vending_, answer.hovered_slot), true, false);
  Note(ShopItemAt(vending_, answer.describe_slot), false, true, answer.item_gesture);
  if (answer.close_shop && vending_.shop_open()) {
    if (auto sent = ro::net::RequestVendingShopClose(context.link, vending_); !sent) {
      ro::core::LogError("échoppe : {}", ro::core::Describe(sent.error()));
    }
  }
}

void VendingStage::FollowLog(VendingWindows& windows) {
  const ro::ui::VendingLogAction answer = std::exchange(windows.log_answer, {});
  const std::span<const ro::sim::VendingSale> journal = vending_.journal();
  if (answer.hovered_rank > 0 && answer.hovered_rank <= journal.size()) {
    Note(&journal[answer.hovered_rank - 1].item, true, false);
  }
  if (answer.describe_rank > 0 && answer.describe_rank <= journal.size()) {
    Note(&journal[answer.describe_rank - 1].item, false, true, answer.item_gesture);
  }
}

void VendingStage::SayOwnNews(VendingWindows& windows, const TradeContext& context) {
  // LE RÉSULTAT DE L'OUVERTURE : une boîte pour l'échec, une ligne pour la case
  // interdite, le récapitulatif pour la réussite — l'échoppe telle que le
  // serveur l'a acceptée, sa liste étant arrivée juste avant.
  if (SaleListRecapDue(vending_, opening_result_said_)) {
    std::vector<TradeRecapItem> items;
    for (const ro::sim::VendingLine& line : vending_.shop_lines()) {
      items.push_back({.name = context.items.NameOf(line.item, context.creators),
                       .unit_price_zeny = line.price_zeny,
                       .amount = line.item.amount});
    }
    const ro::sim::SessionSettings& rules = context.world.session();
    const VendingTax tax{.permyriad = rules.vending_tax_permyriad,
                         .from_zeny = rules.vending_tax_min_zeny};
    for (VendingChatLine& line : SaleListRecap(context.words, items, ZenyOf(context.world), tax)) {
      Say(context, std::move(line.text), line.color);
    }
  }
  if (vending_.opening_result_seq() != opening_result_said_) {
    opening_result_said_ = vending_.opening_result_seq();
    VendingOpeningNews news = SaidOfOpeningResult(context.words, vending_.opening_result());
    if (news.said == VendingOpeningSaid::kNotice) {
      windows.notice.text = news.text.empty()
                                ? std::string(ro::core::Tr("L'ouverture du shop a échoué."))
                                : std::move(news.text);
      windows.notice.asked = false;
    } else if (news.said == VendingOpeningSaid::kLine) {
      Say(context, std::move(news.text), kVendingFailedColor);
    }
  }
  // CHAQUE VENTE SE DIT, avec le nom composé et la quantité de CETTE vente.
  if (vending_.journal_seq() != journal_said_) {
    journal_said_ = vending_.journal_seq();
    const std::span<const ro::sim::VendingSale> journal = vending_.journal();
    if (journal.size() < sales_said_) {
      sales_said_ = 0;
    }
    for (std::size_t rank = sales_said_; rank < journal.size(); ++rank) {
      const ro::sim::VendingSale& sale = journal[rank];
      if (sale.item.item_id == 0) {
        continue;
      }
      VendingChatLine line = SaidOfSale(
          context.words, context.items.NameOf(sale.item, context.creators), sale.item.amount);
      Say(context, std::move(line.text), line.color);
    }
    sales_said_ = journal.size();
  }
  // 🔴 L'ÉCHOPPE VIDE SE FERME D'ELLE-MÊME, comme chez le client d'origine : le
  // serveur la garderait ouverte, enseigne comprise, sans rien à vendre.
  if (vending_.sold_out() && sold_out_closed_ != vending_.shop_opening_seq()) {
    sold_out_closed_ = vending_.shop_opening_seq();
    Say(context, std::string(ro::vfs::TradeText(context.words, ro::vfs::TradeWord::kItemsSoldOut)),
        kVendingSoldOutColor);
    if (auto sent = ro::net::RequestVendingShopClose(context.link, vending_); !sent) {
      ro::core::LogError("échoppe : {}", ro::core::Describe(sent.error()));
    }
  }
}

// L'ÉCHOPPE QUI VIENT DE S'OUVRIR SE RETIENT, telle que le serveur l'a
// acceptée : c'est elle que « Reprendre » reposera.
void VendingStage::RememberOpenedShop(const TradeContext& context) {
  if (!vending_.shop_open() || vending_.shop_opening_seq() == shop_remembered_) {
    return;
  }
  shop_remembered_ = vending_.shop_opening_seq();
  // Une échoppe que ce client n'a pas demandée n'a pas de titre retenu.
  if (vending_.shop_title().empty() || context.world.player_name().empty()) {
    return;
  }
  RememberedVendingShop shop;
  shop.character = context.world.player_name();
  shop.title = vending_.shop_title();
  for (const ro::sim::VendingLine& line : vending_.shop_lines()) {
    shop.lines.push_back(RememberedVendingLine{.item_id = line.item.item_id,
                                               .refine_level = line.item.refine_level,
                                               .cards = line.item.cards,
                                               .amount = line.item.amount,
                                               .price_zeny = line.price_zeny});
  }
  memory_.Remember(std::move(shop));
  SaveMemory();
}

void VendingStage::OpenWindows(VendingWindows& windows) {
  // LA PRÉPARATION, sur chaque annonce neuve ; ses réglages viennent du disque.
  if (!vending_.preparing()) {
    windows.setup.open = false;
  } else if (vending_.preparation_seq() != windows.setup.preparation_seen) {
    ro::ui::OpenVendingSetupOn(windows.setup, vending_.preparation_seq());
    windows.setup.grid = memory_.grid;
    windows.setup.confirm_high_prices = memory_.confirm_high_prices;
  }
  // MON ÉCHOPPE : ⚠ SA FENÊTRE SE RELÈVE À CHAQUE OUVERTURE NEUVE, sans quoi
  // l'interrupteur resté baissé de la précédente se lirait comme une demande
  // de fermeture.
  if (!vending_.shop_open()) {
    windows.shop.open = false;
    windows.shop.confirming = false;
  } else if (vending_.shop_opening_seq() != windows.shop.opening_seen) {
    windows.shop.open = true;
    windows.shop.opening_seen = vending_.shop_opening_seq();
    windows.shop.confirming = false;
  }
  // LE JOURNAL paraît quand l'échoppe se ferme après au moins une vente, et
  // s'en va quand une autre s'ouvre.
  if (vending_.shop_open()) {
    windows.log.open = false;
  } else if (shop_was_open_ && !vending_.journal().empty()) {
    windows.log.open = true;
    windows.log.journal_seen = vending_.journal_seq();
  }
  shop_was_open_ = vending_.shop_open();
  // LA VISITE, sur chaque vendeur neuf.
  if (!vending_.visiting()) {
    windows.visit.open = false;
  } else if (vending_.visit_opening_seq() != windows.visit.opening_seen) {
    ro::ui::OpenVendingVisitOn(windows.visit, vending_.visit_opening_seq());
    visit_outcome_.clear();
  }
}

}  // namespace moonclient
