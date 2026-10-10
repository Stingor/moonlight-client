#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_pooled_node_shape.h"

namespace ro::render {

// UN EFFET QU'UN NŒUD FAIT NAÎTRE en se peignant.
//
// Trois familles de nœuds ne dessinent pas, ou pas seulement : leur figure
// fait naître un AUTRE effet, à chaque image, sur un acteur ou à un point du
// monde. `render` ne connaît aucun acteur et ne voit pas le monde : le nœud
// EXPRIME ici sa demande, et c'est `app` qui la satisfait — il résout
// l'acteur, et joue l'effet par le même chemin que les autres.
//
// ⚠ CHAQUE DEMANDE EST UNE INSTANCE NEUVE. Le client ne cherche pas si
// l'effet joue déjà sur l'acteur : il en bâtit un de plus à chaque peinture,
// et les instances s'empilent. C'est `app` qui décide ce que « de plus » veut
// dire pour une teinte d'acteur, qui ne s'empile pas chez nous.

// OÙ L'EFFET DEMANDÉ NAÎT.
enum class EffectSpawnPlace : std::uint8_t {
  // Sur l'acteur que le nœud désigne, à un ÉCART de sa place.
  kOnDesignatedActor,
  // À un point du monde, que personne ne porte.
  kAtPoint,
};

struct EffectSpawnRequest {
  std::uint32_t effect_id = 0;
  EffectSpawnPlace place = EffectSpawnPlace::kAtPoint;
  // L'IDENTIFIANT DE L'ACTEUR DÉSIGNÉ, rangé en FLOTTANT comme le nœud le
  // garde. Voir `DesignatedActorId`.
  float designated_actor = 0.0F;
  // Dans NOTRE repère : l'écart depuis l'acteur désigné, ou le point du monde.
  glm::vec3 position{0.0F, 0.0F, 0.0F};
};

// L'IDENTIFIANT D'UN ACTEUR, tel qu'un nœud le range : en flottant.
//
// ⚠ UN FLOTTANT NE TIENT QUE VINGT-QUATRE BITS. Au-delà de 16 777 216, un
// identifiant s'arrondit — au pair le plus proche, puis au multiple de quatre
// au-delà du double — et désigne alors un autre acteur, ou personne. Le serveur
// numérote ses monstres à la suite, de trois millions cent en montant, à chaque
// apparition : les leurs y passent. C'est le comportement reproduit, pas une
// imprécision.
[[nodiscard]] float StoredDesignatedActor(std::uint32_t actor_id);

// L'identifiant qu'un flottant rangé désigne, relu en entier sans signe et
// SATURÉ : négatif ou non numérique donne zéro, trop grand donne le maximum.
[[nodiscard]] std::uint32_t DesignatedActorId(float stored);

// VRAI QUAND LE FLOTTANT DÉSIGNE LE JOUEUR : la comparaison se fait EN
// FLOTTANT, avant toute recherche dans la liste des acteurs.
[[nodiscard]] bool DesignatesPlayer(float stored, std::uint32_t player_id);

// L'ACTEUR QU'UN IDENTIFIANT DÉSIGNE UNE FOIS RANGÉ EN FLOTTANT, tel que
// l'instance le relit : le joueur par la comparaison en flottant, les autres par
// l'identifiant relu en entier. Zéro quand il ne désigne personne.
[[nodiscard]] std::uint32_t DesignatedActorAfterStorage(std::uint32_t actor_id,
                                                        std::uint32_t player_id);

// VRAI POUR UNE FORME QUI NE DESSINE RIEN : elle ne fait que demander des
// effets. La composition la saute, sans la compter parmi les muettes.
[[nodiscard]] bool EffectNodeShapeOnlySpawns(PooledNodeShape shape);

// LES DEMANDES QU'UNE PEINTURE DE CE NŒUD ÉMET, ajoutées à `out`. Rien pour
// une forme qui ne fait naître aucun effet.
void CollectEffectSpawnRequests(const PooledEffectNode& pooled,
                                std::vector<EffectSpawnRequest>& out);

}  // namespace ro::render
