#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "ro/sim/buying_store.h"
#include "ro/sim/inventory.h"
#include "ro/sim/player_status.h"

namespace ro::sim {

// CE QUE LE SERVEUR REFUSERAIT, DIT AVANT D'ÉMETTRE.
//
// 🔴 LE SERVEUR REFUSE UNE ÉCHOPPE ENTIÈRE POUR UNE LIGNE, ET SANS DIRE LAQUELLE.
// Son refus d'ouverture n'a qu'un code pour une douzaine de causes, et celui
// d'une vente n'en a qu'un autre pour autant. Ces fonctions nomment la cause, et
// la ligne : la fenêtre les traduit et désigne ce qui cloche.
//
// Elles sont PURES : ce qu'on vérifie arrive en arguments, rien n'est lu
// ailleurs. ⚠ ELLES NE VOIENT QUE CE QU'ON LEUR DONNE. L'objet que la base du
// serveur n'accepte pas en échoppe et la surcharge à l'ouverture ne se jugent
// que si l'appelant tient la table des propriétés d'item ; sans elle, ce refus
// revient du serveur. ⚠ LA SURCHARGE DE L'ACHETEUR, à la vente, revient
// toujours du serveur : le client ne connaît pas le poids d'un autre joueur.

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
  // La base du serveur ne l'accepte pas en échoppe d'achat : drapeau
  // `BuyingStore` baissé, ou objet qui ne s'échange pas.
  kNotForBuyingStore,
  // Tout acheter mènerait le poids porté à la surcharge que le serveur refuse.
  kOverweight,
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

// LE SEUIL DE SURCHARGE À L'OUVERTURE, en pour cent du poids maximal : le
// serveur refuse dès que le poids porté, plus tout ce que l'échoppe achèterait,
// l'atteint (`major_overweight_rate` de sa configuration de combat).
inline constexpr std::int64_t kBuyingStoreOverweightPercent = 90;
inline constexpr std::int64_t kWeightPercentScale = 100;

// CE QUE LA BASE DU SERVEUR DIT D'UN OBJET MIS EN ACHAT, quand le client la
// connaît. Chaque champ vide veut dire « on ne sait pas ».
struct BuyingStoreItemFacts {
  std::optional<bool> allowed;
  // Le poids d'une unité, en dixièmes.
  std::optional<std::uint32_t> unit_weight;
};

// Le poids que porte le personnage et son maximum, en dixièmes ;
// `kUnknownStat` tant que le serveur ne les a pas dits.
struct CarriedWeight {
  std::int64_t weight = kUnknownStat;
  std::int64_t max_weight = kUnknownStat;
};

// Ce que coûterait l'échoppe si tout était acheté.
[[nodiscard]] std::int64_t BuyingStoreDraftCost(std::span<const BuyingStoreWish> lines) noexcept;

// Tous les problèmes d'une échoppe en préparation, dans l'ordre des lignes.
// Vide : le client n'a rien à redire.
//
// `title_on_wire` est le titre tel qu'il partira ; `zeny` est ce que le joueur
// possède ; `bag` est son sac. `facts` dit, ligne à ligne, ce que la base du
// serveur sait de l'objet : vide, ou d'une autre longueur que `lines`, rien
// n'en est jugé. La surcharge ne se juge que si chaque ligne a son poids et
// que `carried` est connu.
[[nodiscard]] std::vector<BuyingStoreDraftProblem> CheckBuyingStoreDraft(
    std::string_view title_on_wire, std::int64_t zeny_limit, std::span<const BuyingStoreWish> lines,
    std::size_t slots, std::span<const InventoryItem> bag, std::int64_t zeny,
    std::span<const BuyingStoreItemFacts> facts = {}, CarriedWeight carried = {});

// Ce que rapporterait cette vente, aux prix de l'acheteur. Une ligne d'un objet
// qu'il ne veut pas compte pour rien.
[[nodiscard]] std::int64_t BuyingStoreSaleTotal(
    const VisitedBuyingStore& store, std::span<const BuyingStoreSaleLine> lines) noexcept;

// Tous les problèmes d'une vente. `zeny` est ce que le joueur possède.
[[nodiscard]] std::vector<BuyingStoreSaleProblem> CheckBuyingStoreSale(
    const VisitedBuyingStore& store, std::span<const BuyingStoreSaleLine> lines,
    std::span<const InventoryItem> bag, std::int64_t zeny);

}  // namespace ro::sim
