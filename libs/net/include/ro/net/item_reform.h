#pragma once

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/inventory.h"
#include "ro/sim/item_reform.h"

namespace ro::net {

// LA RÉFORME, SUR LE FIL.
//
// L'ouverture et le résultat ne sont qu'à elle. Les changements de carte, elle les
// lit sans les prendre (`kUnhandled`) : ils la ferment en silence, le serveur ayant
// déjà désarmé.
//
// ⚠ LA RÉUSSITE ARRIVE EN TROIS TEMPS : la pièce retirée, la pièce réformée remise
// à la même case, puis le résultat. Les deux premiers sont au sac.
[[nodiscard]] FrameOutcome ApplyItemReformFrame(const Frame& frame, sim::ItemReform& reform);

// « Réforme la pièce choisie. » Refuse d'émettre ce que
// `sim::CheckItemReformRequest` rejette : le serveur ne répondrait pas, et la
// fenêtre attendrait pour rien.
[[nodiscard]] core::Result<void> RequestItemReform(Connection& link, sim::ItemReform& reform,
                                                   const sim::ItemReformTrigger& trigger,
                                                   const sim::Inventory& bag);

// Émet la fermeture due (`sim::ItemReform::TakeFarewell`).
void SayItemReformFarewell(Connection& link, sim::ItemReform& reform);

}  // namespace ro::net
