#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// ── LES PAQUETS DE GROUPE ET D'AMIS QU'AUCUNE STRUCTURE NE DÉCRIT ───────────
//
// Même règle que `guild_packets.h` : le serveur les lit ou les écrit champ par
// champ, et chaque disposition est RECOUPÉE avec ce que sa table déclare — les
// décalages lus par son gestionnaire à l'envoi, la longueur fixe à la réception.

// « Demande à ce joueur d'être mon ami. » Par le NOM : le serveur cherche le
// personnage, qui doit être en ligne.
struct FriendAddRequest {
  static constexpr std::uint16_t kOpcode = 0x0202;
  static constexpr std::size_t kWireSize = 26;
  static constexpr std::size_t kNameSize = 24;

  // ⚠ DÉJÀ DANS L'ENCODAGE DU FIL : c'est l'appelant qui convertit.
  std::string name;
};

// « Retire cet ami. » Le serveur le retire aussi de SA liste à lui.
struct FriendRemoveRequest {
  static constexpr std::uint16_t kOpcode = 0x0203;
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t account_id = 0;
  std::uint32_t char_id = 0;
};

// « Voici ma réponse à sa demande d'amitié. » La réponse tient sur quatre
// octets, un pour oui, zéro pour non.
struct FriendAnswerRequest {
  static constexpr std::uint16_t kOpcode = 0x0208;
  static constexpr std::size_t kWireSize = 14;
  static constexpr std::uint32_t kAccepted = 1;
  static constexpr std::uint32_t kRefused = 0;

  std::uint32_t account_id = 0;
  std::uint32_t char_id = 0;
  std::uint32_t answer = kRefused;
};

// « Passe la direction du groupe à ce membre. » Réservé au chef.
struct PartyLeaderRequest {
  static constexpr std::uint16_t kOpcode = 0x07DA;
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t account_id = 0;
};

// « Voici les règles du groupe. » Réservé au chef ; le serveur ignore en
// silence celles d'un simple membre.
struct PartyOptionsRequest {
  static constexpr std::uint16_t kOpcode = 0x07D7;
  static constexpr std::size_t kWireSize = 8;

  // Un pour le partage égal de l'EXP, zéro pour chacun la sienne.
  std::uint32_t exp_share = 0;
  // Un pour le ramassage par le groupe, zéro pour chacun le sien.
  std::uint8_t item_pickup = 0;
  // Un pour la répartition du butin entre les membres.
  std::uint8_t item_share = 0;
};

[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const FriendAddRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const FriendRemoveRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const FriendAnswerRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const PartyLeaderRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const PartyOptionsRequest& request);

// ── CE QUE LE SERVEUR ENVOIE ────────────────────────────────────────────────

// Quelqu'un demande à être notre ami.
struct FriendRequestNotice {
  static constexpr std::uint16_t kOpcode = 0x0207;
  static constexpr std::size_t kWireSize = 34;

  std::uint32_t account_id = 0;
  std::uint32_t char_id = 0;
  // Tel que le fil le porte.
  std::string name;
};

// Le sort d'une demande d'amitié, la nôtre ou celle qu'on vient d'accepter.
struct FriendAddResult {
  static constexpr std::uint16_t kOpcode = 0x0209;
  static constexpr std::size_t kWireSize = 36;

  std::uint16_t result = 0;
  std::uint32_t account_id = 0;
  std::uint32_t char_id = 0;
  std::string name;
};

// Un ami retiré de la liste : par nous, ou par lui.
struct FriendRemovedNotice {
  static constexpr std::uint16_t kOpcode = 0x020A;
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t account_id = 0;
  std::uint32_t char_id = 0;
};

// Les règles du groupe, redites à chaque changement et à l'entrée en jeu.
struct PartyOptionsNotice {
  static constexpr std::uint16_t kOpcode = 0x07D8;
  static constexpr std::size_t kWireSize = 8;
  // La valeur d'EXP qui ne décrit pas une règle mais un REFUS : l'écart de
  // niveau interdit le partage, et la règle en place ne change pas.
  static constexpr std::uint32_t kExpChangeRefused = 2;

  std::uint32_t exp_share = 0;
  std::uint8_t item_pickup = 0;
  std::uint8_t item_share = 0;
};

[[nodiscard]] core::Result<FriendRequestNotice> DecodeFriendRequestNotice(
    std::span<const std::uint8_t> frame);
[[nodiscard]] core::Result<FriendAddResult> DecodeFriendAddResult(
    std::span<const std::uint8_t> frame);
[[nodiscard]] core::Result<FriendRemovedNotice> DecodeFriendRemovedNotice(
    std::span<const std::uint8_t> frame);
[[nodiscard]] core::Result<PartyOptionsNotice> DecodePartyOptionsNotice(
    std::span<const std::uint8_t> frame);

// QUI, DANS LE GROUPE, EST HORS DU PARTAGE D'EXP.
//
// La structure engendrée s'arrête au bloc fixe ; les entrées qui suivent sont
// écrites par le code du serveur, cinq octets chacune, sans alignement.
struct PartyShareEntry {
  std::uint32_t account_id = 0;
  std::uint8_t flags = 0;
};
struct PartyShareState {
  static constexpr std::uint16_t kOpcode = 0x0F35;
  static constexpr std::size_t kFixedSize = 8;
  static constexpr std::size_t kEntrySize = 5;

  // Le seuil d'inactivité du serveur, en secondes ; zéro éteint la règle.
  std::uint16_t idle_s = 0;
  std::vector<PartyShareEntry> entries;
};
// ⚠ LE COMPTE ANNONCÉ NE COMMANDE PAS LA LECTURE : la longueur de la trame la
// borne, et une trame qui en annonce plus qu'elle n'en porte rend ce qu'elle
// porte. Une entrée de compte nul est sautée.
[[nodiscard]] core::Result<PartyShareState> DecodePartyShareState(
    std::span<const std::uint8_t> frame);

}  // namespace ro::proto
