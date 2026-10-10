// L'échoppe d'un autre joueur, dans la boucle : l'achat qu'on rejuge, les refus
// qu'on dit, et le panier qu'une liste neuve borne ou vide. Voir
// `vending_stage.h`.

#include <algorithm>
#include <limits>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/vending.h"
#include "ro/sim/world.h"

#include "game/vending_said.h"
#include "game/vending_stage.h"

namespace moonclient {
namespace {

const ro::sim::InventoryItem* OfferItemAt(const ro::sim::Vending& vending,
                                          std::uint16_t shop_slot) {
  if (shop_slot == 0) {
    return nullptr;
  }
  for (const ro::sim::VendingLine& offer : vending.offers()) {
    if (offer.item.client_slot == shop_slot) {
      return &offer.item;
    }
  }
  return nullptr;
}

std::vector<ro::sim::VendingBasketLine> BasketLinesOf(
    std::span<const ro::ui::VendingBasketEntry> entries) {
  std::vector<ro::sim::VendingBasketLine> lines;
  lines.reserve(entries.size());
  for (const ro::ui::VendingBasketEntry& entry : entries) {
    lines.push_back(ro::sim::VendingBasketLine{
        .shop_slot = entry.shop_slot,
        .amount = static_cast<std::uint16_t>(
            std::clamp(entry.amount, 0, int{std::numeric_limits<std::uint16_t>::max()}))});
  }
  return lines;
}

}  // namespace

void VendingStage::FollowVisit(VendingWindows& windows, const TradeContext& context) {
  ro::ui::VendingVisitWindowState& state = windows.visit;
  const ro::ui::VendingVisitAction answer = std::exchange(windows.visit_answer, {});
  Note(OfferItemAt(vending_, answer.hovered_slot), true, false);
  Note(OfferItemAt(vending_, answer.describe_slot), false, true, answer.item_gesture);
  if (!vending_.visiting()) {
    return;
  }
  // AUCUN PAQUET NE FERME UNE VISITE : la clore ici suffit. ⚠ Seulement une
  // fenêtre qui la montrait déjà.
  const bool shown = state.opening_seen == vending_.visit_opening_seq();
  if (answer.close || (!state.open && shown)) {
    vending_.CloseVisit();
    purchase_in_flight_ = false;
    return;
  }
  if (!shown || answer.buy.empty() || vending_.purchase_pending()) {
    return;
  }
  // ⚠ REJUGÉ ICI, Ctrl + clic compris : un achat envoyé sans compter les zeny,
  // que le serveur ignore sans un mot, laisserait le joueur sans explication.
  const std::vector<ro::sim::VendingBasketLine> lines = BasketLinesOf(answer.buy);
  const std::int64_t zeny = ZenyOf(context.world);
  if (const std::vector<ro::sim::VendingBasketRefusal> refusals =
          ro::sim::CheckVendingBasket(lines, vending_.offers(), zeny);
      !refusals.empty()) {
    Say(context, VendingBasketFaultText(refusals.front().fault), kVendingFailedColor);
    return;
  }
  const std::uint32_t refusals_before = vending_.purchase_result_seq();
  if (auto sent = ro::net::RequestVendingPurchase(context.link, vending_, lines, zeny); !sent) {
    ro::core::LogError("échoppe : {}", ro::core::Describe(sent.error()));
    return;
  }
  purchase_in_flight_ = true;
  purchase_from_basket_ = !answer.immediate;
  purchase_refusals_at_send_ = refusals_before;
}

void VendingStage::SayVisitNews(VendingWindows& windows, const TradeContext& context) {
  // UN REFUS D'ACHAT, avec les phrases du client : le nom est celui de l'offre à
  // la case renvoyée, lue AVANT que la liste redemandée ne la remplace.
  if (vending_.purchase_result_seq() != purchase_result_said_) {
    purchase_result_said_ = vending_.purchase_result_seq();
    const ro::sim::InventoryItem* item = OfferItemAt(vending_, vending_.purchase_result_slot());
    const std::string name =
        item != nullptr ? context.items.NameOf(*item, context.creators) : std::string();
    VendingChatLine line = SaidOfPurchaseRefusal(context.words, vending_.purchase_result(), name,
                                                 vending_.purchase_result_amount());
    visit_outcome_ = line.text;
    Say(context, std::move(line.text), line.color);
  }
  // LA LISTE REDEMANDÉE APRÈS UN ACHAT EST REVENUE : sans refus neuf, l'achat a
  // été servi, et le panier qui l'a porté se vide. Refusé, il reste tel quel.
  if (vending_.visit_seq() != visit_list_seen_) {
    visit_list_seen_ = vending_.visit_seq();
    if (purchase_in_flight_ && !vending_.purchase_pending()) {
      purchase_in_flight_ = false;
      if (vending_.purchase_result_seq() == purchase_refusals_at_send_) {
        visit_outcome_.clear();
        if (purchase_from_basket_) {
          windows.visit.basket.clear();
        }
      }
    }
  }
  // LE VENDEUR EST PARTI pendant qu'on regardait : la fenêtre se ferme, et on
  // dit pourquoi.
  if (!vending_.visiting() && windows.visit.open &&
      windows.visit.opening_seen == vending_.visit_opening_seq() &&
      vending_.visit_end() == ro::sim::VendingVisitEnd::kVendorGone) {
    Say(context, ro::core::Tr("Le vendeur a fermé son shop."), kVendingFailedColor);
  }
}

}  // namespace moonclient
