#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_bearer_follow.h"
#include "ro/render/effect_chased_drift.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_frame_beat.h"
#include "ro/render/effect_haunt.h"
#include "ro/render/effect_launch.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_blend.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_burst.h"
#include "ro/render/effect_track_motion.h"
#include "ro/render/effect_trail_history.h"

namespace ro::render {

// SEMER et FAIRE VIVRE des nœuds procéduraux.
//
// Un effet procédural n'est pas UN nœud mais une petite foule : le client en
// sème un ou plusieurs à chaque tic tant que l'effet dure, chacun avec ses
// propres tirages. C'est ce grouillement qui fait la différence entre un halo
// figé et un buff qui scintille.
//
// Description PURE : aucune texture n'est chargée ici, aucun contexte
// graphique n'est touché. Voir `docs/adr/0014`.

// Un nœud vivant, et où il joue.
struct PooledEffectNode {
  EffectNode node;
  // Le point du sol d'où l'effet part, en unités du monde — le même repère que
  // celui d'un effet en fichier.
  glm::vec3 ground_position{0.0F, 0.0F, 0.0F};
  // OÙ SE TROUVE LE PORTEUR DE CET EFFET, à ce tic.
  //
  // ⚠ LE CLIENT RANGE SON PORTEUR DANS LE NŒUD, pas dans la troupe, et un
  // mouvement va y relire sa position à chaque tic : c'est ainsi qu'un éventail
  // suit le personnage qui l'a lancé. Le champ est donc PAR NŒUD et non par
  // troupe — deux effets joués ensemble n'ont pas le même porteur.
  //
  // Il vaut l'ancre au semis, et `EffectNodePool::MoveAnchored` le déplace du
  // pas du porteur quand le nœud est accroché à un acteur ET que son effet le
  // suit à ce tic — voir `bearer_follow`. Sinon il reste où il est né.
  glm::vec3 owner_position{0.0F, 0.0F, 0.0F};
  // OÙ SE TROUVE L'ACTEUR QUE CET EFFET VISE, et s'il y en a un.
  //
  // ⚠ C'EST LE JUMEAU DE `owner_position`, POUR L'AUTRE BOUT. Le client va
  // chercher cet acteur par son identifiant à chaque tic, et n'en lit que
  // la position : `render` n'a donc pas besoin de le connaître, seulement
  // de recevoir ce point. Vide veut dire « pas de cible », et une figure
  // qui en veut une s'éteint alors — comme le client.
  //
  // Comme `owner_position`, il ne se rafraîchit pas encore tout seul : le
  // bout lointain reste où la cible était au semis. Faux si elle marche,
  // jamais inventé.
  std::optional<glm::vec3> target_position;
  // CE À QUOI CE NŒUD EST ACCROCHÉ, ou zéro s'il reste où il est né.
  //
  // ⚠ UN NUMÉRO OPAQUE POUR `render`, qui ne connaît ni acteur ni monde : il ne
  // sert qu'à retrouver ENSEMBLE les nœuds qu'un même porteur emmène. C'est
  // l'appelant qui sait ce qu'il désigne, et lui seul.
  //
  // Il est PAR NŒUD, comme `owner_position`, et pour la même raison : deux
  // effets joués en même temps n'ont pas le même porteur.
  std::uint32_t anchor_id = 0;
  // QUEL EFFET A SEMÉ CE NŒUD, zéro quand il ne vient pas d'un effet nommé.
  //
  // 🔴 IL SERT À LE RETIRER. Le client retire un effet d'un acteur par son
  // IDENTIFIANT — message 109, cas 6 de sa liste d'effets —, et détruire l'instance emporte les
  // nœuds qu'elle a bâtis. Sans ce champ, un effet coupé laissait derrière lui tout ce qu'il avait
  // déjà semé, chacun mourant de sa propre vie.
  std::uint32_t sown_by_effect_id = 0;
  // OÙ SON ANCRE SE TENAIT LA DERNIÈRE FOIS QU'ON L'A SUIVIE.
  //
  // 🔴 C'EST UN REPÈRE POUR TRANSLATER, ET NON UN ÉCART POUR REPOSER. Faire
  // suivre un porteur en RECALCULANT la place du nœud — `ancre + écart de
  // naissance` — le téléporte à son point de départ à chaque image, et efface
  // tout ce que son mouvement a accumulé depuis : le vol d'une étincelle de
  // téléportation y perdait sa montée, qui ne dépassait jamais UN tic. Ce qui
  // se garde est donc l'ancienne place de l'ancre, dont on déduit le PAS à
  // ajouter ; ce que le nœud a fait de lui-même survit.
  //
  // ⚠ ET CE PAS NE DOIT RIEN RECOLLER NON PLUS : la place du nœud dans sa
  // couronne, sa sortie du terrain et son ENFOUISSEMENT — les trente unités
  // sous lesquelles une pointe de glace naît — se conservent d'eux-mêmes, une
  // translation ne les touchant pas.
  //
  // Elle est nulle pour un nœud sans ancre, et le rester ne coûte rien.
  glm::vec3 anchor_ground_position{0.0F, 0.0F, 0.0F};
  // OÙ LA PARTICULE PART, et ce qui la freine ou la fait tomber. ⚠ LA DIRECTION
  // SE TIRE UNE FOIS ET NE CHANGE PLUS — le client bâtit sa matrice au semis et
  // n'en relit que la troisième colonne. Nulle quand rien ne part.
  glm::vec3 launch_direction{0.0F, 0.0F, 0.0F};
  // 🔴 VRAI QUAND L'IMAGE SE COUCHE LE LONG DE CE VOL. Voir
  // `EffectNodePattern::turns_with_flight` : l'angle d'écran se recalcule à
  // chaque image depuis la direction ci-dessus, parce qu'il dépend de la caméra.
  bool turns_with_flight = false;
  float step_decay = 0.0F;
  float fall_speed = 0.0F;
  float fall_acceleration = 0.0F;
  // LE SOUS-TYPE 8 : la vitesse verticale se retranche au pas, et un
  // BALANCEMENT roulé écarte le nœud de son axe — voir
  // `EffectNodePattern::brakes_along_axis`. Le tangage cumulé est en unités du
  // client, négatif vers le haut ; l'écart se RECALCULE depuis lui à chaque
  // tic, d'où le dernier écart gardé pour n'en poser que la différence.
  bool brakes_along_axis = false;
  float swing_speed = 0.0F;
  float swing_acceleration = 0.0F;
  float swing_amount = 0.0F;
  float roll_degrees = 0.0F;
  glm::vec3 swing_offset{0.0F, 0.0F, 0.0F};
  // LE SOL OÙ LE NŒUD S'ARRÊTE, quand il s'y arrête : la hauteur du terrain au
  // point de l'effet, posée au semis. Voir `kEffectNodeStopsAtGround`.
  //
  // ⚠ `render` NE CONNAÎT PAS LE TERRAIN, et n'a pas à le connaître : le client
  // relit la hauteur sous le nœud à chaque tic, nous prenons celle du point de
  // l'effet — une flèche vise ce point, et c'est là qu'elle se plante.
  bool stops_at_floor = false;
  float floor_y = 0.0F;
  // 🔴 ET UNE PIÈCE REBONDIT au lieu de s'arrêter : sous le terrain, sa vitesse
  // verticale se retourne divisée par un et demi. C'est le sous-type 6 tout
  // entier — voir `EffectNodePattern::bounces_at_floor`.
  bool bounces_at_floor = false;
  // LE BRUIT DE CE REBOND et son nombre de variantes, recopiés du patron. Vide
  // pour un rebond muet, ce qu'est tout nœud sauf ceux d'EF_COIN.
  std::string bounce_sound;
  std::uint8_t bounce_sound_draws = 0;
  // ⚠ CE QUE LE TIC VIENT DE FAIRE, et non un état du nœud : la troupe le
  // remet à faux avant chaque avance, et l'appelant le relit APRÈS pour en
  // tirer un son. `render` ne connaît pas l'audio ; c'est `app` qui joue.
  bool bounced_this_tick = false;
  PooledNodeShape shape = PooledNodeShape::kSprite;
  // Le couple .spr/.act pour un sprite — un seul —, les textures dans l'ordre
  // du défilement pour une figure du monde.
  //
  // ⚠ CHAQUE NŒUD PORTE LES SIENS, plutôt qu'un tableau rangé à côté de la
  // troupe : elle TASSE ses morts, et un tableau parallèle ferait hériter
  // chaque survivant des textures de son voisin.
  std::vector<std::string> resource_paths;

