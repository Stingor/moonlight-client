#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "ro/render/effect_node.h"
#include "ro/render/effect_sprite_draw_order.h"

namespace ro::render {

// LA PLACE ET LE CAP D'UN EXEMPLAIRE, fixés par son rang dans la boucle du
// bâtisseur : ni tirés, ni répartis. Les trois pointes de pierre en trépied en
// ont chacune une. L'écart est dans le repère du CLIENT — abscisse, profondeur
// — et le cap est l'angle qu'il écrit ; la traduction se fait au semis.
struct EffectCopyPlacement {
  float offset_x = 0.0F;
  float offset_z = 0.0F;
  float yaw_degrees = 0.0F;
};
inline constexpr std::size_t kEffectCopyPlacementCapacity = 3;

// « CE PATRON NE SÈME RIEN SOUS `/mineffect` » : la valeur que prend la
// période des effets allégés quand le bâtisseur s'abstient tout à fait.
inline constexpr std::int32_t kEffectNodeNotSownWithMinimalEffects = -1;

// CE QUE LES BÂTISSEURS DE PROJECTILES ET D'ANNEAUX ÉCRIVENT ENCORE, et que le
// reste du patron ne sait pas dire. Tout à son défaut : rien ne change.
//
// ⚠ UN SEUL CHAMP DANS LE PATRON, et c'est voulu : un agrégat positionnel qui
// reçoit une structure à la place d'un nombre ne compile pas, là où un champ
// isolé mal placé se lirait comme son voisin.
struct EffectNodeSupplement {
  // LA PÉRIODE SOUS `/mineffect` : zéro garde celle du patron, un nombre la
  // remplace, `kEffectNodeNotSownWithMinimalEffects` supprime le semis.
  std::int32_t period_ticks_with_minimal_effects = 0;
  // LE FONDU RAPPORTÉ À LA VIE TIRÉE : la sortie tombe à `vie − vie / diviseur`,
  // en entiers, calculée après le tirage de la vie. Zéro : la sortie du patron.
  std::int32_t fade_out_life_divisor = 0;
  // L'INCLINAISON NE PLACE QUE LE DÉPART : le bâtisseur incline le décalage de
  // naissance, puis rebâtit la matrice du vol sur le seul cap — la particule
  // file à plat.
  bool launch_pitch_places_only = false;
  // L'ORBITE ACCÉLÈRE : ajouté au pas angulaire AVANT que le cap n'avance.
  float orbit_acceleration_per_tick = 0.0F;
  // LE CAP DE DÉPART DE L'ORBITE PAR RANG : l'exemplaire k part à l'angle du
  // client `k × pas`, porté par la troisième ligne de sa rotation de lacet.
  // Zéro : le cap se tire.
  float copy_orbit_step_degrees = 0.0F;
  // L'ACCÉLÉRATION VERTICALE TIRÉE, dans le signe du client, bornes comprises
  // et avec son nombre de valeurs. Deux bornes nulles : celle qui se déduit
  // de `fall_acceleration_share`.
  float fall_acceleration_low = 0.0F;
  float fall_acceleration_high = 0.0F;
  std::int32_t fall_acceleration_values = 0;
  // LES TIRAGES QUE LE BÂTISSEUR JETTE avant ceux du lancer.
  std::uint8_t launch_draws_discarded = 0;
  // LA SECONDE IMAGE-CLÉ DU RAYON D'UNE COURONNE : voir
  // `EffectNode::second_ring_radius_growth`.
  float second_ring_radius_growth = kEffectNodeKeyframeUnset;
  float second_ring_radius_acceleration = kEffectNodeKeyframeUnset;
  // LES IMAGES-CLÉS D'ALPHA : voir `EffectAlphaKeyframe`.
  std::array<EffectAlphaKeyframe, kEffectAlphaKeyframeCapacity> alpha_keyframes{};
  // LA PLACE ET LE CAP DE CHAQUE EXEMPLAIRE, par rang ; zéro entrée : aucun.
  std::array<EffectCopyPlacement, kEffectCopyPlacementCapacity> copy_placements{};
  std::uint8_t copy_placement_count = 0;
  // LE CAP TIRÉ PAR SECTEUR QUI S'ÉLARGIT : l'exemplaire k (à partir de zéro)
  // prend `min(plafond, secteur × k + tirage(secteur × (k + 1)))`, angle du
  // client. Zéro : pas de tel cap.
  float copy_yaw_sector_degrees = 0.0F;
  float copy_yaw_ceiling_degrees = 0.0F;
  // LA MARQUE DE LA CROIX DE POINTES : trois la peint d'un bleu additif retiré
  // à chaque image. Voir `EffectCrossSpikeRequest::draws_its_own_color`.
  std::uint8_t cross_spike_mark = 0;
  // LES TIRAGES QUE LE BÂTISSEUR JETTE À LA PLACE DE L'ÉCHELLE : un cercle
  // calculé puis écrasé, ou une échelle dont la division entière rend toujours
  // la même valeur. Ils avancent le générateur juste avant le tirage de
  // l'échelle.
  std::uint8_t draws_discarded_before_scale = 0;
  // LE LACET SE TIRE AVANT LE PENCHANT : le bâtisseur écrit 780 puis 784.
  bool yaw_drawn_before_lean = false;
  // L'ORDRE DES TIRAGES D'UN SPRITE : voir `EffectSpriteDraw`. Vide : l'ordre
  // du semis.
  EffectSpriteDrawOrder sprite_draw_order;
  // CE NŒUD EST LE PROJECTILE DE SON EFFET : l'instance garde ses deux angles
  // — calculés de sa place vers le point visé — et sa place courante. Voir
  // `EffectClientArrowAngles`.
  bool records_projectile = false;
  // CE NŒUD PART DU DERNIER PROJECTILE : il naît à sa place courante, et son
  // cap et son inclinaison tirés s'ajoutent aux siens.
  bool launches_from_last_projectile = false;
  // LA PROFONDEUR SE TIRE AVANT L'ABSCISSE, pour la place comme pour le point
  // visé : le bâtisseur écrit 924 puis 916.
  bool offsets_drawn_depth_first = false;
  // L'ANGLE DU BRAS, ÉCRIT FIXE par le bâtisseur au lieu d'être tiré : l'angle
  // du client, en degrés.
  bool arm_is_fixed = false;
  float arm_client_degrees = 0.0F;
  // LE PAS DU BRAS REMPLACÉ À UN ÂGE DU NŒUD : la gerbe qui monte en spirale
  // s'élargit, puis se resserre. Un âge négatif : aucun.
  std::int32_t orbit_radius_step_switch_tick = -1;
  float orbit_radius_step_after_switch = 0.0F;
  // LE SPRITE SE TIENT SUR LE POINT VISÉ, et non au point de son effet : le
  // mouvement l'y repose à chaque tic. Un effet né sur le lanceur et visant sa
  // cible se dessine donc sur la cible. Son action, elle, garde la direction
  // du point de l'effet vers le point visé.
  bool sits_on_aim_point = false;
  // LA VIE QUE L'INSTANCE SE RÉCRIT À CHAQUE SEMIS : `base − âge`, tous les
  // `période` tics. Zéro : elle ne la récrit pas. Le fondu du nœud s'engage
  // alors au premier semis où son reste dépasse celui de l'instance plus un.
  // Voir `EffectNodeFadesWithARewritingInstance`.
  std::int32_t instance_life_rewrite_base = 0;
  std::int32_t instance_life_rewrite_period_ticks = 0;
  // L'ÂGE OÙ LE BÂTISSEUR RAMÈNE SON INSTANCE À ZÉRO : l'horaire du patron se
  // rejoue alors sans fin, et l'instance ne meurt jamais de sa durée. Zéro :
  // l'âge ne repart pas. Voir `EffectNodeSchedule::cycle_ticks`.
  std::int32_t age_wrap_ticks = 0;
  // PASSÉ SA DERNIÈRE IMAGE, LE SPRITE REVIENT À LA PREMIÈRE ET S'Y TIENT, sans
  // mourir : son mouvement ne lit pas ce que l'avance d'image lui rend, et
  // l'image d'un rang au-delà de l'action est la première. Voir
  // `EffectNode::rests_on_first_frame`.
  bool rests_on_first_frame = false;
};

}  // namespace ro::render
