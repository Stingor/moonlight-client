#pragma once

#include <cstdint>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/sim/packed_position.h"

namespace ro::net {

// ── LES ORDRES D'UN COMPAGNON : homoncule ou mercenaire ─────────────────────
//
//     0232 aller à une case    0233 attaquer    0234 revenir au maître
//
// Le serveur reconnaît le compagnon à son acteur, et ne répond rien : le
// compagnon bouge ou frappe, ou rien ne se passe. Les skills d'un compagnon
// passent par `RequestSkillOnTarget` et `RequestSkillOnGround`, que le serveur
// aiguille vers lui d'après l'identifiant du skill.

[[nodiscard]] core::Result<void> RequestCompanionMove(Connection& link, std::uint32_t companion_id,
                                                      sim::CellPosition cell);

// UN SEUL COUP : l'IA renouvelle l'attaque à chaque délai d'attaque.
[[nodiscard]] core::Result<void> RequestCompanionAttack(Connection& link,
                                                        std::uint32_t companion_id,
                                                        std::uint32_t target_id);

[[nodiscard]] core::Result<void> RequestCompanionReturn(Connection& link,
                                                        std::uint32_t companion_id);

}  // namespace ro::net
