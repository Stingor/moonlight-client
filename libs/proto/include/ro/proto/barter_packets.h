#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// ── LE TROC ÉTENDU : la liste que le générateur ne sait pas lire ─────────────
//
// 🔴 CHAQUE ARTICLE PORTE UN NOMBRE VARIABLE DE DEVISES. Le serveur déclare la
// structure avec UNE devise fixe (`currencies[1]`, faute de pouvoir imbriquer
// deux tableaux ouverts), puis écrit l'article SANS cette devise-là et enchaîne
// `currency_count` devises de douze octets (`clif_barter_extended_open`). Lue
// au mot, la structure donnerait un pas fixe de quarante-huit octets, qui ne
// tombe juste que si CHAQUE article a exactement une devise : zéro devise, ou
// deux, et toute la suite de la liste serait décalée. Le générateur écarte donc
// ce paquet (`tools/packetgen/refus.csv`), et on le lit ici article par article.
//
// Les cinq autres paquets du troc — la liste simple, les deux achats, les deux
// fermetures — ont un pas fixe et sont engendrés dans `map_packets.h`.

// Une devise d'un article : ce qu'il faut rendre pour l'obtenir.
struct ExpandedBarterCurrency {
  static constexpr std::size_t kWireSize = 12;

  // ⚠ L'IDENTIFIANT BRUT de la base, pas l'apparence que le serveur substitue
  // ailleurs (`client_nameid`) : c'est le seul champ d'objet de la liste qu'il
  // écrit tel quel.
  std::uint32_t item_id = 0;
  // Le raffinement exigé. ⚠ ZÉRO VEUT DIRE « QUELCONQUE » OU « EXACTEMENT +0 » :
  // le serveur écrit zéro pour les deux, et le fil ne les sépare plus.
  std::uint16_t refine_level = 0;
  std::uint32_t amount = 0;
  // La famille, repliée comme partout (`itemtype()`).
  std::uint16_t type = 0;
};

// Un article de la liste étendue.
struct ExpandedBarterItem {
  // La partie fixe, sans les devises.
  static constexpr std::size_t kFixedWireSize = 36;

  std::uint32_t item_id = 0;
  std::uint16_t type = 0;
  // Le stock. 0xFFFFFFFF : illimité.
  std::uint32_t stock = 0;
  // En dixièmes d'unité, comme le serveur compte les poids.
  std::uint32_t weight = 0;
  // L'index de l'article DANS LA BOUTIQUE : c'est lui que l'achat renvoie.
  std::uint32_t shop_index = 0;
  // Le prix en zeny, EN PLUS des devises.
  std::uint32_t zeny = 0;
  std::uint16_t view_sprite = 0;
  std::uint32_t location = 0;
  // Le raffinement de l'objet livré.
  std::uint32_t refine_level = 0;
  std::vector<ExpandedBarterCurrency> currencies;
};

// `ZC_NPC_EXPANDED_BARTER_MARKET_ITEMINFO`.
struct ExpandedBarterItemList {
  static constexpr std::uint16_t kOpcode = 0x0B79;
  static constexpr std::size_t kHeaderSize = 8;

  // Le compte que le serveur annonce. ⚠ INDICATIF : c'est le parcours de la
  // trame jusqu'à son dernier octet qui dit ce qu'elle porte.
  std::int32_t announced_count = 0;
  std::vector<ExpandedBarterItem> items;
};

// ⚠ REFUSE une trame dont la longueur annoncée n'est pas la sienne, et une
// trame dont le dernier article ne tombe pas EXACTEMENT sur sa fin : un reste,
// ou un compte de devises qui déborde, dit une disposition qui a changé.
[[nodiscard]] core::Result<ExpandedBarterItemList> DecodeExpandedBarterItemList(
    std::span<const std::uint8_t> frame);

// L'ENVERS DU DÉCODEUR, pour les essais : le client n'émet pas ce paquet.
[[nodiscard]] std::vector<std::uint8_t> EncodeExpandedBarterItemList(
    const ExpandedBarterItemList& list);

}  // namespace ro::proto
