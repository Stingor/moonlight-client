#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// ── LES PAQUETS DE GUILDE QUE LE SERVEUR LIT CHAMP PAR CHAMP ────────────────
//
// Même règle que `manual_packets.h`, dont ils sont la famille : aucune
// structure ne les décrit côté serveur, et chaque disposition est RECOUPÉE
// avec les décalages que sa table déclare avant d'être émise. Ils vivent à
// part parce que l'autre en-tête touche son plafond de lignes.

// « Envoie-moi cette partie de la fenêtre de guilde. »
//
// 🔴 LE SERVEUR NE POUSSE NI LES MEMBRES, NI LES POSTES, NI LES COMPÉTENCES,
// NI LES EXPULSIONS : il les envoie sur cette demande, une partie à la fois.
struct GuildMenuRequest {
  static constexpr std::uint16_t kOpcode = 0x014F;
  static constexpr std::size_t kWireSize = 6;

  // Les parties que le serveur sait envoyer.
  enum Part : std::int32_t {
    kBasics = 0,
    kMembers = 1,
    kPositions = 2,
    kSkills = 3,
    kBans = 4,
  };
  std::int32_t part = kBasics;
};

// « Quels onglets de la fenêtre de guilde m'ouvres-tu ? » Le serveur répond par
// `ZC_ACK_GUILD_MENUINTERFACE 0x014E` — qu'il ne pousse de lui-même qu'au
// maître, à la connexion.
struct GuildMenuInterfaceRequest {
  static constexpr std::uint16_t kOpcode = 0x014D;
  static constexpr std::size_t kWireSize = 2;
};

// « Change l'annonce de la guilde. » Réservé au maître.
struct GuildNoticeRequest {
  static constexpr std::uint16_t kOpcode = 0x016E;
  static constexpr std::size_t kWireSize = 186;
  static constexpr std::size_t kSubjectSize = 60;
  static constexpr std::size_t kNoticeSize = 120;

  std::uint32_t guild_id = 0;
  // ⚠ DÉJÀ DANS L'ENCODAGE DU FIL : c'est l'appelant qui convertit.
  std::string subject;
  std::string notice;
};

// Un poste tel qu'il part sur le fil, et tel qu'il revient dans l'accusé.
struct GuildPositionEntry {
  static constexpr std::size_t kWireSize = 40;
  static constexpr std::size_t kNameSize = 24;

  std::int32_t position = 0;
  std::int32_t rights = 0;
  // ⚠ LE SERVEUR NE LE LIT PAS — il relit la part d'expérience à sa place —
  // mais le client d'origine y remet le numéro du poste, et on fait pareil.
  std::int32_t ranking = 0;
  std::int32_t pay_rate = 0;
  std::string name;
};

// « Voici les postes modifiés. » Réservé au maître ; le serveur applique
// chaque entrée telle quelle et rediffuse un accusé par poste.
struct GuildPositionsRequest {
  static constexpr std::uint16_t kOpcode = 0x0161;
  // L'opcode et la longueur, avant les entrées.
  static constexpr std::size_t kHeaderSize = 4;

  std::vector<GuildPositionEntry> entries;
};

// « Crée cette guilde. » Le serveur ignore le personnage transmis — il prend
// celui de la session — mais le client d'origine le remplit.
struct GuildCreateRequest {
  static constexpr std::uint16_t kOpcode = 0x0165;
  static constexpr std::size_t kWireSize = 30;
  static constexpr std::size_t kNameSize = 24;

  std::uint32_t character_id = 0;
  std::string name;
};

// « Romps cette alliance, ou retire cette hostilité. » Réservé au maître.
struct GuildRelationDeleteRequest {
  static constexpr std::uint16_t kOpcode = 0x0183;
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t guild_id = 0;
  // Zéro pour un allié, un pour un ennemi.
  std::uint32_t relation = 0;
};

