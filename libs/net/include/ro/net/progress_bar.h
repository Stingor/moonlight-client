#pragma once

#include <cstdint>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/progress_bar.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// LA BARRE DE PROGRESSION D'UN SCRIPT, dans les deux sens — voir
// `ro/sim/progress_bar.h`.

// ZC_PROGRESS, ZC_PROGRESS_ACTOR et ZC_PROGRESS_CANCEL. `now_ms` date le départ
// de la barre dans l'horloge de l'appelant. Rend `kUnhandled` pour toute autre
// trame.
[[nodiscard]] FrameOutcome ApplyProgressFrame(const Frame& frame, sim::World& world,
                                              sim::ProgressReport& report, std::uint32_t now_ms);

// CZ_PROGRESS, s'il est dû : la barre du joueur est échue, ou le serveur l'a
// annulée. À appeler à chaque image, APRÈS les trames : une annulation reçue
// part dans la même image.
//
// 🔴 UNE SEULE FOIS PAR BARRE. Une réponse de trop, arrivée avant l'échéance
// d'une barre suivante, la prendrait pour annulée et fermerait son script.
[[nodiscard]] core::Result<void> ReportProgressDone(Connection& link, const sim::World& world,
                                                    sim::ProgressReport& report,
                                                    std::uint32_t now_ms);

}  // namespace ro::net
