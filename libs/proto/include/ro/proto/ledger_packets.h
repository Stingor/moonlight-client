#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/core/error.h"

// CE QUE LE SERVEUR DE CARTE DIT DU PERSONNAGE SANS QUE LE GÉNÉRATEUR LE
// DÉCRIVE : il écrit ces quatre paquets champ par champ, sans structure à
// relever.
namespace ro::proto {

// L'IDENTIFIANT DE COMPTE, redit par le serveur de carte dès la connexion.
struct AccountIdNotice {
  static constexpr std::uint16_t kOpcode = 0x0283;
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t account_id = 0;
};

// ⚠ REFUSE une trame qui ne fait pas sa longueur, ou qui porte un autre opcode.
[[nodiscard]] core::Result<AccountIdNotice> DecodeAccountIdNotice(
    std::span<const std::uint8_t> frame);
// L'envers du décodeur, pour les essais : le client n'émet pas ce paquet.
[[nodiscard]] std::vector<std::uint8_t> EncodeAccountIdNotice(const AccountIdNotice& notice);

// UN GAIN — OU UNE PERTE — D'EXPÉRIENCE.
struct ExpNotice {
  static constexpr std::uint16_t kOpcode = 0x0ACC;
  static constexpr std::size_t kWireSize = 18;

  // Le compte du personnage concerné : le serveur ne l'envoie qu'à lui.
  std::uint32_t account_id = 0;
  // NÉGATIF pour une perte : le serveur envoie la valeur absolue multipliée
  // par moins un.
  std::int64_t amount = 0;
  // Le paramètre gagné : 1 pour l'expérience de base, 2 pour celle de métier.
  std::uint16_t var_id = 0;
  // Un pour l'expérience d'une QUÊTE, zéro pour toute autre.
  std::uint16_t exp_type = 0;
};

[[nodiscard]] core::Result<ExpNotice> DecodeExpNotice(std::span<const std::uint8_t> frame);
[[nodiscard]] std::vector<std::uint8_t> EncodeExpNotice(const ExpNotice& notice);

// UNE CHASSE d'une quête : quel monstre, combien sont tombés sur combien.
struct QuestHuntEntry {
  static constexpr std::size_t kWireSize = 44;
  static constexpr std::size_t kMobNameSize = 24;

  // Le serveur la numérote `quête × 1000 + rang de la chasse`.
  std::uint32_t hunt_id = 0;
  // La race, la taille ou l'élément qu'une chasse sans monstre désigne, dans
  // cet ordre de priorité ; zéro pour une chasse d'un monstre précis.
  std::uint32_t mob_type = 0;
  std::uint32_t mob_id = 0;
  std::int16_t level_min = 0;
  std::int16_t level_max = 0;
  std::int16_t killed = 0;
  std::int16_t total = 0;
  // Octets du fil, tels quels : le nom du monstre, ou la description de la
  // chasse.
  std::string mob_name;
};

struct QuestEntry {
  // La quête, sans ses chasses.
  static constexpr std::size_t kHeadSize = 15;

  std::uint32_t quest_id = 0;
  bool active = false;
  // L'instant où la quête a commencé, puis celui où elle expire, en secondes
  // Unix ; zéro pour une quête sans limite de temps.
  std::uint32_t started_unix_s = 0;
  std::uint32_t expires_unix_s = 0;
  std::vector<QuestHuntEntry> hunts;
};

// LA LISTE ENTIÈRE DES QUÊTES, à l'entrée sur la carte.
//
// ⚠ UNE CHASSE FAIT 44 OCTETS : le serveur écrit ce champ par champ dans la
// disposition de 2015, non dans celle que sa structure déclare.
struct QuestList {
  static constexpr std::uint16_t kOpcode = 0x09F8;
  static constexpr std::size_t kHeaderSize = 8;

  std::vector<QuestEntry> quests;
};

// ⚠ REFUSE une trame dont la longueur annoncée n'est pas la sienne, ou dont les
// quêtes ne remplissent pas exactement la queue.
[[nodiscard]] core::Result<QuestList> DecodeQuestList(std::span<const std::uint8_t> frame);
[[nodiscard]] std::vector<std::uint8_t> EncodeQuestList(const QuestList& list);

// UNE PIÈCE DU JEU D'ÉQUIPEMENT DE RECHANGE.
struct EquipSwitchEntry {
  static constexpr std::size_t kWireSize = 6;

  // La case de l'objet dans le sac, VUE DU CLIENT — celle de
  // `InventoryItem::client_slot`.
  std::uint16_t client_slot = 0;
  // Les emplacements qu'elle occupe dans ce jeu.
  std::uint32_t location = 0;
};

// LE JEU D'ÉQUIPEMENT DE RECHANGE, entier, à l'entrée sur la carte.
struct EquipSwitchList {
  static constexpr std::uint16_t kOpcode = 0x0A9B;
  static constexpr std::size_t kHeaderSize = 4;

  std::vector<EquipSwitchEntry> entries;
};

[[nodiscard]] core::Result<EquipSwitchList> DecodeEquipSwitchList(
    std::span<const std::uint8_t> frame);
[[nodiscard]] std::vector<std::uint8_t> EncodeEquipSwitchList(const EquipSwitchList& list);

}  // namespace ro::proto
