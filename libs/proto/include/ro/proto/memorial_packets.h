#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ro::proto {

// ── L'INSTANCE (« MEMORIAL DUNGEON ») SUR LE FIL ────────────────────────────
//
// Les quatre paquets que le serveur écrit à la main (`clif_instance_*`), hors
// des structures engendrées, et la commande de destruction. Le détail maison —
// difficulté, turbo, droits — passe par la structure engendrée
// `ZcMoonlightInstanceInfo` (0x0F40).
//
// ⚠ LE NOM DE 61 OCTETS EST EN PAGE DE CODES DU JEU : le décodeur le rend tel
// quel, et c'est `net` qui le convertit.

// La taille du nom d'instance sur le fil, zéro compris.
inline constexpr std::size_t kInstanceNameBytes = 61;

// L'INSTANCE EST RÉSERVÉE (`clif_instance_create`) :
//
//     02CB <nom>.61B <position d'attente>.W
struct InstanceCreatedNotice {
  static constexpr std::uint16_t kOpcode = 0x02CB;
  static constexpr std::size_t kWireSize = 65;

  std::string name;
  std::uint16_t wait_position = 0;
};

// LA FILE AVANCE (`clif_instance_changewait`) : 02CC <position>.W
//
// ⚠ 0xFFFF DIT L'ANNULATION de la réservation, et non une position.
struct InstanceWaitNotice {
  static constexpr std::uint16_t kOpcode = 0x02CC;
  static constexpr std::size_t kWireSize = 4;
  static constexpr std::uint16_t kCancelled = 0xFFFF;

  std::uint16_t wait_position = 0;
};

// L'INSTANCE EST OUVERTE (`clif_instance_status`) :
//
//     02CD <nom>.61B <fin de vie>.L <fermeture à vide>.L
//
// Deux instants en secondes Unix de l'horloge du SERVEUR ; 0 dit « aucun ».
struct InstanceStatusNotice {
  static constexpr std::uint16_t kOpcode = 0x02CD;
  static constexpr std::size_t kWireSize = 71;

  std::string name;
  std::uint32_t keep_limit_unix_s = 0;
  std::uint32_t idle_limit_unix_s = 0;
};

// L'INSTANCE CHANGE (`clif_instance_changestatus`) : 02CE <type>.L <limite>.L
//
// Type 0 : la fermeture à vide devient `limit_unix_s` (0 : plus aucune). Les
// autres détruisent l'instance — voir `InstanceChange`.
struct InstanceChangeNotice {
  static constexpr std::uint16_t kOpcode = 0x02CE;
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t type = 0;
  std::uint32_t limit_unix_s = 0;
};

// « DÉTRUIS MON INSTANCE » (`CZ_MEMORIALDUNGEON_COMMAND`) : 02CF <commande>.L,
// toujours la commande 3, la seule que le serveur lise.
inline constexpr std::uint16_t kInstanceCommandOpcode = 0x02CF;
inline constexpr std::uint32_t kInstanceCommandDestroy = 3;

[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, InstanceCreatedNotice& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, InstanceWaitNotice& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, InstanceStatusNotice& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, InstanceChangeNotice& packet);

// L'ENVERS DES DÉCODEURS, pour les essais : le client n'émet pas ces paquets.
[[nodiscard]] std::vector<std::uint8_t> Encode(const InstanceCreatedNotice& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const InstanceWaitNotice& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const InstanceStatusNotice& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const InstanceChangeNotice& packet);

[[nodiscard]] std::vector<std::uint8_t> EncodeInstanceDestroyCommand();

}  // namespace ro::proto
