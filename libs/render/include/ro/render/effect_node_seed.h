#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "ro/render/effect_haunt.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_sprite_draw_order.h"

namespace ro::render {

struct EffectChasedDriftPattern;

// LA DESCRIPTION D'UN SEMIS : ce qu'un patron dit d'un nœud avant qu'il existe.
//
// ⚠ ELLE VIT À PART DU RÉSERVOIR QUI LA FAIT VIVRE. Décrire et faire vivre sont
// deux affaires : la première se lit d'un bout à l'autre, la seconde est un
// catalogue de formes. Les mêler faisait passer l'en-tête sous le plafond de
// lignes, et surtout obligeait à lire tout le réservoir pour comprendre ce
// qu'un patron porte.

// Une PLAGE de tirage, bornes comprises.
struct EffectNodeRange {
  float low = 0.0F;
  float high = 0.0F;
  // 🔴 COMBIEN DE VALEURS LE CLIENT Y TIRE. Son tirage est `rand() % n`, que
  // des constantes ajoutent, multiplient ou divisent : il ne rend que `n`
  // valeurs également espacées de `low` à `high` — `(rand() % 60 + 30) / 10`
  // en fait soixante, de dixième en dixième. Zéro : le nombre n'est pas relevé,
  // et le tirage reste continu.
  std::int32_t values = 0;
};

// UN TIRAGE DANS UNE PLAGE, comme le client le fait quand elle porte son
// nombre de valeurs : `low + (Next % n) · (high − low) / (n − 1)`. Une plage
// vide rend sa borne basse SANS tirer — deux patrons voisins gardent ainsi la
// même suite de tirages, que l'un d'eux tire ou non.
[[nodiscard]] float DrawEffectNodeRange(const EffectNodeRange& range, std::uint32_t& generator);

// UN ENTIER TIRÉ DANS UNE PLAGE, bornes comprises : `low + Next % (high − low + 1)`.
// C'est le tirage d'un champ ENTIER du client — une vie, une cadence —, dont les
// valeurs sont toutes celles de la plage.
[[nodiscard]] std::int32_t DrawEffectNodeInteger(std::int32_t low, std::int32_t high,
                                                 std::uint32_t& generator);

// QUAND un patron sème, et combien de fois.
//
// ⚠ UN EFFET N'EST PAS UN SEMIS MAIS UNE SUITE DE SEMIS. Le client rappelle son
// bâtisseur à CHAQUE tic et garde chaque semis derrière une condition sur le
// compteur : EF_HEAL4 pose son anneau au tic zéro, un second au tic un, et lâche
// une bouffée d'étincelles tous les NEUF tics jusqu'au cinquantième. Tout semer
// d'un coup fait naître ensemble ce qui devait couler — sept fois moins de vert
// qu'il n'en faut, et c'est ce qu'on voyait en jeu.
//
// Une période NULLE veut dire « une seule fois, au premier tic », et c'est le
// cas le plus fréquent. Une période ne se DÉDUIT jamais : elle vient d'un
// modulo du client.
struct EffectNodeSchedule {
  std::int32_t first_tick = 0;
  std::int32_t last_tick = 0;
  std::int32_t period_ticks = 0;
  // LA PHASE DU MODULO : le semis tombe quand `(tic + phase) % période == 0`.
  // Le client écrit `(âge + 5) % 11` pour Soul Strike, et ses
  // boules naissent au sixième tic, non au onzième. Nulle presque partout, et
  // c'est ce qui laisse les trois autres champs s'écrire sans elle.
  std::int32_t phase_ticks = 0;
  // L'ÂGE OÙ LE BÂTISSEUR RAMÈNE SON INSTANCE À ZÉRO : l'horaire se rejoue alors
  // à l'identique, sans fin, et les trois premiers champs se lisent dans le
  // cycle. Zéro : l'âge ne repart jamais.
  std::int32_t cycle_ticks = 0;
};

// Ce tic-là est-il un tic de semis ?
[[nodiscard]] bool EffectNodeScheduleSowsAt(const EffectNodeSchedule& schedule, std::int32_t tick);

// Le DERNIER tic où ce patron sème : au-delà, son horloge n'a plus rien à faire.
[[nodiscard]] std::int32_t EffectNodeScheduleLastSowTick(const EffectNodeSchedule& schedule);

// UNE INSTANCE QUI SE RÉCRIT UNE VIE À CHAQUE SEMIS : `base − âge`, à chaque âge
// multiple de `period_ticks`. Une base nulle : elle ne la récrit pas.
struct EffectInstanceRewrite {
  std::int32_t base = 0;
  std::int32_t period_ticks = 0;
  // Le tic de l'instance où le nœud naît.
  std::int32_t sown_at_tick = 0;
  // LA VIE FIXE QUI LUI ÉQUIVAUT : celle que la table des vies porte pour elle,
  // et qui donne à ses nœuds autant de mises à jour. Elle meurt au passage
  // `death_tick − 1`, son âge, une fois crû, ayant rejoint la vie écrite.
  std::int32_t death_tick = 0;
};

// UNE INSTANCE DONT LE BÂTISSEUR RAMÈNE L'ÂGE À ZÉRO : elle ne meurt jamais,
// mais sa vie de table, que rien ne récrit, borne encore ses nœuds par le
// fondu. Une vie nulle : son âge ne reboucle pas.
struct EffectInstanceCycle {
  std::int32_t life_ticks = 0;
  // L'âge de l'instance où le nœud naît, dans son cycle.
  std::int32_t sown_at_cycle_tick = 0;
};

// Le patron d'un nœud : ce que le client tire, et entre quelles bornes.
//
// Les rampes ne sont pas décrites par leur pas mais par leur DURÉE, et le pas
// s'en déduit — c'est ainsi que le client procède, et c'est ce qui garde une
// montée d'alpha cohérente quand on change le plafond.
struct EffectNodeSeed {
  // Le fichier de la texture, dans l'archive.
  std::string_view resource_path;
  std::int32_t life_ticks = 0;
  // LA BORNE HAUTE D'UNE VIE TIREE, et zero quand il n'y a pas de tirage.
  //
  // 🔴 LE CLIENT TIRE LA VIE DE CHAQUE PARTICULE. Une vie unique les fait toutes
  // disparaitre au meme instant — et surtout parcourir exactement la meme
  // distance, ce qui change une gerbe en cercle.
  std::int32_t life_ticks_high = 0;
  // En combien de tics l'alpha atteint son plafond, puis en combien il redescend.
  std::int32_t alpha_rise_ticks = 1;
  std::int32_t fade_out_ticks = 0;
  float alpha_max = kEffectNodeChannelMax;
  // 🔴 OU BIEN L'ALPHA DE DÉPART ET SON PAS, TELS QUE LE CLIENT LES ÉCRIT — champs
  // 804 et 808 —, et ils l'emportent alors sur la durée de montée. Un sprite
  // qui part à deux cent vingt SANS pas reste à deux cent vingt : la durée d'une
  // montée d'un tic le portait au plafond, et le rendait plein là où le client
  // le veut pâle. La borne haute, quand elle n'est pas nulle, fait TIRER le
  // départ de chaque nœud.
  std::optional<float> alpha_start;
  float alpha_start_high = 0.0F;
  float alpha_step = 0.0F;
  // 🔴 LA TEINTE DU NŒUD, ET LA GRAINE N'EN PORTAIT AUCUNE. Le patron la relève
  // — `EffectNodePattern::color` —, la voie des ANNEAUX la posait, et celle des
  // SPRITES la laissait tomber en chemin : les quatre boules de Fire Ball
  // sortaient blanches et pleines, là où le client n'en garde qu'une ainsi et
  // teinte les trois autres. Deux patrons de sprite sur deux cent quatre-vingts
  // en portent une ; l'absence ne se voyait donc que sur eux.
  std::array<float, 3> color = {kEffectNodeChannelMax, kEffectNodeChannelMax,
                                kEffectNodeChannelMax};

