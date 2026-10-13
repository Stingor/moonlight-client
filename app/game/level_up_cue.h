#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "ro/net/world_update.h"
#include "ro/sim/world.h"

namespace moonclient {

// LA MONTÉE DE NIVEAU DU JOUEUR, VUE ET ENTENDUE.
//
// Lit le nouveau niveau de base ou de métier qu'annonce ZC_PAR_CHANGE, sème
// sur le joueur l'effet de sa classe, et rend le son à jouer — `levelup.wav`
// pour le niveau de base, rien pour celui de métier. Voir
// ro/sim/level_up_effect.h.
//
// ⚠ À APPELER APRÈS LE MONDE : la trame a déjà rangé le nouveau niveau, et
// cette lecture ne la réclame pas — elle ne change donc pas son issue.
[[nodiscard]] std::optional<std::string_view> CueLevelUp(const ro::net::Frame& frame,
                                                         ro::sim::World& world,
                                                         std::uint32_t now_ms);

}  // namespace moonclient
