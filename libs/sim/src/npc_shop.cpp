#include "ro/sim/npc_shop.h"

#include <algorithm>

namespace ro::sim {

void NpcShop::Open(NpcShopKind kind, NpcShopDeals deals, std::uint32_t npc_id) {
  kind_ = kind;
  deals_ = deals;
  npc_id_ = npc_id;
  ++opening_seq_;
  buy_offers_.clear();
  sell_offers_.clear();
  buy_seq_ = 0;
  sell_seq_ = 0;
  points_ = 0;
  secondary_points_ = 0;
  armed_ = false;
  awaiting_result_ = false;
  outcome_ = NpcShopOutcome{};
  // ⚠ UNE BOUTIQUE QUI EN REMPLACE UNE AUTRE N'EN DOIT PLUS LE CONGÉ : le
  // serveur n'en tient qu'une, et c'est la nouvelle.
  farewell_due_ = NpcShopFarewell::kNone;
}

void NpcShop::TellDealChoice(std::uint32_t npc_id) {
  // ⚠ LE CHOIX N'ARME PAS LA BOUTIQUE : le serveur ne la retient qu'en
  // envoyant une liste, en réponse au choix d'achat ou de vente.
  Open(NpcShopKind::kZeny, NpcShopDeals::kBuyAndSell, npc_id);
}

void NpcShop::TellBuyList(std::span<const NpcShopOffer> offers) {
  // Une boutique NOMMÉE reçoit sa liste en réponse à notre choix. Toute autre
  // liste d'achat est une ouverture neuve : le `callshop` d'un script.
  if (kind_ != NpcShopKind::kZeny || deals_ != NpcShopDeals::kBuyAndSell) {
    Open(NpcShopKind::kZeny, NpcShopDeals::kBuyOnly, 0);
  }
  buy_offers_.assign(offers.begin(), offers.end());
  ++buy_seq_;
  armed_ = true;
}

void NpcShop::TellSellList(std::span<const NpcShopSellOffer> offers) {
  if (kind_ != NpcShopKind::kZeny || deals_ != NpcShopDeals::kBuyAndSell) {
    Open(NpcShopKind::kZeny, NpcShopDeals::kSellOnly, 0);
  }
  sell_offers_.assign(offers.begin(), offers.end());
  ++sell_seq_;
  armed_ = true;
}

void NpcShop::TellMarket(std::span<const NpcShopOffer> offers) {
  Open(NpcShopKind::kMarket, NpcShopDeals::kBuyOnly, 0);
  buy_offers_.assign(offers.begin(), offers.end());
  ++buy_seq_;
  // Le market reste armé tant qu'il est ouvert : le serveur ne l'oublie pas
  // après un achat.
  armed_ = true;
}

void NpcShop::TellPointShop(std::span<const NpcShopOffer> offers, std::uint32_t points,
                            std::uint32_t secondary_points) {
  Open(NpcShopKind::kPoints, NpcShopDeals::kBuyOnly, 0);
  buy_offers_.assign(offers.begin(), offers.end());
  ++buy_seq_;
  points_ = points;
  secondary_points_ = secondary_points;
  armed_ = true;
}

void NpcShop::NoteDealSent() noexcept {
  awaiting_result_ = true;
  awaited_kind_ = kind_;
  // Le market et la boutique à points restent armés ; seule la boutique à
  // zeny est oubliée par le serveur après chaque transaction.
  if (kind_ == NpcShopKind::kZeny) {
    armed_ = false;
  }
}

void NpcShop::RecordOutcome(NpcShopSide side, std::uint16_t result) {
  const NpcShopKind kind = open() ? kind_ : awaited_kind_;
  awaiting_result_ = false;
  ++outcome_count_;
  outcome_ = NpcShopOutcome{.kind = kind, .side = side, .result = result, .seq = outcome_count_};
}

bool NpcShop::Awaits(NpcShopKind kind) const noexcept {
  return kind_ == kind || (awaiting_result_ && awaited_kind_ == kind);
}

void NpcShop::TellBuyResult(std::uint8_t result) {
  if (!Awaits(NpcShopKind::kZeny)) {
    return;
  }
  RecordOutcome(NpcShopSide::kBuy, result);
  if (single_use()) {
    Close();
  }
}

void NpcShop::TellSellResult(std::uint8_t result) {
  if (!Awaits(NpcShopKind::kZeny)) {
    return;
  }
  RecordOutcome(NpcShopSide::kSell, result);
  if (single_use()) {
    Close();
  }
}

void NpcShop::TellMarketResult(std::uint16_t result, std::span<const MarketBought> bought) {
  if (!Awaits(NpcShopKind::kMarket)) {
    return;
  }
  RecordOutcome(NpcShopSide::kBuy, result);
  if (result != kNpcShopDealSucceeded) {
    return;
  }
  for (const MarketBought& line : bought) {
    const auto offer = std::ranges::find_if(
        buy_offers_, [&line](const NpcShopOffer& on) { return on.item_id == line.item_id; });
    if (offer == buy_offers_.end() || offer->stock == kNpcShopUnlimitedStock) {
      continue;
    }
    offer->stock = std::max<std::int64_t>(offer->stock - line.amount, 0);
  }
  ++buy_seq_;
}

void NpcShop::TellPointShopResult(std::uint16_t result, std::uint32_t points,
                                  std::uint32_t secondary_points) {
  if (!Awaits(NpcShopKind::kPoints)) {
    return;
  }
  points_ = points;
  secondary_points_ = secondary_points;
  RecordOutcome(NpcShopSide::kBuy, result);
}

void NpcShop::Close() noexcept {
  if (!open()) {
    return;
  }
  farewell_due_ =
      kind_ == NpcShopKind::kMarket ? NpcShopFarewell::kMarketClose : NpcShopFarewell::kTradeQuit;
  kind_ = NpcShopKind::kNone;
  armed_ = false;
  // ⚠ UNE TRANSACTION PARTIE ATTEND TOUJOURS SON RÉSULTAT : le joueur qui ferme
  // aussitôt après avoir acheté doit quand même lire ce que le serveur répond.
}

void NpcShop::CloseForMapChange() noexcept { Close(); }

void NpcShop::CloseSilently() noexcept {
  kind_ = NpcShopKind::kNone;
  armed_ = false;
  awaiting_result_ = false;
  farewell_due_ = NpcShopFarewell::kNone;
}

NpcShopFarewell NpcShop::TakeFarewell() noexcept {
  const NpcShopFarewell due = farewell_due_;
  farewell_due_ = NpcShopFarewell::kNone;
  return due;
}

}  // namespace ro::sim