  EffectNodeRange half_width;
  EffectNodeRange half_height;
  EffectNodeRange step_length;
  EffectNodeRange orbit_degrees;

  // LE LANCER : ce qui fait PARTIR la particule. Le cap est un ÉCART à la face
  // du porteur, la chute sépare la famille qui s'écarte de celle qui retombe, et
  // tout est expliqué dans `ro/render/effect_launch.h`.
  EffectNodeRange launch_yaw_degrees;
  EffectNodeRange launch_pitch_degrees;
  EffectNodeRange launch_distance;
  float launch_drop = 0.0F;
  float step_decay_share = 0.0F;
  // ET CE QUE LE PAS GAGNE PAR TIC EN DUR, qui s'ajoute à la part ci-dessus. Un
  // pas nul qui en gagne un PART quand même : voir `DrawEffectLaunch`.
  float step_change_per_tick = 0.0F;
  // Voir `EffectNodePattern::aim_step_factor` et `copy_roll_start_degrees`.
  float aim_step_factor = 1.0F;
  float copy_roll_start_degrees = 0.0F;
  float copy_roll_step_degrees = 0.0F;
  EffectNodeRange fall_speed;
  float fall_acceleration_share = 0.0F;
  // 🔴 VRAI QUAND LA PARTICULE VISE LA CIBLE DE L'EFFET : sa direction est
  // celle qui va de sa naissance à la cible, À PLAT, et son pas la distance
  // divisée par sa vie — elle arrive au dernier tic. Le client la fait se
  // tourner vers la cible et écrit `pas = distance / vie`. La boule de Fire
  // Ball et les âmes de Soul Strike
  // sont de celles-là ; sans cible, elles ne partent pas.
  bool aims_at_target = false;
  // LE SOUS-TYPE 8 : sa vitesse verticale se RETRANCHE au pas au lieu de
  // tomber, et un balancement perpendiculaire au vol, roulé de `roll_degrees`
  // autour de l'axe, fait l'arc d'une boule. Voir
  // `EffectNodePattern::brakes_along_axis` et `PooledEffectNode::swing_amount`.
  bool brakes_along_axis = false;
  EffectNodeRange swing_speed;
  float swing_acceleration_share = 0.0F;
  float roll_degrees = 0.0F;
  // COMBIEN DE TRACES LA TRAÎNÉE GARDE, un pour un sprite qui se dessine une
  // seule fois. Voir `EffectNodePattern::trail_trace_count`.
  std::int32_t trail_trace_count = 1;
  // LA ROTATION D'ÉCRAN DU SPRITE, et si elle se dessine — voir
  // `EffectNode::draws_rotated`. Une flèche d'arc s'oriente ainsi sur son vol.
  float spin_degrees = 0.0F;
  bool draws_rotated = false;
  // 🔴 ET LA MÊME ROTATION, MAIS RECALCULÉE À CHAQUE IMAGE, depuis la direction
  // du vol. Voir `EffectNodePattern::turns_with_flight`.
  //
  // ⚠ CE N'EST PAS `draws_rotated` AVEC UN ANGLE : celui-là fige l'angle au
  // semis, et une caméra qui tourne le laisse en arrière. Un angle d'écran suit
  // la caméra, et c'est la composition — la seule qui la connaisse — qui le
  // pose.
  bool turns_with_flight = false;

