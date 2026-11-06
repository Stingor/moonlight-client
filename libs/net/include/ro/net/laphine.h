#pragma once

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/inventory.h"
#include "ro/sim/laphine.h"

namespace ro::net {

// LE LAPHINE, SUR LE FIL.
//
// Les ouvertures et les résultats ne sont qu'à lui. Les changements de carte, il
// les lit sans les prendre (`kUnhandled`) : ils le ferment en silence, le serveur
// ayant déjà désarmé.
//
// ⚠ L'AMÉLIORATION RÉUSSIE ARRIVE EN DEUX TEMPS : la pièce changée d'abord, sur
// `bag`, puis le résultat qui ferme.
[[nodiscard]] FrameOutcome ApplyLaphineFrame(const Frame& frame, sim::Laphine& laphine,
                                             sim::Inventory& bag);

// « Fais-le avec ce qui est déposé. » La forme dépend de l'espèce : la synthèse
// liste chaque pile et la quantité que la recette en prend, l'amélioration ne
// nomme que la pièce.
//
// Refuse d'émettre ce que `sim::CheckLaphineRequest` rejette : plusieurs refus du
// serveur sont muets, et la fenêtre attendrait pour rien.
[[nodiscard]] core::Result<void> RequestLaphineMake(Connection& link, sim::Laphine& laphine,
                                                    const sim::LaphineRecipe& recipe,
                                                    const sim::Inventory& bag);

// Émet toutes les fermetures dues (`sim::Laphine::TakeFarewell`).
void SayLaphineFarewells(Connection& link, sim::Laphine& laphine);

}  // namespace ro::net
