#pragma once

#include <cstdint>

#include "ro/sim/world_fwd.h"

namespace ro::sim {

// Change l'apparence d'une unité au sol — un piège qui se déclenche prend celle
// du piège usé — et donne à son modèle l'ordre de jouer son animation. Une
// unité cachée reste muette : elle ne dit pas plus ce qu'elle devient que ce
// qu'elle était.
//
// Faux s'il n'y avait pas d'unité sous cet identifiant.
bool ChangeSkillUnitLook(World& world, std::uint32_t id, std::uint32_t unit_id,
                         std::uint32_t now_ms);

}  // namespace ro::sim
