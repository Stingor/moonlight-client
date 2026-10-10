#pragma once

#include <cstdint>

#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// UN FLAG GRAFFITI POSÉ AU SOL : `ZC_MOONLIGHT_FLAG_GRAFFITI 0x0F36`.
//
//     0f36 <len>.W <unité>.L <lanceur>.L <x>.W <y>.W <guilde>.L <version>.L <niveau>.B
//
// L'emblème de la guilde du lanceur, pris à la pose, que le client peint sur
// (2 × niveau + 1)² cases autour de l'unité. Le serveur ne l'envoie qu'à qui a
// annoncé savoir le peindre — voir `kUiCapFlagGraffiti` — et le retire par le
// retrait ordinaire des unités au sol.
[[nodiscard]] FrameOutcome ApplyFlagGraffitiFrame(const Frame& frame, sim::World& world,
                                                  std::uint32_t now_ms);

}  // namespace ro::net
