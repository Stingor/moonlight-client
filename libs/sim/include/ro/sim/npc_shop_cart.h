#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ro/sim/inventory.h"
#include "ro/sim/npc_shop.h"

namespace ro::sim {

// LE PANIER D'UNE BOUTIQUE DE PNJ, VU PAR LE SERVEUR AVANT QU'IL LE VOIE.
//
// Le serveur revalide tout, et c'est lui qui tranche. Mais plusieurs de ses
// refus coûtent au joueur : un achat refusé part en entier, une vente dont UNE
// ligne est fausse échoue toute, un objet non empilable demandé à cinq est
// ramené à un SANS le dire, et une vente qui ferait déborder la bourse est
// payée TRONQUÉE — la différence est perdue. Ce qui se sait d'avance se dit
// d'avance : ces fonctions rendent les raisons, la fenêtre les montre et retient
// le bouton.
//
// ⚠ LE POIDS VIENT DE DEUX SOURCES : la liste d'un market porte celui d'une
// unité ; ailleurs, c'est la table des propriétés d'item, que l'appelant pose
// sur chaque ligne. Sans l'une ni l'autre, le serveur reste seul juge.

// Les sortes d'objets qui ne s'empilent pas : armes, armures, œufs de familier,
// équipements de familier, équipements d'ombre (`itemdb_isstackable2` du
// serveur, types `IT_WEAPON`, `IT_ARMOR`, `IT_PETEGG`, `IT_PETARMOR`,
// `IT_SHADOWGEAR`).
inline constexpr std::uint8_t kItemTypeWeapon = 4;
inline constexpr std::uint8_t kItemTypeArmor = 5;
inline constexpr std::uint8_t kItemTypePetEgg = 7;
inline constexpr std::uint8_t kItemTypePetArmor = 8;
inline constexpr std::uint8_t kItemTypeShadowGear = 12;

[[nodiscard]] bool ItemTypeStacks(std::uint8_t item_type) noexcept;

// Ce que la bourse peut tenir : `MAX_ZENY` du serveur. Au-delà, une vente est
// créditée TRONQUÉE (`npc_selllist`).
inline constexpr std::int64_t kMostZeny = 2147483647;

// UNE LIGNE DU PANIER D'ACHAT : l'objet, et combien.
struct NpcShopBuyLine {
  std::uint32_t item_id = 0;
  std::int64_t amount = 0;
  // Le poids d'une unité selon la table des propriétés d'item, en dixièmes ;
  // vide sans table. Celui que la liste d'un market porte l'emporte.
  std::optional<std::uint32_t> table_unit_weight;
};

// UNE LIGNE DU PANIER DE VENTE : la case du sac, et combien.
struct NpcShopSellLine {
  std::uint16_t client_slot = 0;
  std::int64_t amount = 0;
};

// Ce qui retiendrait le serveur.
enum class NpcShopIssue : std::uint8_t {
  // Rien au panier.
  kEmptyCart,
  // Une quantité nulle ou négative.
  kBadAmount,
  // Un objet que la boutique ne propose pas, ou une case qu'elle n'achète pas.
  kNotOffered,
  // Plus que ce que la bourse, ou les points, peuvent payer.
  kLacksMoney,
  // Plus que ce qui reste en rayon (market).
  kOverStock,
  // Un objet non empilable demandé à plus d'un : le serveur le ramènerait à
  // un en silence.
  kOnePerPurchase,
  // Ce qu'on a déjà, plus ce qu'on achète, dépasse une pile.
  kOverStack,
  // Plus lourd que ce que le personnage peut porter.
  kOverWeight,
  // Vendre plus qu'on n'en a.
  kOverOwned,
  // Une vente qui ferait déborder la bourse : la recette serait tronquée.
  kZenyOverflow,
};

struct NpcShopVerdict {
  std::vector<NpcShopIssue> issues;
  // Le total, en zeny ou en points.
  std::int64_t total = 0;
  // Le poids ajouté, en dixièmes ; valable seulement si `weight_known`.
  std::int64_t added_weight = 0;
  bool weight_known = false;

  [[nodiscard]] bool clear() const noexcept { return issues.empty(); }
  [[nodiscard]] bool Has(NpcShopIssue issue) const noexcept;
};

// Le panier d'achat, contre la boutique ouverte. `money` : les zeny, ou les
// points d'une boutique à points. `bag` : le sac, pour les piles. `weight` et
// `max_weight` : ceux du personnage, en dixièmes, `kUnknownStat` s'ils ne sont
// pas encore dits — le poids n'est alors pas jugé.
[[nodiscard]] NpcShopVerdict CheckNpcShopBuy(const NpcShop& shop,
                                             std::span<const NpcShopBuyLine> cart,
                                             std::int64_t money, std::span<const InventoryItem> bag,
                                             std::int64_t weight, std::int64_t max_weight);

// 🔴 CE QUE LA LISTE DE VENTE MONTRE d'une case que le serveur accepte : une
// pile non vide, et pas un favori quand le verrou de vente des favoris est
// posé — un réglage du joueur, que le serveur ignore. Les ajouts en bloc ne
// prennent que dans cette liste : un favori verrouillé n'entre au panier par
// aucun d'eux.
[[nodiscard]] bool NpcShopListsForSale(const InventoryItem& item, bool favorites_locked) noexcept;

// Le panier de vente, contre la liste que le serveur a envoyée et le sac.
// `zeny` : ce que la bourse porte déjà.
[[nodiscard]] NpcShopVerdict CheckNpcShopSell(const NpcShop& shop,
                                              std::span<const NpcShopSellLine> cart,
                                              std::span<const InventoryItem> bag,
                                              std::int64_t zeny);

}  // namespace ro::sim