  // LES QUATRE PISTES, et le numéro de teinte qu'elles partagent.
  //
  // Vides pour toute forme qui n'en a pas — une piste éteinte n'avance ni ne se
  // dessine, si bien qu'un sprite n'en paie rien. Voir `effect_track.h` : la
  // troupe les fait battre, et la fenêtre les compose.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = {};
  std::uint8_t trail_variant = 0;

  // LES DEUX ANGLES DE BILLBOARD que la caméra donne, et qu'un mouvement RANGE
  // dans les pistes.
  //
  // ⚠ ILS ENTRENT PAR LA TROUPE ET NON PAR LE DESSIN, parce que c'est le client
  // qui en décide ainsi : son mouvement les pose en tête de la première et de la
  // deuxième piste, et son dessinateur les y relit. Les recalculer au moment de
  // composer donnerait le même résultat une fois sur deux — et l'autre fois, un
  // quad tourné d'un tic de retard sur sa propre piste.
  //
  // `render` ne connaît pas la caméra : c'est `app` qui les pose avant
  // d'avancer la troupe. Voir `EffectBillboardTurnDegrees`.
  std::int16_t billboard_turn_degrees = 0;
  std::int16_t billboard_tilt_degrees = 0;
  // LES DEUX ANGLES DU NŒUD, ceux dont le client bâtit sa matrice. ⚠ À NE PAS CONFONDRE avec les
  // deux précédents, qui viennent de la CAMÉRA : ceux-ci sont posés au semis, et l'acteur qui porte
  // l'effet les décide. Voir `EffectNodeOriented`.
  float node_yaw_degrees = 0.0F;
  float node_pitch_degrees = 0.0F;
  // ET LE TROISIÈME, LE ROULIS. Voir `EffectRingPattern::roll_degrees` :
  // il s'applique AVANT les deux autres, et c'est lui qui couche un quad au sol.
  float node_roll_degrees = 0.0F;
  // 🔴 ET LE CAP TOURNE, pour dix-huit nœuds connus sur quatre-vingt-douze. Le
  // mouvement intègre le pas dans le cap, et la variation du pas dans le pas —
  // la figure pivote sur elle-même.
  //
  // ⚠ DEUX FOIS PAR TIC : le client écrit le cap une première fois, puis À
  // NOUVEAU, la matrice se bâtissant entre les deux. Nous
  // ne composons qu'une fois par image, si bien que l'angle DESSINÉ diffère du
  // sien d'un pas — trois degrés pour EF_FIREPILLARON, sous le seuil du visible.
  // Ce qui compte est la CADENCE, et elle est double.
  //
  // ⚠ L'INCLINAISON, ELLE, NE TOURNE JAMAIS : son pas et la variation de son
  // pas valent zéro sur les quatre-vingt-douze nœuds, alors que dix-huit ont un
  // pas de cap. Ce n'est pas un champ oublié.
  float node_yaw_step = 0.0F;
  float node_yaw_step_change = 0.0F;
  // COMBIEN DE FOIS PAR TIC LE PAS S'AJOUTE AU CAP. Deux pour le mouvement du
  // tronc de cône ci-dessus ; UNE pour la sphère du sous-type 14, dont le cas
  // du répartiteur n'écrit le cap qu'une fois, et pour les plans et quads des
  // sous-types 15 et 16.
  float yaw_steps_per_tick = 2.0F;
  // L'ANGLE DU CORPS DU NŒUD, en degrés du CLIENT : nul à la naissance, le
  // bâtisseur qui fait voler le nœud vers la cible l'y tourne. Le quad orienté
  // le lit sous `kEffectOrientedQuadLeansByCameraAltitude` et
  // `kEffectOrientedQuadYawFollowsBody`. Voir `EffectNodeBodyTowardTarget`.
  float node_body_degrees = 0.0F;

