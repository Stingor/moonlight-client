// Les mots de la boutique de vote. Voir l'en-tête.

#include "ro/vfs/cash_shop_words.h"

#include "ro/core/i18n.h"

namespace ro::vfs {

std::string_view CashShopName(const ItemWords& words) { return words.cash_shop.shop_name; }

std::string_view CashShopCurrency(const ItemWords& words) { return words.cash_shop.currency; }

std::string_view CashShopEventPoints(const ItemWords& words) {
  return words.cash_shop.event_points;
}

std::string_view CashShopUseEventPoints(const ItemWords& words) {
  return words.cash_shop.use_event_points;
}

std::string_view CashShopRefusalText(const ItemWords& words, CashShopRefusal refusal) {
  const CashShopWords& said = words.cash_shop;
  switch (refusal) {
    case CashShopRefusal::kDealFailed:
      return said.deal_failed;
    case CashShopRefusal::kSomeFailed:
      return said.some_failed;
    case CashShopRefusal::kRuneOvercount:
      return said.rune_overcount;
    case CashShopRefusal::kEachItemOvercount:
      return said.each_item_overcount;
    case CashShopRefusal::kUnknown:
      return said.unknown_error;
    case CashShopRefusal::kBusy:
      return said.busy;
    case CashShopRefusal::kBillingExpired:
      return said.billing_expired;
    case CashShopRefusal::kBillingError:
      return said.billing_error;
  }
  return {};
}

void FillCashShopWords(CashShopWords& words) {
  // CE SERVEUR APPELLE SA MONNAIE « VOTE », et sa boutique « Vote Shop ».
  words.shop_name = core::Tr("Vote Shop");
  words.currency = core::Tr("Vote");
  words.event_points = core::Tr("Points événement");
  words.use_event_points = core::Tr("Utiliser pts év.");
  words.deal_failed = core::Tr("L'échange a échoué.");
  words.some_failed = core::Tr("Certains items n'ont pas pu être achetés.");
  words.rune_overcount = core::Tr(
      "Achat de runes échoué : le nombre maximal d'items pouvant être détenus est dépassé.");
  words.each_item_overcount = core::Tr("Achat échoué : nombre maximal atteint pour cet item.");
  words.unknown_error = core::Tr("Achat échoué : erreur inconnue.");
  words.busy = core::Tr("Veuillez réessayer plus tard.");
  words.billing_expired =
      core::Tr("Votre session de facturation a expiré. Veuillez relancer le client");
  words.billing_error = core::Tr("Une erreur est survenue. Veuillez réessayer");
}

}  // namespace ro::vfs
