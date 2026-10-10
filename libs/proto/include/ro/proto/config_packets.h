#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// ── LES RÉGLAGES QUE LE SERVEUR TIENT POUR LE PERSONNAGE ────────────────────
//
// Une demande (`CZ_CONFIG`) et son écho (`ZC_CONFIG`), écrits à la main : le
// serveur les lit et les écrit champ par champ, sans structure à engendrer.
// L'état au login, lui, arrive par `ZcConfigNotify`, qui est engendré.

// Le réglage visé, numéroté comme le serveur (`e_config_type`).
//
// ⚠ LE SENS N'EST PAS LE MÊME PARTOUT : zéro montre l'équipement caché, zéro
// montre les costumes affichés.
enum class ConfigType : std::uint32_t {
  // Un : les autres joueurs peuvent voir notre équipement.
  kShowEquipment = 0,
  kCall = 1,
  kPetAutofeed = 2,
  kHomunculusAutofeed = 3,
  // Non nul : nos costumes sont MASQUÉS, pour nous comme pour les autres.
  kHideCostumes = 5,
};

// « Change ce réglage. » Le serveur répond par un écho, même quand rien ne
// change ; il se tait sur un réglage qu'il ne sait pas appliquer.
struct ConfigRequest {
  static constexpr std::uint16_t kOpcode = 0x02D8;
  static constexpr std::size_t kWireSize = 10;

  ConfigType type = ConfigType::kShowEquipment;
  // Toute valeur non nulle vaut « vrai » pour le serveur.
  std::uint32_t value = 0;
};

[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const ConfigRequest& request);

// L'écho : le réglage, et sa valeur ramenée à zéro ou un.
//
// ⚠ LE TYPE RESTE UN NOMBRE : le serveur peut en dire un que ce client ne
// nomme pas, et le décodeur n'a pas à le refuser pour autant.
struct ConfigAck {
  static constexpr std::uint16_t kOpcode = 0x02D9;
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t type = 0;
  std::uint32_t value = 0;
};

// ⚠ REFUSE UNE TRAME QUI NE FAIT PAS SA LONGUEUR, ou qui porte un autre opcode.
[[nodiscard]] core::Result<ConfigAck> DecodeConfigAck(std::span<const std::uint8_t> frame);

// L'envers du décodeur, pour les essais : le client n'émet pas d'écho.
[[nodiscard]] std::vector<std::uint8_t> EncodeConfigAck(const ConfigAck& ack);

}  // namespace ro::proto