  // QUELLE animation du `.act` le nœud joue. Voir `EffectNodePattern::action`,
  // et `EffectNodePattern::action_high` pour une action TIRÉE.
  std::int32_t action = 0;
  std::int32_t action_high = 0;
  std::int32_t action_alternate = -1;
  // Voir `EffectNodePattern::draws_bulb`.
  bool draws_bulb = false;
  // DE COMBIEN L'ÉCHELLE DU SPRITE CHANGE À CHAQUE TIC — l'échelle elle-même
  // se tire au semis, hors de la graine. Voir `EffectNodePattern::sprite_scale_step`.
  float sprite_scale_step = 0.0F;
  // OU QUELLE PART DE L'ÉCHELLE TIRÉE, divisée par la vie tirée : voir
  // `EffectNodePattern::sprite_scale_step_share`. Non nulle, elle l'emporte.
  float sprite_scale_step_share = 0.0F;
  // L'AVANCE D'IMAGE d'un sprite joué comme un nœud : combien d'images son
  // action compte, si elles bouclent, et — quand la cadence du `.act` ne tient
  // pas dans un compte entier de tics — leur période fractionnaire. Voir
  // `EffectNode::frame_period_ticks`. Les défauts sont ceux du nœud : une
  // image, qui boucle, à la retenue entière.
  //
  // ⚠ LE COMPTE D'IMAGES N'EST PAS UNE AFFAIRE DE PATRON : c'est l'action du
  // `.act` qui le dit, et le semis le lit dans le sprite chargé. Un compte laissé
  // à un figeait tout sprite de nœud sur sa première image.
  std::size_t texture_count = 1;
  bool loops_frames = true;
  float frame_period_ticks = 0.0F;
  // TOUS LES COMBIEN DE TICS L'IMAGE CHANGE — quatre par défaut chez le client,
  // et TIRÉ quand la borne haute le dépasse. Voir
  // `EffectNodePattern::sprite_frame_hold_ticks`.
  std::int32_t frame_hold_ticks = kEffectNodeDefaultFrameHoldTicks;
  std::int32_t frame_hold_ticks_high = 0;
  // ET SI, SANS BOUCLE, LA DERNIÈRE IMAGE SE FIGE au lieu de tuer le nœud.
  // Voir `EffectNode::holds_last_frame`.
  bool holds_last_frame = false;
  // Voir `EffectNode::rests_on_first_frame`.
  bool rests_on_first_frame = false;