  // LE CENTRE ET LE RAYON D'UNE ORBITE, et de combien de degrés elle avance par
  // tic.
  //
  // 🔴 UNE ORBITE N'EST PAS UN LANCER. Un nœud lancé part dans une direction et
  // s'en éloigne ; celui-ci tourne autour d'un point à distance constante, et sa
  // position se RECALCULE à chaque tic depuis son angle. Le semis figeait la
  // place d'une particule dispersée une fois pour toutes, si bien que les
  // étincelles d'un point de téléportation restaient où elles étaient nées.
  //
  // ⚠ SA HAUTEUR N'EN EST PAS : ce que le vol a accumulé en ordonnée reste. Le
  // recalcul ne touche que le plan du sol, sans quoi une étincelle qui monte
  // retomberait à chaque tic.
  //
  // Un rayon nul ou un pas nul n'orbite pas, et c'est le cas de presque tous.
  //
  // ⚠ ET L'ANCRE N'EST PAS TOUJOURS LE CENTRE DE L'EFFET : un bâtisseur qui a
  // baké sa dispersion en décalage fixe centre le cercle sur le point baké,
  // lui-même écarté du centre. Voir `EffectNodePattern::scatter_counts_twice`.
  glm::vec3 orbit_anchor{0.0F, 0.0F, 0.0F};
  float orbit_radius = 0.0F;
  float orbit_degrees_per_tick = 0.0F;
  // ET LE BRAS S'ALLONGE : le client intègre ce pas dans la distance
  // avant de recomposer la position.
  float orbit_radius_step_per_tick = 0.0F;
  // LA RONDE D'UNE APPARITION, et le point autour duquel elle tourne. Voir
  // `effect_haunt.h` : comme une orbite, sa place se RECALCULE depuis ce centre
  // à chaque tic ; son alpha et son action sont les siens, et la troupe les
  // recopie dans le nœud. Sans vol, rien ne change.
  EffectHaunt haunt;
  // LE NŒUD RESTE OÙ IL EST NÉ, même quand son effet suit le porteur. Voir
  // `EffectNodePattern::nodes_stay_where_sown` : `Add` lui donne alors
  // l'horaire d'un effet qui ne suit jamais, au lieu de celui de son effet.
  bool stays_where_sown = false;
  glm::vec3 haunt_center{0.0F, 0.0F, 0.0F};

  // VERS OÙ REGARDE LE PORTEUR DE CE NŒUD, en degrés.
  //
  // ⚠ CE N'EST PAS UN ANGLE DE CAMÉRA, et les deux ne se remplacent pas : le
  // client va le lire dans l'ACTEUR qui porte l'effet, pas dans la vue. Un
  // bouclier qui suivrait la caméra tournerait quand le joueur fait pivoter
  // l'écran, ce qui n'arrive pas dans le client.
  //
  // Il vaut zéro tant que personne ne le met à jour — le nœud regarde alors
  // vers l'est, ce qui est arbitraire mais jamais inventé. La coquille le reçoit
  // de `MoveAnchored`, comme l'anneau double ; voir `AdvanceSphereShellTracks`.
  float owner_facing_degrees = 0.0F;

  // À QUELLE HAUTEUR LE SPRITE DU PORTEUR VEUT SON AURA.
  //
  // ⚠ C'EST UN NOMBRE QUE L'ACTEUR DONNE, pas une mesure que `render` peut
  // faire : le client va le chercher dans le sprite de l'acteur, selon son
  // job, et n'en lit qu'un flottant. Le mouvement le range dans chaque piste, où
  // la figure le RETRANCHE de la hauteur de ses coins — et retrancher, dans
  // le repère du client, ÉLÈVE.
  //
  // Zéro tant que personne ne le pose : l'aura tourne alors aux pieds du
  // porteur, ce qui est faux pour un grand sprite mais jamais inventé.
  float owner_aura_height = 0.0F;

  // LES CRANS DE TRAÎNÉE, du plus récent au plus ancien.
  //
  // ⚠ SA LONGUEUR EST CELLE QUE LE NŒUD DÉCLARE, et elle vaut UN par défaut : le
  // sprite se dessine alors une fois. Vide pour toute forme qui n'en a pas — et
  // c'est le cas de toutes sauf une. Voir `effect_trail_history.h`.
  std::vector<EffectTrailStep> trail;

