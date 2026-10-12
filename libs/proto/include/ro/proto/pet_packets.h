#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// ── LES PAQUETS DU FAMILIER QUE LE GÉNÉRATEUR N'ÉCRIT PAS ──────────────────
//
// La fiche (`ZcPropertyPet`), le repas (`ZcFeedPet`) et les changements d'état
// (`ZcChangestatePet`) sont engendrés dans `map_packets.h`. Restent ici ceux que
// le serveur écrit à la main, et les cinq demandes du client.

// LA LISTE D'ÉCLOSION : les œufs du sac, désignés par leur case vue du client.
// Un en-tête de quatre octets, puis deux octets par œuf.
//
// ⚠ LA CASE EST CELLE QU'IL FAUT RENVOYER TELLE QUELLE : le serveur y a déjà
// ajouté son décalage, et le retranche à la réponse.
struct PetEggList {
  static constexpr std::uint16_t kOpcode = 0x01A6;
  static constexpr std::size_t kHeaderSize = 4;
  static constexpr std::size_t kEntrySize = 2;

  std::vector<std::uint16_t> client_slots;
};

// ⚠ REFUSE une trame dont la longueur annoncée n'est pas la sienne, ou dont la
// queue ne se découpe pas en cases entières.
[[nodiscard]] core::Result<PetEggList> DecodePetEggList(std::span<const std::uint8_t> frame);
// L'envers du décodeur, pour les essais : le client n'émet pas ce paquet.
[[nodiscard]] std::vector<std::uint8_t> EncodePetEggList(const PetEggList& list);

// LE RÉSULTAT D'UNE ÉVOLUTION, numéroté comme le serveur.
enum class PetEvolutionOutcome : std::uint32_t {
  kFailUnknown = 0,
  kFailNoPet = 1,
  kFailNotPetEgg = 2,
  kFailRecipe = 3,
  kFailMaterial = 4,
  // Intimité insuffisante — et, sur ce serveur, accessoire porté.
  kFailIntimacy = 5,
  kSuccess = 6,
};

struct PetEvolutionResult {
  static constexpr std::uint16_t kOpcode = 0x09FC;
  static constexpr std::size_t kWireSize = 6;

  // UN NOMBRE ET NON L'ÉNUMÉRATION : le serveur peut en dire un que ce client ne
  // nomme pas.
  std::uint32_t result = 0;
};

[[nodiscard]] core::Result<PetEvolutionResult> DecodePetEvolutionResult(
    std::span<const std::uint8_t> frame);
[[nodiscard]] std::vector<std::uint8_t> EncodePetEvolutionResult(const PetEvolutionResult& result);

// UN ORDRE AU FAMILIER, numéroté comme le serveur le lit.
enum class PetCommand : std::uint8_t {
  // Le serveur renvoie la fiche.
  kStatus = 0,
  kFeed = 1,
  kPerform = 2,
  kReturnToEgg = 3,
  kUnequipAccessory = 4,
};

struct PetCommandRequest {
  static constexpr std::uint16_t kOpcode = 0x01A1;
  static constexpr std::size_t kWireSize = 3;

  PetCommand command = PetCommand::kStatus;
};

// LE NOUVEAU NOM, sur vingt-quatre octets complétés de zéros. Le serveur n'en
// garde que vingt-trois.
struct PetRenameRequest {
  static constexpr std::uint16_t kOpcode = 0x01A5;
  static constexpr std::size_t kWireSize = 26;
  static constexpr std::size_t kNameSize = 24;

  std::string name;
};

// L'ŒUF CHOISI DANS LA LISTE D'ÉCLOSION, par la case que la liste a donnée.
struct PetEggChoice {
  static constexpr std::uint16_t kOpcode = 0x01A7;
  static constexpr std::size_t kWireSize = 4;

  std::uint16_t client_slot = 0;
};

// L'ÉVOLUTION DEMANDÉE, vers l'œuf CIBLE.
//
// ⚠ LA FORME MINIMALE, SANS LISTE DE MATÉRIAUX : le serveur ne lit que l'œuf
// cible, et relit les matériaux dans sa propre base. Le paquet reste à longueur
// variable, et l'annonce.
struct PetEvolutionRequest {
  static constexpr std::uint16_t kOpcode = 0x09FB;
  static constexpr std::size_t kWireSize = 8;

  std::uint32_t target_egg_id = 0;
};

// CE QUE LE FAMILIER DIT OU MONTRE : un entier que le client du maître
// compose et que le serveur renvoie tel quel à tous ceux qui voient le
// familier. Voir `ro/sim/pet_talk.h`.
struct PetActRequest {
  static constexpr std::uint16_t kOpcode = 0x01A9;
  static constexpr std::size_t kWireSize = 6;

  std::int32_t data = 0;
};

[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const PetCommandRequest& request);
// ⚠ REFUSE un nom vide, ou plus long que le champ : le tronquer pourrait couper
// un caractère en deux.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const PetRenameRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const PetEggChoice& choice);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const PetEvolutionRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const PetActRequest& request);

}  // namespace ro::proto
