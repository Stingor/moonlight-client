#include "effects/effect_node_view.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <utility>

#include "ro/formats/act.h"
#include "ro/render/effect_anchored_skirt_seed.h"
#include "ro/render/effect_arc_ribbon.h"
#include "ro/render/effect_arc_ribbon_seed.h"
#include "ro/render/effect_bobbing_note.h"
#include "ro/render/effect_breathing_quad_seed.h"
#include "ro/render/effect_camera_fan_seed.h"
#include "ro/render/effect_colored_veil.h"
#include "ro/render/effect_cone_wedge_seed.h"
#include "ro/render/effect_dark_veil_seed.h"
#include "ro/render/effect_drifting_mote_seed.h"
#include "ro/render/effect_fading_sprite_seed.h"
#include "ro/render/effect_falling_seed.h"
#include "ro/render/effect_floor_spinner.h"
#include "ro/render/effect_flying_shard_seed.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_frame_and_bar_seed.h"
#include "ro/render/effect_gliding_slash_seed.h"
#include "ro/render/effect_glow_sprite_seed.h"
#include "ro/render/effect_ground_box_seed.h"
#include "ro/render/effect_ground_curtain_seed.h"
#include "ro/render/effect_ground_ribbon_seed.h"
#include "ro/render/effect_heart_ring_seed.h"
#include "ro/render/effect_homing_spark_seed.h"
#include "ro/render/effect_launch.h"
#include "ro/render/effect_layered_ring_seed.h"
#include "ro/render/effect_lens_flare_seed.h"
#include "ro/render/effect_life_table.h"
#include "ro/render/effect_lightning_fan_seed.h"
#include "ro/render/effect_marching_sower.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_node_sprite_seed.h"
#include "ro/render/effect_palette_billboard_seed.h"
#include "ro/render/effect_particle_figures.h"
#include "ro/render/effect_particle_ring_seed.h"
#include "ro/render/effect_pentagon_trail_seed.h"
#include "ro/render/effect_pulsing_rings_seed.h"
#include "ro/render/effect_quad_burst_seed.h"
#include "ro/render/effect_ribbon_wave_seed.h"
#include "ro/render/effect_rising_spark_seed.h"
#include "ro/render/effect_scatter_seed.h"
#include "ro/render/effect_screen_burst_seed.h"
#include "ro/render/effect_screen_seed.h"
#include "ro/render/effect_scrolling_band_seed.h"
#include "ro/render/effect_seeded_ring.h"
#include "ro/render/effect_sheathed_bolt_seed.h"
#include "ro/render/effect_snaking_arc_seed.h"
#include "ro/render/effect_spark_tube_seed.h"
#include "ro/render/effect_spiral_fan.h"
#include "ro/render/effect_spiral_fan_seed.h"
#include "ro/render/effect_spiral_stack_seed.h"
#include "ro/render/effect_sprite_fan_seed.h"
#include "ro/render/effect_swarm_seed.h"
#include "ro/render/effect_tinted_quad.h"
#include "ro/render/effect_tinted_quad_seed.h"
#include "ro/render/effect_track_figures.h"
#include "ro/render/effect_trail_history.h"
#include "ro/render/effect_tri_plane_seed.h"
#include "ro/render/effect_triple_wall_seed.h"
#include "ro/render/effect_turning_ring_seed.h"
#include "ro/render/effect_typed_ribbon_seed.h"
#include "ro/render/effect_warp_ring_seed.h"
#include "ro/render/effect_white_burst_seed.h"

#include "effects/effect_footprint_sow.h"
#include "effects/effect_rising_quad_sow.h"
#include "effects/effect_spinning_box_sow.h"

