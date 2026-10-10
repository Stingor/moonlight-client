#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/core/error.h"
#include "ro/proto/map_packets.h"

namespace ro::proto {

// ── LA BARRE DE PROGRESSION D'UN SCRIPT ─────────────────────────────────────
//
// Les commandes `progressbar` et `progressbar_npc` du serveur. Trois des quatre
// paquets s'écrivent champ par champ côté serveur, sans structure que le
// générateur puisse lire ; le quatrième (`ZcProgressActor`) en a une, mais son
// opcode n'y est pas attaché.

// ZC_PROGRESS : une barre au-dessus du JOUEUR, et le script s'arrête jusqu'à
// ce que le client réponde `ProgressDone`.
struct ProgressStart {
  static constexpr std::uint16_t kOpcode = 0x02F0;
  static constexpr std::size_t kWireSize = 10;

  // La couleur que le script a demandée. Le client ne la montre pas : sa barre
  // est celle d'une incantation.
  std::uint32_t color = 0;
  // ⚠ EN SECONDES, et non en millisecondes.
  std::uint32_t seconds = 0;
};

[[nodiscard]] core::Result<ProgressStart> DecodeProgressStart(std::span<const std::uint8_t> frame);
[[nodiscard]] std::vector<std::uint8_t> EncodeProgressStart(const ProgressStart& start);

// CZ_PROGRESS : « la barre est finie », qu'elle soit allée au bout ou qu'elle
// ait été annulée. C'est le SERVEUR qui distingue les deux, à l'horloge : avant
// l'échéance qu'il a posée, il ferme le script ; après, il le reprend.
struct ProgressDone {
  static constexpr std::uint16_t kOpcode = 0x02F1;
  static constexpr std::size_t kWireSize = 2;
};

// ⚠ REFUSÉ si le serveur ne déclare plus le lire sous cet opcode et cette
// longueur : la réponse partirait dans le vide, et le script resterait arrêté.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> EncodeProgressDone();

// ZC_PROGRESS_CANCEL : le serveur retire la barre du joueur — un pas demandé,
// un objet utilisé, un coup reçu. Rien d'autre que l'opcode.
struct ProgressCancel {
  static constexpr std::uint16_t kOpcode = 0x02F2;
  static constexpr std::size_t kWireSize = 2;
};

[[nodiscard]] std::vector<std::uint8_t> EncodeProgressCancel();

// ZC_PROGRESS_ACTOR : une barre au-dessus d'un PNJ (`progressbar_npc`). Les
// champs sont ceux de `ZcProgressActor` — `time` en SECONDES restantes ;
// aucune réponse n'est attendue.
inline constexpr std::uint16_t kProgressActorOpcode = 0x09D1;

[[nodiscard]] std::vector<std::uint8_t> EncodeProgressActor(const ZcProgressActor& packet);

}  // namespace ro::proto
