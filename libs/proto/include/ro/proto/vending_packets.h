#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// ── L'ÉCHOPPE DE VENTE : les trois demandes qui n'ont pas de structure ─────────
//
// Les paquets reçus de l'échoppe et la demande d'achat (`CzPcPurchaseItemlistFrommc2`)
// sont décrits par le serveur et engendrés dans `map_packets.h`. Ces trois-là n'y
// figurent pas : le serveur les lit champ par champ. Ils sont donc écrits ici, et
// recoupés avec ce que sa table déclare lire.

// Une ligne de l'échoppe qu'on ouvre.
struct VendingOpenLine {
  // ⚠ LA CASE DE CHARRETTE VUE DU CLIENT, soit l'index du serveur plus deux :
  // celle que la liste de la charrette a donnée, renvoyée telle quelle.
  std::uint16_t cart_slot = 0;
  std::uint16_t amount = 0;
  std::uint32_t price_zeny = 0;
};

// « Ouvre mon échoppe », ou « j'abandonne la préparation ».
//
// 🔴 L'ABANDON EST CE MÊME PAQUET, `open` à faux. Le serveur (`clif_parse_OpenVending`)
// remet la préparation à zéro DÈS QU'IL LIT l'octet d'abandon, avant tout autre
// contrôle — carte ou case interdite, titre vide compris. Puis il continue comme
// pour une ouverture : un titre vide le fait s'arrêter là, un titre non vide
// l'envoie jusqu'à `vending_openvending`, qui refuse en silence faute de
// préparation. L'abandon part donc avec un titre VIDE et sans ligne : c'est le
// chemin le plus court, et il ne dépend d'aucune ligne.
//
// ⚠ Sur une carte ou une case où l'on ne vend pas, le serveur répond pourtant à
// l'abandon par son message « on ne peut pas ouvrir d'échoppe ici » : le contrôle
// suit la remise à zéro, il ne la précède pas.
struct VendingOpenRequest {
  static constexpr std::uint16_t kOpcode = 0x01B2;
  // Opcode, longueur, titre, octet d'ouverture.
  static constexpr std::size_t kHeaderSize = 85;
  static constexpr std::size_t kLineSize = 8;
  // Le titre, NUL compris (`MESSAGE_SIZE`).
  static constexpr std::size_t kTitleBytes = 80;
  static constexpr std::uint8_t kCancel = 0;
  static constexpr std::uint8_t kOpen = 1;

  // Les OCTETS DU FIL, déjà convertis et déjà coupés : au plus `kTitleBytes - 1`.
  std::string title_wire;
  bool open = true;
  std::vector<VendingOpenLine> lines;
};

// « Je ferme mon échoppe. » Le serveur ne répond RIEN à l'intéressé : l'enseigne
// disparaît chez les voisins, et c'est tout.
struct VendingCloseRequest {
  static constexpr std::uint16_t kOpcode = 0x012E;
  static constexpr std::size_t kWireSize = 2;
};

// « Montre-moi l'échoppe de ce vendeur. » La réponse est la liste des offres ;
// un vendeur absent, ou qui ne vend plus, ne vaut AUCUNE réponse.
struct VendingListRequest {
  static constexpr std::uint16_t kOpcode = 0x0130;
  static constexpr std::size_t kWireSize = 6;

  // L'identifiant de COMPTE du vendeur, celui de son acteur.
  std::uint32_t vendor_aid = 0;
};

// ⚠ REFUSE un titre de `kTitleBytes` octets ou plus : il ne tiendrait pas avec
// son NUL, et le couper ici montrerait au joueur autre chose que ce qui part.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const VendingOpenRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const VendingCloseRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const VendingListRequest& request);

}  // namespace ro::proto