  // QUAND CE NŒUD REPREND LA PLACE DE SON PORTEUR : l'horaire de l'effet qui l'a
  // semé. Voir `EffectBearerFollow`.
  //
  // ⚠ L'ANCRE NE DIT PAS À ELLE SEULE QU'ON SUIT. Un impact posé sur un acteur
  // lui appartient — il disparaît avec lui, on le retire par lui —, mais il
  // reste où il est né : c'est ce champ, et non `anchor_id`, qui en décide.
  EffectBearerFollow bearer_follow = kEffectFollowsEveryTick;
  // À QUEL ÂGE DE SON EFFET CE NŒUD EST NÉ. Son âge à lui plus celui-ci donne
  // l'âge de l'effet, et c'est l'âge de l'effet que l'horaire compte.
  std::int32_t sown_at_effect_tick = 0;
  // JUSQU'À QUEL ÂGE DE SON EFFET LE PORTEUR A DÉJÀ ÉTÉ SUIVI. Une image voit
  // passer plusieurs tics, parfois aucun : un recalage ne se joue qu'une fois.
  std::int32_t followed_through_tick = 0;
  // LA VOIE RADIALE ACCÉLÉRÉE, vide pour presque tous. Voir `EffectRadialLane` :
  // elle s'intègre dans `ground_position`, si bien qu'un porteur qui marche
  // l'emmène sans rien effacer de ce qu'elle a parcouru.
  EffectRadialLane radial;
  // LA DÉRIVE D'UN PÉTALE, éteinte pour presque tous. Voir `EffectChasedDrift` :
  // elle se sème à l'entrée dans la troupe, d'après l'effet qui sème.
  EffectChasedDrift chased_drift;
  // UN SPRITE VISÉ QUI MEURT PRÈS DE SA CIBLE, à trois unités au sol de
  // `target_position`. Voir `EffectNodeArrivedAtTarget`.
  bool dies_near_target = false;
  // L'ORBITE QUI ACCÉLÈRE, ajoutée au pas angulaire avant chaque avance, et le
  // cap d'où elle est partie. ⚠ LA PLACE LIT L'ANGLE EN DEGRÉS ENTIERS : le
  // client tronque l'angle avant de bâtir sa matrice, si bien qu'un pas
  // fractionnaire avance la place par à-coups d'un degré. Le tour parcouru se
  // tronque donc, et non le cap de départ — exact tant que l'angle du client
  // reste positif, ce que sont tous les départs et tous les pas portés.
  float orbit_acceleration_per_tick = 0.0F;
  float orbit_start_degrees = 0.0F;
  // L'ACTEUR QUE LE NŒUD VISE, zéro pour aucun : un numéro opaque, comme
  // `anchor_id`. Non nul, `target_position` reprend sa place à chaque fois que
  // la fenêtre la donne — voir `EffectNodePool::MoveTargeted`. Un trait tendu
  // vers une cible qui marche la suit au lieu de viser la case qu'elle a
  // quittée.
  std::uint32_t target_actor_id = 0;
  // LE NUMÉRO DE L'INSTANCE DONT CE NŒUD EST LE PROJECTILE, ou zéro. Les nœuds
  // qui partent du dernier projectile naissent à la place du plus récent qui la
  // porte. Voir `EffectNodeSupplement::records_projectile`.
  std::uint32_t projectile_serial = 0;
  // LE PAS DU BRAS REMPLACÉ À UN ÂGE DU NŒUD, moins un pour aucun. Voir
  // `EffectNodeSupplement::orbit_radius_step_switch_tick`.
  std::int32_t orbit_radius_step_switch_tick = -1;
  float orbit_radius_step_after_switch = 0.0F;
  // LE MODE DE LA GERBE À DEUX MODES, recopié de sa graine : voir
  // `EffectParticleRingStyleSeed::spray_mode`.
  float spray_mode = 0.0F;
  // COMBIEN DE MISES À JOUR IL RECEVRA ENCORE AVANT QUE L'INSTANCE QUI L'A SEMÉ
  // NE MEURE ET NE L'EMPORTE ; moins un quand sa vie n'est pas connue. Voir
  // `EffectNodePool::InstanceBeatsLeft`.
  std::int32_t instance_beats_left = -1;
  // LE MÊME COMPTE PAR LA SEULE VIE DE L'INSTANCE, sans la mort de son porteur :
  // c'est ce reste-là que le fondu du nœud lit. Voir
  // `EffectNodePool::InstanceLifeBeatsLeft`.
  std::int32_t instance_life_beats_left = -1;
  // 🔴 VRAI QUAND L'IMAGE TOURNE DE LA FACE DE SON VOL VUE DE LA CAMÉRA, du
  // lacet seul et recalculée à chaque image — voir
  // `EffectProjectileScreenAngleDegrees` : c'est ce que le client fait d'un
  // projectile d'acteur, et la flèche d'un arc est de ceux-là. La direction est
  // `launch_direction` ; la composition, seule à connaître la caméra, pose
  // l'angle.
  //
  // ⚠ CE N'EST PAS `turns_with_flight`, qui suit la PROJECTION du vol, plongée
  // comprise. Les deux angles ne coïncident que pour une caméra à plat.
  bool turns_with_flight_heading = false;
  // CE QUE L'IMAGE TOURNE EN PLUS DE CETTE FACE, en degrés d'écran sur
  // [0, 360) : le trait d'une baguette, dont le `.act` n'est pas dessiné sur
  // l'axe de la flèche. Zéro pour tout projectile du client.
  std::int32_t flight_heading_turn_degrees = 0;
  // NÉ DE L'INSTANCE D'UN CHAPEAU : la porte de dessin retire ses exceptions à
  // ce qu'un chapeau fait naître. Voir `EffectNodePool::SowAsHat`.
  bool from_hat = false;
  // VRAI POUR UN PROJECTILE D'ACTEUR, qui porte l'ombre d'un acteur — sa
  // silhouette sous le soleil, l'ombre ronde au sol sans lui. Faux pour tout
  // autre nœud, et pour la lueur qui accompagne un projectile : c'est de la
  // lumière.
  bool casts_shadow = false;
  // LA DERNIÈRE PISTE DE L'ANNEAU EN COUCHES ONDULE SANS FIN : voir
  // `EffectLayeredRingNodePattern::waves_last_track`.
  bool waves_last_track = false;
  // L'ÉTAT DU TIC PRÉCÉDENT, d'où le dessin glisse vers l'état courant : voir
  // `effect_node_blend.h`. Rien pour un nœud né depuis le dernier tic, ou dont
  // la forme se peint au tic.
  std::optional<EffectNodeKinematics> previous_tick;
};

// UN NŒUD QUI NAÎT À SON ANCRE : sa forme, et le même point pour le sol d'où il
// part et pour son porteur. Tout autre champ garde son défaut.
[[nodiscard]] inline PooledEffectNode PooledNodeAt(PooledNodeShape shape, glm::vec3 anchor) {
  PooledEffectNode pooled;
  pooled.shape = shape;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  return pooled;
}

// Au-delà de ce nombre de tics dus, la troupe abandonne le reste.
//
// Une pause au débogueur ou un chargement de carte laisse des secondes à
// rattraper. Les jouer toutes ferait défiler l'effet à toute allure devant le
// joueur — mieux vaut qu'il ait manqué ce qu'il n'a pas vu.
inline constexpr std::int32_t kEffectNodeMaxCatchUpTicks = kEffectTicksPerSecond;

// La troupe : ce qui vit, ce qui avance, ce qui s'en va.
class EffectNodePool {
 public:
  void Add(PooledEffectNode node) {
    node.anchor_id = next_anchor_;
    node.sown_by_effect_id = next_effect_;
    node.bearer_follow = node.stays_where_sown ? kEffectStaysWhereBorn : next_follow_;
    node.sown_at_effect_tick = next_effect_tick_;
    node.followed_through_tick = next_effect_tick_;
    // LA PLACE DE L'ANCRE SE PREND ICI : c'est d'elle que se mesurera le PAS du
    // porteur à la première fois qu'on le suivra.
    if (next_anchor_ != 0) {
      node.anchor_ground_position = next_anchor_position_;
    }
    // L'ÉCHÉANCE PRIME SUR TOUT — elle dit une heure, pas une préférence ; le
    // plancher, lui, relève sans jamais raccourcir. Voir `LiveExactly`.
    if (next_life_exact_ > 0) {
      node.node.life_ticks = next_life_exact_;
    } else {
      node.node.life_ticks = std::max(node.node.life_ticks, next_life_floor_);
    }
    node.instance_beats_left = InstanceBeatsLeft();
    node.instance_life_beats_left = InstanceLifeBeatsLeft();
    node.from_hat = next_from_hat_;
    nodes_.push_back(std::move(node));
  }

