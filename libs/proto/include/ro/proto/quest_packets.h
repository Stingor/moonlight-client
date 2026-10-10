#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/core/error.h"

// LES PAQUETS DU SUIVI DES QUÊTES, hors la liste entière (`ledger_packets.h`).
//
// Le serveur les écrit champ par champ dans la disposition de 2015, non dans
// les structures que le générateur relève : celles-ci ne servent pas.
namespace ro::proto {

// Une chasse annoncée avec une quête neuve. Le total n'y figure pas : il
// arrive dans `QuestHuntTotals`.
struct QuestAddedHunt {
  static constexpr std::size_t kWireSize = 42;
  static constexpr std::size_t kMobNameSize = 24;

  // Le serveur la numérote `quête × 1000 + rang de la chasse`.
  std::uint32_t hunt_id = 0;
  std::uint32_t mob_type = 0;
  std::uint32_t mob_id = 0;
  std::int16_t level_min = 0;
  std::int16_t level_max = 0;
  std::int16_t killed = 0;
  // Octets du fil jusqu'au premier zéro.
  std::string mob_name;
};

// UNE QUÊTE NEUVE (ou rendue), longueur FIXE : le serveur remplit le paquet
// jusqu'à trois chasses et laisse le reste non initialisé.
struct QuestAdded {
  static constexpr std::uint16_t kOpcode = 0x09F9;
  static constexpr std::size_t kWireSize = 143;
  static constexpr std::size_t kMaxHunts = 3;

  std::uint32_t quest_id = 0;
  bool active = false;
  // En secondes Unix ; zéro pour une quête sans limite de temps.
  std::uint32_t expires_unix_s = 0;
  std::vector<QuestAddedHunt> hunts;
};

// ⚠ REFUSE une longueur autre que la sienne, un autre opcode, et plus de trois
// chasses. L'instant de début que le paquet porte n'est pas lu : le journal ne
// s'en sert pas, et un serveur plus ancien n'en écrivait qu'un octet.
[[nodiscard]] core::Result<QuestAdded> DecodeQuestAdded(std::span<const std::uint8_t> frame);
[[nodiscard]] std::vector<std::uint8_t> EncodeQuestAdded(const QuestAdded& added);

struct QuestHuntTotal {
  static constexpr std::size_t kWireSize = 12;

  // L'identifiant de la QUÊTE, comme le client d'origine le lit ; un serveur
  // plus ancien y écrivait celui de la chasse (`quête × 1000 + rang`).
  std::uint32_t quest_or_hunt_id = 0;
  // Zéro pour une chasse générique (race, taille, élément).
  std::uint32_t mob_id = 0;
  std::int16_t total = 0;
  std::int16_t killed = 0;
};

// LES TOTAUX DES CHASSES D'UNE QUÊTE NEUVE, envoyés juste après `QuestAdded`.
struct QuestHuntTotals {
  static constexpr std::uint16_t kOpcode = 0x08FE;
  static constexpr std::size_t kHeaderSize = 4;

  std::vector<QuestHuntTotal> entries;
};

// ⚠ REFUSE une longueur qui n'est pas l'en-tête plus des entrées entières, ou
// qui ne dit pas celle de la trame.
[[nodiscard]] core::Result<QuestHuntTotals> DecodeQuestHuntTotals(
    std::span<const std::uint8_t> frame);
[[nodiscard]] std::vector<std::uint8_t> EncodeQuestHuntTotals(const QuestHuntTotals& totals);

struct QuestHuntCount {
  static constexpr std::size_t kWireSize = 12;

  std::uint32_t quest_id = 0;
  std::uint32_t hunt_id = 0;
  std::int16_t total = 0;
  std::int16_t killed = 0;
};

// LA PROGRESSION D'UNE QUÊTE : toutes ses chasses, pas la seule qui a bougé.
// Elle peut ne porter aucune entrée.
struct QuestHuntProgress {
  static constexpr std::uint16_t kOpcode = 0x09FA;
  static constexpr std::size_t kHeaderSize = 6;

  std::vector<QuestHuntCount> entries;
};

// ⚠ REFUSE une longueur qui n'est pas celle de la trame, un nombre d'entrées
// qui contredit la longueur, et une queue qui n'est pas faite d'entrées entières.
[[nodiscard]] core::Result<QuestHuntProgress> DecodeQuestHuntProgress(
    std::span<const std::uint8_t> frame);
[[nodiscard]] std::vector<std::uint8_t> EncodeQuestHuntProgress(const QuestHuntProgress& progress);

// UNE QUÊTE RETIRÉE. Le même paquet pour une quête achevée et pour une quête
// abandonnée : le client ne peut pas les distinguer.
struct QuestRemoved {
  static constexpr std::uint16_t kOpcode = 0x02B4;
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t quest_id = 0;
};

[[nodiscard]] core::Result<QuestRemoved> DecodeQuestRemoved(std::span<const std::uint8_t> frame);
[[nodiscard]] std::vector<std::uint8_t> EncodeQuestRemoved(const QuestRemoved& removed);

// UNE QUÊTE DEVENUE ACTIVE OU INACTIVE, réponse à `QuestActiveRequest`.
struct QuestActiveNotice {
  static constexpr std::uint16_t kOpcode = 0x02B7;
  static constexpr std::size_t kWireSize = 7;

  std::uint32_t quest_id = 0;
  bool active = false;
};

[[nodiscard]] core::Result<QuestActiveNotice> DecodeQuestActiveNotice(
    std::span<const std::uint8_t> frame);
[[nodiscard]] std::vector<std::uint8_t> EncodeQuestActiveNotice(const QuestActiveNotice& notice);

// « ACTIVE (OU DÉSACTIVE) CETTE QUÊTE » : ce que le client émet. Le serveur
// répond `QuestActiveNotice`.
struct QuestActiveRequest {
  static constexpr std::uint16_t kOpcode = 0x02B6;
  static constexpr std::size_t kWireSize = 7;

  std::uint32_t quest_id = 0;
  bool active = false;
};

[[nodiscard]] core::Result<QuestActiveRequest> DecodeQuestActiveRequest(
    std::span<const std::uint8_t> frame);
[[nodiscard]] std::vector<std::uint8_t> EncodeQuestActiveRequest(const QuestActiveRequest& request);

}  // namespace ro::proto
