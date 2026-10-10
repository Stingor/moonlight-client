#pragma once

#include <cstdint>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/item_news.h"

namespace ro::net {

// ── LES NOUVELLES D'OBJETS, TIRÉES DU FIL ───────────────────────────────────
//
// Deux trames qui ne changent pas le sac et ont pourtant quelque chose à dire :
//
//   · `ZC 0x0B67`, un membre du groupe a ramassé un objet ;
//   · `ZC 0x02BB`, une pièce d'équipement s'est abîmée. Le sac la lit aussi,
//     pour marquer la pièce : voir `ApplyFrame`.
//
// Elles sont déposées telles quelles dans `sim::ItemNews` ; c'est l'application
// qui sait nommer l'objet et le membre. Ce qui n'est pas l'une d'elles rend
// `kUnhandled`.
[[nodiscard]] FrameOutcome ApplyItemNewsFrame(const Frame& frame, sim::ItemNews& news,
                                              std::uint32_t now_ms);

}  // namespace ro::net
