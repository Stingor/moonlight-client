#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// ── LA BOUTIQUE D'UN PNJ : les paquets que le serveur écrit champ par champ ──
//
// Les autres — le choix d'achat ou de vente, les deux listes, la vente, le
// market — sont décrits par une structure du serveur et engendrés dans
// `map_packets.h`. Ceux-ci n'en ont pas, ou leur structure ne porte pas son
// opcode : ils sont écrits ici, et recoupés avec ce que le serveur déclare lire.

// CE QUE LE CLIENT D'ORIGINE ACCEPTE D'ÉMETTRE EN UNE TRAME DE COMMERCE : il
// refuse d'envoyer une liste d'achat qui la dépasse. Le serveur, lui, ne borne
// que par la longueur sur seize bits ; garder la borne du client, c'est ne
// jamais lui envoyer ce qu'aucun client n'a jamais envoyé.
inline constexpr std::size_t kTradeFrameMostBytes = 0x800;

// UNE LIGNE D'ACHAT À UN PNJ : combien, et de quoi.
struct NpcBuyLine {
  std::uint16_t amount = 0;
  std::uint32_t item_id = 0;
};

// « J'achète tout cela. » `CZ_PC_PURCHASE_ITEMLIST`.
//
// 🔴 IL NE VAUT QUE POUR UNE BOUTIQUE ARMÉE. Le serveur oublie la boutique
// après CHAQUE achat (`npc_shopid = 0`, `clif_parse_NpcBuyListSend`) : un second
// achat sans nouveau choix d'achat échoue en « zeny insuffisant ». C'est à
// `net` de réarmer avant d'émettre.
struct NpcBuyRequest {
  static constexpr std::uint16_t kOpcode = 0x00C8;
  static constexpr std::size_t kHeaderSize = 4;
  static constexpr std::size_t kLineSize = 6;

  std::vector<NpcBuyLine> lines;
};

// « J'ai fermé la boutique. » `CZ_NPC_TRADE_QUIT`.
//
// 🔴 C'EST LUI QUI REND LA MARCHE. Tant que le serveur tient une boutique pour
// ouverte (`npc_shopid`), `pc_cant_act2` refuse tout déplacement. Fermer le
// DIALOGUE n'y touche pas : il termine le script, pas la boutique.
struct NpcTradeQuit {
  static constexpr std::uint16_t kOpcode = 0x09D4;
  static constexpr std::size_t kWireSize = 2;
};

// « J'ai fermé le market. » `CZ_NPC_MARKET_CLOSE`.
//
// 🔴 L'OUVERTURE D'UN MARKET ARME L'ÉTAT D'ÉCHANGE DU PERSONNAGE
// (`state.trading`), et seul ce paquet le désarme : sans lui, plus aucune
// boutique, aucun échange, aucun choix d'achat n'est accepté.
struct NpcMarketClose {
  static constexpr std::uint16_t kOpcode = 0x09D8;
  static constexpr std::size_t kWireSize = 2;
};

// « J'achète tout cela » à une boutique à points ou à objet-devise.
// `CZ_PC_BUY_CASH_POINT_ITEM`.
//
// ⚠ LE SERVEUR EXIGE QUE LA LONGUEUR ET LE COMPTE S'ACCORDENT au sizeof près
// (`clif_parse_npccashshop_buy`), et ignore sans réponse une trame qui ment.
struct PointShopBuyRequest {
  static constexpr std::uint16_t kOpcode = 0x0288;
  static constexpr std::size_t kHeaderSize = 10;
  static constexpr std::size_t kLineSize = 6;

  // Les points de la SECONDE monnaie à dépenser. Toujours zéro pour une
  // boutique à objet-devise ou à variable : elles n'en ont pas.
  std::uint32_t secondary_points = 0;
  std::vector<NpcBuyLine> lines;
};

// Le résultat d'une vente à un PNJ. `ZC_PC_SELL_RESULT` : zéro réussi, un échec.
struct NpcSellResult {
  static constexpr std::uint16_t kOpcode = 0x00CB;
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t result = 0;
};

// Le résultat d'un achat à une boutique à points : les soldes à jour, et le
// code. `ZC_PC_CASH_POINT_UPDATE`.
struct PointShopUpdate {
  static constexpr std::uint16_t kOpcode = 0x0289;
  static constexpr std::size_t kWireSize = 12;

  std::uint32_t points = 0;
  std::uint32_t secondary_points = 0;
  std::uint16_t result = 0;
};

// ⚠ REFUSENT une liste vide et une trame au-delà de `kTradeFrameMostBytes` :
// la première n'achète rien et le serveur la compte en échec, la seconde ne
// part jamais d'un client.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const NpcBuyRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const PointShopBuyRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const NpcTradeQuit& quit);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const NpcMarketClose& close);

// ⚠ REFUSENT une trame d'un autre opcode ou d'une autre longueur.
[[nodiscard]] core::Result<NpcSellResult> DecodeNpcSellResult(std::span<const std::uint8_t> frame);
[[nodiscard]] core::Result<PointShopUpdate> DecodePointShopUpdate(
    std::span<const std::uint8_t> frame);

// L'ENVERS DES DÉCODEURS, pour les essais : le client n'émet pas ces paquets.
[[nodiscard]] std::vector<std::uint8_t> EncodeNpcSellResult(const NpcSellResult& result);
[[nodiscard]] std::vector<std::uint8_t> EncodePointShopUpdate(const PointShopUpdate& update);

}  // namespace ro::proto