// « Propose une alliance à la guilde de ce joueur. » Réservé au maître.
//
// ⚠ LE SERVEUR NE LIT QUE LE PREMIER CHAMP, et sa table en déclare pourtant
// trois : les deux autres désignent QUI demande — soi-même, compte puis
// personnage. On les remplit quand même, la disposition est celle que la table
// recoupe.
struct GuildAllianceRequest {
  static constexpr std::uint16_t kOpcode = 0x0170;
  static constexpr std::size_t kWireSize = 14;

  std::uint32_t target_account_id = 0;
  std::uint32_t own_account_id = 0;
  std::uint32_t own_char_id = 0;
};

// « Voici ma réponse à l'alliance proposée. » Le serveur retrouve la guilde
// par le COMPTE du joueur qui l'a proposée, celui que la demande reçue nomme.
struct GuildAllianceAnswer {
  static constexpr std::uint16_t kOpcode = 0x0172;
  static constexpr std::size_t kWireSize = 10;
  static constexpr std::uint32_t kRefuse = 0;
  static constexpr std::uint32_t kAccept = 1;

  std::uint32_t inviter_account_id = 0;
  std::uint32_t answer = kRefuse;
};

// « Déclare la guilde de ce joueur ennemie. » Réservé au maître, et immédiat :
// l'autre guilde n'a rien à accepter.
struct GuildOppositionRequest {
  static constexpr std::uint16_t kOpcode = 0x0180;
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t target_account_id = 0;
};

[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const GuildMenuRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(
    const GuildMenuInterfaceRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const GuildNoticeRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const GuildPositionsRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const GuildCreateRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(
    const GuildRelationDeleteRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const GuildAllianceRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const GuildAllianceAnswer& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const GuildOppositionRequest& request);

// ── CE QUE LE SERVEUR ENVOIE SANS STRUCTURE LISIBLE ─────────────────────────

// Les noms des postes : une entrée de vingt-huit octets par poste.
inline constexpr std::uint16_t kGuildPositionNamesOpcode = 0x0166;
struct GuildPositionName {
  std::int32_t position = 0;
  // Tel que le fil le porte.
  std::string name;
};
[[nodiscard]] core::Result<std::vector<GuildPositionName>> DecodeGuildPositionNames(
    std::span<const std::uint8_t> frame);

// L'accusé d'un changement de postes : une entrée de quarante octets par poste,
// la même forme que la demande.
inline constexpr std::uint16_t kGuildPositionsChangedOpcode = 0x0174;
[[nodiscard]] core::Result<std::vector<GuildPositionEntry>> DecodeGuildPositionsChanged(
    std::span<const std::uint8_t> frame);

// L'IMAGE D'UN EMBLÈME, poussée par le fil : la guilde, la version, puis un BMP
// compressé en zlib jusqu'au bout de la trame.
//
// ⚠ C'EST LE CHEMIN HISTORIQUE, et le serveur ne l'emprunte que pour un
// emblème déposé par l'ancien paquet. Un emblème déposé par le service web n'a
// pas d'image ici : il se télécharge.
inline constexpr std::uint16_t kGuildEmblemImageOpcode = 0x0152;
struct GuildEmblemImage {
  std::int32_t guild_id = 0;
  std::int32_t version = 0;
  std::vector<std::uint8_t> compressed;
};
[[nodiscard]] core::Result<GuildEmblemImage> DecodeGuildEmblemImage(
    std::span<const std::uint8_t> frame);

// ── LES DEUX ANNONCES QUI NE PORTENT PAS LEUR OPCODE ────────────────────────
//
// Le serveur les pose depuis son code, par une constante de son énumération de
// paquets : le départ d'un membre, et son expulsion. Les dispositions sont
// celles de `ZcAckLeaveGuild2` et `ZcAckBanGuild3`.
inline constexpr std::uint16_t kGuildMemberLeftOpcode = 0x0A83;
inline constexpr std::uint16_t kGuildMemberExpelledOpcode = 0x0A82;

}  // namespace ro::proto
