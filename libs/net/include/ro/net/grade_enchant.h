#pragma once

#include <cstdint>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/grade_enchant.h"
#include "ro/sim/inventory.h"

namespace ro::net {

// L'AMÉLIORATION DE GRADE, SUR LE FIL.
//
// L'ouverture de l'interface des grades, la proposition et le résultat sont à
// elle ; une ouverture d'une autre interface ne l'est pas (`kUnhandled`). Les
// changements de carte, elle les lit sans les prendre : ils la ferment en silence.
//
// ⚠ LE SAC CHANGE AVEC LE RÉSULTAT : une réussite ramène la pièce à +0 et lui
// donne son nouveau grade, que le serveur ne renvoie pas. Une pièce détruite, elle,
// quitte le sac par sa propre trame.
[[nodiscard]] FrameOutcome ApplyGradeEnchantFrame(const Frame& frame, sim::GradeEnchant& grade,
                                                  sim::Inventory& bag);

// Demande au serveur ce que coûte la pièce qu'on vient de choisir, une fois.
void AskGradeEnchantOffer(Connection& link, sim::GradeEnchant& grade);

// « Améliore la pièce choisie. » Refuse d'émettre ce que
// `sim::CheckGradeEnchantRequest` rejette : le serveur ne répondrait pas.
[[nodiscard]] core::Result<void> RequestGradeEnchant(Connection& link, sim::GradeEnchant& grade,
                                                     sim::GradeEnchantAttempt attempt,
                                                     const sim::Inventory& bag, std::int64_t zeny);

// Émet la fermeture due (`sim::GradeEnchant::TakeFarewell`).
void SayGradeEnchantFarewell(Connection& link, sim::GradeEnchant& grade);

}  // namespace ro::net
