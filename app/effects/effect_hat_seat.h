#pragma once

#include <cstdint>

#include "ro/render/hat_effect_anchor.h"
#include "ro/sim/actor.h"

#include "effects/effect_map_memory.h"

namespace moonclient {

// LE PORTEUR TEL QUE L'ANCRE D'UN CHAPEAU LE LIT : son métier MONTRÉ — un corps
// monté porte son chapeau plus haut —, sa monture du shop, et ce que ses effets
// en cours font à la taille de son corps.
[[nodiscard]] ro::render::HatEffectBearer HatBearerOf(const ro::sim::Actor& actor,
                                                      std::uint32_t now_ms);

// POSE UN `.str` DE CHAPEAU À SON ANCRE, sur ce porteur à cet instant.
//
// 🔴 À CHAQUE IMAGE, ET NON À LA NAISSANCE SEULE : le client recalcule l'ancre
// à chaque mise à jour du fichier. Un porteur qui monte en selle, en descend ou
// rapetisse déplace aussitôt ce qu'il porte ; figée à la naissance, l'ancre
// attendait le resemis suivant. Rien pour un effet sans recette.
void ReseatHatStr(EffectMapMemory::Playing& playing, const ro::sim::Actor& bearer,
                  std::uint32_t now_ms);

}  // namespace moonclient
