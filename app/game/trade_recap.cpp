// Les récapitulatifs de l'ouverture d'une échoppe. Voir l'en-tête.

#include "game/trade_recap.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "ro/ui/inventory_window.h"
#include "ro/vfs/trade_words.h"

namespace moonclient {
namespace {

using ro::vfs::TradeWord;

// Dix-millièmes par pour cent, et chiffres après la virgule d'un pour cent.
constexpr int kPermyriadPerPercent = 100;
constexpr std::size_t kPercentFractionDigits = 2;

// Le trait qui ferme le récapitulatif. Il n'appartient à aucune langue.
constexpr std::string_view kRecapClosingRule =
    "============================ ============================";

void Push(std::vector<VendingChatLine>& lines, std::string text, std::uint32_t color) {
  if (!text.empty()) {
    lines.push_back({std::move(text), color});
  }
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

std::string VendingTaxPercent(int permyriad) {
  std::string percent = std::to_string(permyriad / kPermyriadPerPercent);
  std::string fraction = std::to_string(permyriad % kPermyriadPerPercent);
  fraction.insert(0, kPercentFractionDigits - fraction.size(), '0');
  while (!fraction.empty() && fraction.back() == '0') {
    fraction.pop_back();
  }
  if (!fraction.empty()) {
    percent += '.';
    percent += fraction;
  }
  return percent;
}

std::string VendingCommissionText(const ro::vfs::ItemWords& words, const VendingTax& tax) {
  if (tax.permyriad <= 0) {
    return std::string(ro::vfs::TradeText(words, TradeWord::kRecapCommissionNone));
  }
  const std::string percent = VendingTaxPercent(tax.permyriad);
  if (tax.from_zeny <= 0) {
    return ro::vfs::TradeTextWith(words, TradeWord::kRecapCommissionAlways, percent);
  }
  return ro::vfs::TradeTextWith(words, TradeWord::kRecapCommissionFrom, percent,
                                ro::ui::GroupThousands(tax.from_zeny));
}

std::vector<VendingChatLine> SaleListRecap(const ro::vfs::ItemWords& words,
                                           std::span<const TradeRecapItem> items, std::int64_t zeny,
                                           const VendingTax& tax) {
  std::vector<VendingChatLine> lines;
  Push(lines, std::string(ro::vfs::TradeText(words, TradeWord::kRecapSaleTitle)), kRecapFrameColor);
  for (const TradeRecapItem& item : items) {
    Push(lines,
         ro::vfs::TradeTextWith(words, TradeWord::kRecapSaleLine, item.name,
                                ro::ui::GroupThousands(item.unit_price_zeny)),
         kRecapItemColor);
  }
  Push(lines, VendingCommissionText(words, tax), kRecapItemColor);
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
