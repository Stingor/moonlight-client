#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// ── LA RECHERCHE DE GROUPE : annonces de recrutement ────────────────────────
//
// Un joueur publie une annonce — un niveau, un lieu, jusqu'à six métiers ou
// rôles recherchés — ; les autres la cherchent par niveau, par lieu ou par
// métier. Le serveur garde les annonces en mémoire, une par personnage, et
// pousse à TOUS les clients connectés chaque annonce publiée, modifiée ou
// retirée.
//
// 🔴 AUCUN DE CES PAQUETS N'A DE STRUCTURE CÔTÉ SERVEUR : il les lit par une
// table de décalages et les écrit champ par champ. Ils sont donc écrits ici, et
// chaque demande est recoupée avec ce que le serveur déclare lire.

// Le nombre de métiers d'une annonce, et la marque d'une case vide.
inline constexpr std::size_t kPartyBookingJobSlots = 6;
inline constexpr std::int16_t kPartyBookingNoJob = -1;
// La longueur d'un nom de personnage sur le fil.
inline constexpr std::size_t kPartyBookingNameSize = 24;

using PartyBookingJobs = std::array<std::int16_t, kPartyBookingJobSlots>;

// « Publie mon annonce. »
//
// ⚠ LE SERVEUR NE LIT QUE L'OCTET BAS DE CHAQUE MÉTIER, et lit 0xFF comme une
// case vide. Les codes de métier tiennent tous dans un octet ; une case vide
// part en `kPartyBookingNoJob`, dont l'octet bas vaut 0xFF.
struct PartyBookingRegisterRequest {
  static constexpr std::uint16_t kOpcode = 0x0802;
  static constexpr std::size_t kWireSize = 18;

  std::int16_t level = 0;
  std::int16_t map_code = 0;
  PartyBookingJobs jobs = {};
};

// « Cherche les annonces. »
//
// Le serveur garde une annonce si son niveau tombe entre `level` - 15 et
// `level` (zéro : tous les niveaux), puis la filtre par lieu OU par métier :
// ⚠ LES DEUX À LA FOIS NE RENDENT RIEN. Il saute les annonces d'un rang
// inférieur à `first_index`, et rend au plus dix annonces par page.
struct PartyBookingSearchRequest {
  static constexpr std::uint16_t kOpcode = 0x0804;
  static constexpr std::size_t kWireSize = 14;

  std::int16_t level = 0;
  std::int16_t map_code = 0;
  std::int16_t job = kPartyBookingNoJob;
  std::uint32_t first_index = 0;
  // Ignoré par le serveur, qui a son propre plafond.
  std::int16_t result_count = 0;
};

// « Retire mon annonce. »
struct PartyBookingDeleteRequest {
  static constexpr std::uint16_t kOpcode = 0x0806;
  static constexpr std::size_t kWireSize = 2;
};

// « Change les métiers de mon annonce. » Le niveau et le lieu restent ; le
// serveur remet l'heure de l'annonce à l'instant.
struct PartyBookingUpdateRequest {
  static constexpr std::uint16_t kOpcode = 0x0808;
  static constexpr std::size_t kWireSize = 14;

  PartyBookingJobs jobs = {};
};

// UNE ANNONCE telle que le serveur la décrit, dans une page de recherche et
// dans l'avis d'une annonce publiée.
struct PartyBookingEntry {
  static constexpr std::size_t kWireSize = 48;

  // Son rang, unique et croissant sur ce serveur de carte.
  std::uint32_t index = 0;
  // Le personnage qui l'a publiée, tel qu'il est sur le fil.
  std::string leader_name;
  // L'instant de publication ou de la dernière modification, en secondes Unix.
  std::uint32_t posted_unix = 0;
  std::int16_t level = 0;
  std::int16_t map_code = 0;
  PartyBookingJobs jobs = {};
};

// L'accusé de la publication. Zéro : publiée ; un : refusée ; deux : une
// annonce existe déjà pour ce personnage.
struct PartyBookingRegisterAck {
  static constexpr std::uint16_t kOpcode = 0x0803;
  static constexpr std::size_t kWireSize = 4;