  // À QUOI LES PROCHAINS NŒUDS SERONT ACCROCHÉS, zéro pour « posés là ».
  //
  // ⚠ POSÉE SUR LA TROUPE ET NON PASSÉE À CHAQUE SEMIS, parce que les semis
  // sont trente et qu'un seul oublié laisserait un nœud derrière son porteur
  // sans que rien ne le dise. `Add` est l'entonnoir par où ils passent tous.
  // Celui qui la pose la REPOSE À ZÉRO après avoir semé.
  // ⚠ ET LA PLACE DE L'ANCRE AVEC, parce que ce qu'on garde est un ÉCART : le
  // semis donne au nœud un point ABSOLU, dont l'ancre n'est qu'une part. Les
  // deux arrivent ensemble ou l'écart serait pris contre l'origine du monde.
  void AnchorNextTo(std::uint32_t anchor_id,
                    glm::vec3 anchor_ground_position = glm::vec3{0.0F, 0.0F, 0.0F}) noexcept {
    next_anchor_ = anchor_id;
    next_anchor_position_ = anchor_ground_position;
  }

  // DE QUEL EFFET LES PROCHAINS NŒUDS VIENNENT, pour savoir lesquels retirer.
  // Même raison qu'`AnchorNextTo` de vivre sur la troupe : `Add` est le seul
  // entonnoir par où les trente familles passent.
  //
  // ⚠ ET L'EFFET DIT S'ILS SUIVENT SON PORTEUR : c'est une propriété de son
  // identifiant, lue dans la table. Un semis sans effet nommé — zéro — suit à
  // chaque tic, ce qui était la règle de toute ancre avant qu'on ne distingue.
  void SowFor(std::uint32_t effect_id) noexcept {
    next_effect_ = effect_id;
    next_follow_ = effect_id == 0 ? kEffectFollowsEveryTick : EffectBearerFollowOf(effect_id);
  }

  // À QUEL ÂGE DE LEUR EFFET LES PROCHAINS NŒUDS NAISSENT. Zéro pour le premier
  // semis ; l'horloge le pose à chaque tic où elle sème.
  void SowAtEffectTick(std::int32_t effect_tick) noexcept { next_effect_tick_ = effect_tick; }

  // SI LES PROCHAINS NŒUDS NAISSENT DE L'INSTANCE D'UN CHAPEAU : le client marque
  // l'instance, et tous ses nœuds en héritent. Voir `PooledEffectNode::from_hat`.
  void SowAsHat(bool from_hat) noexcept { next_from_hat_ = from_hat; }

  // RETIRE CE QU'UN EFFET A SEMÉ SUR UN PORTEUR.
  //
  // 🔴 LES DEUX TERMES COMPTENT. Par l'effet seul, la nuée d'un autre joueur
  // s'éteindrait avec la nôtre ; par le porteur seul, on emporterait tout ce
  // qu'il porte. Le client désigne le couple : il cherche l'identifiant DANS la
  // liste de l'acteur.
  //
  // ⚠ UN PORTEUR NUL VEUT DIRE « POSÉ LÀ », pas « n'importe lequel » : un effet
  // sans ancre ne se retire que par un appel qui nomme lui aussi zéro.
  void DropFromEffect(std::uint32_t effect_id, std::uint32_t anchor_id) {
    std::erase_if(nodes_, [effect_id, anchor_id](const PooledEffectNode& node) {
      return node.sown_by_effect_id == effect_id && node.anchor_id == anchor_id;
    });
  }

  // RETIRE AU PLUS `count` NŒUDS DE CE COUPLE, LES PLUS ANCIENS D'ABORD, et
  // rend combien sont partis.
  //
  // C'est le retrait instance par instance d'un effet qui ne sème qu'un nœud par
  // instance : le client parcourt la liste de l'acteur dans l'ordre des
  // naissances et détruit la PREMIÈRE qui porte l'identifiant, autant de fois
  // qu'on le lui demande. Tout retirer emporterait ce qu'il laisse au-delà.
  std::size_t DropOldestFromEffect(std::uint32_t effect_id, std::uint32_t anchor_id,
                                   std::size_t count);