  // LE REBOND AU SOL ET SON BRUIT, recopiés du patron — voir
  // `EffectNodePattern::bounces_at_floor`.
  //
  // ⚠ ILS PASSENT PAR LA GRAINE PARCE QUE C'EST ELLE QUI SÈME LES SPRITES. Un
  // patron de sprite ne va PAS au semeur d'anneau : les y poser seuls les
  // laissait sur un chemin que les pièces d'`EF_COIN` n'empruntent jamais, et
  // l'effet restait muet alors que tout le reste était en place.
  bool bounces_at_floor = false;
  std::string_view bounce_sound;
  std::uint8_t bounce_sound_draws = 0;

  std::uint32_t flags = 0;
  // Le mot de rendu du patron. Voir `EffectNode::render_word`.
  std::optional<std::uint32_t> render_word;

  // DE COMBIEN DE DEGRÉS LE NŒUD TOURNE AUTOUR DE SON POINT DE NAISSANCE À
  // CHAQUE TIC. Voir
  // `EffectNodePattern::orbit_degrees_per_tick`.
  //
  // ⚠ IL PASSE PAR LA GRAINE POUR LA MÊME RAISON QUE LE REBOND : c'est elle qui
  // sème les sprites, et un patron de sprite ne va pas au semeur d'anneau.
  float orbit_degrees_per_tick = 0.0F;
  // DE COMBIEN LE BRAS DE L'ORBITE S'ALLONGE À CHAQUE TIC. Voir
  // `EffectNodePattern::orbit_radius_step_per_tick`.
  float orbit_radius_step_per_tick = 0.0F;
  // LA DISPERSION COMPTE-T-ELLE DEUX FOIS ? Voir
  // `EffectNodePattern::scatter_counts_twice` : le bâtisseur a baké l'écart en
  // décalage fixe, et le mouvement l'ajoute une seconde fois en le faisant
  // tourner. Le nœud naît alors au double de sa dispersion, et son cercle est
  // centré sur le point baké.
  bool scatter_counts_twice = false;
  // DE COMBIEN DE DEGRÉS LE SPRITE TOURNE SUR LUI-MÊME À CHAQUE TIC. Voir
  // `EffectNodePattern::sprite_spin_per_tick`.
  float spin_speed_per_tick = 0.0F;
  // L'INCLINAISON AVANT LE CAP. Voir `EffectNodePattern::launch_pitch_before_yaw`.
  bool launch_pitch_before_yaw = false;
  // L'ANGLE DES CALQUES tourne-t-il leurs quads ? Voir
  // `EffectNode::draws_layer_angles`.
  bool draws_layer_angles = false;
  // LA RONDE D'UNE APPARITION. Voir `EffectNodePattern::haunt_flight`.
  EffectHauntFlight haunt_flight = EffectHauntFlight::kNone;
  // Voir `EffectNodePattern::nodes_stay_where_sown`.
  bool nodes_stay_where_sown = false;
  // LE VOL RETOURNÉ ET LA TEINTE TIRÉE. Voir `EffectNodePattern::flies_from_target`
  // et `EffectNodePattern::color_high` : une borne haute nulle ne tire rien.
  bool flies_from_target = false;
  std::array<float, 3> color_high = {};
  bool color_draw_shared = false;
  // LE CAP FIXE D'UN EXEMPLAIRE, et la voie radiale accélérée. Voir
  // `EffectNodePattern::copy_heading_step_degrees` et `radial_speed`. La bascule
  // est ici un COMPTE À REBOURS en tics du nœud, résolu au semis depuis le
  // rendez-vous du patron ; moins un : pas de bascule.
  float copy_heading_step_degrees = 0.0F;
  float radial_speed = 0.0F;
  std::int32_t radial_ticks_to_switch = -1;
  float radial_speed_after_switch = 0.0F;
  std::array<float, 3> radial_drift_after_switch = {};
  // LA DÉRIVE POURSUIVIE que l'effet demande à ses sprites, ou nulle. Ses
  // tirages passent AVANT tous les autres du nœud : voir `SowEffectChasedDrift`.
  const EffectChasedDriftPattern* chased_drift = nullptr;
  // LE CAP SUR LA GRILLE DU CLIENT et l'échelle en pas égaux. Voir
  // `EffectNodePattern::launch_heading_on_client_grid` et `sprite_scale_steps`.
  bool launch_heading_on_client_grid = false;
  std::int32_t sprite_scale_steps = 0;
  // LE CAP VISÉ LU DANS LA TABLE DU CLIENT, et l'écart de la face. Voir
  // `EffectNodePattern::aim_heading_on_client_table`.
  bool aim_heading_on_client_table = false;
  float aim_face_offset_degrees = 0.0F;
  // Voir `EffectNodePattern::dies_near_target`.
  bool dies_near_target = false;
  // Le nombre de valeurs de l'alpha de départ tiré : voir `EffectNodeRange::values`.
  std::int32_t alpha_start_values = 0;
  // Voir `EffectNodePattern::launch_yaw_is_absolute` et `action_from_aim_quadrant`.
  bool launch_yaw_is_absolute = false;
  bool action_from_aim_quadrant = false;
  EffectActionsByQuarter aim_quadrant_actions = kEffectArrowActionsByQuarter;
  // Voir `EffectNodeSupplement::sits_on_aim_point`.
  bool sits_on_aim_point = false;
  // Voir `EffectNodeSupplement` : le fondu rapporté à la vie tirée, l'inclinaison
  // qui ne place que le départ, l'orbite qui accélère et son cap par rang,
  // l'accélération verticale tirée et les tirages jetés avant le lancer.
  std::int32_t fade_out_life_divisor = 0;
  bool launch_pitch_places_only = false;
  float orbit_acceleration_per_tick = 0.0F;
  float copy_orbit_step_degrees = 0.0F;
  EffectNodeRange fall_acceleration;
  std::uint8_t launch_draws_discarded = 0;
  // L'ORDRE DES TIRAGES DU BÂTISSEUR, et ce qui a été tiré d'avance dans cet
  // ordre : voir `EffectSpriteDraw` et `PredrawEffectSprite`.
  EffectSpriteDrawOrder draw_order;
  EffectSpritePredrawn predrawn{};
  // LES ANGLES DU DERNIER PROJECTILE, quand le nœud part de lui : son cap et
  // son inclinaison tirés s'y ajoutent, et la direction passe par les tables du
  // client. Voir `EffectNodeSupplement::launches_from_last_projectile`.
  std::optional<EffectProjectileAngles> last_projectile;
  // Voir `EffectNodeSupplement::orbit_radius_step_switch_tick`.
  std::int32_t orbit_radius_step_switch_tick = -1;
  float orbit_radius_step_after_switch = 0.0F;
  // 🔴 L'IMAGE TOURNE DE LA FACE DE SON VOL VUE DE LA CAMÉRA — le lacet seul,
  // recalculé à chaque image : ce que le client fait d'un projectile d'acteur,
  // la flèche d'un arc. Voir `PooledEffectNode::turns_with_flight_heading`.
  //
  // ⚠ CE N'EST PAS `turns_with_flight` : celui-là suit la PROJECTION du vol,
  // plongée de la caméra comprise, et c'est un autre angle.
  bool turns_with_flight_heading = false;
  // Ce que l'image tourne en plus de cette face : voir
  // `PooledEffectNode::flight_heading_turn_degrees`.
  std::int32_t flight_heading_turn_degrees = 0;
  // LA PREMIÈRE IMAGE TENUE ce nombre de tics avant l'avance : voir
  // `EffectNode::first_frame_hold_ticks`.
  std::int32_t first_frame_hold_ticks = 0;
  // L'angle d'écran en degrés réels, sans troncature : voir
  // `EffectNode::spins_by_exact_degrees`.
  bool spins_by_exact_degrees = false;
  // 🔴 LE VOL VISÉ GARDE SA HAUTEUR : la direction va de la naissance au point
  // visé EN TROIS DIMENSIONS, et le pas est cette distance-là divisée par la
  // vie — la ligne droite d'un projectile d'acteur, qui monte vers une cible
  // plus haute et descend vers une plus basse. Sans lui, un vol visé se
  // calcule À PLAT, et la hauteur reste à la chute. Voir `aims_at_target`.
  bool aim_keeps_height = false;
  // LA VIE QUE SON INSTANCE SE RÉCRIT À CHAQUE SEMIS, quand elle le fait. Voir
  // `EffectNodeFadesWithARewritingInstance`.
  EffectInstanceRewrite instance_rewrite{};
  // LE CYCLE DE SON INSTANCE, quand son âge reboucle. Voir
  // `EffectNodeFadesWithACyclingInstance`.
  EffectInstanceCycle instance_cycle{};
};

// TIRE D'AVANCE, DANS L'ORDRE DU BÂTISSEUR, tout ce que ce semis tirera, et le
// range dans `seed.predrawn` : le semis et le lancer le liront ensuite au lieu
// de tirer. Une sorte que l'ordre ne nomme pas prend sa borne basse. Sans ordre,
// rien ne change.
void PredrawEffectSprite(EffectNodeSeed& seed, EffectNodeRange sprite_scale,
                         std::uint32_t& generator);

// Sème un nœud d'après son patron, en tirant dans le générateur donné.
//
// Le générateur est passé PAR RÉFÉRENCE et avance : semer deux nœuds de suite
// donne deux nœuds différents, et repartir de la même graine rejoue la même
// foule. C'est ce qui rend un effet entier vérifiable au banc.
[[nodiscard]] EffectNode SowEffectNode(const EffectNodeSeed& seed, std::uint32_t& generator);

// 🔴 LE FONDU QU'IMPOSE UNE INSTANCE QUI SE RÉCRIT UNE VIE À CHAQUE SEMIS, jugé
// au semis : la troupe ne lit pas ce reste-là. Voir
// `EffectNode::fades_with_its_instance` pour la règle.
//
// LE RESTE DE L'INSTANCE BAISSE PLUS VITE QUE CELUI DU NŒUD : à chaque semis,
// la vie écrite perd la période entière. L'écart entre les deux restes, fixe
// entre deux semis, grandit donc d'une période à chacun, et le fondu s'engage
// au premier semis où le reste du nœud dépasse celui de l'instance plus un —
// pas forcément à la naissance. Il prend alors le reste de l'instance tel que
// le nœud le lit à ce passage, l'âge de l'instance déjà crû.
void EffectNodeFadesWithARewritingInstance(EffectNode& node, const EffectInstanceRewrite& rewrite);

// LE MÊME FONDU, CONTRE UNE INSTANCE DONT L'ÂGE REBOUCLE, jugé au semis.
//
// Son âge ne fait que croître jusqu'au bout du cycle, et son reste n'y fait que
// baisser, au même pas que celui du nœud : l'écart qui décide ne change pas
// d'un passage à l'autre. Quand l'âge reboucle, son reste remonte et ne peut
// plus le décider. Le fondu s'engage donc dès la naissance, ou jamais.
void EffectNodeFadesWithACyclingInstance(EffectNode& node, const EffectInstanceCycle& cycle);

}  // namespace ro::render
