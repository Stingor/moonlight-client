#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "ro/sim/buying_store.h"
#include "ro/sim/inventory.h"

namespace ro::sim {

// CE QUE LE SERVEUR REFUSERAIT, DIT AVANT D'ÉMETTRE.
//
// 🔴 LE SERVEUR REFUSE UNE ÉCHOPPE ENTIÈRE POUR UNE LIGNE, ET SANS DIRE LAQUELLE.
// Son refus d'ouverture n'a qu'un code pour une douzaine de causes, et celui
// d'une vente n'en a qu'un autre pour autant. Ces fonctions nomment la cause, et
// la ligne : la fenêtre les traduit et désigne ce qui cloche.
//
// Elles sont PURES : ce qu'on vérifie arrive en arguments, rien n'est lu
// ailleurs. ⚠ ELLES NE VOIENT PAS TOUT. Le serveur refuse aussi un objet que sa
// base ne marque pas comme achetable en échoppe (le drapeau `BuyingStore` de
// l'item_db), un objet non échangeable, et une surcharge de poids ; le client ne
// connaît ni ce drapeau ni les poids. Ce refus-là revient du serveur.

// Ce qui ne va pas dans une échoppe en préparation.
enum class BuyingStoreDraftFault : std::uint8_t {
  kNoLine,
  kTooManyLines,
  kEmptyTitle,
  // Le même objet sur deux lignes : le serveur rejette la seconde.
  kDuplicateItem,
  kZeroAmount,
  kZeroPrice,
  kPriceAboveMost,
  // Le serveur exige d'en posséder au moins UN : c'est la pile qu'il complète.
  kNotInBag,
  // Ce qu'on a déjà, plus ce qu'on veut, dépasse `kBuyingStoreMostOwned`.
  kOwnedAboveMost,
  kLimitNotPositive,
  kLimitAboveZeny,
  // ⚠ PAS UN REFUS : la limite ne paie pas tout ce qui est demandé. Le serveur
  // l'accepte, et l'échoppe s'arrêtera quand la limite sera épuisée — ce que le
  // joueur veut peut-être, et doit en tout cas savoir.
  kLimitBelowTotalCost,
};

// Ce qui ne va pas dans une vente à une échoppe visitée.
enum class BuyingStoreSaleFault : std::uint8_t {
  kNoLine,
  // La trame dépasserait ce que le client consent à émettre.
  kTooManyLines,
  // La même case sur deux lignes : le serveur rejette la vente entière.
  kDuplicateSlot,
  kZeroAmount,
  // La case est vide, porte un autre objet, ou pas assez d'exemplaires.
  kNotInBag,
  // L'acheteur ne veut pas cet objet, ou n'en veut plus.
  kNotWanted,
  // Plus que ce qu'il en veut encore, toutes lignes de l'objet comptées.
  kAboveWanted,
  // Le serveur refuse tout objet dont UNE case de carte est remplie — forgé et
  // signé compris : les quatre nombres sont les mêmes.
  kCarded,
  kBound,
  kRented,
  // Une pièce portée : le serveur la retirerait du corps pour la vendre.
  kEquipped,
  // Le total dépasse ce que l'acheteur peut encore dépenser.
  kAboveBuyerLimit,
  // Mon zeny, plus le total, dépasserait ce que la bourse tient.
  kAboveMyPurse,
};

// La ligne en cause, quand une ligne l'est.
inline constexpr std::size_t kNoFaultLine = static_cast<std::size_t>(-1);

struct BuyingStoreDraftProblem {
  BuyingStoreDraftFault fault = BuyingStoreDraftFault::kNoLine;
  std::size_t line = kNoFaultLine;
};

struct BuyingStoreSaleProblem {
  BuyingStoreSaleFault fault = BuyingStoreSaleFault::kNoLine;
  std::size_t line = kNoFaultLine;
};

// Vrai si le serveur refuserait l'ouverture pour cette raison. Faux pour un
// avertissement.
[[nodiscard]] bool BlocksOpening(BuyingStoreDraftFault fault) noexcept;

// Le plus de lignes qu'une vente puisse porter : le client refuse d'émettre une
// trame de plus de 0x800 octets, et celle-ci a douze octets d'en-tête puis huit
// par ligne. ⚠ LA COUCHE RÉSEAU REFAIT LE COMPTE sur la disposition réelle, et
// ses essais vérifient que les deux concordent.
inline constexpr std::size_t kBuyingStoreSaleMostFrameBytes = 0x800;
inline constexpr std::size_t kBuyingStoreSaleHeaderBytes = 12;
inline constexpr std::size_t kBuyingStoreSaleLineBytes = 8;
inline constexpr std::size_t kBuyingStoreMostSaleLines =
    (kBuyingStoreSaleMostFrameBytes - kBuyingStoreSaleHeaderBytes) / kBuyingStoreSaleLineBytes;

// Ce que coûterait l'échoppe si tout était acheté.
[[nodiscard]] std::int64_t BuyingStoreDraftCost(std::span<const BuyingStoreWish> lines) noexcept;

// Tous les problèmes d'une échoppe en préparation, dans l'ordre des lignes.
// Vide : le client n'a rien à redire.
//
// `title_on_wire` est le titre tel qu'il partira ; `zeny` est ce que le joueur
// possède ; `bag` est son sac.
[[nodiscard]] std::vector<BuyingStoreDraftProblem> CheckBuyingStoreDraft(
    std::string_view title_on_wire, std::int64_t zeny_limit, std::span<const BuyingStoreWish> lines,
    std::size_t slots, std::span<const InventoryItem> bag, std::int64_t zeny);

// Ce que rapporterait cette vente, aux prix de l'acheteur. Une ligne d'un objet
// qu'il ne veut pas compte pour rien.
[[nodiscard]] std::int64_t BuyingStoreSaleTotal(
    const VisitedBuyingStore& store, std::span<const BuyingStoreSaleLine> lines) noexcept;

// Tous les problèmes d'une vente. `zeny` est ce que le joueur possède.
[[nodiscard]] std::vector<BuyingStoreSaleProblem> CheckBuyingStoreSale(
    const VisitedBuyingStore& store, std::span<const BuyingStoreSaleLine> lines,
    std::span<const InventoryItem> bag, std::int64_t zeny);

}  // namespace ro::sim
