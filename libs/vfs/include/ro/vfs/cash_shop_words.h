#pragma once

#include <string>
#include <string_view>

#include "ro/vfs/item_words.h"

namespace ro::vfs {

// LES MOTS DE LA BOUTIQUE DE VOTE : des textes de l'interface, posés par
// `FillCashShopWords` (ADR 0090).

[[nodiscard]] std::string_view CashShopName(const ItemWords& words);
[[nodiscard]] std::string_view CashShopCurrency(const ItemWords& words);
[[nodiscard]] std::string_view CashShopEventPoints(const ItemWords& words);
[[nodiscard]] std::string_view CashShopUseEventPoints(const ItemWords& words);

// LE SENS D'UN REFUS D'ACHAT, et rien de son numéro.
//
// 🔴 CE N'EST PAS UN CODE DU PROTOCOLE. Plusieurs codes partagent un même
// message — sept d'entre eux tombent sur « l'échange a échoué » —, et la
// correspondance appartient à qui décode le paquet : `vfs` ne sait que nommer
// les textes.
enum class CashShopRefusal {
  kDealFailed,
  kSomeFailed,
  kRuneOvercount,
  kEachItemOvercount,
  kUnknown,
  kBusy,
  kBillingExpired,
  kBillingError,
};

// Le texte d'un refus.
[[nodiscard]] std::string_view CashShopRefusalText(const ItemWords& words, CashShopRefusal refusal);

// Pose ces mots dans la langue courante.
void FillCashShopWords(CashShopWords& words);

}  // namespace ro::vfs
