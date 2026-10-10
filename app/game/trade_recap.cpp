// Les récapitulatifs de l'ouverture d'une échoppe. Voir l'en-tête.

#include "game/trade_recap.h"

#include <string_view>
#include <utility>

#include "ro/ui/inventory_window.h"
#include "ro/vfs/trade_words.h"

namespace moonclient {
namespace {

using ro::vfs::TradeWord;

// Au-delà de ce prix unitaire, la commission annoncée s'applique.
constexpr std::int64_t kCommissionFreeUpToZeny = 10'000'000;
// Le taux annoncé, en pour cent.
constexpr std::int64_t kCommissionPercent = 5;
constexpr std::int64_t kPercentBase = 100;

// Le trait qui ferme le récapitulatif. Il n'appartient à aucune langue.
constexpr std::string_view kRecapClosingRule =
    "============================ ============================";

void Push(std::vector<VendingChatLine>& lines, std::string text, std::uint32_t color) {
  if (!text.empty()) {
    lines.push_back({std::move(text), color});
  }
}

void PushCommission(std::vector<VendingChatLine>& lines, const ro::vfs::ItemWords& words,
                    std::int64_t unit_price_zeny) {
  Push(lines,
       ro::vfs::TradeTextWith(words, TradeWord::kRecapCommission,
                              ro::ui::GroupThousands(AnnouncedCommission(unit_price_zeny))),
       kRecapItemColor);
}

// La fin commune : le zeny du joueur, puis le trait.
void PushClosing(std::vector<VendingChatLine>& lines, const ro::vfs::ItemWords& words,
                 std::int64_t zeny) {
  Push(lines,
       ro::vfs::TradeTextWith(words, TradeWord::kRecapCurrentZeny, ro::ui::GroupThousands(zeny)),
       kRecapZenyColor);
  Push(lines, std::string(kRecapClosingRule), kRecapFrameColor);
}

}  // namespace

std::int64_t AnnouncedCommission(std::int64_t unit_price_zeny) {
  if (unit_price_zeny <= kCommissionFreeUpToZeny) {
    return 0;
  }
  return unit_price_zeny * kCommissionPercent / kPercentBase;
}

std::vector<VendingChatLine> SaleListRecap(const ro::vfs::ItemWords& words,
                                           std::span<const TradeRecapItem> items,
                                           std::int64_t zeny) {
  std::vector<VendingChatLine> lines;
  Push(lines, std::string(ro::vfs::TradeText(words, TradeWord::kRecapSaleTitle)), kRecapFrameColor);
  for (const TradeRecapItem& item : items) {
    Push(lines,
         ro::vfs::TradeTextWith(words, TradeWord::kRecapSaleLine, item.name,
                                ro::ui::GroupThousands(item.unit_price_zeny)),
         kRecapItemColor);
    PushCommission(lines, words, item.unit_price_zeny);
  }
  PushClosing(lines, words, zeny);
  return lines;
}

std::vector<VendingChatLine> BuyListRecap(const ro::vfs::ItemWords& words,
                                          std::span<const TradeRecapItem> items,
                                          std::int64_t zeny) {
  std::vector<VendingChatLine> lines;
  Push(lines, std::string(ro::vfs::TradeText(words, TradeWord::kRecapBuyTitle)), kRecapFrameColor);
  for (const TradeRecapItem& item : items) {
    Push(lines,
         ro::vfs::TradeTextWith(words, TradeWord::kRecapBuyLine, item.name,
                                ro::ui::GroupThousands(item.unit_price_zeny),
                                ro::ui::GroupThousands(item.amount)),
         kRecapItemColor);
    PushCommission(lines, words, item.unit_price_zeny);
  }
  PushClosing(lines, words, zeny);
  return lines;
}

bool SaleListRecapDue(const ro::sim::Vending& vending, std::uint32_t said_seq) {
  return vending.opening_result_seq() != said_seq &&
         vending.opening_result() == ro::sim::kVendingOpenSucceeded && vending.shop_open() &&
         !vending.shop_title().empty();
}

bool BuyListRecapDue(const ro::sim::BuyingStore& store, std::uint32_t said_seq) {
  return store.opening_seq() != said_seq && store.open() && !store.title().empty();
}

}  // namespace moonclient