  std::uint16_t result = 0;
};

// Une page de résultats. ⚠ LE COMPTE SE LIT DANS LA LONGUEUR.
struct PartyBookingSearchAck {
  static constexpr std::uint16_t kOpcode = 0x0805;
  // L'opcode, la longueur, et l'octet « d'autres résultats ».
  static constexpr std::size_t kHeaderSize = 5;

  bool more_results = false;
  std::vector<PartyBookingEntry> entries;
};

// L'accusé du retrait. Zéro : retirée ; un : retirée parce qu'expirée ; deux :
// refusé ; trois : aucune annonce.
struct PartyBookingDeleteAck {
  static constexpr std::uint16_t kOpcode = 0x0807;
  static constexpr std::size_t kWireSize = 4;

  std::uint16_t result = 0;
};

// UNE ANNONCE VIENT D'ÊTRE PUBLIÉE, par n'importe qui.
struct PartyBookingInsertNotice {
  static constexpr std::uint16_t kOpcode = 0x0809;
  static constexpr std::size_t kWireSize = 2 + PartyBookingEntry::kWireSize;

  PartyBookingEntry entry;
};

// LES MÉTIERS D'UNE ANNONCE ONT CHANGÉ.
struct PartyBookingUpdateNotice {
  static constexpr std::uint16_t kOpcode = 0x080A;
  static constexpr std::size_t kWireSize = 18;

  std::uint32_t index = 0;
  PartyBookingJobs jobs = {};
};

// UNE ANNONCE A ÉTÉ RETIRÉE : par son auteur, ou parce qu'il s'est déconnecté.
struct PartyBookingDeleteNotice {
  static constexpr std::uint16_t kOpcode = 0x080B;
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t index = 0;
};

[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(
    const PartyBookingRegisterRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(
    const PartyBookingSearchRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(
    const PartyBookingDeleteRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(
    const PartyBookingUpdateRequest& request);

// ⚠ LES DÉCODEURS REFUSENT UNE TRAME QUI NE TOMBE PAS JUSTE : une longueur
// annoncée qui n'est pas celle de la trame, un reste qui ne fait pas une
// annonce entière.
[[nodiscard]] core::Result<PartyBookingRegisterAck> DecodePartyBookingRegisterAck(
    std::span<const std::uint8_t> frame);
[[nodiscard]] core::Result<PartyBookingSearchAck> DecodePartyBookingSearchAck(
    std::span<const std::uint8_t> frame);
[[nodiscard]] core::Result<PartyBookingDeleteAck> DecodePartyBookingDeleteAck(
    std::span<const std::uint8_t> frame);
[[nodiscard]] core::Result<PartyBookingInsertNotice> DecodePartyBookingInsertNotice(
    std::span<const std::uint8_t> frame);
[[nodiscard]] core::Result<PartyBookingUpdateNotice> DecodePartyBookingUpdateNotice(
    std::span<const std::uint8_t> frame);
[[nodiscard]] core::Result<PartyBookingDeleteNotice> DecodePartyBookingDeleteNotice(
    std::span<const std::uint8_t> frame);

// L'ENVERS DES DÉCODEURS, pour les essais : le client n'émet aucun de ces
// paquets.
[[nodiscard]] std::vector<std::uint8_t> EncodePartyBookingRegisterAck(
    const PartyBookingRegisterAck& ack);
[[nodiscard]] std::vector<std::uint8_t> EncodePartyBookingSearchAck(
    const PartyBookingSearchAck& ack);
[[nodiscard]] std::vector<std::uint8_t> EncodePartyBookingDeleteAck(
    const PartyBookingDeleteAck& ack);
[[nodiscard]] std::vector<std::uint8_t> EncodePartyBookingInsertNotice(
    const PartyBookingInsertNotice& notice);
[[nodiscard]] std::vector<std::uint8_t> EncodePartyBookingUpdateNotice(
    const PartyBookingUpdateNotice& notice);
[[nodiscard]] std::vector<std::uint8_t> EncodePartyBookingDeleteNotice(
    const PartyBookingDeleteNotice& notice);

}  // namespace ro::proto
