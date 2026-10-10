#include "ro/render/effect_node_sprite_seed.h"

#include <algorithm>
#include <cstddef>

#include "ro/render/effect_chased_drift.h"
#include "ro/render/effect_duration.h"
#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node_pattern_table.h"

namespace ro::render {
namespace {

// Le tour complet, en degrés : chaque nœud tire son azimut dedans.
constexpr float kFullTurnDegrees = 359.0F;
// Combien de degrés entiers ce tirage compte.
constexpr std::int32_t kTurnValues = 360;
// Le quart de tour qui sépare l'angle du client du cap d'un bras.
constexpr float kQuarterTurnDegrees = 90.0F;

}  // namespace

EffectNodeSeed EffectNodeSeedFromPattern(const EffectNodePattern& pattern, std::int32_t ticks_left,
                                         std::int32_t tick, std::int32_t param) {
  EffectNodeSeed seed;
  // ⚠ UN RENDEZ-VOUS REMPLACE LA VIE, il ne la borne pas. Sans rendez-vous, la
  // vie reste celle du patron — TIRÉE s'il le faut — et c'est la fin de l'effet
  // qui l'éteint en fondu : voir `EffectNode::fades_with_its_instance`.
  //
  // 🔴 ET LA VIE VIENT PARFOIS DE L'EFFET : le bâtisseur recopie alors la durée
  // de l'instance dans son nœud — le feu de Demonstration vit autant que son
  // unité. Prendre la vie du patron, qui n'est que le défaut du constructeur,
  // l'éteignait en une seconde et demie. La fenêtre du tirage, elle, reste
  // celle du patron, comme pour un anneau : voir `EffectNodeFromPattern`.
  if (pattern.life_until_tick > 0) {
    seed.life_ticks = ticks_left;
  } else {
    const std::int32_t base = pattern.life_from_effect ? EffectNodeLifeFor(pattern.effect_id) +
                                                             pattern.life_offset_from_effect
                                                       : pattern.life_ticks;
    seed.life_ticks = base;
    seed.life_ticks_high = pattern.life_ticks_high > pattern.life_ticks
                               ? base + (pattern.life_ticks_high - pattern.life_ticks)
                               : pattern.life_ticks_high;
  }
  // La table porte le TIC où la sortie commence, comme le client ; le patron
  // du nœud veut une DURÉE. Moins un veut dire qu'il n'y a pas de sortie ; zéro
  // fait fondre dès la naissance, sur toute la vie.
  seed.fade_out_ticks = pattern.fade_out_tick >= 0 ? pattern.life_ticks - pattern.fade_out_tick : 0;
  seed.alpha_max = pattern.alpha_max;
  // LA DÉRIVE POURSUIVIE, qu'un sprite seul peut porter.
  if (pattern.shape == EffectNodeShape::kSprite) {
    seed.chased_drift = EffectChasedDriftPatternFor(pattern.effect_id);
  }
  seed.launch_heading_on_client_grid = pattern.launch_heading_on_client_grid;
  seed.launch_yaw_is_absolute = pattern.launch_yaw_is_absolute;
  seed.action_from_aim_quadrant = pattern.action_from_aim_quadrant;
  seed.aim_quadrant_actions = pattern.aim_quadrant_actions;
  seed.sits_on_aim_point = pattern.supplement.sits_on_aim_point;
  // L'INSTANCE QUI SE RÉCRIT UNE VIE À CHAQUE SEMIS a, dans la table des vies,
  // celle qui lui équivaut. Voir `EffectNodeFadesWithARewritingInstance`.
  if (pattern.supplement.instance_life_rewrite_base > 0 &&
      EffectInstanceLifeIsKnown(pattern.effect_id)) {
    seed.instance_rewrite = {pattern.supplement.instance_life_rewrite_base,
                             pattern.supplement.instance_life_rewrite_period_ticks, tick,
                             EffectInstanceLifeFor(pattern.effect_id)};
  }
  seed.instance_cycle = EffectNodePatternInstanceCycle(pattern, tick);
  seed.sprite_scale_steps = pattern.sprite_scale_steps;
  seed.aim_heading_on_client_table = pattern.aim_heading_on_client_table;
  seed.aim_face_offset_degrees = pattern.aim_face_offset_degrees;
  seed.dies_near_target = pattern.dies_near_target;
  // LA TEINTE DU PATRON, que cette voie laissait tomber : voir
  // `EffectNodeSeed::color`.
  for (std::size_t canal = 0; canal < seed.color.size(); ++canal) {
    seed.color[canal] = static_cast<float>(pattern.color[canal]);
  }
  // 🔴 ET LE RANG DU SEMIS LES REMPLACE TOUS DEUX, quand l'effet éteint sa
  // traînée par l'arrière : la première boule de Fire Ball reste blanche et
  // pleine, les trois suivantes s'orangent et s'effacent. Voir
  // `EffectTintByRank`, qui dit pourquoi le patron ne peut pas le porter.
  const EffectTintByRank* par_rang =
      EffectTintByRankFor(pattern.effect_id, EffectNodePatternSowingRank(pattern, tick));
  if (par_rang != nullptr) {
    if (!par_rang->keeps_color) {
      for (std::size_t canal = 0; canal < seed.color.size(); ++canal) {
        seed.color[canal] = static_cast<float>(par_rang->color[canal]);
      }
    }
    seed.alpha_max = par_rang->alpha;
  }
  // LE PATRON DIT UN DÉPART ET UN PAS, et le nœud les prend TELS QUELS, comme
  // ceux des anneaux. Les traduire en durée de montée ramenait au plafond un
  // sprite qui part pâle sans monter — voir `EffectNodeSeed::alpha_start`.
  // ⚠ SOUS LE PLAFOND, que le rang d'une traînée a pu abaisser ci-dessus.
  seed.alpha_start = std::min(pattern.alpha_start, seed.alpha_max);
  seed.alpha_start_high = pattern.alpha_start_high;
  seed.alpha_start_values =
      pattern.draw_values[static_cast<std::size_t>(EffectDrawnRange::kAlphaStart)];
  seed.alpha_step = pattern.alpha_step;
  // ⚠ SAUF POUR UN CAP SUR LA GRILLE : son bâtisseur ne tire aucun bras, et un
  // tirage de plus s'intercalerait entre son cap et sa teinte.
  // 🔴 L'ANGLE DU BRAS EST UN `rand() % 360` DU CLIENT : trois cent soixante
  // degrés entiers, et non un tirage continu.
  // ⚠ ET CERTAINS BÂTISSEURS L'ÉCRIVENT FIXE : l'angle du client `a` y porte le
  // bras par la troisième ligne de sa rotation de lacet, soit le cap `a − 90`
  // chez nous. Voir `EffectNodeSupplement::arm_is_fixed`.
  seed.orbit_degrees = pattern.launch_heading_on_client_grid
                           ? EffectNodeRange{0.0F, 0.0F}
                           : EffectNodeRange{0.0F, kFullTurnDegrees, kTurnValues};
  if (pattern.supplement.arm_is_fixed) {
    const float cap = pattern.supplement.arm_client_degrees - kQuarterTurnDegrees;
    seed.orbit_degrees = EffectNodeRange{cap, cap};
  }
  // LE LANCER, tel que le patron le porte. Tout à zéro veut dire « ce nœud ne
  // part nulle part », et c'est le cas de l'immense majorité.
  // ⚠ CHAQUE PLAGE AVEC SON NOMBRE DE VALEURS : voir `EffectNodeRange::values`.
  seed.launch_yaw_degrees = EffectNodePatternRange(pattern, EffectDrawnRange::kLaunchYaw,
                                                   pattern.launch_yaw_low, pattern.launch_yaw_high);
  seed.launch_pitch_degrees = EffectNodePatternRange(
      pattern, EffectDrawnRange::kLaunchPitch, pattern.launch_pitch_low, pattern.launch_pitch_high);
  seed.launch_distance =
      EffectNodePatternRange(pattern, EffectDrawnRange::kLaunchDistance,
                             pattern.launch_distance_low, pattern.launch_distance_high);
  seed.launch_drop = pattern.launch_drop;
  seed.step_length =
      EffectNodePatternRange(pattern, EffectDrawnRange::kStep, pattern.step_low, pattern.step_high);
  seed.step_decay_share = pattern.step_decay_share;
  seed.step_change_per_tick = pattern.step_change_per_tick;
  seed.aim_step_factor = pattern.aim_step_factor;
  seed.copy_roll_start_degrees = pattern.copy_roll_start_degrees;
  seed.copy_roll_step_degrees = pattern.copy_roll_step_degrees;
  seed.fall_speed = EffectNodePatternRange(pattern, EffectDrawnRange::kFallSpeed,
                                           pattern.fall_speed_low, pattern.fall_speed_high);
  seed.fall_acceleration_share = pattern.fall_acceleration_share;
  seed.aims_at_target = pattern.aims_at_target;
  // ⚠ VISER N'EST PAS S'ORIENTER, et les deux champs sont séparés pour cela :
  // les âmes de Soul Strike visent sans que leur image tourne.
  seed.turns_with_flight = pattern.turns_with_flight;
  // LE SOUS-TYPE 8 : le frein le long de l'axe, le balancement, et le roulis
  // du RANG de ce semis — `base + écart × rang`, lu par paramètre. Voir
  // `EffectRollByParam`.
  seed.brakes_along_axis = pattern.brakes_along_axis;
  seed.swing_speed = EffectNodePatternRange(pattern, EffectDrawnRange::kSwingSpeed,
                                            pattern.swing_speed_low, pattern.swing_speed_high);
  seed.swing_acceleration_share = pattern.swing_acceleration_share;
  if (const EffectRollByParam* roll = EffectRollByParamFor(pattern.effect_id, param);
      roll != nullptr) {
    seed.roll_degrees =
        roll->base_degrees +
        roll->spread_degrees * static_cast<float>(EffectNodePatternSowingRank(pattern, tick));
  }
  // LA TRAÎNÉE, telle que le patron la déclare : `EffectNodePattern::trail_trace_count`.
  seed.trail_trace_count = pattern.trail_trace_count;
  // ⚠ ET L'ACTION VIENT DU PATRON. Sans cette ligne, tout sprite jouerait sa
  // première animation, et deux effets qui ne diffèrent QUE par elle se
  // montreraient à l'identique.
  seed.action = pattern.action;
  seed.action_high = pattern.action_high;
  seed.action_alternate = pattern.action_alternate;
  seed.draws_bulb = pattern.draws_bulb;
  // ET LE PAS D'ÉCHELLE : la boule de feu de Sight rétrécit le
  // long de sa traînée.
  seed.sprite_scale_step = pattern.sprite_scale_step;
  seed.sprite_scale_step_share = pattern.sprite_scale_step_share;
  // ET LE REBOND AU SOL, avec le bruit qu'il fait — voir
  // `EffectNodePattern::bounces_at_floor`.
  seed.bounces_at_floor = pattern.bounces_at_floor;
  // L'ORBITE : le nœud tourne autour de son point de naissance, il ne se
  // déplace pas vers un cap. Sans dispersion elle ne fait rien — une
  // particule née au centre tourne autour d'elle-même.
  seed.orbit_degrees_per_tick = pattern.orbit_degrees_per_tick;
  // ET SON BRAS S'ALLONGE ; et le bâtisseur a pu baker ce bras en
  // décalage fixe, auquel cas la dispersion compte deux fois. Voir
  // `EffectNodePattern::scatter_counts_twice`.
  seed.orbit_radius_step_per_tick = pattern.orbit_radius_step_per_tick;
  seed.scatter_counts_twice = pattern.scatter_counts_twice;
  // L'ANIMATION DU `.act` : voir `EffectNodePattern::sprite_frame_hold_ticks`.
  if (pattern.sprite_frame_hold_ticks > 0) {
    seed.frame_hold_ticks = pattern.sprite_frame_hold_ticks;
    seed.frame_hold_ticks_high = pattern.sprite_frame_hold_ticks_high;
  }
  seed.loops_frames = !pattern.sprite_plays_once;
  seed.holds_last_frame = pattern.sprite_holds_last_frame;
  seed.rests_on_first_frame = pattern.supplement.rests_on_first_frame;
  seed.bounce_sound = pattern.bounce_sound;
  seed.bounce_sound_draws = pattern.bounce_sound_draws;
  // LES DRAPEAUX DU SPRITE, que cette voie laissait tomber : les étincelles de
  // Blessing ne clignotaient pas. Voir `EffectNodePattern::sprite_flags`.
  seed.flags = pattern.sprite_flags;
  if (par_rang != nullptr && par_rang->keeps_layer_color) {
    seed.flags |= kEffectNodeKeepsLayerColor;
  }
  seed.render_word = pattern.render_word;
  // LA ROTATION PROPRE DU SPRITE, et l'ordre dans lequel son lancer compose ses
  // deux angles. Voir `EffectNodePattern::sprite_spin_per_tick` et
  // `launch_pitch_before_yaw`.
  seed.spin_speed_per_tick = pattern.sprite_spin_per_tick;
  seed.launch_pitch_before_yaw = pattern.launch_pitch_before_yaw;
  // UN NŒUD D'EFFET DESSINE SES CALQUES TOURNÉS. Voir `EffectNode::draws_layer_angles`.
  seed.draws_layer_angles = true;
  seed.haunt_flight = pattern.haunt_flight;
  seed.nodes_stay_where_sown = pattern.nodes_stay_where_sown;
  seed.flies_from_target = pattern.flies_from_target;
  for (std::size_t canal = 0; canal < seed.color_high.size(); ++canal) {
    seed.color_high[canal] = static_cast<float>(pattern.color_high[canal]);
  }
  seed.color_draw_shared = pattern.color_draw_shared;
  // CE QUE LE TIC DE SEMIS CHANGE : la vague suivante naît plus bas, et sa
  // bascule tombe plus tôt dans sa vie — le rendez-vous est un tic de l'EFFET.
  // Voir `EffectNodePattern::launch_drop_sowing_tick_divisor`.
  if (pattern.launch_drop_sowing_tick_divisor > 0) {
    seed.launch_drop += static_cast<float>(tick / pattern.launch_drop_sowing_tick_divisor);
  }
  seed.copy_heading_step_degrees = pattern.copy_heading_step_degrees;
  seed.radial_speed = pattern.radial_speed;
  // ⚠ UN RENDEZ-VOUS DÉJÀ PASSÉ NE BASCULE JAMAIS : l'âge du nœud part de zéro
  // et n'égale pas un nombre négatif.
  if (pattern.radial_speed != 0.0F && pattern.radial_switch_until_tick > 0 &&
      tick <= pattern.radial_switch_until_tick) {
    seed.radial_ticks_to_switch = pattern.radial_switch_until_tick - tick;
  }
  seed.radial_speed_after_switch = pattern.radial_speed_after_switch;
  seed.radial_drift_after_switch = pattern.radial_drift_after_switch;
  // CE QUE LE SUPPLÉMENT DU PATRON DIT D'UN SPRITE : voir `EffectNodeSupplement`.
  const EffectNodeSupplement& en_plus = pattern.supplement;
  seed.fade_out_life_divisor = en_plus.fade_out_life_divisor;
  seed.launch_pitch_places_only = en_plus.launch_pitch_places_only;
  seed.orbit_acceleration_per_tick = en_plus.orbit_acceleration_per_tick;
  seed.copy_orbit_step_degrees = en_plus.copy_orbit_step_degrees;
  seed.fall_acceleration =
      EffectNodeRange{en_plus.fall_acceleration_low, en_plus.fall_acceleration_high,
                      en_plus.fall_acceleration_values};
  seed.launch_draws_discarded = en_plus.launch_draws_discarded;
  seed.draw_order = en_plus.sprite_draw_order;
  seed.orbit_radius_step_switch_tick = en_plus.orbit_radius_step_switch_tick;
  seed.orbit_radius_step_after_switch = en_plus.orbit_radius_step_after_switch;
  return seed;
}

EffectInstanceCycle EffectNodePatternInstanceCycle(const EffectNodePattern& pattern,
                                                   std::int32_t tick) {
  const std::int32_t wrap_ticks = pattern.supplement.age_wrap_ticks;
  if (wrap_ticks <= 0 || !EffectDurationIsKnown(pattern.effect_id)) {
    return {};
  }
  return {EffectDurationTicks(pattern.effect_id), tick % wrap_ticks};
}

}  // namespace ro::render
