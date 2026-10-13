#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_screen_node.h"

namespace moonclient {

// UN NŒUD D'ECRAN EN VIE : son etat, son image, et OU SON PORTEUR SE TIENT
// dans le monde.
//
// ⚠ LA POSITION DU PORTEUR EST GARDEE EN COORDONNEES DU MONDE, et projetee a
// chaque image : la projeter une fois pour toutes au semis figerait le nœud a
// l'ecran des que la camera bouge.
struct EffectScreenNodeInPlay {
  ro::render::EffectScreenNode node;
  // ⚠ PLUSIEURS IMAGES, ET L'INDEX DE VUE CHOISIT. Un batisseur qui COMPOSE le
  // nom de son image en pose huit, et l'index du noeud tourne dessus. N'en
  // porter qu'une figerait l'explosion sur sa premiere.
  std::vector<std::string> texture_paths;
  glm::vec3 owner_position{0.0F, 0.0F, 0.0F};
  bool beats_every_frame = false;  // voir `EffectBeatsEveryFrame`
  // Voir `PooledEffectNode::instance_beats_left`.
  std::int32_t instance_beats_left = -1;
  bool instance_beats_every_frame = false;  // voir `EffectInstanceBeatsEveryFrame`
  std::uint32_t owner_actor_id = 0;         // qui le porte, ou zéro : voir `FollowActor`
  bool from_hat = false;                    // voir `SowAsHat`
  // LE NUMÉRO DU NŒUD, croissant et jamais repris, posé à son premier battement ;
  // zéro tant qu'il n'en a pas. Voir `EffectNodeSteps`.
  std::uint64_t serial = 0;
};

}  // namespace moonclient
