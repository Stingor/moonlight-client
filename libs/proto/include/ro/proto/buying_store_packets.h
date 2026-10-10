#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// ── L'ÉCHOPPE D'ACHAT : les paquets que le serveur n'a pas décrits ────────────
//
// La liste de mon échoppe, celle d'une échoppe visitée, un achat conclu, le
// refus d'une vente et les deux enseignes sont décrits par le serveur et
// engendrés dans `map_packets.h`. Ceux-ci n'y figurent pas, ou sans leur opcode :
// le serveur les écrit et les lit champ par champ (`WFIFOW`, `RFIFOL`). Ils sont
// donc écrits ici, et les demandes sont recoupées avec ce que sa table déclare
// lire.

// ── REÇUS ────────────────────────────────────────────────────────────────────

// « Prépare ton échoppe » : la réponse du serveur à la compétence ou à l'objet
// qui l'ouvre, avec le nombre de lignes permises (`clif_buyingstore_open`).
struct BuyingStorePreparation {
  static constexpr std::uint16_t kOpcode = 0x0810;
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t slots = 0;
};

// « Ton échoppe n'a pas ouvert » (`clif_buyingstore_open_failed`). Le poids n'a
// de sens que pour le refus de surcharge : c'est le poids PROJETÉ, objets voulus
// compris, en dixièmes comme tous les poids du serveur.
struct BuyingStoreOpenFailure {
  static constexpr std::uint16_t kOpcode = 0x0812;
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t result = 0;
  std::uint32_t weight = 0;
};

// « Ton échoppe s'arrête » : tout est acheté, ou la limite de zeny est épuisée
// (`clif_buyingstore_trade_failed_buyer`). ⚠ LE SERVEUR L'A DÉJÀ FERMÉE quand ce
// paquet part : il n'y a rien à lui répondre.
struct BuyingStoreEnd {
  static constexpr std::uint16_t kOpcode = 0x081A;
  static constexpr std::size_t kWireSize = 4;

  std::uint16_t result = 0;
};

// « Tu as vendu » : une ligne d'une vente à une échoppe d'achat, au vendeur
// (`clif_buyingstore_delete_item`).
//
// 🔴 CE PAQUET RETIRE AUSSI L'OBJET DU SAC. Le serveur l'envoie À LA PLACE du
// retrait ordinaire ; qui l'ignore garde dans son sac ce qu'il a vendu.
//
// ⚠ LE PRIX EST UNITAIRE : le total de la ligne est à calculer.
struct BuyingStoreItemSold {
  static constexpr std::uint16_t kOpcode = 0x081C;
  static constexpr std::size_t kWireSize = 10;

  // La case du SAC, vue du client (index du serveur + 2).
  std::uint16_t client_slot = 0;
  std::uint16_t amount = 0;
  std::uint32_t unit_price = 0;
};

// ⚠ REFUSENT une trame qui n'est pas exactement la leur, en opcode et en taille.
[[nodiscard]] core::Result<BuyingStorePreparation> DecodeBuyingStorePreparation(
    std::span<const std::uint8_t> frame);
[[nodiscard]] core::Result<BuyingStoreOpenFailure> DecodeBuyingStoreOpenFailure(
    std::span<const std::uint8_t> frame);
[[nodiscard]] core::Result<BuyingStoreEnd> DecodeBuyingStoreEnd(
    std::span<const std::uint8_t> frame);
[[nodiscard]] core::Result<BuyingStoreItemSold> DecodeBuyingStoreItemSold(
    std::span<const std::uint8_t> frame);

// ── ÉMIS ─────────────────────────────────────────────────────────────────────

// Une ligne voulue : l'objet, combien, et à quel prix unitaire.
struct BuyingStoreOpenLine {
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t item_id = 0;
  std::uint16_t amount = 0;
  std::uint32_t price = 0;
};

// « Ouvre mon échoppe » — ou, `open` faux, « j'y renonce »
// (`clif_parse_ReqOpenBuyingStore`).
//
// ⚠ LE TITRE EST DÉJÀ EN OCTETS DU FIL, et il tient dans 79 octets : le serveur
// en copie 79 et termine lui-même. Plus long, l'encodeur REFUSE plutôt que de
// couper au milieu d'un caractère — la coupe se fait en amont, là où l'on sait
// en quel encodage le titre est écrit.
struct BuyingStoreOpenRequest {
  static constexpr std::uint16_t kOpcode = 0x0811;
  static constexpr std::size_t kHeaderSize = 89;
  // Le champ du titre, terminateur compris : MESSAGE_SIZE.
  static constexpr std::size_t kTitleSize = 80;

  std::uint32_t zeny_limit = 0;
  bool open = true;
  std::string title_on_wire;
  std::vector<BuyingStoreOpenLine> lines;
};

// « Ferme mon échoppe » (`clif_parse_ReqCloseBuyingStore`). Le serveur ne répond
// RIEN au propriétaire : l'enseigne ne disparaît que chez les autres.
struct BuyingStoreCloseRequest {
  static constexpr std::uint16_t kOpcode = 0x0815;
  static constexpr std::size_t kWireSize = 2;
};

// « Montre-moi ce que cet acheteur veut » (`clif_parse_ReqClickBuyingStore`). La
// réponse est la liste de l'échoppe ; un acheteur hors de vue ou qui n'achète
// plus ne vaut AUCUNE réponse.
struct BuyingStoreVisitRequest {
  static constexpr std::uint16_t kOpcode = 0x0817;
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t buyer_account_id = 0;
};

// Une ligne de vente : quelle case du sac, quel objet, combien.
//
// ⚠ L'OBJET EST RÉPÉTÉ, et le serveur le compare à ce que la case contient : une
// case qui a changé entre-temps fait échouer la vente ENTIÈRE.
struct BuyingStoreTradeLine {
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t client_slot = 0;
  std::uint32_t item_id = 0;
  std::uint16_t amount = 0;
};

// « Je vends ceci à cet acheteur » (`clif_parse_ReqTradeBuyingStore`).
//
// ⚠ L'ÉCHOPPE EST NOMMÉE DEUX FOIS : par le compte de l'acheteur, et par le
// numéro que sa liste portait. Un numéro qui ne correspond plus — l'acheteur a
// fermé puis rouvert — fait échouer la vente.
struct BuyingStoreTradeRequest {
  static constexpr std::uint16_t kOpcode = 0x0819;
  static constexpr std::size_t kHeaderSize = 12;
  // LA PLUS LONGUE TRAME que le client d'origine consente à émettre. Au-delà, il
  // n'envoie rien ; on refuse de même, plutôt que de livrer au serveur une
  // trame qu'aucun client n'a jamais produite.
  static constexpr std::size_t kMostFrameBytes = 0x800;

  std::uint32_t buyer_account_id = 0;
  std::uint32_t store_id = 0;
  std::vector<BuyingStoreTradeLine> lines;
};

[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const BuyingStoreOpenRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(
    const BuyingStoreCloseRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(
    const BuyingStoreVisitRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(
    const BuyingStoreTradeRequest& request);

}  // namespace ro::proto