namespace moonclient {
namespace {

constexpr std::string_view kBlanks = " \t";
constexpr std::string_view kNodePrefix = "/noeud";

}  // namespace

std::optional<std::string> EffectNodeCommand(std::string_view line) {
  if (!line.starts_with(kNodePrefix)) {
    return std::nullopt;
  }
  std::string_view rest = line.substr(kNodePrefix.size());
  // Une blanche est EXIGÉE après la commande, comme pour « /effet » : sans
  // elle, tout mot plus long qui commencerait pareil serait pris pour elle.
  if (!rest.empty() && kBlanks.find(rest.front()) == std::string_view::npos) {
    return std::nullopt;
  }
  const std::size_t first = rest.find_first_not_of(kBlanks);
  if (first == std::string_view::npos) {
    return std::string();
  }
  const std::size_t last = rest.find_last_not_of(kBlanks);
  return std::string(rest.substr(first, last - first + 1));
}

const ro::render::LoadedActorSprite* EffectNodesInPlay::LoadedSprite(
    const std::string& sprite_path) {
  if (const auto found = sprites_.find(sprite_path); found != sprites_.end()) {
    return &found->second;
  }
  const std::array<ro::render::ActorPieceRequest, 1> request = {
      ro::render::ActorPieceRequest{ro::render::ActorPart::kBody, sprite_path}};
  auto loaded = ro::render::LoadActorSprites(*vfs_, request);
  if (!loaded || loaded->empty()) {
    return nullptr;
  }
  return &sprites_.emplace(sprite_path, std::move(loaded->front())).first->second;
}

const ro::formats::Image& EffectNodesInPlay::TextureFor(const std::string& path) {
  if (vfs_->overlay_revision() != textures_revision_) {
    textures_.clear();
    textures_revision_ = vfs_->overlay_revision();
  }
  if (!textures_.contains(path)) {
    // ⚠ `Read` ET NON `ReadRaw`, et la distinction n'est pas cosmétique : ces
    // chemins viennent de NOS tables engendrées, donc d'une source UTF-8, là où
    // `ReadRaw` attend des octets CP949 déjà tirés d'un fichier. Les deux se
    // comportent pareil sur un nom ASCII — c'est pourquoi l'erreur est restée
    // invisible — et `ReadRaw` ne trouve JAMAIS un nom coréen. Nos tables
    // en portent quatre-vingt-dix-huit.
    auto brut = vfs_->Read(path);
    ro::formats::Image image;
    if (brut) {
      // Le NOM entre dans le décodage : c'est son extension qui dit le format,
      // et le contenu seul ne le dirait pas.
      if (auto decodee = ro::formats::DecodeImage(path, *brut); decodee) {
        image = std::move(*decodee);
        // LE MAGENTA EST LA TRANSPARENCE DU JEU, pas celle du format, et à la
        // précision de sa texture de seize bits. Un BMP n'a aucun canal alpha,
        // si bien que son fond arrive OPAQUE ; le catalogue des effets en
        // fichier fait la même passe, le noir en plus.
        //
        // ⚠ ICI LE NOIR RESTE PEINT : les textures d'éclair ont un fond NOIR et
        // pas un seul pixel magenta. C'est l'ADDITION qui les découpe, au
        // moment de peindre, et non une couleur clé.
        ro::formats::ApplyEffectTextureColorKey(image);
        ro::formats::QuantizeToEffectTextureDepth(image);
      }
    }
    // L'ÉCHEC SE RETIENT comme le succès : un chemin qui n'existe pas
    // n'existera pas davantage au coup suivant, et le relire à chaque impact
    // coûterait une lecture d'archive par coup porté.
    textures_.emplace(path, std::move(image));
  }
  return textures_.find(path)->second;
}
bool EffectNodesInPlay::PlayProcedural(std::uint32_t effect_id, glm::vec3 ground_position,
                                       const std::optional<glm::vec3>& target_position,
                                       float owner_facing_degrees, std::uint32_t attached_actor_id,
                                       std::int32_t asked_param, std::int32_t life_floor_ticks,
                                       std::int32_t life_exact_ticks, float instance_scale,
                                       std::uint32_t designated_actor_id, float instance_second) {
  const std::int32_t param = InstanceParamOf(asked_param);
  const std::int32_t bearer_death_tick = std::exchange(next_bearer_death_tick_, 0);
  // ⚠ SUR TOUTE LA FONCTION, et non sur le seul semis des patrons : les
  // familles suivantes sèment chacune les leurs, et un effet d'incantation
  // comme l'anneau en couches naît DANS l'une d'elles. Un plancher posé trop
  // tôt laisserait précisément le cercle du cast à sa bouffée.
  pool_.LiveAtLeast(life_floor_ticks);
  pool_.LiveExactly(life_exact_ticks);
  pool_.DieWithBearerAt(bearer_death_tick);
  // TOUTES, et non l'une puis l'autre en cas d'échec : un effet peut poser une
  // nuée ET un anneau, et s'arrêter au premier semis réussi le tronquerait.
  // L'EFFET ENTRE EN SCÈNE AVEC SON HORLOGE, et ne sème d'abord que ce qui est
  // dû au premier tic. Le reste viendra, tic après tic.
  const std::span<const ro::render::EffectNodePattern> patterns =
      ro::render::EffectNodePatternsFor(effect_id);
  // Ce qui naît ici s'accroche au porteur — voir `EffectNodePool::AnchorNextTo`.
  // ⚠ AVEC SA PLACE, parce que ce qui est gardé est un ÉCART : le point de
  // l'effet EST celui du porteur à cet instant.
  //
  // 🔴 ET ELLE COURT SUR TOUTE LA FONCTION, non sur ce seul semis. Elle était
  // reposée à zéro juste après lui, si bien que les QUARANTE AUTRES familles
  // naissaient sans porteur — dont la colonne de Safety Wall et ses anneaux. Un
  // nœud sans ancre ne meurt que de sa vie, dix mille tics, et restait sur une
  // case vide longtemps après que le serveur avait levé le mur.
  //
  // ⚠ L'HORLOGE, ELLE, LES ANCRAIT TOUTES (voir `StepClocks`) : le même nœud
  // s'accrochait ou non selon qu'il naissait au tic zéro ou au tic un. C'est
  // cette asymétrie qui a fait disparaître certaines figures d'un mur levé et
  // pas d'autres.
  const ro::render::EffectNodePool::AnchoredSowing anchor{pool_, attached_actor_id, ground_position,
                                                          effect_id, sowing_hat_};
  // LA NAISSANCE D'UNE INSTANCE REMET À ZÉRO LE COMPTEUR QU'ELLE PARTAGE avec
  // ses sœurs — voir `EffectParamCounterIsShared`.
  if (ro::render::EffectParamCounterIsShared(effect_id)) {
    shared_param_sowings_ = 0;
  }
  const bool seeds_played = PlaySeedsDueAt(patterns, ground_position, 0, owner_facing_degrees,
                                           target_position, param, instance_scale, instance_second);
  // JUSQU'À QUEL TIC CET EFFET SÈME ENCORE : c'est ce qui décide si une
  // horloge se pose, et jusqu'où elle bat. Le calcul vit avec l'horloge —
  // voir `effect_node_sow_clocked.cpp`.
  // ⚠ ET UN PORTEUR QUI MEURT AVANT LUI ARRÊTE SES SEMIS À SA MORT.
  const std::int32_t last_sow_tick =
      LastSowTick(effect_id, patterns, ground_position, target_position);
  const std::int32_t last_tick =
      bearer_death_tick > 0 ? std::min(last_sow_tick, bearer_death_tick - 1) : last_sow_tick;

  if (last_tick > 0) {
    clocks_.push_back({effect_id, ground_position, 0, target_position, last_tick,
                       owner_facing_degrees, attached_actor_id, param, life_floor_ticks,
                       instance_scale, ground_position, instance_second});
    clocks_.back().bearer_death_tick = bearer_death_tick;
    clocks_.back().from_hat = sowing_hat_;
  }
  const bool rings_played = PlayRings(effect_id, ground_position);
  const bool rising_particles_played = PlayRisingParticles(effect_id, ground_position, 0);
  const bool arc_ribbons_played =
      PlayArcRibbons(effect_id, ground_position, instance_scale, attached_actor_id);
  const bool tinted_quads_played = PlayTintedQuads(effect_id, ground_position);
  const bool scatter_fields_played = PlayScatterFields(effect_id, ground_position);
  const bool falling_items_played = PlayFallingItems(effect_id, ground_position);
  const bool sown_figures_played =
      PlaySownFigures(effect_id, ground_position, 0, instance_scale, instance_second);
  const bool spiral_fans_played = PlaySpiralFans(effect_id, ground_position);
  const bool layered_rings_played = PlayLayeredRings(effect_id, ground_position);
  const bool particle_rings_played = PlayParticleRings(effect_id, ground_position);
  const bool seeded_rings_played = PlaySeededRings(effect_id, ground_position, 0);
  const bool rising_sparks_played = PlayRisingSparks(effect_id, ground_position, 0);
  const bool scrolling_bands_played = PlayScrollingBands(effect_id, ground_position);
  const bool dark_veils_played = PlayDarkVeils(effect_id, ground_position);
  const bool typed_ribbons_played = PlayTypedRibbons(effect_id, ground_position);
  const bool pentagon_trails_played = PlayPentagonTrails(effect_id, ground_position);
  const bool palette_billboards_played = PlayPaletteBillboards(effect_id, ground_position, 0);
  const bool spiral_stacks_played = PlaySpiralStacks(effect_id, ground_position);
  const bool breathing_quads_played = PlayBreathingQuads(effect_id, ground_position);
  const bool ground_discs_played = PlayGroundDiscs(effect_id, ground_position);
  const bool cone_wedges_played = PlayConeWedges(effect_id, ground_position);
  const bool ground_ribbons_played = PlayGroundRibbons(effect_id, ground_position, target_position);
  const bool pulsing_rings_played = PlayPulsingRings(effect_id, ground_position);
  const bool warp_rings_played = PlayWarpRings(effect_id, ground_position);
  const bool screen_nodes_played =
      PlayScreenNodes(effect_id, ground_position, 0, attached_actor_id);
  const bool lens_flares_played = PlayLensFlares(effect_id, ground_position, attached_actor_id);
  const bool hit_crosses_played = PlayHitCrosses(effect_id, ground_position, attached_actor_id);
  const bool split_beams_played = PlaySplitBeams(effect_id, ground_position, attached_actor_id);
  const bool spiral_columns_played = PlaySpiralColumns(effect_id, ground_position, target_position);
  const bool concentric_squares_played = PlayConcentricSquares(effect_id, ground_position, param);
  const bool triple_walls_played = PlayTripleWalls(effect_id, ground_position, 0);
  const bool grand_crosses_played = PlayGrandCrosses(effect_id, ground_position);
  const bool sphere_shells_played = PlaySphereShells(effect_id, ground_position);
  const bool swarms_played = PlaySwarms(effect_id, ground_position, 0);
  const bool ground_curtains_played = PlayGroundCurtains(effect_id, ground_position);
  const bool spark_tubes_played = PlaySparkTubes(effect_id, ground_position, 0, target_position);
  const bool gliding_slashes_played =
      PlayGlidingSlashes(effect_id, ground_position, target_position);
  const bool quad_bursts_played = PlayQuadBursts(effect_id, ground_position);
  const bool flying_shards_played = PlayFlyingShards(effect_id, ground_position);
  const bool drifting_motes_played = PlayDriftingMotes(effect_id, ground_position, 0);
  const bool radial_petals_played = PlayRadialPetals(effect_id, ground_position, 0);
  const bool sheathed_bolts_played =
      PlaySheathedBolts(effect_id, ground_position, 0, target_position);
  const bool frame_and_bars_played = PlayFrameAndBars(effect_id, ground_position);
  const bool ground_boxes_played = PlayGroundBoxes(effect_id, ground_position);
  const bool ground_gusts_played = PlayGroundGusts(effect_id, ground_position);
  const bool homing_gusts_played =
      PlayHomingGusts(effect_id, ground_position, target_position, param);
  const bool rolling_gusts_played = PlayRollingGusts(effect_id, ground_position, target_position);
  const bool swirl_trails_played = PlaySwirlTrails(effect_id, ground_position, 0);
  const bool drifting_icons_played = PlayDriftingIcons(effect_id, ground_position);
  const bool tethered_beams_played =
      PlayTetheredBeams(effect_id, ground_position, target_position, designated_actor_id);
  const bool thrown_items_played =
      PlayThrownItems(effect_id, ground_position, 0, target_position, param);
  const bool pie_wedges_played = PlayPieWedges(effect_id, ground_position);
  const bool spike_beams_played = PlaySpikeBeams(effect_id, ground_position);
  const bool aura_columns_played = PlayAuraColumns(effect_id, ground_position);
  const bool rain_particles_played = PlayRainParticles(effect_id, ground_position, 0);
  const bool double_ribbons_played = PlayDoubleRibbons(effect_id, ground_position);
  const bool spiral_trails_played = PlaySpiralTrails(effect_id, ground_position);
  const bool rainbow_trails_played = PlayRainbowTrails(effect_id, ground_position);
  const bool fading_streaks_played = PlayFadingStreaks(effect_id, ground_position, target_position);
  const bool growing_sprites_played = PlayGrowingSprites(effect_id, ground_position);
  const bool grid_planes_played = PlayGridPlanes(effect_id, ground_position);
  const bool cycling_skirts_played = PlayCyclingSkirts(effect_id, ground_position);
  const bool shield_boomerangs_played =
      PlayShieldBoomerangs(effect_id, ground_position, target_position, param);
  const bool spin_ribbons_played = PlaySpinRibbons(effect_id, ground_position);
  const bool snaking_arcs_played = PlaySnakingArcs(effect_id, ground_position, 0);
  const bool spinning_rounds_played = PlaySpinningRounds(effect_id, ground_position);
  const bool wandering_motes_played = PlayWanderingMotes(effect_id, ground_position);
  const bool job_auras_played = PlayJobAuras(effect_id, ground_position);
  const bool cloud_rings_played = PlayCloudRings(effect_id, ground_position, 0);
  const bool twin_discs_played = PlayTwinDiscs(effect_id, ground_position);
  const bool fading_twin_discs_played = PlayFadingTwinDiscs(effect_id, ground_position);
  const bool expanding_rings_played = PlayExpandingRings(effect_id, ground_position, 0);
  const bool rainstorm_cylinders_played = PlayRainstormCylinders(effect_id, ground_position);
  const bool thunder_scatters_played =
      PlayThunderScatters(effect_id, ground_position, target_position);
  const bool thunder_freezes_played = PlayThunderFreezes(effect_id, ground_position);
  const bool arcing_lights_played =
      PlayArcingLights(effect_id, ground_position, designated_actor_id);
  const bool spiral_emitters_played =
      PlaySpiralEmitters(effect_id, ground_position, designated_actor_id);
  const bool walking_sowers_played = PlayWalkingSowers(effect_id, ground_position, target_position);
  const bool aimed_spokes_played = PlayAimedSpokes(effect_id, ground_position, target_position);
  const bool shockwave_rays_played = PlayShockwaveRays(effect_id, ground_position);
  const bool tapered_sheets_played = PlayTaperedSheets(effect_id, ground_position);
  const bool break_rings_played = PlayBreakRings(effect_id, ground_position, 0);
  const bool tetra_vortices_played =
      PlayTetraVortices(effect_id, ground_position, target_position, 0);
  const bool ring_quartets_played = PlayRingQuartets(effect_id, ground_position);
  const bool ribbon_waves_played = PlayRibbonWaves(effect_id, ground_position);
  const bool turning_rings_played = PlayTurningRings(effect_id, ground_position);
  const bool screen_bursts_played =
      PlayScreenBursts(effect_id, ground_position, 0, attached_actor_id);
  const bool sprite_fans_played = PlaySpriteFans(effect_id, ground_position, 0);
  const bool anchored_skirts_played = PlayAnchoredSkirts(effect_id, ground_position, 0);
  const bool lightning_fans_played = PlayLightningFans(effect_id, ground_position, 0);
  const bool electric_arcs_played =
      PlayElectricArcs(effect_id, ground_position, owner_facing_degrees);
  const bool homing_sparks_played = PlayHomingSparks(effect_id, ground_position);
  const bool heal_sparkles_played = PlayHealSparkles(effect_id, ground_position);
  const bool tri_planes_played = PlayTriPlanes(effect_id, ground_position, 0);
  const bool teleport_rings_played = PlayTeleportRings(effect_id, ground_position);
  // ⚠ AUCUN DE SES DEUX NŒUDS NE NAÎT ICI : les deux attendent leur tic, et
  // c'est l'horloge qui les sème. L'appel reste pour que la fenêtre réponde
  // qu'elle connaît l'effet.
  const bool camera_fans_played = PlayCameraFans(effect_id, ground_position, 0);
  const bool fading_sprites_played = PlayFadingSprites(effect_id, ground_position, 0);
  const bool heart_rings_played = PlayHeartRings(effect_id, ground_position);
  const bool magic_targets_played = PlayMagicTargets(effect_id, ground_position);
  const bool spinning_boxes_played = SowSpinningBoxes(
      pool_, effect_id, ground_position, static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U,
      [this](const std::string& path) { return !TextureFor(path).empty(); });
  const bool rising_quads_played =
      SowRisingQuads(pool_, effect_id, ground_position, ro::render::EffectNodeLifeFor(effect_id),
                     static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U,
                     [this](const std::string& path) { return !TextureFor(path).empty(); });
  // L'EMPREINTE reçoit son angle par la face du porteur : c'est l'angle de la
  // marche que celui qui la sème a calculé.
  const bool footprints_played =
      SowFootprints(pool_, effect_id, ground_position, owner_facing_degrees,
                    [this](const std::string& path) { return !TextureFor(path).empty(); });
  // Celui qui pose le plancher le repose ; l'ancre, elle, se repose d'elle-même
  // à la fin de sa portée — voir `AnchoredSowing`.
  pool_.LiveAtLeast(0);
  pool_.LiveExactly(0);
  pool_.DieWithBearerAt(0);
  // 🔴 UNE HORLOGE POSÉE EST UN SEMIS, même si le tic zéro n'a rien mis en
  // scène. Chaque figure d'un patron a son tic d'entrée, et celles des flèches
  // de glace de Cold Bolt commencent au DOUZIÈME : le tic zéro est vide, et
  // rendre « rien semé » faisait deux dégâts d'un coup — un avertissement de
  // panne sur un effet qui se joue très bien, et le SILENCE, puisque l'appelant
  // n'annonce le bruit que d'un effet joué.
  const bool clock_placed = last_tick > 0;
  return clock_placed || magic_targets_played || teleport_rings_played || camera_fans_played ||
         fading_sprites_played || heart_rings_played || seeds_played || rings_played ||
         rising_particles_played || arc_ribbons_played || tinted_quads_played ||
         scatter_fields_played || falling_items_played || sown_figures_played ||
         spiral_fans_played || layered_rings_played || particle_rings_played ||
         seeded_rings_played || rising_sparks_played || scrolling_bands_played ||
         dark_veils_played || typed_ribbons_played || pentagon_trails_played ||
         palette_billboards_played || spiral_stacks_played || breathing_quads_played ||
         cone_wedges_played || ground_ribbons_played || pulsing_rings_played || warp_rings_played ||
         screen_nodes_played || lens_flares_played || hit_crosses_played || split_beams_played ||
         spiral_columns_played || concentric_squares_played || triple_walls_played ||
         sphere_shells_played || swarms_played || ground_curtains_played || spark_tubes_played ||
         gliding_slashes_played || quad_bursts_played || flying_shards_played ||
         drifting_motes_played || sheathed_bolts_played || frame_and_bars_played ||
         ground_boxes_played || ground_gusts_played || homing_gusts_played ||
         rolling_gusts_played || swirl_trails_played || drifting_icons_played ||
         tethered_beams_played || shield_boomerangs_played || spin_ribbons_played ||
         snaking_arcs_played || spinning_rounds_played || wandering_motes_played ||
         anchored_skirts_played || lightning_fans_played || electric_arcs_played ||
         homing_sparks_played || heal_sparkles_played || tri_planes_played || job_auras_played ||
         screen_bursts_played || sprite_fans_played || cloud_rings_played || twin_discs_played ||
         ring_quartets_played || ribbon_waves_played || turning_rings_played ||
         grand_crosses_played || ground_discs_played || thrown_items_played || pie_wedges_played ||
         growing_sprites_played || grid_planes_played || cycling_skirts_played ||
         spike_beams_played || aura_columns_played || rain_particles_played ||
         double_ribbons_played || spiral_trails_played || rainbow_trails_played ||
         fading_streaks_played || fading_twin_discs_played || expanding_rings_played ||
         rainstorm_cylinders_played || thunder_scatters_played || thunder_freezes_played ||
         tetra_vortices_played || radial_petals_played || arcing_lights_played ||
         aimed_spokes_played || shockwave_rays_played || tapered_sheets_played ||
         break_rings_played || spiral_emitters_played || walking_sowers_played ||
         spinning_boxes_played || rising_quads_played || footprints_played;
}

void EffectNodesInPlay::NoticeDrawGate(EffectDrawGate gate) {
  draw_gate_ = std::move(gate);
  // LA TROUPE EN GARDE UNE COPIE : elle juge un nœud par son porteur et, pour un
  // trait de Devotion, par l'acteur qu'il vise.
  pool_.SetDrawGate([gate = draw_gate_](const ro::render::PooledEffectNode& node) {
    // ⚠ LE CLIENT JUGE LA CIBLE D'UN TRAIT DE DEVOTION AVANT SON PORTEUR.
    if (node.sown_by_effect_id == kDevotionLinkEffect && !gate.DrawsTarget(node.target_actor_id)) {
      return false;
    }
    return gate.DrawsEffect(node.sown_by_effect_id, node.anchor_id, node.from_hat);
  });
}

bool EffectNodesInPlay::KnowsProcedural(std::uint32_t effect_id) {
  // LA LISTE DES VINGT-TROIS FAMILLES VIT DANS `render`, et une seule fois : elle
  // sert aussi à choisir l'effet qu'une compétence montre, et deux copies
  // divergeraient à la vingt-quatrième.
  return ro::render::KnowsProceduralEffect(effect_id);
}

std::int32_t EffectNodesInPlay::ProceduralLifeTicks(std::uint32_t effect_id) {
  std::int32_t life_ticks = 0;
  for (const ro::render::EffectNodePattern& pattern :
       ro::render::EffectNodePatternsFor(effect_id)) {
    // ⚠ LA BORNE HAUTE, ET NON LA BASSE : un nœud peut TIRER sa vie, et
    // l'effet doit durer assez pour le plus tenace d'entre eux. Prendre la
    // basse couperait la queue de la grille de pointes.
    life_ticks = std::max(life_ticks, std::max(pattern.life_ticks, pattern.life_ticks_high));
  }
  if (!ro::render::EffectRingNodePatternsFor(effect_id).empty()) {
    life_ticks =
        std::max(life_ticks,
                 ro::render::EffectRingInstanceLifeTicks(ro::render::EffectNodeLifeFor(effect_id)));
  }
  if (!ro::render::EffectRisingParticlePatternsFor(effect_id).empty()) {
    life_ticks = std::max(life_ticks, ro::render::kEffectRisingParticleLifeTicks);
  }
  // ⚠ LA BOUFFÉE BLANCHE PORTE SA VIE DANS SON PATRON, là où l'aîné du même
  // sous-type la tient d'une constante de famille : ses quatre styles ne
  // s'éteignent pas au même tic, parce qu'une marque nulle monte moins haut et
  // retombe donc plus tôt. Une seule constante en aurait coupé trois.
  for (const ro::render::EffectWhiteBurstPattern& bouffee :
       ro::render::EffectWhiteBurstPatternsFor(effect_id)) {
    life_ticks = std::max(life_ticks, bouffee.life_ticks);
  }
  if (!ro::render::EffectArcRibbonNodePatternsFor(effect_id).empty() ||
      !ro::render::EffectTintedQuadNodePatternsFor(effect_id).empty() ||
      !ro::render::EffectScatterNodePatternsFor(effect_id).empty() ||
      !ro::render::EffectFallingNodePatternsFor(effect_id).empty() ||
      !ro::render::EffectSpiralFanNodePatternsFor(effect_id).empty() ||
      !ro::render::EffectParticleRingNodePatternsFor(effect_id).empty() ||
      !ro::render::EffectSeededRingPatternsFor(effect_id).empty()) {
    life_ticks = std::max(life_ticks, ro::render::kEffectNodeDefaultLifeTicks);
  }
  // La vie vient du PATRON : les deux bâtisseurs de cette famille ne la posent
  // pas de la même façon, et l'un ne la pose pas du tout.
  for (const ro::render::EffectLayeredRingNodePattern& ring :
       ro::render::EffectLayeredRingNodePatternsFor(effect_id)) {
    life_ticks = std::max(life_ticks, ring.life_ticks);
  }
  // L'ONDE DE RUBAN vit ce que le constructeur du client lui laisse : ni son
  // bâtisseur ni son mouvement ne posent de vie. Son cycle de respiration fait
  // une centaine de tics, et une vie plus courte l'éteindrait avant la fin du
  // premier.
  if (!ro::render::EffectRibbonWavePatternsFor(effect_id).empty() ||
      !ro::render::EffectTurningRingPatternsFor(effect_id).empty()) {
    life_ticks = std::max(life_ticks, ro::render::kEffectNodeDefaultLifeTicks);
  }
  // LES MONSTRES recopient la vie de leur instance, moins l'âge de leur semis ;
  // les carrés semés des sous-types 143, 156 et 172 la recopient telle quelle.
  if (!ro::render::EffectGlowSpriteNodePatternsFor(effect_id).empty()) {
    life_ticks = std::max(life_ticks, ro::render::EffectNodeLifeFor(effect_id));
  }
  for (const ro::render::EffectFloorSpinnerPattern& quad :
       ro::render::EffectFloorSpinnerPatternsFor(effect_id)) {
    life_ticks = std::max(life_ticks, quad.life_ticks);
  }
  for (const ro::render::EffectBobbingNotePattern& note :
       ro::render::EffectBobbingNotePatternsFor(effect_id)) {
    life_ticks = std::max(life_ticks, note.life_ticks);
  }
  for (const ro::render::EffectColoredVeilPattern& voile :
       ro::render::EffectColoredVeilPatternsFor(effect_id)) {
    life_ticks = std::max(life_ticks, voile.life_ticks);
  }
  // 🔴 ET LES ANNEAUX QUI PULSENT, QUI MANQUAIENT ICI. Leur vie vient de la
  // table des durées, comme leur semis la prend — `PlayPulsingRings`. Sans cette
  // ligne, un effet qui n'a QUE cette famille rendait ZÉRO, et un zéro n'est pas
  // une vie courte : c'est une réponse fausse que l'appelant prend au mot.
  // Safety Wall s'y resemait à CHAQUE TIC.
  if (!ro::render::EffectPulsingRingsPatternsFor(effect_id).empty()) {
    life_ticks = std::max(life_ticks, ro::render::EffectNodeLifeFor(effect_id));
  }
  // ⚠ ET LES ANNEAUX D'UN PORTAIL, pour la même raison : leur rayon ne s'arrête
  // jamais de tourner, et c'est la VIE DU NŒUD, elle seule, qui les éteint.
  if (!ro::render::EffectWarpRingPatternsFor(effect_id).empty()) {
    life_ticks = std::max(life_ticks, ro::render::EffectNodeLifeFor(effect_id));
  }
  // 🔴 ET LA DURÉE DE L'EFFET FERME LA MARCHE, parce qu'un ZÉRO N'EST PAS UNE
  // VIE COURTE : c'est une réponse fausse, et les trois appelants la
  // transforment en période d'UN TIC — un resemis soixante fois par seconde.
  //
  // Cette fonction parcourt une famille après l'autre, et celle qu'elle oublie
  // ne se signale pas. ONZE visuels d'unité au sol y tombaient, dont le mur de
  // Safety Wall ; les chapeaux et les cercles d'incantation lisent le même
  // nombre. La table des durées du client répond pour presque tout, et c'est
  // celle que le client compare lui-même.
  if (life_ticks <= 0 && ro::render::EffectNodeLifeIsKnown(effect_id)) {
    life_ticks = ro::render::EffectNodeLifeFor(effect_id);
  }
  return life_ticks;
}

}  // namespace moonclient