  // UN NŒUD QUI VISE UN ABSENT PERD SA CIBLE, et le trait tendu vers lui
  // s'éteint au tic suivant — pour de bon, même si l'acteur revient : le client
  // le cherche par son identifiant à chaque tic, et ne rallume pas une piste
  // éteinte. `still_here` répond pour l'appelant, comme pour
  // `DropAnchoredUnless`.
  template <typename Predicate>
  void LoseTargetsUnless(Predicate still_here) {
    for (PooledEffectNode& node : nodes_) {
      if (node.target_actor_id != 0 && !still_here(node.target_actor_id)) {
        node.target_position.reset();
      }
    }
  }

  // Vrai si cet effet a encore un nœud sur ce porteur : le même couple que
  // `DropFromEffect`.
  [[nodiscard]] bool HasFromEffect(std::uint32_t effect_id, std::uint32_t anchor_id) const {
    return std::ranges::any_of(nodes_, [effect_id, anchor_id](const PooledEffectNode& node) {
      return node.sown_by_effect_id == effect_id && node.anchor_id == anchor_id;
    });
  }

  // L'ANCRE D'UN SEMIS ENTIER, reposée à la sortie de la portée.
  //
  // 🔴 ELLE EXISTE PARCE QUE LA REPOSER À LA MAIN S'EST MAL PASSÉ. Le semis
  // d'un effet procédural appelle QUARANTE familles à la suite ; l'ancre y était
  // reposée après la PREMIÈRE, et les trente-neuf autres naissaient donc sans
  // porteur. L'horloge, elle, les ancrait toutes : un même nœud s'accrochait ou
  // non selon qu'il naissait au tic zéro ou au tic un, et le mur de Safety Wall
  // restait à l'écran dix mille tics après sa levée.
  //
  // ⚠ UNE PORTÉE, PAS UN APPEL APPARIÉ : la faute consistait à reposer TROP TÔT,
  // pas à oublier de reposer. Seule la portée l'empêche.
  class [[nodiscard]] AnchoredSowing {
   public:
    AnchoredSowing(EffectNodePool& pool, std::uint32_t anchor_id, glm::vec3 anchor_ground_position,
                   std::uint32_t effect_id = 0, bool from_hat = false) noexcept
        : pool_(&pool) {
      pool.AnchorNextTo(anchor_id, anchor_ground_position);
      pool.SowFor(effect_id);
      pool.SowAsHat(from_hat);
    }
    AnchoredSowing(const AnchoredSowing&) = delete;
    AnchoredSowing& operator=(const AnchoredSowing&) = delete;
    AnchoredSowing(AnchoredSowing&&) = delete;
    AnchoredSowing& operator=(AnchoredSowing&&) = delete;
    ~AnchoredSowing() {
      pool_->AnchorNextTo(0);
      pool_->SowFor(0);
      pool_->SowAtEffectTick(0);
      pool_->SowAsHat(false);
    }

   private:
    EffectNodePool* pool_;
  };

  // COMBIEN DE TICS LES PROCHAINS NŒUDS VIVENT AU MOINS, zéro pour « ce que dit
  // leur patron ».
  //
  // Le patron donne la vie d'une BOUFFÉE ; un effet qui accompagne une action
  // dure ce que l'action dure. Le client résout les deux dans cet ordre : il
  // pose la vie voulue sur l'instance, puis le bâtisseur applique son propre
  // plancher au premier tic — d'où un plancher qui RELÈVE et ne raccourcit
  // jamais. Une incantation de quatre secondes garde donc son cercle quatre
  // secondes, et une incantation plus brève que la bouffée la laisse finir.
  //
  // ⚠ POSÉE SUR LA TROUPE, pour la raison d'`AnchorNextTo` : `Add` est le seul
  // entonnoir, et trente semis à corriger un à un en laisseraient un derrière.
  // Celui qui la pose la REPOSE À ZÉRO après avoir semé.
  void LiveAtLeast(std::int32_t life_floor_ticks) noexcept { next_life_floor_ = life_floor_ticks; }

  // COMBIEN DE TICS LES PROCHAINS NŒUDS VIVENT, EXACTEMENT. Zéro rend la main
  // au patron et au plancher.
  //
  // 🔴 CE N'EST PAS `LiveAtLeast`, ET LES DEUX VIENNENT DU CLIENT. Il pose une
  // vie VOULUE sur l'instance quand l'effet accompagne une action, puis laisse
  // le bâtisseur relever la sienne au premier tic : c'est un plancher. Mais un
  // repère qu'il pose au sol porte une ÉCHÉANCE — `timeGetTime() + durée` — et
  // meurt à l'heure dite, quelle que soit la vie que sa table lui donne.
  //
  // ⚠ LA DIFFÉRENCE SE PAIE COMPTANT. La grille de visée déclare trois mille
  // tics — quarante-huit secondes ; posée avec un PLANCHER de la durée du
  // sort, elle gardait sa propre vie et restait sur la carte longtemps après
  // l'incantation. Un plancher ne raccourcit jamais.
  void LiveExactly(std::int32_t life_ticks) noexcept { next_life_exact_ = life_ticks; }

  // LE TIC OÙ L'INSTANCE QUI SÈME LES PROCHAINS NŒUDS PERD SON PORTEUR, compté
  // depuis sa naissance ; zéro quand rien ne l'y oblige.
  //
  // 🔴 CE N'EST NI UN PLANCHER NI UNE ÉCHÉANCE DE NŒUD : c'est la mort de
  // l'INSTANCE, qui emporte ses nœuds quelle que soit leur propre vie. Un
  // effet semé par un lanceur de case meurt avec ce lanceur ; celui d'Ice Wall
  // se déclare dix mille tics, et ses pointes quittent la case au bout de six
  // secondes. Voir `PooledEffectNode::instance_beats_left`.
  void DieWithBearerAt(std::int32_t bearer_death_tick) noexcept {
    next_bearer_death_tick_ = bearer_death_tick;
  }

