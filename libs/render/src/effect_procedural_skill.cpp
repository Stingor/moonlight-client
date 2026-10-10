// CE QU'UNE COMPÉTENCE MONTRE, quand son effet n'a pas de fichier.
//
// ⚠ LA QUESTION NE SE POSE PAS AU GÉNÉRATEUR. Le client attache plusieurs
// effets à une même compétence, et vingt-trois familles de patrons peuvent en faire
// l'image — chacune engendrée par un script à part, qui ne voit pas les vingt-deux
// autres. Celui qui triait la jointure ne connaissait que la sienne : toute
// compétence servie par une autre famille passait pour muette, et Heal en était.
//
// La table porte donc les couples du client sans les trier, et le choix se fait
// ici, où les vingt-trois tables sont visibles d'un coup. Sa réponse suit le dessin :
// une famille de plus portée, et des compétences se montrent sans qu'on ait
// réengendré quoi que ce soit.

#include "ro/render/effect_aimed_spoke_seed.h"
#include "ro/render/effect_anchored_skirt_seed.h"
#include "ro/render/effect_arc_ribbon_seed.h"
#include "ro/render/effect_arcing_light_seed.h"
#include "ro/render/effect_aura_column_seed.h"
#include "ro/render/effect_bobbing_note.h"
#include "ro/render/effect_break_ring_seed.h"
#include "ro/render/effect_breathing_quad_seed.h"
#include "ro/render/effect_camera_fan_seed.h"
#include "ro/render/effect_cloud_ring_seed.h"
#include "ro/render/effect_colored_veil.h"
#include "ro/render/effect_concentric_square_seed.h"
#include "ro/render/effect_cone_wedge_seed.h"
#include "ro/render/effect_cycling_skirt_seed.h"
#include "ro/render/effect_dark_veil_seed.h"
#include "ro/render/effect_double_ribbon_seed.h"
#include "ro/render/effect_drifting_icon_seed.h"
#include "ro/render/effect_drifting_mote_seed.h"
#include "ro/render/effect_electric_arc_seed.h"
#include "ro/render/effect_expanding_ring_seed.h"
#include "ro/render/effect_fading_sprite_seed.h"
#include "ro/render/effect_fading_streak_seed.h"
#include "ro/render/effect_fading_twin_disc_seed.h"
#include "ro/render/effect_falling_seed.h"
#include "ro/render/effect_floor_spinner.h"
#include "ro/render/effect_flying_shard_seed.h"
#include "ro/render/effect_frame_and_bar_seed.h"
#include "ro/render/effect_gliding_slash_seed.h"
#include "ro/render/effect_glow_sprite_seed.h"
#include "ro/render/effect_grand_cross_seed.h"
#include "ro/render/effect_grid_plane_seed.h"
#include "ro/render/effect_ground_box_seed.h"
#include "ro/render/effect_ground_curtain_seed.h"
#include "ro/render/effect_ground_disc_seed.h"
#include "ro/render/effect_ground_gust_seed.h"
#include "ro/render/effect_ground_ribbon_seed.h"
#include "ro/render/effect_growing_sprite_seed.h"
#include "ro/render/effect_heal_sparkle_seed.h"
#include "ro/render/effect_heart_ring_seed.h"
#include "ro/render/effect_hit_cross_seed.h"
#include "ro/render/effect_homing_gust_seed.h"
#include "ro/render/effect_homing_spark_seed.h"
#include "ro/render/effect_job_aura_seed.h"
#include "ro/render/effect_layered_ring_seed.h"
#include "ro/render/effect_lens_flare_seed.h"
#include "ro/render/effect_lightning_fan_seed.h"
#include "ro/render/effect_magic_target_seed.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_palette_billboard_seed.h"
#include "ro/render/effect_particle_ring_seed.h"
#include "ro/render/effect_particle_seed.h"
#include "ro/render/effect_pentagon_trail_seed.h"
#include "ro/render/effect_pie_wedge_seed.h"
#include "ro/render/effect_pulsing_rings_seed.h"
#include "ro/render/effect_quad_burst_seed.h"
#include "ro/render/effect_radial_petal_seed.h"
#include "ro/render/effect_rain_particle_seed.h"
#include "ro/render/effect_rainbow_trail_seed.h"
#include "ro/render/effect_rainstorm_cylinder_seed.h"
#include "ro/render/effect_ribbon_wave_seed.h"
#include "ro/render/effect_ring_quartet_seed.h"
#include "ro/render/effect_ring_starburst_seed.h"
#include "ro/render/effect_rising_quad.h"
#include "ro/render/effect_rising_spark_seed.h"
#include "ro/render/effect_rolling_gust_seed.h"
#include "ro/render/effect_scatter_seed.h"
#include "ro/render/effect_screen_burst_seed.h"
#include "ro/render/effect_screen_seed.h"
#include "ro/render/effect_scrolling_band_seed.h"
#include "ro/render/effect_seeded_ring.h"
#include "ro/render/effect_sheathed_bolt_seed.h"
#include "ro/render/effect_shield_boomerang_seed.h"
#include "ro/render/effect_shockwave_ray_seed.h"
#include "ro/render/effect_snaking_arc_seed.h"
#include "ro/render/effect_spark_tube_seed.h"
#include "ro/render/effect_sphere_shell_seed.h"
#include "ro/render/effect_spike_beam_seed.h"
#include "ro/render/effect_spin_ribbon_seed.h"
#include "ro/render/effect_spinning_box_seed.h"
#include "ro/render/effect_spinning_round_seed.h"
#include "ro/render/effect_spiral_column_seed.h"
#include "ro/render/effect_spiral_emitter_seed.h"
#include "ro/render/effect_spiral_fan_seed.h"
#include "ro/render/effect_spiral_stack_seed.h"
#include "ro/render/effect_spiral_trail_seed.h"
#include "ro/render/effect_sprite_fan_seed.h"
#include "ro/render/effect_swarm_seed.h"
#include "ro/render/effect_swirl_trail_seed.h"
#include "ro/render/effect_tapered_sheet_seed.h"
#include "ro/render/effect_teleport_rings_seed.h"
#include "ro/render/effect_tethered_beam_seed.h"
#include "ro/render/effect_tetra_vortex_seed.h"
#include "ro/render/effect_thrown_item_seed.h"
#include "ro/render/effect_thunder_freeze_seed.h"
#include "ro/render/effect_thunder_scatter_seed.h"
#include "ro/render/effect_tinted_quad_seed.h"
#include "ro/render/effect_track_seed.h"
#include "ro/render/effect_tri_plane_seed.h"
#include "ro/render/effect_triple_wall_seed.h"
#include "ro/render/effect_turning_ring_seed.h"
#include "ro/render/effect_twin_disc_seed.h"
#include "ro/render/effect_typed_ribbon_seed.h"
#include "ro/render/effect_walking_sower_seed.h"
#include "ro/render/effect_wandering_motes_seed.h"
#include "ro/render/effect_warp_ring_seed.h"
#include "ro/render/effect_white_burst_seed.h"

