// LE SEMIS D'UN NŒUD D'ANNEAU — sa place, son orientation, et ce qui le fait
// partir.
//
// ⚠ À PART DU RESTE, ET C'EST LE PLAFOND DE HUIT CENTS LIGNES QUI L'A DÉCIDÉ.
// La couture suit une frontière réelle pour autant : une figure d'anneau ne se
// sème pas comme une troupe de sprites — elle n'a ni dispersion, ni exemplaires
// tirés au sort, et elle porte DEUX ANGLES que la figure lit comme une
// orientation là où une particule y voit une direction (ADR 0017).
//
// C'est la même coupure que celle de la voie d'écran, et pour la même raison :
// ce qui ne partage ni troupe ni générateur n'a pas à partager de fichier.

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_launch.h"
#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_node_sprite_seed.h"
#include "ro/render/effect_node_world.h"
#include "ro/render/effect_ring_flight.h"
#include "ro/render/pvp_rank_tint.h"

#include "effects/effect_node_view.h"

namespace moonclient {
namespace {

// LE TIRAGE QUI ÉCARTE LA VISÉE D'UN NŒUD : `rand() % 4 − 2`, soit de moins deux
// à plus un sur chaque axe du sol — c'est ce qui fait que deux flèches ne
// tombent pas exactement au même point.
constexpr std::uint32_t kAimJitterSpan = 4;
constexpr float kAimJitterBias = 2.0F;
// LE TOUR, en degrés entiers : un cap tiré `rand() % 360`.
constexpr std::int32_t kEffectRingTurnDegrees = 360;
// Le quart de tour qui sépare le cap du client de celui de l'axe d'une pointe.
constexpr float kQuarterTurnDegrees = 90.0F;

}  // namespace

bool EffectNodesInPlay::SowRing(const ro::render::EffectNodePattern& pattern,
                                glm::vec3 ground_position, std::int32_t life_left_ticks,
                                std::int32_t effect_tick, float owner_facing_degrees,
                                std::int32_t param,
                                const std::optional<glm::vec3>& target_position) {
  // TOUTES les textures du patron, dans l'ordre du défilement. La PREMIÈRE
  // décide : un nœud dont l'image de tête ne se lit pas n'a rien à montrer.
  std::vector<std::string> paths;
  for (const std::string_view path : ro::render::EffectNodePatternTextures(pattern)) {
    paths.emplace_back(path);
    TextureFor(paths.back());
  }
  if (paths.empty() || TextureFor(paths.front()).empty()) {
    return false;
  }

  // ⚠ UN PATRON PEUT SE SEMER PLUSIEURS FOIS. Le client pose vingt-cinq pics
  // de terre en grille pour l'effet 142 et sept pour le 79 ; n'en poser qu'un
  // donnait une figure juste et un effet méconnaissable. Voir
  // `EffectRingPattern::instance_count`.
  //
  // LA GRAINE VIENT DE LA TROUPE DÉJÀ SEMÉE, comme pour les autres semis : le
  // patron peut TIRER sa demi-largeur, son penchant et sa place, et deux
  // exemplaires doivent alors différer. Une graine fixe les empilerait.
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  const std::uint16_t count = std::max<std::uint16_t>(pattern.ring.instance_count, 1);
  for (std::uint16_t exemplaire = 0; exemplaire < count; ++exemplaire) {
    if (!SowOneRing(pattern, ground_position, life_left_ticks, effect_tick, paths, exemplaire,
                    count, generator, owner_facing_degrees, param, target_position)) {
      return false;
    }
  }
  return true;
}

bool EffectNodesInPlay::SowOneRing(const ro::render::EffectNodePattern& pattern,
                                   glm::vec3 ground_position, std::int32_t life_left_ticks,
                                   std::int32_t effect_tick, const std::vector<std::string>& paths,
                                   std::uint16_t exemplaire, std::uint16_t count,
                                   std::uint32_t& generator, float owner_facing_degrees,
                                   std::int32_t param,
                                   const std::optional<glm::vec3>& target_position) {
  ro::render::PooledEffectNode pooled;
  switch (pattern.shape) {
    case ro::render::EffectNodeShape::kRingCylinder:
      pooled.shape = ro::render::PooledNodeShape::kRingCylinder;
      break;
    // Les sous-types 15 et 16 partagent leur mouvement, qui n'ajoute le pas au
    // cap qu'UNE fois par tic : à deux, le réticule d'EF_LOCKON tournerait deux
    // fois trop vite.
    case ro::render::EffectNodeShape::kCrossedPlanes:
      pooled.shape = ro::render::PooledNodeShape::kCrossedPlanes;
      pooled.yaw_steps_per_tick = 1.0F;
      break;
    case ro::render::EffectNodeShape::kOrientedQuad:
      pooled.shape = ro::render::PooledNodeShape::kOrientedQuad;
      pooled.yaw_steps_per_tick = 1.0F;
      break;
    case ro::render::EffectNodeShape::kCrossSpikes:
      pooled.shape = ro::render::PooledNodeShape::kCrossSpikes;
      break;
    case ro::render::EffectNodeShape::kUvSphere:
      pooled.shape = ro::render::PooledNodeShape::kUvSphere;
      // Le sous-type 14 n'écrit le cap qu'UNE fois par tic, là où le tronc de
      // cône l'écrit deux fois.
      pooled.yaw_steps_per_tick = 1.0F;
      break;
    default:
      pooled.shape = ro::render::PooledNodeShape::kRingBand;
      break;
  }
  // PLUSIEURS EXEMPLAIRES PAR PLACE : la place est celle de la case, et le rang
  // dans la case choisit l'image quand les textures se distribuent. Voir
  // `EffectRingPattern::copies_per_place`.
  const std::uint16_t par_place = std::max<std::uint16_t>(pattern.ring.copies_per_place, 1);
  const auto rank_in_place = static_cast<std::size_t>(exemplaire % par_place);
  if (pattern.textures_per_copy && paths.size() > 1) {
    const std::size_t image =
        par_place > 1 ? rank_in_place * paths.size() / par_place : exemplaire % paths.size();
    pooled.resource_paths = {paths[image]};
  } else {
    pooled.resource_paths = paths;
  }
  // LES DEUX ANGLES DU NŒUD, dont la figure tirera son orientation. Ce sont les
  // mêmes que ceux d'un lancer : un anneau les lit comme une ORIENTATION, une
  // particule comme une DIRECTION.
  //
  // 🔴 ET LE LACET EST PARFOIS CELUI DU PORTEUR, sans écart tiré : le bâtisseur
  // y écrit l'angle de base tel quel. Mais SEULEMENT parfois — voir
  // `node_yaw_follows_owner`, qui le dit patron par patron. Un anneau dont le
  // bâtisseur écrit son propre lacet tournerait avec l'acteur sans raison.
  //
  // ⚠ ET LE CAP CHANGE DE REPÈRE EN CHEMIN. Le patron porte l'angle du CLIENT,
  // la figure veut celui de NOTRE monde. Voir `EffectClientYawInWorld`.
  //
  // 🔴 ET LE CAP DU PORTEUR N'EST PAS TOUJOURS L'ANGLE DE BASE : certains
  // bâtisseurs y écrivent l'angle du CORPS, son symétrique. Voir
  // `EffectRingPattern::yaw_is_owner_body_angle`.
  const float followed_heading =
      pattern.ring.yaw_is_owner_body_angle
          ? ro::render::kEffectBodyAngleMirrorDegrees - owner_facing_degrees
          : owner_facing_degrees;
  pooled.node_yaw_degrees = ro::render::EffectClientYawInWorld(
      pattern.launch_yaw_low + (pattern.node_yaw_follows_owner ? followed_heading : 0.0F));
  pooled.node_pitch_degrees = pattern.launch_pitch_low;
  pooled.node_roll_degrees = pattern.ring.roll_degrees;
  // ⚠ ET LE CAP TOURNE, quand le patron le dit. Il n'y a PAS de changement de
  // repère ici : un pas est une différence d'angles, et les deux repères
  // tournent en sens opposés — le signe du pas suit donc celui du cap, que
  // `EffectClientYawInWorld` a déjà retourné.
  pooled.node_yaw_step = -pattern.ring.yaw_step_degrees;
  pooled.node_yaw_step_change = -pattern.ring.yaw_step_change;
  // ⚠ LA PLACE D'UN EXEMPLAIRE S'AJOUTE AU POINT DE L'EFFET, et elle se tire
  // pour une couronne. Voir `EffectNodeInstanceOffset`.
  // ⚠ ET L'ENFOUISSEMENT DESCEND, là où le client l'écrit en montant : son
  // ordonnée va vers le bas. Recopier son signe ferait naître la pointe en
  // l'air. Voir `EffectRingPattern::burial_depth`.
  // ⚠ ET LA PLACE DU NŒUD CHANGE DE REPÈRE, comme son cap : l'ordonnée et la
  // profondeur du client sont les nôtres au signe près. Voir
  // `EffectClientOffsetInWorld`.
  // 🔴 ET L'ANNEAU SORT DU SOL COMME LES SPRITES : sans cela, une couronne
  // couchée naîtrait EXACTEMENT dans le plan du terrain et s'y noierait. Voir
  // `kSownHeight`, qui dit pourquoi six dixièmes.
  // ⚠ ET LA PLACE PEUT SE TIRER, en abscisse et en profondeur : une flèche
  // naît de vingt-cinq à trente-quatre sur l'une, de quinze à vingt-quatre sur
  // l'autre. Une borne haute nulle veut dire « fixe », comme partout.
  // ⚠ AVEC LE NOMBRE DE VALEURS QUE LE CLIENT Y TIRE : voir `EffectNodeRange::values`.
  const auto draw = [&generator, &pattern](ro::render::EffectDrawnRange plage, float bottom,
                                           float top) {
    return ro::render::DrawEffectNodeRange(
        ro::render::EffectNodePatternRange(pattern, plage, bottom, top), generator);
  };
  // 🔴 LE CERCLE ENTIER PASSE AVANT TOUT AUTRE TIRAGE DU NŒUD : le bâtisseur
  // tire son cap puis son rayon avant de poser quoi que ce soit d'autre. Voir
  // `EffectRingPattern::birth_circle_is_integer`. Le cap entier se lit tel quel
  // dans les tables du client, et la place est `rayon × (sin, 0, cos)` dans son
  // repère.
  // LE CAP PAR SECTEUR QUI S'ÉLARGIT, tiré le premier de tous : voir
  // `EffectNodeSupplement::copy_yaw_sector_degrees`.
  const ro::render::EffectNodeSupplement& en_plus = pattern.supplement;
  std::optional<float> cap_du_client;
  if (en_plus.copy_yaw_sector_degrees > 0.0F) {
    const auto secteur = static_cast<std::uint32_t>(en_plus.copy_yaw_sector_degrees);
    const auto drawn = static_cast<float>(ro::render::NextEffectNodeDraw(generator) %
                                          (secteur * (exemplaire + 1U)));
    cap_du_client =
        std::min(en_plus.copy_yaw_ceiling_degrees,
                 en_plus.copy_yaw_sector_degrees * static_cast<float>(exemplaire) + drawn);
  }
  // LA PLACE ET LE CAP FIXÉS PAR RANG, quand le bâtisseur les écrit.
  const bool place_du_rang = exemplaire < en_plus.copy_placement_count;
  if (place_du_rang) {
    cap_du_client = en_plus.copy_placements[exemplaire].yaw_degrees;
  }
  glm::vec3 full_circle_offset{0.0F, 0.0F, 0.0F};
  if (pattern.ring.birth_circle_is_integer) {
    const auto cap = static_cast<float>(ro::render::NextEffectNodeDraw(generator) %
                                        static_cast<std::uint32_t>(kEffectRingTurnDegrees));
    const float radius = pattern.ring.birth_radius_draw.modulus > 0
                             ? static_cast<float>(ro::render::DrawEffectClientInteger(
                                   pattern.ring.birth_radius_draw, generator))
                             : pattern.launch_distance_low;
    const float radians = glm::radians(cap);
    full_circle_offset = ro::render::EffectClientOffsetInWorld(radius * std::sin(radians), 0.0F,
                                                               radius * std::cos(radians));
  }
  // LA PLACE ET L'ENFOUISSEMENT, en ENTIERS quand le client les tire ainsi.
  const auto draw_x = [&] {
    return place_du_rang ? en_plus.copy_placements[exemplaire].offset_x
           : pattern.ring.offset_x_draw.modulus > 0
               ? static_cast<float>(
                     ro::render::DrawEffectClientInteger(pattern.ring.offset_x_draw, generator))
               : draw(ro::render::EffectDrawnRange::kOffsetX, pattern.ring.offset_x,
                      pattern.ring.offset_x_high);
  };
  const auto draw_z = [&] {
    return place_du_rang ? en_plus.copy_placements[exemplaire].offset_z
                         : draw(ro::render::EffectDrawnRange::kOffsetZ, pattern.ring.offset_z,
                                pattern.ring.offset_z_high);
  };
  // ⚠ DANS L'ORDRE DU BÂTISSEUR : voir `EffectNodeSupplement::offsets_drawn_depth_first`.
  float drawn_offset_x = 0.0F;
  float drawn_offset_z = 0.0F;
  if (en_plus.offsets_drawn_depth_first) {
    drawn_offset_z = draw_z();
    drawn_offset_x = draw_x();
  } else {
    drawn_offset_x = draw_x();
    drawn_offset_z = draw_z();
  }
  // L'ENFOUISSEMENT SE TIRE LUI AUSSI, mais seulement quand sa borne haute est
  // posée. Voir `EffectRingPattern::burial_depth_high`.
  float enfouissement = pattern.ring.burial_depth;
  if (pattern.ring.burial_depth_draw.modulus > 0) {
    enfouissement =
        pattern.ring.burial_depth + static_cast<float>(ro::render::DrawEffectClientInteger(
                                        pattern.ring.burial_depth_draw, generator));
  } else if (pattern.ring.burial_depth_high != 0.0F) {
    enfouissement = draw(ro::render::EffectDrawnRange::kBurial, pattern.ring.burial_depth,
                         pattern.ring.burial_depth_high);
  }
  // LES TIRAGES QUE LE BÂTISSEUR JETTE avancent quand même le générateur.
  for (std::uint8_t jete = 0; jete < pattern.ring.draws_discarded_after_burial; ++jete) {
    static_cast<void>(ro::render::NextEffectNodeDraw(generator));
  }
  const glm::vec3 shift =
      ro::render::EffectNodeInstanceOffset(
          pattern.ring.instance_placement, static_cast<std::uint16_t>(count / par_place),
          static_cast<std::uint16_t>(exemplaire / par_place), pattern.ring.instance_spacing,
          pattern.ring.instance_spacing_high, pooled.node_yaw_degrees, generator,
          pattern.draw_values[static_cast<std::size_t>(ro::render::EffectDrawnRange::kSpacing)]) +
      ro::render::EffectClientOffsetInWorld(drawn_offset_x, enfouissement, drawn_offset_z);
  // LA LEVÉE, pour un nœud que le client pose dans le plan du sol et lui seul.
  const float levee = SownLiftFor(shift.y);
  pooled.ground_position = ground_position + glm::vec3{0.0F, levee, 0.0F} + shift;

  // La traduction du patron en nœud vit dans `render`, où elle s'éprouve sur la
  // table entière. La fenêtre n'a pas à savoir quel champ va où.
  pooled.node = ro::render::EffectNodeFromPattern(pattern, generator);
  // LE CAP QUE LE RANG A FIXÉ OU TIRÉ, traduit pour l'axe du penchant : l'angle
  // du client `ψ` y devient `90 − ψ` (voir `EffectNodeLeanAxis`).
  if (cap_du_client.has_value()) {
    pooled.node.yaw_degrees = kQuarterTurnDegrees - *cap_du_client;
  }
  // LA MARQUE DE LA CROIX, que la première piste porte pour les quatre faces.
  pooled.tracks.front().mark[0] = en_plus.cross_spike_mark;
  // 🔴 LE MOT QUE LA FILE REÇOIT EST CELUI DU DESSINATEUR, qui peut ignorer
  // celui du nœud : voir `EffectNodeQueuedRenderWord`. Les pointes de Grimtooth
  // naissent sous le sol, et y restent cachées.
  if (pooled.node.render_word.has_value()) {
    pooled.node.render_word = ro::render::EffectNodeQueuedRenderWord(
        pooled.shape, *pooled.node.render_word, en_plus.cross_spike_mark);
  }
  // La vie que le bâtisseur écrit, avant que celle de l'effet ne la borne : c'est
  // elle qui divise la distance d'un vol.
  const std::int32_t written_life_ticks = pooled.node.life_ticks;
  // 🔴 ET POUR UN EFFET, LA TEINTE VIENT DU RANG ET NON DE LA TABLE. L'anneau du
  // classement PVP est presque blanc au premier et rouge vif au dixième : le
  // client écrit ses trois canaux lui-même, selon le rang. Voir
  // `ro/render/pvp_rank_tint.h`.
  //
  // ⚠ LA TEINTE SEULE, ET PAS L'ALPHA : le patron porte déjà les 254 du
  // constructeur, et les reposer ici sauterait la montée qui les amène.
  if (pattern.effect_id == ro::render::kPvpTopRankEffect) {
    pooled.node.color = ro::render::PvpTopRankTint(param);
  }
  // 🔴 ET SA VIE NE DÉPASSE PAS CELLE DE SON EFFET : le client détruit l'instance
  // et ses enfants ensemble. Voir `PlaySeedsDueAt`, qui dit quand on a le
  // droit de borner (`EffectInstanceLifeIsKnown`).
  //
  // ⚠ SAUF QUAND LE BÂTISSEUR DONNE UN RENDEZ-VOUS : la vie est alors ce qu'il
  // RESTE, et non le plus court des deux — et la SORTIE le suit, parce que le
  // client écrit « vie moins dix » dans son tic de sortie. Un
  // fondu laissé sur l'ancienne vie commencerait après la mort du nœud.
  if (pattern.life_until_tick > 0) {
    const std::int32_t fondu = pooled.node.life_ticks - pooled.node.fade_out_tick;
    pooled.node.life_ticks = life_left_ticks;
    pooled.node.fade_out_tick = std::max(0, life_left_ticks - fondu);
  }
  // Et un nœud qui survivrait à son effet s'éteint avec lui, en fondu : la
  // troupe le juge à chaque mise à jour, voir
  // `EffectNode::fades_with_its_instance`. Une instance dont l'âge reboucle se
  // juge dès le semis.
  pooled.node.fades_with_its_instance = true;
  const ro::render::EffectInstanceCycle cycle =
      ro::render::EffectNodePatternInstanceCycle(pattern, effect_tick);
  ro::render::EffectNodeFadesWithACyclingInstance(pooled.node, cycle);

  // 🔴 ET UN NŒUD D'ANNEAU PEUT PARTIR, LUI AUSSI : sans ce lancer, une figure
  // d'anneau qui AVANCE resterait clouée à sa naissance. Les traits
  // d'EF_ENHANCE montent de deux unités par tic.
  //
  // ⚠ SANS GARDE : un patron qui ne part nulle part rend une direction NULLE et
  // ne consomme aucun tirage — c'est le cas de l'immense majorité des anneaux,
  // et `DrawEffectLaunch` s'en va avant de toucher au générateur.
  ro::render::EffectNodeSeed start;
  // ⚠ CHAQUE PLAGE AVEC SON NOMBRE DE VALEURS : voir `EffectNodeRange::values`.
  using ro::render::EffectDrawnRange;
  start.launch_yaw_degrees = ro::render::EffectNodePatternRange(
      pattern, EffectDrawnRange::kLaunchYaw, pattern.launch_yaw_low, pattern.launch_yaw_high);
  start.launch_pitch_degrees = ro::render::EffectNodePatternRange(
      pattern, EffectDrawnRange::kLaunchPitch, pattern.launch_pitch_low, pattern.launch_pitch_high);
  // 🔴 LA DISTANCE NE PART PAS LE LONG DU DÉPART, ET C'EST TOUTE LA DIFFÉRENCE.
  // Le client place son nœud sur un cercle HORIZONTAL — une rotation en lacet,
  // un rayon tiré — PUIS remplace sa matrice par le quart de tour qui l'envoie
  // vers le haut. Les deux rotations ne sont pas la même, et les confondre
  // pousse la naissance DANS la direction du mouvement : les traits
  // d'EF_ENHANCE partiraient tous du même point, à des hauteurs différentes, et
  // monteraient en une seule colonne au lieu de s'égailler.
  start.launch_distance = {0.0F, 0.0F};
  start.launch_drop = pattern.launch_drop;
  start.step_length = ro::render::EffectNodePatternRange(pattern, EffectDrawnRange::kStep,
                                                         pattern.step_low, pattern.step_high);
  start.step_decay_share = pattern.step_decay_share;
  start.step_change_per_tick = pattern.step_change_per_tick;
  start.fall_speed = ro::render::EffectNodePatternRange(
      pattern, EffectDrawnRange::kFallSpeed, pattern.fall_speed_low, pattern.fall_speed_high);
  start.fall_acceleration_share = pattern.fall_acceleration_share;
  // 🔴 ET LE CAP NE SE COMPOSE AVEC LE PORTEUR QUE SI LE PATRON LE DIT. Une
  // particule de troupe compose TOUJOURS — le client écrit « orientation de
  // l'acteur plus un tirage » —, mais les traits d'EF_ENHANCE ne composent rien :
  // leur bâtisseur REMPLACE la matrice du nœud par un quart de tour pur, sans
  // lacet. Leur donner la face du porteur les enverrait dans la direction où
  // l'on regarde au lieu de les faire monter.
  //
  // ⚠ C'EST LE MÊME DRAPEAU QUE POUR L'ANGLE DU NŒUD, quelques lignes plus haut,
  // et il doit le rester : un nœud dont le lacet suit le porteur et dont le
  // lancer ne le suivrait pas partirait de travers par rapport à sa propre
  // figure.
  const float face = pattern.node_yaw_follows_owner ? owner_facing_degrees : 0.0F;
  const ro::render::EffectLaunchDraw lance = ro::render::DrawEffectLaunch(
      start, face, pooled.node.life_ticks, pooled.node.step_length, generator);
  pooled.launch_direction = lance.direction;
  pooled.ground_position += lance.offset;
  // 🔴 LA VARIATION DU PAS NE COMPTE QU'UNE FOIS PAR TIC. Le nœud d'anneau la
  // porte déjà — `EffectNode::step_length_change_per_tick`, que son avance ajoute
  // au pas à chaque tic, comme le client fait `700 += 704` une seule fois — ; la
  // laisser aussi dans le freinage du lancer la doublait : les rayons
  // d'EF_DECAGILITY tombaient deux fois trop vite. Le lancer ne la lit que pour
  // savoir que le nœud PART : voir le freinage, plus bas.
  const float carried_variation =
      pooled.node.step_length_change_per_tick != 0.0F ? pattern.step_change_per_tick : 0.0F;
  // LE CERCLE DE NAISSANCE, tiré plus haut en entiers comme le client le fait.
  // ⚠ C'EST LE SEUL : aucun anneau ne tire un cercle continu.
  pooled.ground_position += full_circle_offset;
  pooled.step_decay = lance.step_decay - carried_variation;
  // 🔴 UN NŒUD QUI VISE SON POINT ne tire ni cap ni inclinaison : les deux se
  // CALCULENT de sa place de naissance vers le point de l'effet — plus un
  // tirage de deux unités, celui du client —, et son pas l'y mène. La figure
  // s'aligne sur ce trajet, son axe partagé devant. Voir
  // `EffectRingPattern::aims_at_anchor`.
  //
  // ⚠ LE PAS DU PATRON EST NÉGATIF le long d'un axe que le client a retourné :
  // c'est sa grandeur qui compte, et le sens est celui de la visée.
  if (pattern.ring.aims_at_anchor) {
    const auto offset = [&generator]() {
      return static_cast<float>(ro::render::NextEffectNodeDraw(generator) % kAimJitterSpan) -
             kAimJitterBias;
    };
    float aim_x = 0.0F;
    float aim_z = 0.0F;
    if (en_plus.offsets_drawn_depth_first) {
      aim_z = offset();
      aim_x = offset();
    } else {
      aim_x = offset();
      aim_z = offset();
    }
    const glm::vec3 aimed = ground_position + glm::vec3{0.0F, levee, 0.0F} +
                            ro::render::EffectClientOffsetInWorld(aim_x, 0.0F, aim_z);
    const glm::vec3 vers = aimed - pooled.ground_position;
    if (glm::length(vers) > 0.0F) {
      const glm::vec3 direction = glm::normalize(vers);
      pooled.launch_direction = direction;
      pooled.node.step_length = std::abs(pooled.node.step_length);
      pooled.node_yaw_degrees =
          std::atan2(direction.x, direction.z) * 180.0F / std::numbers::pi_v<float>;
      pooled.node_pitch_degrees = -std::asin(direction.y) * 180.0F / std::numbers::pi_v<float>;
    }
    // 🔴 LE PROJECTILE DE SON INSTANCE garde ses angles comme le client les
    // calcule — le cap vers le point visé, l'inclinaison de sa HAUTEUR sur la
    // distance de sa naissance — et vole le long de la ligne qu'ils bâtissent,
    // lue dans les tables. L'instance les retient, et retient ce nœud comme le
    // sien. Voir `EffectNodeSupplement::records_projectile`.
    if (pattern.supplement.records_projectile && sowing_clock_ != nullptr) {
      const ro::render::EffectProjectileAngles angles = ro::render::EffectClientArrowAngles(
          drawn_offset_x, drawn_offset_z, -enfouissement, aim_x, aim_z);
      pooled.launch_direction =
          -ro::render::EffectClientPitchedHeading(angles.yaw_degrees, angles.pitch_degrees);
      sowing_clock_->last_projectile = angles;
      if (sowing_clock_->projectile_serial == 0) {
        sowing_clock_->projectile_serial = ++projectiles_numerotes_;
      }
      pooled.projectile_serial = sowing_clock_->projectile_serial;
    }
    // ET ELLE SE PLANTE : le bit deux de ce sous-type arrête le nœud au sol, à
    // la hauteur du point de l'effet. Voir `kEffectNodeStopsAtGround`.
    pooled.stops_at_floor = (pooled.node.flags & ro::render::kEffectNodeStopsAtGround) != 0;
    pooled.floor_y = ground_position.y + levee;
  }
  // 🔴 UN NŒUD QUI VOLE VERS LA CIBLE y va à plat, en toute sa vie : la
  // distance au sol, divisée par la vie, fait son pas. Il meurt en arrivant —
  // voir `EffectNodeArrivedAtTarget`, qui lit la cible posée ici. Sans cible, il
  // reste à son point.
  if (target_position.has_value()) {
    pooled.target_position = target_position;
    const glm::vec3 vers{target_position->x - pooled.ground_position.x, 0.0F,
                         target_position->z - pooled.ground_position.z};
    // ET IL TOURNE SON CORPS VERS ELLE, écart nul compris : voir `EffectNodeBodyTowardTarget`.
    if (pattern.ring.flies_to_target) {
      pooled.node_body_degrees = ro::render::EffectNodeBodyTowardTarget(vers);
    }
    if (pattern.ring.flies_to_target && glm::length(vers) > 0.0F) {
      pooled.launch_direction = glm::normalize(vers);
      pooled.node.step_length =
          glm::length(vers) / static_cast<float>(std::max(written_life_ticks, 1));
      pooled.step_decay = 0.0F;
    }
  }
  pooled.fall_speed = lance.fall_speed;
  pooled.fall_acceleration = lance.fall_acceleration;
  // ET LE REBOND, que le patron porte pour tout le sous-type 6. ⚠ LE SOL N'EST
  // PAS CELUI DU SEMIS ICI : la troupe le relit sous la particule à chaque tic,
  // comme le client. Voir `AdvanceLaunchedNode`.
  pooled.bounces_at_floor = pattern.bounces_at_floor;
  pooled.bounce_sound = std::string(pattern.bounce_sound);
  pooled.bounce_sound_draws = pattern.bounce_sound_draws;
  pool_.Add(std::move(pooled));
  return true;
}

}  // namespace moonclient