  // LA VIE QUE PORTE L'INSTANCE qui sème les prochains nœuds, avant qu'aucun
  // bâtisseur ne la relève : l'échéance quand il y en a une, la durée de
  // l'action qu'elle accompagne sinon, et à défaut celle que sa table donne.
  // Un bâtisseur qui DÉCIDE sur cette vie — et non seulement la recopie — la
  // lit ici.
  [[nodiscard]] std::int32_t InstanceLifeTicks(std::int32_t table_life_ticks) const noexcept {
    if (next_life_exact_ > 0) {
      return next_life_exact_;
    }
    return next_life_floor_ > 0 ? next_life_floor_ : table_life_ticks;
  }

  // EMMÈNE CE QUI EST ACCROCHÉ LÀ, nœuds déjà nés compris — chacun aux tics où
  // son effet reprend la place de son porteur, et à ceux-là seulement. Voir
  // `PooledEffectNode::bearer_follow`.
  //
  // 🔴 C'EST CE QUI MANQUAIT À UN EFFET QUI DURE. Faire suivre l'HORLOGE ne
  // déplace que les semis À VENIR ; un effet qui sème une fois et vit ensuite
  // dix mille tics — l'anneau du classement PVP en est un — n'a plus rien à
  // semer et restait donc à la case où son porteur se tenait quand il l'a reçu.
  //
  // `owner_body_degrees`, quand l'appelant le connaît, est l'angle du CORPS du
  // porteur tel que le client le tient : la coquille sphérique le relit à chaque
  // tic, qu'elle suive ou non sa place, et c'est lui qui tourne le bouclier.
  void MoveAnchored(std::uint32_t anchor_id, glm::vec3 ground_position,
                    std::optional<float> owner_body_degrees = std::nullopt);

  [[nodiscard]] bool HasAnchored(std::uint32_t anchor_id) const;

  // LA CIBLE A BOUGÉ : chaque nœud qui la vise reprend sa place comme point
  // visé. Voir `PooledEffectNode::target_actor_id`.
  void MoveTargeted(std::uint32_t target_actor_id, glm::vec3 ground_position);

  [[nodiscard]] bool HasTargeted(std::uint32_t target_actor_id) const;

  // RETIRE CE QUI EST ACCROCHÉ À UNE ANCRE QUE `still_here` REFUSE.
  //
  // 🔴 UN PORTEUR PARTI EMPORTE CE QU'IL PORTAIT, et sans cela il le LAISSE : un
  // nœud accroché ne meurt que de sa vie, et celle de l'anneau du classement PVP
  // vaut dix mille tics. Un joueur qui quitte la carte laissait donc son anneau
  // tourner sur une case vide jusqu'au changement de carte suivant.
  //
  // ⚠ CE QUI N'A PAS D'ANCRE NE BOUGE PAS. Zéro n'est pas une ancre absente,
  // c'est l'absence d'ancre : un impact posé au sol n'appartient à personne, et
  // demander à `still_here` ce qu'elle pense de zéro effacerait tout le décor.
  //
  // ⚠ UN PRÉDICAT ET NON UNE LISTE, parce que `render` ne connaît ni acteur ni
  // monde : l'appelant seul sait qui est encore là, et le lui demander évite de
  // bâtir un ensemble à chaque image pour n'y répondre que quelques fois.
  template <typename Predicate>
  void DropAnchoredUnless(Predicate still_here) {
    std::erase_if(nodes_, [&still_here](const PooledEffectNode& node) {
      return node.anchor_id != 0 && !still_here(node.anchor_id);
    });
  }

  // Fait avancer la troupe du temps écoulé, et retire ceux qui ont fini.
  //
  // Les nœuds battent par PAS DE SEIZE MILLISECONDES, et le reste d'un pas
  // incomplet est gardé : sans cela, une machine à cent vingt images par
  // seconde n'avancerait jamais d'un seul tic. C'est le compte du client, au
  // millième près : il divise le temps dû par le pas et ne consomme que ce que la division rend.
  // Rend le nombre de tics battus, zéro pour une image plus courte qu'un tic.
  //
  // ⚠ UNE AVANCE PAR IMAGE, JUSTE AVANT DE COMPOSER : elle finit par ce que le
  // dessin du client écrit dans les pistes — voir `CapTintedQuadAlpha`.
  std::int32_t Advance(std::uint32_t elapsed_ms);

  // LES TROIS TEMPS D'`Advance`, pour qui doit glisser entre deux passes ce que
  // le client y glisse : le bâtisseur de chaque instance sème au début du
  // passage, et ses nœuds sont mis à jour dans ce même passage. Combien de
  // tics cette image doit (le reste est gardé), UNE passe — `single_frame`
  // pour une image qui n'en doit aucun —, puis la fin d'image.
  [[nodiscard]] std::int32_t TakeDueTicks(std::uint32_t elapsed_ms);
  void Pass(bool single_frame);
  void FinishFrame();

  // LE POIDS DU TIC COURANT dans l'image, du reste d'un pas entamé : voir
  // `TickBlendWeight` et `effect_node_blend.h`. Un sans `blended`.
  [[nodiscard]] float TickWeight(bool blended) const noexcept;

  // LES DEUX ANGLES DE BILLBOARD, posés sur toute la troupe avant qu'elle
  // avance. `render` ne connaît pas la caméra : l'appelant les calcule et les
  // donne, ce qui garde le rendu testable hors jeu.
  //
  // ⚠ À APPELER AVANT `Advance`, et non après : c'est le mouvement qui les range
  // dans les pistes, et les poser ensuite les ferait lire avec un tic de retard.
  void FaceCamera(std::int16_t turn_degrees, std::int16_t tilt_degrees) {
    turn_degrees_ = turn_degrees;
    for (PooledEffectNode& node : nodes_) {
      node.billboard_turn_degrees = turn_degrees;
      node.billboard_tilt_degrees = tilt_degrees;
    }
  }