namespace ro::render {

bool KnowsProceduralEffect(std::uint32_t effect_id) {
  return !EffectNodePatternsFor(effect_id).empty() ||
         !EffectRingNodePatternsFor(effect_id).empty() ||
         !EffectRisingParticlePatternsFor(effect_id).empty() ||
         !EffectArcRibbonNodePatternsFor(effect_id).empty() ||
         !EffectTintedQuadNodePatternsFor(effect_id).empty() ||
         !EffectScatterNodePatternsFor(effect_id).empty() ||
         !EffectFallingNodePatternsFor(effect_id).empty() ||
         !EffectGlowSpriteNodePatternsFor(effect_id).empty() ||
         !EffectSpiralFanNodePatternsFor(effect_id).empty() ||
         !EffectLayeredRingNodePatternsFor(effect_id).empty() ||
         !EffectParticleRingNodePatternsFor(effect_id).empty() ||
         !EffectSeededRingPatternsFor(effect_id).empty() ||
         !EffectRisingSparkPatternsFor(effect_id).empty() ||
         !EffectScrollingBandPatternsFor(effect_id).empty() ||
         !EffectDarkVeilPatternsFor(effect_id).empty() ||
         !EffectTypedRibbonPatternsFor(effect_id).empty() ||
         !EffectPentagonTrailPatternsFor(effect_id).empty() ||
         !EffectPaletteBillboardPatternsFor(effect_id).empty() ||
         !EffectSpiralStackPatternsFor(effect_id).empty() ||
         !EffectBreathingQuadPatternsFor(effect_id).empty() ||
         !EffectGroundDiscPatternsFor(effect_id).empty() ||
         !EffectThrownItemPatternsFor(effect_id).empty() ||
         !EffectPieWedgePatternsFor(effect_id).empty() ||
         !EffectGrowingSpritePatternsFor(effect_id).empty() ||
         !EffectGridPlanePatternsFor(effect_id).empty() ||
         !EffectCyclingSkirtPatternsFor(effect_id).empty() ||
         !EffectGrandCrossPatternsFor(effect_id).empty() ||
         !EffectSpikeBeamPatternsFor(effect_id).empty() ||
         !EffectAuraColumnPatternsFor(effect_id).empty() ||
         !EffectRainParticlePatternsFor(effect_id).empty() ||
         !EffectDoubleRibbonPatternsFor(effect_id).empty() ||
         !EffectSpiralTrailPatternsFor(effect_id).empty() ||
         !EffectRainbowTrailPatternsFor(effect_id).empty() ||
         !EffectFadingStreakPatternsFor(effect_id).empty() ||
         !EffectConeWedgePatternsFor(effect_id).empty() ||
         !EffectGroundRibbonPatternsFor(effect_id).empty() ||
         !EffectPulsingRingsPatternsFor(effect_id).empty() ||
         !EffectWarpRingPatternsFor(effect_id).empty() ||
         !EffectSpiralColumnPatternsFor(effect_id).empty() ||
         !EffectConcentricSquarePatternsFor(effect_id).empty() ||
         !EffectTripleWallPatternsFor(effect_id).empty() ||
         !EffectSphereShellPatternsFor(effect_id).empty() ||
         !EffectSwarmPatternsFor(effect_id).empty() ||
         !EffectGroundCurtainPatternsFor(effect_id).empty() ||
         !EffectSparkTubePatternsFor(effect_id).empty() ||
         !EffectGlidingSlashPatternsFor(effect_id).empty() ||
         !EffectQuadBurstPatternsFor(effect_id).empty() ||
         !EffectFlyingShardPatternsFor(effect_id).empty() ||
         !EffectDriftingMotePatternsFor(effect_id).empty() ||
         !EffectSheathedBoltPatternsFor(effect_id).empty() ||
         !EffectFrameAndBarPatternsFor(effect_id).empty() ||
         !EffectGroundBoxPatternsFor(effect_id).empty() ||
         !EffectGroundGustPatternsFor(effect_id).empty() ||
         !EffectHomingGustPatternsFor(effect_id).empty() ||
         !EffectRollingGustPatternsFor(effect_id).empty() ||
         !EffectSwirlTrailPatternsFor(effect_id).empty() ||
         !EffectDriftingIconPatternsFor(effect_id).empty() ||
         !EffectElectricArcPatternsFor(effect_id).empty() ||
         !EffectTetheredBeamPatternsFor(effect_id).empty() ||
         !EffectSpinRibbonPatternsFor(effect_id).empty() ||
         !EffectSnakingArcPatternsFor(effect_id).empty() ||
         !EffectSpinningRoundPatternsFor(effect_id).empty() ||
         !EffectWanderingMotePatternsFor(effect_id).empty() ||
         !EffectAnchoredSkirtPatternsFor(effect_id).empty() ||
         !EffectLightningFanPatternsFor(effect_id).empty() ||
         !EffectHealSparklePatternsFor(effect_id).empty() ||
         !EffectRisingQuadPatternsFor(effect_id).empty() ||
         !EffectHomingSparkPatternsFor(effect_id).empty() ||
         !EffectTeleportRingPatternsFor(effect_id).empty() ||
         !EffectCameraFanPatternsFor(effect_id).empty() ||
         !EffectFadingSpritePatternsFor(effect_id).empty() ||
         !EffectHeartRingPatternsFor(effect_id).empty() ||
         // ⚠ ONZE FAMILLES MANQUAIENT À CETTE LISTE, portées et interrogées par
         // personne. Une famille qu'on porte sans l'ajouter ici laisse ses
         // compétences passer pour MUETTES : `DrawableProceduralEffectOfSkill`
         // cherche le premier effet que nous savons dessiner, et il ne voit que
         // ce que cette liste nomme. Le test des effets de compétence est
         // désormais la garde — il exige de CHAQUE entrée de la table qu'elle
         // ait un fichier ou une réponse ici.
         !EffectCloudRingPatternsFor(effect_id).empty() ||
         !EffectJobAuraPatternsFor(effect_id).empty() ||
         !EffectRibbonWavePatternsFor(effect_id).empty() ||
         !EffectRingQuartetPatternsFor(effect_id).empty() ||
         !EffectRingStarburstPatternsFor(effect_id).empty() ||
         !EffectScreenBurstPatternsFor(effect_id).empty() ||
         !EffectScreenPatternsFor(effect_id).empty() ||
         // 🔴 LES DEUX FAMILLES PORTÉES À LA MAIN SONT ICI AUSSI, et c'est la
         // seule liste qui décide si un effet est JOUÉ. La gerbe de reflets n'y
         // était pas : elle ne passait que parce que son identifiant traînait
         // encore dans la table des nœuds d'écran, et le jour où il en est sorti
         // l'effet a cessé de paraître. Une famille portée sans être admise ici
         // est une famille MUETTE.
         !EffectLensFlarePatternsFor(effect_id).empty() ||
         !EffectHitCrossPatternsFor(effect_id).empty() ||
         !EffectShieldBoomerangPatternsFor(effect_id).empty() ||
         !EffectSpriteFanPatternsFor(effect_id).empty() ||
         !EffectTurningRingPatternsFor(effect_id).empty() ||
         !EffectTwinDiscPatternsFor(effect_id).empty() ||
         !EffectFadingTwinDiscPatternsFor(effect_id).empty() ||
         !EffectExpandingRingPatternsFor(effect_id).empty() ||
         !EffectRainstormCylinderPatternsFor(effect_id).empty() ||
         !EffectThunderScatterPatternsFor(effect_id).empty() ||
         !EffectThunderFreezePatternsFor(effect_id).empty() ||
         !EffectTetraVortexPatternsFor(effect_id).empty() ||
         !EffectRadialPetalPatternsFor(effect_id).empty() ||
         !EffectArcingLightPatternsFor(effect_id).empty() ||
         !EffectAimedSpokePatternsFor(effect_id).empty() ||
         !EffectShockwaveRayPatternsFor(effect_id).empty() ||
         !EffectTaperedSheetPatternsFor(effect_id).empty() ||
         !EffectBreakRingPatternsFor(effect_id).empty() ||
         !EffectWhiteBurstPatternsFor(effect_id).empty() ||
         !EffectTriPlanePatternsFor(effect_id).empty() ||
         !EffectMagicTargetPatternsFor(effect_id).empty() ||
         !EffectSpinningBoxPatternsFor(effect_id).empty() ||
         // ⚠ DEUX FAMILLES QUI NE PEIGNENT RIEN ELLES-MÊMES : elles se montrent
         // par l'effet qu'elles font naître. Voir `effect_spawn_request.h`.
         !EffectSpiralEmitterPatternsFor(effect_id).empty() ||
         !EffectWalkingSowerPatternsFor(effect_id).empty() ||
         !EffectFloorSpinnerPatternsFor(effect_id).empty() ||
         !EffectBobbingNotePatternsFor(effect_id).empty() ||
         !EffectColoredVeilPatternsFor(effect_id).empty();
}

const ProceduralSkillEffect* DrawableProceduralEffectOfSkill(std::uint16_t skill_id) {
  // LE PREMIER, dans l'ordre du client : c'est celui qu'il joue, et les autres
  // mènent au même acteur. En semer deux doublerait la troupe.
  for (const ProceduralSkillEffect& couple : ProceduralEffectsOfSkill(skill_id)) {
    if (KnowsProceduralEffect(couple.effect_id)) {
      return &couple;
    }
  }
  return nullptr;
}

}  // namespace ro::render
