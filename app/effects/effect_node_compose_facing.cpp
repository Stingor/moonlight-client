#include "ro/render/camera.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_node_world.h"

// L'ORIENTATION D'UNE FIGURE DU MONDE, sortie de la boucle de composition de
// `effect_node_compose.cpp`, qui la déclare.
namespace moonclient {

// `yaw_degrees` est le cap du nœud tel qu'il se peint : celui du tic, ou entre
// deux tics — voir `ro::render::EffectNodeDrawnYawDegrees`.
ro::render::EffectRingOrientation EffectNodeFacingOf(const ro::render::PooledEffectNode& pooled,
                                                     const ro::render::Camera& camera,
                                                     float yaw_degrees) {
  // ⚠ CHAQUE FIGURE SON ORIENTATION. Le mouvement du quad orienté bâtit sa
  // matrice à partir de la CAMÉRA ; les anneaux, eux, la tirent des DEUX
  // ANGLES de leur nœud.
  //
  // 🔴 ET UN ANNEAU N'EST PAS TOUJOURS COUCHÉ. Tant que ces angles n'étaient
  // pas portés, toute figure du monde restait à plat : l'anneau bleu d'un
  // impact, que le client dresse d'un quart de tour, se voyait par la
  // tranche. ⚠ SANS GARDE — le jeu ORIENTE toujours, il ne « mesure » rien ;
  // deux angles nuls rendent l'identité, qui EST l'orientation couchée.
  //
  // 🔴 MAIS LE TRONC DE CÔNE EST DÉJÀ DEBOUT, ET SON QUART DE TOUR LE
  // COUCHERAIT. Le client écrit ses deux cercles dans un plan que la matrice
  // du nœud redresse ensuite — une rotation autour de l'abscisse,
  // d'exactement quatre-vingt-dix degrés. Notre figure, elle,
  // est bâtie DEBOUT : deux cercles horizontaux, l'un au plan du nœud,
  // l'autre à sa demi-hauteur. Lui appliquer l'inclinaison de la table la
  // recouche, et l'anneau devient un cercle VERTICAL centré aux pieds —
  // dont la moitié passe sous le sol. Vu en jeu sur EF_WARP.
  //
  // ⚠ LES ONZE PATRONS DE CETTE FIGURE PORTENT QUATRE-VINGT-DIX, SANS
  // EXCEPTION. C'est la signature d'une convention de repère, non d'un choix
  // par effet : le quart de tour se replie donc dans la figure, comme celui
  // des pointes en croix (`effect_cross_spikes.h`), et l'inclinaison du
  // client reste dans la table — c'est une donnée, elle ne se retouche pas.
  const float quart_de_tour = pooled.shape == ro::render::PooledNodeShape::kRingCylinder
                                  ? ro::render::kEffectRingCylinderUprightDegrees
                                  : 0.0F;
  //
  // ⚠ ET LES ANGLES PASSENT PAR LES TABLES DU CLIENT, tronqués, un degré de
  // travers quand ils sont négatifs : voir `EffectNodeOrientedAsClient`.
  // 🔴 LE QUAD ORIENTÉ A TROIS RÈGLES, dans cet ordre : altitude de la caméra (0x200), face à
  // la caméra (0x40), angles du nœud. Voir `kEffectOrientedQuadLeansByCameraAltitude`.
  const bool oriented_quad = pooled.shape == ro::render::PooledNodeShape::kOrientedQuad;
  const bool leans_by_camera_altitude =
      oriented_quad && ro::render::EffectOrientedQuadLeansByCameraAltitude(pooled.node.flags);
  const bool face_camera = oriented_quad && ro::render::EffectNodeFacesCamera(pooled.node.flags);
  // Sous 0x400, le lacet suit l'angle du corps : voir `EffectOrientedQuadYawDegrees`.
  return leans_by_camera_altitude
             ? ro::render::EffectNodeLeaningByCameraAltitude(camera, pooled.node_body_degrees)
         : face_camera   ? ro::render::EffectNodeFacingCamera(camera, pooled.node_pitch_degrees)
         : oriented_quad ? ro::render::EffectNodeOrientedAsClient(
                               ro::render::EffectOrientedQuadYawDegrees(
                                   pooled.node.flags, yaw_degrees, pooled.node_body_degrees,
                                   pooled.node_yaw_step),
                               pooled.node_pitch_degrees, pooled.node_roll_degrees)
                         : ro::render::EffectNodeOrientedAsClient(
                               yaw_degrees, pooled.node_pitch_degrees, 0.0F, quart_de_tour);
}

}  // namespace moonclient
