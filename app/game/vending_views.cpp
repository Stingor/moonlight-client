// Ce que les fenêtres de l'échoppe de vente dessinent, composé à chaque image.
// Voir `vending_stage.h`.

#include <algorithm>
#include <cstring>
#include <limits>

#include "ro/core/i18n.h"
#include "ro/core/text.h"
#include "ro/sim/player_status.h"
#include "ro/sim/world.h"

#include "game/vending_said.h"
#include "game/vending_stage.h"

namespace moonclient {
namespace {

// Ajoute une raison, une seule fois : douze lignes au même défaut ne font
// qu'une raison à lire.
void AddReason(std::vector<std::string_view>& reasons, std::string_view reason) {
  if (!reason.empty() && std::ranges::find(reasons, reason) == reasons.end()) {
    reasons.push_back(reason);
  }
}

// Pourquoi une pile du cart ne peut pas se mettre en vente : le serveur
// refuserait l'échoppe ENTIÈRE.
const char* StackRefusal(const ro::sim::InventoryItem& item) {
  if (!item.identified) {
    return ro::core::Tr("Non identifié : le serveur refuserait tout le shop.");
  }
  if (item.damaged) {
    return ro::core::Tr("Abîmé : le serveur refuserait tout le shop.");
  }
  if (item.rented()) {
    return ro::core::Tr("Loué : le serveur refuserait tout le shop.");
  }
  return "";
}

}  // namespace

std::vector<ro::sim::VendingSetupLine> VendingStage::SetupLinesOf(
    const ro::ui::VendingSetupWindowState& state, std::vector<std::size_t>& unreadable) {
  std::vector<ro::sim::VendingSetupLine> lines;
  lines.reserve(state.lines.size());
  for (std::size_t rank = 0; rank < state.lines.size(); ++rank) {
    const ro::ui::VendingSetupLineState& line = state.lines[rank];
    const std::optional<std::int64_t> price = ro::ui::VendingLinePrice(line);
    if (!price.has_value()) {
      unreadable.push_back(rank);
    }
    // ⚠ SATURÉ, JAMAIS REPLIÉ : un prix trop grand doit rester trop grand pour
    // le contrôle qui le refuse.
    const std::int64_t shown = price.value_or(0);
    lines.push_back(ro::sim::VendingSetupLine{
        .cart_slot = line.cart_slot,
        .amount = static_cast<std::uint16_t>(
            std::clamp(line.amount, 0, int{std::numeric_limits<std::uint16_t>::max()})),
        .price_zeny = static_cast<std::uint32_t>(
            std::clamp<std::int64_t>(shown, 0, std::numeric_limits<std::uint32_t>::max()))});
  }
  return lines;
}

std::int64_t VendingStage::ZenyOf(const ro::sim::World& world) {
  const std::int64_t zeny = world.player_status().zeny;
  return zeny == ro::sim::kUnknownStat ? 0 : zeny;
}

void VendingStage::ComposeSetup(const ro::ui::VendingSetupWindowState& state,
                                const TradeContext& context) {
  if (!vending_.preparing()) {
    return;
  }
  const ro::sim::World& world = context.world;
  const std::span<const ro::sim::InventoryItem> cart = world.cart().items();

  // LE STOCK : chaque pile du cart, avec ce qui l'empêcherait d'être vendue.
  stock_names_.clear();
  stock_.clear();
  for (const ro::sim::InventoryItem& item : cart) {
    if (item.item_id == 0 || item.amount <= 0) {
      continue;
    }
    stock_names_.push_back(context.items.NameOf(item, context.creators));
    ro::ui::VendingStockView row;
    row.item.item_id = item.item_id;
    row.item.icon = context.items.IconOf(item.item_id);
    row.item.damaged = item.damaged;
    row.cart_slot = item.client_slot;
    row.amount = item.amount;
    row.refine_level = item.refine_level;
    row.refusal = StackRefusal(item);
    stock_.push_back(row);
  }
  for (std::size_t rank = 0; rank < stock_.size(); ++rank) {
    stock_[rank].item.name = stock_names_[rank];
  }
  setup_view_.stock = stock_;
  setup_view_.permitted_lines = vending_.permitted_lines();

  // LE TITRE TEL QU'IL PARTIRA, et ce qui reste d'octets sur le fil.
  const std::string_view typed(state.title.data());
  fitted_title_ = ro::sim::FitVendingTitle(typed);
  const std::size_t wire = ro::core::WireFromUtf8Text(fitted_title_).size();
  setup_view_.fitted_title = fitted_title_;
  setup_view_.title_cut = fitted_title_.size() < typed.size();
  setup_view_.title_bytes_left =
      ro::sim::kVendingTitleMostTextBytes - std::min(wire, ro::sim::kVendingTitleMostTextBytes);

  // LE VERDICT : les mêmes contrôles que le verbe, plus le prix illisible.
  std::vector<std::size_t> unreadable;
  const std::vector<ro::sim::VendingSetupLine> lines = SetupLinesOf(state, unreadable);
  const std::int64_t zeny = ZenyOf(world);
  const std::vector<ro::sim::VendingSetupRefusal> refusals =
      ro::sim::CheckVendingSetup(fitted_title_, lines, vending_.permitted_lines(), cart, zeny);
  setup_blockers_.clear();
  line_fault_texts_.assign(state.lines.size(), std::string());
  const auto fault_on = [this](std::size_t rank, std::string_view text) {
    std::string& fault = line_fault_texts_[rank];
    if (fault.find(text) == std::string::npos) {
      fault += fault.empty() ? "" : "\n";
      fault += text;
    }
  };
  for (const std::size_t rank : unreadable) {
    const bool empty = state.lines[rank].price_text[0] == '\0';
    const char* reason =
        empty ? ro::core::Tr("Une ligne n'a pas de prix.") : ro::core::Tr("Un prix ne se lit pas.");
    AddReason(setup_blockers_, reason);
    fault_on(rank, empty ? ro::core::Tr("Donne-lui un prix.")
                         : ro::core::Tr("Prix illisible : des chiffres, avec ou sans séparateurs "
                                        "entre groupes de trois."));
  }
  for (const ro::sim::VendingSetupRefusal& refusal : refusals) {
    // Un prix illisible part à zéro : son « prix nul » est déjà dit.
    const bool said = refusal.fault == ro::sim::VendingSetupFault::kZeroPrice &&
                      std::ranges::find(unreadable, refusal.line) != unreadable.end();
    if (said) {
      continue;
    }
    const char* text = VendingSetupFaultText(refusal.fault);
    AddReason(setup_blockers_, text);
    if (refusal.line < line_fault_texts_.size()) {
      fault_on(refusal.line, text);
    }
  }
  line_faults_.assign(line_fault_texts_.begin(), line_fault_texts_.end());
  setup_view_.blockers = setup_blockers_;
  setup_view_.line_faults = line_faults_;
  setup_view_.takings = ro::sim::VendingSetupTakings(lines);
  setup_view_.zeny = world.player_status().zeny == ro::sim::kUnknownStat
                         ? ro::ui::kTradeNoPurse
                         : world.player_status().zeny;
  setup_view_.awaiting = vending_.opening_pending();

  resume_title_.clear();
  if (const RememberedVendingShop* last = memory_.LastOf(world.player_name());
      last != nullptr && !last->lines.empty()) {
    resume_title_ = last->title.empty() ? std::string("...") : last->title;
  }
  setup_view_.resume_title = resume_title_;
}

void VendingStage::ComposeShop(const TradeContext& context) {
  if (!vending_.shop_open()) {
    return;
  }
  shop_names_.clear();
  shop_lines_.clear();
  for (const ro::sim::VendingLine& line : vending_.shop_lines()) {
    shop_names_.push_back(context.items.NameOf(line.item, context.creators));
    ro::ui::VendingShopLineView shown;
    shown.item.item_id = line.item.item_id;
    shown.item.icon = context.items.IconOf(line.item.item_id);
    shown.item.damaged = line.item.damaged;
    shown.cart_slot = line.item.client_slot;
    shown.amount = line.item.amount;
    shown.price = line.price_zeny;
    shop_lines_.push_back(shown);
  }
  for (std::size_t rank = 0; rank < shop_lines_.size(); ++rank) {
    shop_lines_[rank].item.name = shop_names_[rank];
  }
  shop_view_.title = vending_.shop_title();
  shop_view_.lines = shop_lines_;
  shop_view_.earned = vending_.earned_zeny();
}

void VendingStage::ComposeLog(const TradeContext& context) {
  const std::span<const ro::sim::VendingSale> journal = vending_.journal();
  log_names_.clear();
  log_times_.clear();
  log_lines_.clear();
  for (std::size_t rank = 0; rank < journal.size(); ++rank) {
    const ro::sim::VendingSale& sale = journal[rank];
    log_names_.push_back(sale.item.item_id == 0
                             ? std::string(ro::core::Tr("(item inconnu)"))
                             : context.items.NameOf(sale.item, context.creators));
    log_times_.push_back(VendingSoldAtText(sale.sold_at_unix_s));
    ro::ui::VendingLogLineView shown;
    shown.item.item_id = sale.item.item_id;
    shown.item.icon = context.items.IconOf(sale.item.item_id);
    shown.rank = rank;
    shown.amount = sale.item.amount;
    shown.zeny = sale.zeny;
    shown.buyer_char_id = sale.buyer_char_id;
    log_lines_.push_back(shown);
  }
  for (std::size_t rank = 0; rank < log_lines_.size(); ++rank) {
    log_lines_[rank].item.name = log_names_[rank];
    log_lines_[rank].sold_at = log_times_[rank];
  }
  log_view_.sales = log_lines_;
  log_view_.total = vending_.earned_zeny();
}

void VendingStage::ComposeVisit(ro::ui::VendingVisitWindowState& state,
                                const TradeContext& context) {
  if (!vending_.visiting()) {
    return;
  }
  const ro::sim::World& world = context.world;
  // LE VENDEUR ET SON ENSEIGNE, s'ils sont en vue.
  vendor_name_.clear();
  visit_title_.clear();
  if (const ro::sim::Actor* vendor = world.Find(vending_.vendor_aid()); vendor != nullptr) {
    vendor_name_ = vendor->name;
    if (vendor->signboard.has_value()) {
      visit_title_ = vendor->signboard->title;
    }
  }
  // LES OFFRES, sous leur nom COMPLET : raffinement, cartes, options — l'objet
  // n'est pas à nous, et c'est tout ce qu'on en sait.
  offer_names_.clear();
  offers_.clear();
  for (const ro::sim::VendingLine& offer : vending_.offers()) {
    offer_names_.push_back(context.items.NameOf(offer.item, context.creators));
    ro::ui::VendingOfferView shown;
    shown.item.item_id = offer.item.item_id;
    shown.item.icon = context.items.IconOf(offer.item.item_id);
    shown.item.damaged = offer.item.damaged;
    shown.shop_slot = offer.item.client_slot;
    shown.stock = offer.item.amount;
    shown.price = offer.price_zeny;
    offers_.push_back(shown);
  }
  for (std::size_t rank = 0; rank < offers_.size(); ++rank) {
    offers_[rank].item.name = offer_names_[rank];
  }
  // Le panier se borne à la liste d'aujourd'hui AVANT d'être jugé.
  ro::ui::FitVendingBasket(state.basket, offers_);

  const std::int64_t zeny = ZenyOf(world);
  std::vector<ro::sim::VendingBasketLine> lines;
  lines.reserve(state.basket.size());
  for (const ro::ui::VendingBasketEntry& entry : state.basket) {
    lines.push_back(ro::sim::VendingBasketLine{
        .shop_slot = entry.shop_slot,
        .amount = static_cast<std::uint16_t>(
            std::clamp(entry.amount, 0, int{std::numeric_limits<std::uint16_t>::max()}))});
  }
  visit_blockers_.clear();
  if (!lines.empty()) {
    for (const ro::sim::VendingBasketRefusal& refusal :
         ro::sim::CheckVendingBasket(lines, vending_.offers(), zeny)) {
      AddReason(visit_blockers_, VendingBasketFaultText(refusal.fault));
    }
  }
  visit_view_.vendor_name = vendor_name_;
  visit_view_.title = visit_title_;
  visit_view_.offers = offers_;
  visit_view_.zeny = world.player_status().zeny == ro::sim::kUnknownStat
                         ? ro::ui::kTradeNoPurse
                         : world.player_status().zeny;
  visit_view_.awaiting = vending_.purchase_pending();
  visit_view_.blockers = visit_blockers_;
  visit_view_.basket_total = ro::sim::VendingBasketCost(lines, vending_.offers());
  visit_view_.outcome = visit_outcome_;
}

}  // namespace moonclient
