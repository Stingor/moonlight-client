#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "ro/render/hat_effect_anchor.h"
#include "ro/sim/actor.h"

#include "effects/effect_map_memory.h"

namespace moonclient {

// LE PORTEUR TEL QUE L'ANCRE D'UN CHAPEAU LE LIT : son métier MONTRÉ — un corps
// monté porte son chapeau plus haut —, sa monture du shop, ce que ses effets en
// cours font à la taille de son corps, et où sa tête est passée à cette image
// (`EffectActorView::HeadShiftOf`).
[[nodiscard]] ro::render::HatEffectBearer HatBearerOf(const ro::sim::Actor& actor,
                                                      glm::vec2 head_shift_pixels,
                                                      std::uint32_t now_ms);

// POSE UN `.str` DE CHAPEAU À SON ANCRE, sur ce porteur à cet instant.
//
// 🔴 À CHAQUE IMAGE, ET NON À LA NAISSANCE SEULE : le client recalcule l'ancre
// à chaque mise à jour du fichier. Un porteur qui monte en selle, en descend ou
// rapetisse déplace aussitôt ce qu'il porte ; figée à la naissance, l'ancre
// attendait le resemis suivant. Et une tête qui s'assied ou frappe emporte ce
// qui la suit. Rien pour un effet sans recette.
void ReseatHatStr(EffectMapMemory::Playing& playing, const ro::sim::Actor& bearer,
                  glm::vec2 head_shift_pixels, std::uint32_t now_ms);

}  // namespace moonclient
