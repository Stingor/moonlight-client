#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/actor_effect_table.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/sprite_renderer.h"

#include "world/actor_ghosts.h"

namespace moonclient {

// LE CORPS DU PANTIN, tel qu'il est composé à cette image, et la durée d'un
// tour de son animation — ce que vit une rémanence.
struct DollBody {
  ro::render::ComposedActor composed;
  float animation_duration_ms = 0.0F;
};

// LE CORPS DU PANTIN QU'UN EFFET DE CHAPEAU CHANGE, tel que la partie le
// peindrait : rapetissé, teint, aux calques additifs, suivi de ses rémanences.
//
// 🔴 L'IMAGE COMPOSÉE DU PANTIN NE SAIT RIEN DE CELA. Elle est un corps nu,
// posé par l'interface en recouvrement ; un costume qui rapetisse son porteur,
// l'éclaire ou sème ses ombres ne s'y voyait donc pas. Le porteur factice de
// `DollEffects` reçoit ces effets comme n'importe quel acteur, et ce qu'ils
// font au corps se lit ici, par les mêmes tables que la partie.
//
// Remplit `out` — les rémanences d'abord, puis le corps, à peindre dans cet
// ordre — et rend vrai quand l'un des effets joués change l'acteur. Rend faux
// et laisse `out` vide sinon : l'image composée suffit.
//
// `feet` est la place du porteur, où la caméra du pantin pose ses pieds.
// ⚠ LES RÉMANENCES COPIENT LE CORPS, mais le corps lui-même est désigné par
// adresse : `body` doit vivre jusqu'à la peinture.
[[nodiscard]] bool ComposeChangedDollBody(std::span<const ro::render::ActorEffectPlay> played,
                                          const DollBody& body, std::uint32_t bearer_id,
                                          glm::vec3 feet, ActorGhosts& ghosts, std::uint32_t now_ms,
                                          std::vector<ro::render::DrawnActor>& out);

}  // namespace moonclient