  // LE DERNIER CAP DE VUE que la troupe a recu.
  //
  // ⚠ UN SEMIS QUI FIGE L'ANGLE DE NAISSANCE EN A BESOIN, et il ne peut pas
  // l'interroger sur son noeud : celui-ci n'existe pas encore quand il le lui
  // faut. L'arc-en-ciel du sous-type 93 est dans ce cas — son inclinaison est le
  // cap du tic ou le sort est lance, et elle ne bouge plus jamais.
  [[nodiscard]] std::int16_t billboard_turn_degrees() const noexcept { return turn_degrees_; }

  // CE QUE LE MONDE DIT À LA TROUPE : l'altitude d'un point, et si la carte
  // courante élargit les dispersions. `render` ne connaît pas la carte ; c'est
  // l'appelant qui répond, ce qui garde le rendu testable hors jeu.
  void SetWorld(EffectScatterWorld world) { world_ = std::move(world); }
  // CE QUE LA TROUPE SAIT DU MONDE, pour un SEMIS qui lit le sol à la naissance
  // de ses pistes : les icônes du sous-type 123 se posent sur le relief.
  [[nodiscard]] const EffectScatterWorld& world() const noexcept { return world_; }

  // LA PORTE DE DESSIN : un nœud qu'elle refuse ne se dessine pas, et le plafond
  // que le dessin écrit ne le touche pas. `render` ne connaît pas les acteurs :
  // c'est l'appelant qui répond, à chaque image. Sans porte, tout se dessine.
  void SetDrawGate(std::function<bool(const PooledEffectNode&)> gate) {
    draw_gate_ = std::move(gate);
  }
  [[nodiscard]] bool PassesDrawGate(const PooledEffectNode& node) const {
    return !draw_gate_ || draw_gate_(node);
  }
  // CE QUI SE DESSINE À CETTE IMAGE : ce que la porte laisse passer, et qui a
  // déjà reçu une mise à jour. 🔴 Le client ne sème un nœud qu'en passant le
  // bâtisseur, et le met à jour dans ce même passage ; un nœud semé à la
  // naissance de son effet, avant tout passage, n'existe pas encore chez lui —
  // sauf pour une instance qui bat à chaque image, dont une image sans tic
  // sème sans mettre à jour. Un projectile d'acteur, qu'aucun effet ne sème
  // (numéro zéro), se montre dès sa naissance.
  [[nodiscard]] bool IsShown(const PooledEffectNode& node) const {
    return PassesDrawGate(node) && (node.node.age_ticks > 0 || node.sown_by_effect_id == 0 ||
                                    EffectInstanceBeatsEveryFrame(node.sown_by_effect_id));
  }

  // LES REBONDS DE L'IMAGE, que l'appelant EMPORTE pour en tirer un son. La
  // liste s'emporte parce qu'un rebond ne sonne qu'une fois, et qu'une image
  // peut avancer de plusieurs tics : le témoin par nœud en perdrait.
  [[nodiscard]] std::vector<EffectNodeSound> TakeBounces() {
    std::vector<EffectNodeSound> partis;
    partis.swap(bounces_);
    return partis;
  }

  [[nodiscard]] std::span<const PooledEffectNode> nodes() const noexcept { return nodes_; }
  [[nodiscard]] std::size_t size() const noexcept { return nodes_.size(); }

  void Clear() {
    nodes_.clear();
    bounces_.clear();
    owed_ms_ = 0;
  }

 private:
  // Voir `PooledEffectNode::instance_beats_left`.
  [[nodiscard]] std::int32_t InstanceBeatsLeft() const;
  // Voir `PooledEffectNode::instance_life_beats_left`.
  [[nodiscard]] std::int32_t InstanceLifeBeatsLeft() const;

  std::vector<PooledEffectNode> nodes_;
  std::vector<EffectNodeSound> bounces_;
  // Ce qui reste d'un pas entamé, en millisecondes — toujours moins qu'un pas.
  std::uint32_t owed_ms_ = 0;
  EffectScatterWorld world_;
  std::function<bool(const PooledEffectNode&)> draw_gate_;
  // Le dernier cap de vue, pour les semis qui figent leur angle de naissance.
  std::int16_t turn_degrees_ = 0;
  // Voir `AnchorNextTo`. Zéro tant que personne ne dit le contraire, et un semis
  // ordinaire n'a donc rien à faire.
  std::uint32_t next_anchor_ = 0;
  // De quel effet viennent les prochains nœuds — voir `SowFor`.
  std::uint32_t next_effect_ = 0;
  // Et quand ils reprennent la place de leur porteur — voir `SowFor`.
  EffectBearerFollow next_follow_ = kEffectFollowsEveryTick;
  // Voir `SowAtEffectTick`.
  std::int32_t next_effect_tick_ = 0;
  // Voir `SowAsHat`.
  bool next_from_hat_ = false;
  // Voir `AnchorNextTo`. Elle ne sert qu'à prendre l'écart d'un nœud ancré.
  glm::vec3 next_anchor_position_{0.0F, 0.0F, 0.0F};
  // Voir `LiveAtLeast`. Zéro laisse chaque patron décider, ce qui est le cas de
  // tout effet qui n'accompagne pas une action de durée connue.
  std::int32_t next_life_floor_ = 0;
  // Voir `LiveExactly`. Zéro rend la main aux deux précédents.
  std::int32_t next_life_exact_ = 0;
  // Voir `DieWithBearerAt`. Zéro : l'instance vit ce que sa table lui donne.
  std::int32_t next_bearer_death_tick_ = 0;
  // LA GRAINE DES MOUVEMENTS, et elle vit dans la troupe : une particule qui
  // renaît tire sa nouvelle place, et repartir de la même graine rejoue la même
  // nuée. La garder ici est ce qui rend un effet entier reproductible.
  std::uint32_t draw_ = 1;
};

}  // namespace ro::render
