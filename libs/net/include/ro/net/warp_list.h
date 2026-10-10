#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// LA LISTE DE DESTINATIONS d'une compétence de déplacement, et la réponse du
// joueur. Voir `ro/sim/warp_menu.h` pour ce que le client en retient.
//
// Le serveur l'envoie pour Teleport à partir du niveau 2, pour Warp Portal et
// pour Odin's Recall : `[opcode:2][longueur:2][compétence:2]`, puis une entrée
// de seize octets par destination, nom de fichier `.gat` compris.
[[nodiscard]] FrameOutcome ApplyWarpListFrame(const Frame& frame, sim::World& world);

// Ce qui précède la première destination.
inline constexpr std::size_t kWarpListHeaderSize = 6;
// Une destination sur le fil.
inline constexpr std::size_t kWarpListEntrySize = 16;
// ⚠ LE CLIENT N'EN GARDE QUE QUINZE : il pose un zéro sur le seizième octet
// avant de lire, si bien qu'un nom qui remplit tout le champ perd sa dernière
// lettre.
inline constexpr std::size_t kWarpListKeptNameSize = kWarpListEntrySize - 1;

// LE MOT QUI ANNULE. Le serveur le reconnaît tel quel à la place d'une carte,
// et oublie alors la compétence en attente.
inline constexpr std::string_view kWarpCancelName = "cancel";

// « Va là. » `destination` est le nom tel que la liste l'a donné, ou le mot
// d'annulation.
//
// ⚠ ÉMET, ET RIEN DE PLUS : fermer la question est l'affaire de l'appelant.
[[nodiscard]] core::Result<void> RequestWarpPoint(Connection& link, std::uint16_t skill_id,
                                                  std::string_view destination);

}  // namespace ro::net
