#include "game/vending_said.h"

#include <array>
#include <ctime>

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::vfs::TradeWord;

// Les codes de refus d'achat que le client d'origine lit et que `sim` ne nomme
// pas : le vendeur qui ne peut pas encaisser, et l'échoppe d'achat qu'on
// refuse.
constexpr std::uint8_t kVendingBuySellerZeny = 8;
constexpr std::uint8_t kVendingBuyRestricted = 15;

// « L'achat de %s a échoué : %s », la raison étant un autre mot du jeu.
VendingChatLine FailedBecause(const ro::vfs::ItemWords& words, std::string_view item_name,
                              TradeWord reason) {
  if (item_name.empty()) {
    return {};
  }
  return {ro::vfs::TradeTextWith(words, TradeWord::kBuyFromMerchantFailed, item_name,
                                 ro::vfs::TradeText(words, reason)),
          kVendingLightRedColor};
}

VendingChatLine Plain(const ro::vfs::ItemWords& words, TradeWord word, std::uint32_t color) {
  return {std::string(ro::vfs::TradeText(words, word)), color};
}

}  // namespace

VendingChatLine SaidOfPurchaseRefusal(const ro::vfs::ItemWords& words, std::uint8_t result,
                                      std::string_view item_name, std::int64_t amount) {
  switch (result) {
    case ro::sim::kVendingBuyNoZeny:
      return FailedBecause(words, item_name, TradeWord::kLacksZeny);
    case ro::sim::kVendingBuyOverweight:
      return FailedBecause(words, item_name, TradeWord::kOverWeight);
    case kVendingBuySellerZeny:
      return FailedBecause(words, item_name, TradeWord::kSellerZenyFailed);
    case ro::sim::kVendingBuyOutOfStock:
      if (item_name.empty()) {
        return {};
      }
      return {ro::vfs::TradeTextWith(words, TradeWord::kMerchantOutOfStock, item_name, amount),
              kVendingLightRedColor};
    case ro::sim::kVendingBuyTrading:
      return Plain(words, TradeWord::kMerchantTrading, kVendingLightRedColor);
    case ro::sim::kVendingBuyStoreChanged:
      return Plain(words, TradeWord::kMerchantBadStore, kVendingLightRedColor);
    case ro::sim::kVendingBuyNoSaleInfo:
      return Plain(words, TradeWord::kNoSaleInformation, kVendingFailedColor);
    case kVendingBuyRestricted:
      return Plain(words, TradeWord::kRestrictedStore, kVendingFailedColor);
    default:
      return {};
  }
}

VendingOpeningNews SaidOfOpeningResult(const ro::vfs::ItemWords& words, std::uint8_t result) {
  switch (result) {
    case ro::sim::kVendingOpenFailed:
      return {VendingOpeningSaid::kNotice,
              std::string(ro::vfs::TradeText(words, TradeWord::kShopMakingFailed))};
    case ro::sim::kVendingOpenForbiddenPosition:
      return {VendingOpeningSaid::kLine,
              std::string(ro::vfs::TradeText(words, TradeWord::kShopBadPosition))};
    default:
      return {};
  }
}

VendingChatLine SaidOfSale(const ro::vfs::ItemWords& words, std::string_view item_name,
                           std::int64_t amount) {
  return {ro::vfs::TradeTextWith(words, TradeWord::kItemSold, item_name, amount),
          kVendingSoldColor};
}

const char* VendingSetupFaultText(ro::sim::VendingSetupFault fault) {
  using ro::sim::VendingSetupFault;
  switch (fault) {
    case VendingSetupFault::kNoTitle:
      return ro::core::Tr("Donne un nom au shop.");
    case VendingSetupFault::kNoLines:
      return ro::core::Tr("Pose au moins un item.");
    case VendingSetupFault::kTooManyLines:
      return ro::core::Tr("Plus de lignes que le skill n'en permet.");
    case VendingSetupFault::kDuplicateSlot:
      return ro::core::Tr("Deux lignes pour la même pile.");
    case VendingSetupFault::kZeroPrice:
      return ro::core::Tr("Un item est à prix 0 : il serait donné.");
    case VendingSetupFault::kPriceAboveMost:
      return ro::core::Tr("Prix maximal : 1,000,000,000z par item.");
    case VendingSetupFault::kZeroAmount:
      return ro::core::Tr("Une ligne est à quantité 0.");
    case VendingSetupFault::kNotInCart:
      return ro::core::Tr("Un item n'est plus dans le cart.");
    case VendingSetupFault::kAmountAboveStack:
      return ro::core::Tr("Plus que la pile du cart.");
    case VendingSetupFault::kUnidentified:
      return ro::core::Tr("Un item n'est pas identifié : le serveur refuserait tout le shop.");
    case VendingSetupFault::kDamaged:
      return ro::core::Tr("Un item est abîmé : le serveur refuserait tout le shop.");
    case VendingSetupFault::kRented:
      return ro::core::Tr("Un item est loué : le serveur refuserait tout le shop.");
    case VendingSetupFault::kTakingsOverflow:
      return ro::core::Tr(
          "Zeny + recette dépasseraient 2,147,483,647z : le serveur refuserait le shop.");
  }
  return "";
}

const char* VendingBasketFaultText(ro::sim::VendingBasketFault fault) {
  using ro::sim::VendingBasketFault;
  switch (fault) {
    case VendingBasketFault::kEmpty:
      return ro::core::Tr("Le panier est vide.");
    case VendingBasketFault::kTooManyLines:
      return ro::core::Tr("Plus de 12 lignes : le serveur ignorerait l'achat.");
    case VendingBasketFault::kZeroAmount:
      return ro::core::Tr("Une quantité est nulle.");
    case VendingBasketFault::kUnknownOffer:
      return ro::core::Tr("Un item n'est plus en vente.");
    case VendingBasketFault::kAmountAboveStock:
      return ro::core::Tr("Plus que le stock du vendeur.");
    case VendingBasketFault::kTotalAboveZeny:
      return ro::core::Tr("Zeny insuffisant.");
  }
  return "";
}

std::string VendingSoldAtText(std::uint32_t sold_at_unix_s) {
  const auto stamp = static_cast<std::time_t>(sold_at_unix_s);
  std::tm local{};
#ifdef _WIN32
  if (localtime_s(&local, &stamp) != 0) {
    return "-";
  }
#else
  if (localtime_r(&stamp, &local) == nullptr) {
    return "-";
  }
#endif
  std::array<char, 32> text{};
  if (std::strftime(text.data(), text.size(), "%d/%m %H:%M:%S", &local) == 0) {
    return "-";
  }
  return text.data();
}

}  // namespace moonclient
