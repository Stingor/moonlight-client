#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_screen_node.h"

// CE QUE LA TROUPE DE NŒUDS (`EffectNodesInPlay`) TIENT POUR UNE IMAGE ET POUR
// SES NŒUDS D'ÉCRAN : deux enregistrements sans comportement, rangés à part de
// la classe qui les remplit.
namespace moonclient {

// OÙ UN ACTEUR COMPOSÉ SE POSE, et avec quelle teinte — un par DESSIN et non
// un par acteur. Un sprite à traînée se dessine plusieurs fois, au même
// acteur, à des points et des alphas différents ; ranger le point à côté de
// l'acteur interdirait ce cas.
//
// ⚠ LE POINTEUR VERS L'ACTEUR SE POSE EN DERNIER. La liste des acteurs n'a pas
// fini de grandir quand on remplit celle-ci ; le prendre au fil des ajouts le
// laisserait pendre à la première réallocation.
struct EffectComposedPlacement {
  std::size_t composed_index = 0;
  glm::vec3 ground_position{0.0F, 0.0F, 0.0F};
  std::array<std::uint8_t, 4> tint = {255, 255, 255, 255};
  float scale = 1.0F;
  // Le mot de rendu du nœud posé. Voir `SpriteBatches`.
  std::optional<std::uint32_t> render_word;
  float grow_screen_pixels = 0.0F;  // voir `DrawnActor::grow_screen_pixels`
};

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
};

}  // namespace moonclient
