#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// « JE SUIS PRÊT À JOUER » (`CZ_BLOCKING_PLAY_CANCEL 0x0447`), sans champ.
//
// Le client du jeu l'envoie à la fin du traitement de CHAQUE
// `ZC_MAPPROPERTY_R2 0x099B`, donc à chaque entrée sur une carte. Le serveur y
// répond par `ZC_NOTIFY_ACTORINIT 0x0B1B`, puis redit l'instance du joueur
// (`clif_loadConfirm` → `clif_instance_info`) : sans lui, la fenêtre
// d'instance ne se rafraîchit jamais à l'arrivée, et le serveur ne repose pas
// le mode d'instance qu'il exige pour accepter une destruction.
inline constexpr std::uint16_t kPlayReadyOpcode = 0x0447;
inline constexpr std::size_t kPlayReadyWireSize = 2;

// Refusé si le serveur ne déclare plus ce paquet tel quel : même gestionnaire,
// aucun champ, deux octets.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> EncodePlayReady();

}  // namespace ro::proto
