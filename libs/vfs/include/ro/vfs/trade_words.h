#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace ro::vfs {

struct ItemWords;

// LES MOTS DES COMMERCES : boutiques de PNJ, échoppes de vente et d'achat, troc.
// Des textes de l'interface, posés par `FillTradeWords` (ADR 0090). Le numéro
// de protocole qui déclenche chacun appartient à qui décode le paquet — voir
// `docs/commerces-messages.md` pour la correspondance relevée sur le client
// d'origine.
enum class TradeWord : std::uint8_t {
  // ── Résultats communs ────────────────────────────────────────────────────
  kDealSucceeded,
  kLacksZeny,
  kOverWeight,
  kDealFailed,
  kOverCapacity,
  kEachItemOverCount,
  kSentByRodex,
  kPurchaseFailed,
  kLimitBuyItem,   // un %d
  kLimitSellItem,  // un %d
  kEquipOnlyOne,
  kInventoryKindsFull,
  // ── Boutique à points ────────────────────────────────────────────────────
  kShopItems,
  kItemsYouCanSell,
  kPointShopPoints,  // %s puis %d
  kPointShopEvent,   // %s puis %d
  kPointFailNpc,
  kPointFailSystem,
  kPointFailTrading,
  kPointFailItem,
  kPointFailMoney,
  kPointFailTotal,
  kPointFailSome,
  // ── Échoppe de vente ─────────────────────────────────────────────────────
  kShopMaker,  // « Opening a stall »
  kMyShop,
  kMerchantShop,
  kSoldItemLog,
  kShopMakingFailed,
  kShopBadPosition,
  kShopOverZeny,
  kFreeItemOnShop,
  kNoChatWhileVending,
  kItemSold,  // %s puis %d
  kItemsSoldOut,
  kBuyFromMerchantFailed,  // %s puis %s
  kMerchantOutOfStock,     // %s puis %d
  kMerchantTrading,
  kMerchantBadStore,
  kNoSaleInformation,
  kSellerZenyFailed,
  kCanBuyUpTo,  // un %d
  kRestrictedStore,
  // ── Échoppe d'achat ──────────────────────────────────────────────────────
  kBuyingStoreTitle,
  kBuyingStoreLimit,
  kBuyingStoreList,
  kBuyingStoreWanted,
  kBuyingStoreOpenFailed,
  kBuyingStoreOverweight,
  kBuyingStoreLimitReached,
  kBuyingStoreAllBought,
  kBuyingStoreBuyerBroke,
  kBuyingStoreSold,       // %s, %d, %d
  kBuyingStoreOverCount,  // un %s
  kBuyingStoreSellLimit,  // un %d
  kBuyingStoreBuyerLacks,
  kZenyOutOfRange,
  // ── Troc ─────────────────────────────────────────────────────────────────
  kExchangeFailed,
  kExchangeSucceeded,
  kExchangeOutOfStock,
  kExchangeLacksGoods,
  kBarterList,
  kBarterNameCount,  // %s puis %d
  kBarterEquipped,
  kBuyOneAtATime,
  // ── Échange entre joueurs ───────────────────────────────────────────────
  kDealRejected,
  kDealTooFar,
  kDealNoCharacter,
  kDealPartnerBusy,
  kDealPartnerOverWeight,
  kDealCancelled,
  kDealDone,
  kDealNotDone,
  kDealProposedSuffix,  // se colle au nom du proposant
  kDealTenKinds,
  kDealHoldsItems,
  // ── Recherche d'échoppes ────────────────────────────────────────────────
  kSearchTitle,
  kSearchSell,
  kSearchBuy,
  kSearchShopName,
  kSearchItem,
  kSearchCount,
  kSearchPrice,
  kSearchSimilar,
  kSearchCardHint,
  kSearchUsesLeft,  // un %d
  kSearchOpenStore,
  kSearchTooMany,
  kSearchNoUsesLeft,
  kSearchTooSoon,
  kSearchNothingFound,
  kSearchUnknownItem,
  kSearchBadPrice,
  kSearchNoItemName,
  kSearchUnknownCard,
  kSearchPreviousPage,
  kSearchNextPage,
  // ── Récapitulatifs, à l'ouverture de mon échoppe ────────────────────────
  kRecapSaleTitle,         // bandeau compris
  kRecapBuyTitle,          // bandeau compris
  kRecapSaleLine,          // %s puis %s, retrait compris
  kRecapBuyLine,           // %s, %s, %s
  kRecapCommissionNone,    // la taxe des échoppes de vente est nulle
  kRecapCommissionAlways,  // un %s : le taux, en pour cent
  kRecapCommissionFrom,    // %s puis %s : le taux, puis le seuil d'un achat
  kRecapCurrentZeny,       // un %s, retrait compris
  kCount,
};

inline constexpr std::size_t kTradeWordCount = static_cast<std::size_t>(TradeWord::kCount);

// Les textes posés, rangés par `TradeWord`.
struct TradeWords {
  std::array<std::string, kTradeWordCount> texts;
};

// Le texte d'un mot, sans conversion. ⚠ UN GABARIT (« %s », « %d ») SE LIT PAR LES FONCTIONS
// CI-DESSOUS : le montrer tel quel écrirait ses conversions à l'écran.
[[nodiscard]] std::string_view TradeText(const ItemWords& words, TradeWord word);

// LES GABARITS, IMPRIMÉS.
//
// 🔴 ILS VIENNENT D'UN CATALOGUE DE LANGUE, qu'un joueur peut retoucher : un
// « %s » là où l'on attend un entier lirait un pointeur pris sur la pile. Chaque
// fonction vérifie la SUITE exacte des conversions — nombre, ordre, types — et
// rend un texte VIDE si elle diffère. Un « %% » ne compte pas ; toute autre
// conversion fait refuser le gabarit.
[[nodiscard]] std::string TradeTextWith(const ItemWords& words, TradeWord word,
                                        std::int64_t number);
[[nodiscard]] std::string TradeTextWith(const ItemWords& words, TradeWord word,
                                        std::string_view text);
[[nodiscard]] std::string TradeTextWith(const ItemWords& words, TradeWord word,
                                        std::string_view text, std::int64_t number);
[[nodiscard]] std::string TradeTextWith(const ItemWords& words, TradeWord word,
                                        std::string_view first, std::string_view second);
[[nodiscard]] std::string TradeTextWith(const ItemWords& words, TradeWord word,
                                        std::string_view text, std::int64_t first,
                                        std::int64_t second);
[[nodiscard]] std::string TradeTextWith(const ItemWords& words, TradeWord word,
                                        std::string_view first, std::string_view second,
                                        std::string_view third);

// La suite des conversions d'un gabarit, une lettre par conversion : « s » pour
// une chaîne, « d » pour un entier. Vide si une conversion n'est ni l'une ni
// l'autre — le gabarit est alors refusé en bloc.
[[nodiscard]] std::string TradeFormatShape(std::string_view format);

// Pose ces mots dans la langue courante.
void FillTradeWords(TradeWords& words);

}  // namespace ro::vfs
