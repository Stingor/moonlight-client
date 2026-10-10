// LES SEMIS, UNE MÉTHODE PAR FAMILLE.
//
// Sortis de `effect_node_view.cpp`, qui a dépassé le plafond de huit cents
// lignes en recevant la vingt-deuxième famille. La coupure est naturelle : d'un
// côté la fenêtre — ce qu'elle affiche, ce qu'elle compose, ses horloges —, de
// l'autre ces méthodes qui ne font qu'une chose chacune, semer une famille.
//
// ⚠ ELLES RESTENT MEMBRES DE `EffectNodesInPlay`, et c'est voulu : elles lisent
// sa troupe, son horloge et son cache de textures. Les rendre libres
// obligerait à leur passer trois références de plus, ou à ouvrir la classe.

#include <algorithm>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/log.h"
#include "ro/render/effect_aimed_spoke_seed.h"
#include "ro/render/effect_arc_ribbon_seed.h"
#include "ro/render/effect_aura_column_seed.h"
#include "ro/render/effect_breathing_quad_seed.h"
#include "ro/render/effect_cloud_ring_seed.h"
#include "ro/render/effect_concentric_square_seed.h"
#include "ro/render/effect_cone_wedge_seed.h"
#include "ro/render/effect_cycling_skirt_seed.h"
#include "ro/render/effect_dark_veil_seed.h"
#include "ro/render/effect_double_ribbon_seed.h"
#include "ro/render/effect_drifting_icon_seed.h"
#include "ro/render/effect_electric_arc_seed.h"
#include "ro/render/effect_fading_streak_seed.h"
#include "ro/render/effect_fading_twin_disc_seed.h"
#include "ro/render/effect_falling_seed.h"
#include "ro/render/effect_flying_shard_seed.h"
#include "ro/render/effect_frame_and_bar_seed.h"
#include "ro/render/effect_gliding_slash_seed.h"
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
#include "ro/render/effect_homing_gust_seed.h"
#include "ro/render/effect_homing_spark_seed.h"
#include "ro/render/effect_job_aura_seed.h"
#include "ro/render/effect_layered_ring_seed.h"
#include "ro/render/effect_life_table.h"
#include "ro/render/effect_magic_target_seed.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_particle_ring_seed.h"
#include "ro/render/effect_pentagon_trail_seed.h"
#include "ro/render/effect_pie_wedge_seed.h"
#include "ro/render/effect_pulsing_rings_seed.h"
#include "ro/render/effect_quad_burst_seed.h"
#include "ro/render/effect_rainbow_trail_seed.h"
#include "ro/render/effect_rainstorm_cylinder_seed.h"
#include "ro/render/effect_ribbon_wave_seed.h"
#include "ro/render/effect_ring_quartet_seed.h"
#include "ro/render/effect_ring_starburst_seed.h"
#include "ro/render/effect_rolling_gust_seed.h"
#include "ro/render/effect_scatter_seed.h"
#include "ro/render/effect_scrolling_band_seed.h"
#include "ro/render/effect_shield_boomerang_seed.h"
#include "ro/render/effect_shockwave_ray_seed.h"
#include "ro/render/effect_sphere_shell_seed.h"
#include "ro/render/effect_spike_beam_seed.h"
#include "ro/render/effect_spin_ribbon_seed.h"
#include "ro/render/effect_spinning_round_seed.h"
#include "ro/render/effect_spiral_column_seed.h"
#include "ro/render/effect_spiral_fan_seed.h"
#include "ro/render/effect_spiral_stack_seed.h"
#include "ro/render/effect_spiral_trail_seed.h"
#include "ro/render/effect_sprite_fan_seed.h"
#include "ro/render/effect_tapered_sheet_seed.h"
#include "ro/render/effect_teleport_rings_seed.h"
#include "ro/render/effect_tethered_beam_seed.h"
#include "ro/render/effect_thrown_item_seed.h"
#include "ro/render/effect_thunder_freeze_seed.h"
#include "ro/render/effect_thunder_scatter_seed.h"
#include "ro/render/effect_tinted_quad_seed.h"
#include "ro/render/effect_turning_ring_seed.h"
#include "ro/render/effect_twin_disc_seed.h"
#include "ro/render/effect_typed_ribbon_seed.h"
#include "ro/render/effect_wandering_motes_seed.h"
#include "ro/render/effect_warp_ring_seed.h"
#include "ro/render/effect_white_burst_seed.h"

#include "effects/effect_node_view.h"

namespace moonclient {

bool EffectNodesInPlay::PlayRings(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  for (const ro::render::EffectRingNodePattern& ring :
       ro::render::EffectRingNodePatternsFor(effect_id)) {
    // Un anneau dont la texture ne se lit pas n'a rien à peindre sur son ruban,
    // et le semer poserait quatre rubans blancs.
    if (TextureFor(std::string(ring.texture)).empty()) {
      continue;
    }
    // La traduction du patron en nœud vit dans `render`, où elle s'éprouve sur
    // la table entière : semis des pistes compris.
    pool_.Add(ro::render::SowEffectRingNode(
        ring, ground_position, pool_.InstanceLifeTicks(ro::render::EffectNodeLifeFor(effect_id))));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayRisingParticles(std::uint32_t effect_id, glm::vec3 ground_position,
                                            std::int32_t tick) {
  bool sown = false;
  // La graine vient de l'instant et de ce qui joue déjà : deux lancements du
  // même effet ne dispersent pas leurs particules de la même façon.
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  // L'AÎNÉ NE SÈME QU'À LA NAISSANCE.
  const std::span<const ro::render::EffectRisingParticlePattern> eldest =
      tick == 0 ? ro::render::EffectRisingParticlePatternsFor(effect_id)
                : std::span<const ro::render::EffectRisingParticlePattern>();
  for (const ro::render::EffectRisingParticlePattern& node_pattern : eldest) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    // ⚠ LES PIÈCES LANCÉES EN L'AIR SONT TRENTE NŒUDS : le site boucle sur son
    // bâtisseur, et chaque nœud tire ses propres angles.
    for (std::uint16_t exemplaire = 0; exemplaire < node_pattern.copies; ++exemplaire) {
      pool_.Add(ro::render::SowEffectRisingParticleNode(node_pattern, ground_position, generator));
    }
    sown = true;
  }
  // LE SECOND BÂTISSEUR DU MÊME SOUS-TYPE, et il se sème ici plutôt que dans
  // une méthode à lui : ses nœuds prennent la forme de la particule montante,
  // et un appelant qui aurait à choisir entre deux méthodes pour un seul
  // sous-type finirait par n'en appeler qu'une.
  for (const ro::render::EffectWhiteBurstPattern& node_pattern :
       ro::render::EffectWhiteBurstPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    // ⚠ PLUSIEURS À LA FOIS, et une seule fois : voir `sowings_at_birth`.
    for (std::int32_t sowing = 0; sowing < node_pattern.sowings_at_birth; ++sowing) {
      pool_.Add(ro::render::SowEffectWhiteBurstNode(node_pattern, ground_position, generator));
    }
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayArcRibbons(std::uint32_t effect_id, glm::vec3 ground_position,
                                       float instance_scale, std::uint32_t bearer_id) {
  bool sown = false;
  // LA HAUTEUR QUE LE CORPS DU PORTEUR DONNE, pour les styles qui la lisent.
  const ro::render::EffectScatterWorld& world = pool_.world();
  const float body_lift = ro::render::ArcRibbonLiftFromBody(
      bearer_id != 0 && world.body_first_layer_y ? world.body_first_layer_y(bearer_id)
                                                 : std::nullopt);
  // La graine vient de l'instant et de ce qui joue déjà : deux lancements du
  // même effet ne posent pas leurs arcs au même azimut.
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectArcRibbonNodePattern& node_pattern :
       ro::render::EffectArcRibbonNodePatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    // ⚠ LA VIE VIENT DE L'INSTANCE, et nous ne la portons pas pour ces
    // trente-six : le bâtisseur la recopie sans jamais la poser. Le défaut d'un
    // nœud est ce que le client emploie pour un effet qui n'en pose pas, et
    // l'inventer plus longue garderait l'arc à l'écran après sa compétence.
    pool_.Add(ro::render::SowEffectArcRibbonNode(node_pattern, ground_position,
                                                 ro::render::EffectNodeLifeFor(effect_id),
                                                 generator, instance_scale, body_lift));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayTintedQuads(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectTintedQuadNodePattern& node_pattern :
       ro::render::EffectTintedQuadNodePatternsFor(effect_id)) {
    // ⚠ ON SÈME D'ABORD, ON JUGE LA TEXTURE ENSUITE, et c'est l'inverse des
    // autres familles. Trois styles sur dix ne lisent pas la texture de leur
    // site d'appel : ils la TIRENT AU SORT dans une liste que le style porte,
    // si bien que le patron n'en nomme aucune. Juger `patron.texture` écarterait
    // ces trois-là sans même regarder ce qu'ils prennent.
    ro::render::PooledEffectNode node = ro::render::SowEffectTintedQuadNode(
        node_pattern, ground_position, ro::render::EffectNodeLifeFor(effect_id), generator);
    if (node.resource_paths.empty() || TextureFor(node.resource_paths.front()).empty()) {
      continue;
    }
    pool_.Add(std::move(node));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayScatterFields(std::uint32_t effect_id, glm::vec3 ground_position) {
  // ⚠ LA TROUPE N'A PAS ENCORE SON MONDE, et c'est un manque assumé. Le
  // mouvement du champ demande l'altitude du sol pour poser ses particules sur
  // le relief, et le nom de la carte pour élargir sa dispersion sur l'une
  // d'elles. `EffectNodePool::SetWorld` attend ces deux réponses ; personne ne
  // les lui donne ici, faute que cette fenêtre tienne la carte.
  //
  // Ce que cela coûte est BORNÉ et connu : un seul style sur quatorze se pose au
  // sol, et il gardera la hauteur de son nœud — visiblement faux sur un relief,
  // et préférable à une altitude inventée. Les treize autres ne s'en aperçoivent
  // pas.
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectScatterNodePattern& node_pattern :
       ro::render::EffectScatterNodePatternsFor(effect_id)) {
    const ro::render::EffectScatterStyleSeed* seed =
        ro::render::EffectScatterStyleSeedFor(node_pattern.style);
    // ⚠ LA PREMIÈRE TEXTURE DÉCIDE, comme pour un patron de nœud : un champ dont
    // l'image de tête ne se lit pas n'a rien à montrer. Les suivantes se
    // chargent quand même — le mouvement les fait défiler.
    if (seed == nullptr ||
        TextureFor(std::string(ro::render::EffectScatterStyleTextures(*seed).front())).empty()) {
      continue;
    }
    for (const std::string_view path : ro::render::EffectScatterStyleTextures(*seed)) {
      TextureFor(std::string(path));
    }
    // 🔴 AUTANT DE NŒUDS QUE LE SITE D'APPEL EN SÈME, et non un seul. Chacun
    // porte quatre particules et tire ses propres places : quarante à
    // quatre-vingts nœuds font la nuée, un seul ne fait rien qu'on remarque.
    // Voir `EffectScatterNodePattern::node_count`.
    for (std::uint16_t how_many = 0; how_many < node_pattern.node_count; ++how_many) {
      pool_.Add(ro::render::SowEffectScatterNode(
          node_pattern, ground_position, ro::render::EffectNodeLifeFor(effect_id), generator));
    }
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayFallingItems(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectFallingNodePattern& node_pattern :
       ro::render::EffectFallingNodePatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectFallingNode(
        node_pattern, ground_position, ro::render::EffectNodeLifeFor(effect_id), generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayGroundRibbons(std::uint32_t effect_id, glm::vec3 ground_position,
                                          const std::optional<glm::vec3>& target_position) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectGroundRibbonPattern& node_pattern :
       ro::render::EffectGroundRibbonPatternsFor(effect_id)) {
    const std::string image(ro::render::EffectGroundRibbonTextureFor(node_pattern));
    if (TextureFor(image).empty()) {
      continue;
    }
    // ⚠ LE CAP VIENT DU POINT VISÉ que l'instance reçoit de celui qui la pose.
    // Sans lui, le client divise par une longueur nulle et rend le cap zéro :
    // le ruban pointe droit devant.
    constexpr std::int16_t kNoTarget = 0;
    const std::int16_t cap =
        target_position.has_value()
            ? ro::render::EffectGroundRibbonTargetDegrees(ground_position, *target_position)
            : kNoTarget;
    pool_.Add(ro::render::SowEffectGroundRibbonNode(
        node_pattern, ground_position, cap, ro::render::EffectNodeLifeFor(effect_id), generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayConeWedges(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectConeWedgePattern& node_pattern :
       ro::render::EffectConeWedgePatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    // ⚠ UN NŒUD PAR TOUR DE BOUCLE, et c'est là tout l'éventail : n'en semer
    // qu'un donnerait un seul rayon au lieu de cinq à douze.
    for (std::uint8_t radius = 0; radius < node_pattern.node_count; ++radius) {
      pool_.Add(ro::render::SowEffectConeWedgeNode(node_pattern, radius, ground_position,
                                                   ro::render::EffectNodeLifeFor(effect_id),
                                                   generator));
    }
    sown = node_pattern.node_count > 0;
  }
  return sown;
}

bool EffectNodesInPlay::PlayBreathingQuads(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectBreathingQuadPattern& node_pattern :
       ro::render::EffectBreathingQuadPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectBreathingQuadNode(
        node_pattern, ground_position, ro::render::EffectNodeLifeFor(effect_id), generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayGroundDiscs(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  for (const ro::render::EffectGroundDiscPattern& node_pattern :
       ro::render::EffectGroundDiscPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    // LE RAYON DE L'INSTANCE RESTE À ZÉRO : aucun des six cas ne l'écrit,
    // et le semis retombe alors sur le rayon que le bâtisseur porte en dur.
    pool_.Add(ro::render::SowEffectGroundDiscNode(node_pattern, ground_position,
                                                  ro::render::EffectNodeLifeFor(effect_id)));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlaySpiralStacks(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectSpiralStackPattern& node_pattern :
       ro::render::EffectSpiralStackPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectSpiralStackNode(
        node_pattern, ground_position, ro::render::EffectNodeLifeFor(effect_id), generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayPentagonTrails(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  for (const ro::render::EffectPentagonTrailPattern& node_pattern :
       ro::render::EffectPentagonTrailPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectPentagonTrailNode(node_pattern, ground_position,
                                                     ro::render::EffectNodeLifeFor(effect_id)));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayTypedRibbons(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  for (const ro::render::EffectTypedRibbonPattern& node_pattern :
       ro::render::EffectTypedRibbonPatternsFor(effect_id)) {
    // ⚠ VINGT-SEPT IMAGES, ET IL EN FAUT AU MOINS UNE. Le ruban les prend à tour
    // de rôle ; exiger les vingt-sept refuserait le nœud entier pour une image
    // absente de l'archive, là où le client tourne sur ce qu'il a chargé.
    ro::render::PooledEffectNode node = ro::render::SowEffectTypedRibbonNode(
        node_pattern, ground_position, ro::render::EffectNodeLifeFor(effect_id));
    std::vector<std::string> present_images;
    for (const std::string& image : node.resource_paths) {
      if (!TextureFor(image).empty()) {
        present_images.push_back(image);
      }
    }
    if (present_images.empty()) {
      continue;
    }
    node.resource_paths = std::move(present_images);
    pool_.Add(std::move(node));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayDarkVeils(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  for (const ro::render::EffectDarkVeilPattern& node_pattern :
       ro::render::EffectDarkVeilPatternsFor(effect_id)) {
    // ⚠ LES DEUX IMAGES DOIVENT ÊTRE LÀ : le cœur sans son cadre montrerait un
    // carré aux bords nets là où le client fond les siens.
    bool all = true;
    for (const std::string_view image : node_pattern.textures) {
      if (TextureFor(std::string(image)).empty()) {
        all = false;
      }
    }
    if (!all) {
      continue;
    }
    pool_.Add(ro::render::SowEffectDarkVeilNode(node_pattern, ground_position,
                                                ro::render::EffectNodeLifeFor(effect_id)));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayScrollingBands(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  for (const ro::render::EffectScrollingBandPattern& node_pattern :
       ro::render::EffectScrollingBandPatternsFor(effect_id)) {
    // ⚠ LES TROIS IMAGES DU JEU DOIVENT ÊTRE LÀ, et pas seulement la
    // première : la bande les prend à tour de rôle, et une seule manquante
    // ferait clignoter un segment sur trois.
    bool all = true;
    for (const std::string_view image : node_pattern.textures) {
      if (TextureFor(std::string(image)).empty()) {
        all = false;
      }
    }
    if (!all) {
      continue;
    }
    pool_.Add(ro::render::SowEffectScrollingBandNode(node_pattern, ground_position,
                                                     ro::render::EffectNodeLifeFor(effect_id)));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayParticleRings(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  for (const ro::render::EffectParticleRingNodePattern& node_pattern :
       ro::render::EffectParticleRingNodePatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectParticleRingNode(node_pattern, ground_position));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayLayeredRings(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  for (const ro::render::EffectLayeredRingNodePattern& node_pattern :
       ro::render::EffectLayeredRingNodePatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectLayeredRingNode(node_pattern, ground_position));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayHeartRings(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  // La graine vient de l'instant et de ce qui joue déjà : deux cœurs joués à la
  // suite ne présentent pas leurs anneaux sous le même angle.
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectHeartRingPattern& node_pattern :
       ro::render::EffectHeartRingPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    // ⚠ UN NŒUD PAR POINT DU DESSIN, et les vingt font le cœur. N'en semer qu'un
    // montrerait deux anneaux perdus au milieu de l'écran.
    pool_.Add(ro::render::SowEffectHeartRingNode(
        node_pattern, ground_position, ro::render::EffectNodeLifeFor(effect_id), generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayTeleportRings(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  for (const ro::render::EffectTeleportRingPattern& node_pattern :
       ro::render::EffectTeleportRingPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    // ⚠ LE BÂTISSEUR NE S'EXÉCUTE QU'AU TIC ZÉRO : il teste le tic de l'instance
    // et ne fait rien s'il n'est pas nul. Un nœud par effet, donc, et non un par
    // tic — c'est ce qui le sépare des familles à horaire.
    pool_.Add(ro::render::SowEffectTeleportRingNode(node_pattern, ground_position,
                                                    ro::render::EffectNodeLifeFor(effect_id)));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlaySpiralFans(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectSpiralFanNodePattern& node_pattern :
       ro::render::EffectSpiralFanNodePatternsFor(effect_id)) {
    // ⚠ UN PATRON PEUT SE SEMER PLUSIEURS FOIS, a trente tics d intervalle : le
    // cas du client rappelle son batisseur en boucle avec une phase qui avance.
    // C est le COMPTEUR NEGATIF de la piste qui les echelonne, et le mouvement de
    // l eventail le respecte deja.
    for (std::uint16_t rang = 0; rang < node_pattern.sow_count; ++rang) {
      // ⚠ CHAQUE NOEUD TIRE SON ICONE POUR LUI SEUL, et c est tout l effet : dix
      // icones differentes qui paraissent l une apres l autre. Les tirer une fois
      // pour les dix montrerait dix fois le meme sort.
      const std::string_view tiree = node_pattern.draws_its_icon
                                         ? ro::render::EffectSpiralFanDrawnIcon(generator)
                                         : std::string_view{};
      const std::string path(tiree.empty() ? node_pattern.texture : tiree);
      if (path.empty() || TextureFor(path).empty()) {
        continue;
      }
      const std::int32_t delay = -node_pattern.sow_period_ticks * static_cast<std::int32_t>(rang);
      pool_.Add(ro::render::SowEffectSpiralFanNode(
          node_pattern, ground_position, ro::render::EffectNodeLifeFor(effect_id), delay, path));
      sown = true;
    }
  }
  return sown;
}

bool EffectNodesInPlay::PlayPulsingRings(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  for (const ro::render::EffectPulsingRingsPattern& node_pattern :
       ro::render::EffectPulsingRingsPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    // ⚠ LE CLIENT REPOSE CE NŒUD TOUS LES CENT TICS pour quatre bâtisseurs sur
    // cinq — voir `rebuild_period_ticks`. Nous n'en semons qu'un : les pistes
    // RENAISSENT d'elles-mêmes tant que leur âge global le permet, si bien que
    // la pulsation tient sans la repose. Le champ reste au patron pour le jour
    // où la boucle de mise à jour d'effet arrivera.
    pool_.Add(ro::render::SowEffectPulsingRingsNode(node_pattern, ground_position,
                                                    ro::render::EffectNodeLifeFor(effect_id)));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayWarpRings(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  for (const ro::render::EffectWarpRingPattern& node_pattern :
       ro::render::EffectWarpRingPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    // ⚠ LES DEUX ÉTAGES SE SÈMENT ENSEMBLE, et non à leur tic. Le client pose le
    // premier au tic un et le second au tic deux ; un tic d'écart sur une figure
    // dont le rayon met deux cents tics à se refermer ne se voit pas, et attendre
    // demanderait une boucle de mise à jour d'effet que nous n'avons pas.
    // Le tic reste au patron pour le jour où elle arrivera.
    pool_.Add(ro::render::SowEffectWarpRingNode(node_pattern, ground_position,
                                                ro::render::EffectNodeLifeFor(effect_id)));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlaySpiralColumns(std::uint32_t effect_id, glm::vec3 ground_position,
                                          const std::optional<glm::vec3>& target_position) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  // 🔴 L'ÉCART VERS LA CIBLE, que la figure ajoute à chaque point de la colonne
  // du bâtisseur qui vise : elle se dresse SUR la victime. Sans cible connue,
  // l'écart est nul et la colonne reste sur l'effet — ce que fait l'autre
  // bâtisseur de toute façon.
  const glm::vec3 toward_target =
      target_position.has_value() ? *target_position - ground_position : glm::vec3{0.0F};
  for (const ro::render::EffectSpiralColumnPattern& node_pattern :
       ro::render::EffectSpiralColumnPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    // UN NŒUD PAR TOUR DE LA BOUCLE DU SITE : voir
    // `EffectSpiralColumnPattern::copies`.
    for (std::uint16_t copie = 0; copie < node_pattern.copies; ++copie) {
      pool_.Add(ro::render::SowEffectSpiralColumnNode(node_pattern, ground_position, toward_target,
                                                      ro::render::EffectNodeLifeFor(effect_id),
                                                      generator));
    }
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayConcentricSquares(std::uint32_t effect_id, glm::vec3 ground_position,
                                              std::int32_t count) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectConcentricSquarePattern& node_pattern :
       ro::render::EffectConcentricSquarePatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      ro::core::LogWarning("sphères : effet {}, image introuvable — {}", effect_id,
                           node_pattern.texture);
      continue;
    }
    ro::core::LogInfo("sphères : effet {}, compte {}, {} carré(s) semé(s)", effect_id, count,
                      std::clamp(count, 0, ro::render::kConcentricSquareMaxNodes));
    // LE COMPTE EST LE PARAMÈTRE DE L'INSTANCE : le nombre de sphères que les
    // sphères élémentaires annoncent. UN NŒUD PAR RANG, cinq au plus.
    //
    // ⚠ ET CE COMPTE SERT DEUX FOIS : il borne la boucle À CINQ, mais les seuils
    // de marque relisent sa valeur BRUTE. Le passer borné priverait les grandes
    // gerbes de leurs halos.
    const std::int32_t rangs = std::min(count, ro::render::kConcentricSquareMaxNodes);
    for (std::int32_t rang = 0; rang < rangs; ++rang) {
      pool_.Add(ro::render::SowEffectConcentricSquareNode(
          node_pattern, count, rang, ground_position, ro::render::EffectNodeLifeFor(effect_id),
          generator));
      sown = true;
    }
  }
  return sown;
}

bool EffectNodesInPlay::PlayGrandCrosses(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  for (const ro::render::EffectGrandCrossPattern& node_pattern :
       ro::render::EffectGrandCrossPatternsFor(effect_id)) {
    // ⚠ LES DEUX NŒUDS SE SÈMENT SÉPARÉMENT, et chacun exige SON image. Le cas
    // du client appelle deux bâtisseurs à la suite ; si l'une des deux archives
    // manque, l'autre moitié de la croix se montre quand même — c'est ce que
    // fait le client, qui ne les lie pas.
    if (!TextureFor(std::string(node_pattern.ribbon_texture)).empty()) {
      pool_.Add(ro::render::SowGrandCrossRibbonNode(node_pattern, ground_position,
                                                    ro::render::EffectNodeLifeFor(effect_id)));
      sown = true;
    }
    if (!TextureFor(std::string(node_pattern.frame_texture)).empty()) {
      pool_.Add(ro::render::SowGrandCrossFrameNode(node_pattern, ground_position,
                                                   ro::render::EffectNodeLifeFor(effect_id)));
      sown = true;
    }
  }
  return sown;
}

bool EffectNodesInPlay::PlaySphereShells(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  for (const ro::render::EffectSphereShellPattern& node_pattern :
       ro::render::EffectSphereShellPatternsFor(effect_id)) {
    // ⚠ LES DEUX IMAGES SONT EXIGÉES, pas seulement la première : sans la
    // seconde, la coquille extérieure sortirait des quads qu'aucun lot ne
    // peindrait, et le compte des muettes accuserait la figure au lieu de
    // l'archive.
    //
    // ⚠ ET LA SECONDE EST CELLE DE LA RANGÉE, non celle du fichier. Les deux
    // bâtisseurs n'écrivent pas la même : contrôler la constante ferait dépendre
    // `EF_GUARD2` d'une image qui n'est pas la sienne.
    if (TextureFor(std::string(node_pattern.texture)).empty() ||
        TextureFor(std::string(node_pattern.second_texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectSphereShellNode(node_pattern, ground_position,
                                                   ro::render::EffectNodeLifeFor(effect_id)));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayGroundCurtains(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectGroundCurtainPattern& node_pattern :
       ro::render::EffectGroundCurtainPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectGroundCurtainNode(
        node_pattern, ground_position, ro::render::EffectNodeLifeFor(effect_id), generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayGlidingSlashes(std::uint32_t effect_id, glm::vec3 ground_position,
                                           const std::optional<glm::vec3>& target_position) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectGlidingSlashPattern& node_pattern :
       ro::render::EffectGlidingSlashPatternsFor(effect_id)) {
    // ⚠ UN OCTET SUR TROIS CHARGE QUATRE IMAGES : les exiger TOUTES, parce que
    // l'index d'image en désigne une à chaque cran et qu'une manquante ferait
    // disparaître la taillade un tic sur quatre.
    bool all_loaded = true;
    for (const std::string_view image : ro::render::EffectGlidingSlashTexturesOf(node_pattern)) {
      all_loaded = all_loaded && !TextureFor(std::string(image)).empty();
    }
    if (!all_loaded) {
      continue;
    }
    // 🔴 LE SITE DE LA TAILLADE VIOLETTE BOUCLE HUIT FOIS, et chaque tour passe
    // son azimut au bâtisseur : voir `EffectGlidingSlashPattern::copies`. La
    // cible, elle, sert à celui qui vise.
    for (std::uint16_t copie = 0; copie < node_pattern.copies; ++copie) {
      ro::render::EffectGlidingSlashPattern rotated = node_pattern;
      rotated.azimuth_degrees = static_cast<std::int16_t>(
          node_pattern.azimuth_degrees + node_pattern.azimuth_step_per_copy_degrees * copie);
      ro::render::PooledEffectNode node = ro::render::SowEffectGlidingSlashNode(
          rotated, ground_position, ro::render::EffectNodeLifeFor(effect_id), generator);
      node.target_position = target_position;
      pool_.Add(std::move(node));
    }
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayQuadBursts(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectQuadBurstPattern& node_pattern :
       ro::render::EffectQuadBurstPatternsFor(effect_id)) {
    if (TextureFor(std::string(ro::render::EffectQuadBurstTextureOf(node_pattern.variant)))
            .empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectQuadBurstNode(node_pattern, ground_position, generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayFlyingShards(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectFlyingShardPattern& node_pattern :
       ro::render::EffectFlyingShardPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    const std::uint8_t copies =
        minimal_effects_ ? node_pattern.nodes_when_minimal : node_pattern.nodes_per_spawn;
    for (std::uint8_t copy = 0; copy < copies; ++copy) {
      pool_.Add(ro::render::SowEffectFlyingShardNode(
          node_pattern, ground_position, ro::render::EffectNodeLifeFor(effect_id), generator));
    }
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayHealSparkles(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  // La graine vient de l'instant et de ce qui joue déjà : deux soins de suite ne
  // posent pas leur étincelle sous le même angle.
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectHealSparklePattern& node_pattern :
       ro::render::EffectHealSparklePatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectHealSparkleNode(
        node_pattern, ground_position, ro::render::EffectNodeLifeFor(effect_id), generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayHomingSparks(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectHomingSparkPattern& node_pattern :
       ro::render::EffectHomingSparkPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectHomingSparkNode(
        node_pattern, ground_position, ro::render::EffectNodeLifeFor(effect_id), generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayMagicTargets(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  for (const ro::render::EffectMagicTargetPattern& node_pattern :
       ro::render::EffectMagicTargetPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectMagicTargetNode(node_pattern, ground_position,
                                                   ro::render::EffectNodeLifeFor(effect_id)));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayGroundBoxes(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectGroundBoxPattern& node_pattern :
       ro::render::EffectGroundBoxPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectGroundBoxNode(
        node_pattern, ground_position, ro::render::EffectNodeLifeFor(effect_id), generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlaySpriteFans(std::uint32_t effect_id, glm::vec3 ground_position,
                                       std::int32_t tick) {
  // ⚠ LE BÂTISSEUR SÈME AUX ÂGES ZÉRO ET UN, pas au seul premier tic : sa
  // garde est « âge inférieur ou égal à un ». N'en semer qu'un montrerait la
  // moitié de l'éventail.
  if (tick > ro::render::kSpriteFanLastSowTick) {
    return false;
  }
  bool sown = false;
  for (const ro::render::EffectSpriteFanPattern& node_pattern :
       ro::render::EffectSpriteFanPatternsFor(effect_id)) {
    // Le couple .spr/.act se charge par le chemin commun des sprites de la
    // troupe.
    const ro::render::LoadedActorSprite* charge =
        LoadedSprite(std::string(node_pattern.sprite_base));
    if (charge == nullptr) {
      continue;
    }
    ro::render::PooledEffectNode node = ro::render::SowEffectSpriteFanNode(
        node_pattern, ground_position, ro::render::EffectNodeLifeFor(effect_id), tick);
    // 🔴 L'ANIMATION SE COMPTE SUR LES IMAGES DE SON ACTION, que seul le `.act`
    // dit, et sa dernière image tue le nœud : laissé à un, l'éventail mourrait
    // au premier changement d'image.
    const auto action = static_cast<std::size_t>(std::max(node.node.action, 0));
    const auto& actions = charge->animation.actions();
    node.node.texture_count =
        action < actions.size() ? std::max<std::size_t>(actions[action].frames.size(), 1) : 1;
    pool_.Add(std::move(node));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayRingQuartets(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  for (const ro::render::EffectRingQuartetPattern& node_pattern :
       ro::render::EffectRingQuartetPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectRingQuartetNode(node_pattern, ground_position,
                                                   ro::render::EffectNodeLifeFor(effect_id)));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayRingStarbursts(std::uint32_t effect_id, glm::vec3 ground_position,
                                           std::int32_t tick) {
  bool sown = false;
  for (const ro::render::EffectRingStarburstPattern& pattern :
       ro::render::EffectRingStarburstPatternsFor(effect_id)) {
    if (pattern.sown_at_tick != tick || TextureFor(std::string(pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectRingStarburstNode(pattern, ground_position,
                                                     ro::render::EffectNodeLifeFor(effect_id)));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayTwinDiscs(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  for (const ro::render::EffectTwinDiscPattern& node_pattern :
       ro::render::EffectTwinDiscPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectTwinDiscNode(node_pattern, ground_position,
                                                ro::render::EffectNodeLifeFor(effect_id)));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayFadingTwinDiscs(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  for (const ro::render::EffectFadingTwinDiscPattern& node_pattern :
       ro::render::EffectFadingTwinDiscPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    // ⚠ À L'ÂGE ZÉRO SEULEMENT, et un nœud : le cas appelle le bâtisseur à
    // chaque tic, mais celui-ci ne bâtit que si l'âge de l'instance est nul.
    // Aucune horloge. La vie de l'effet passe par le plancher du
    // voisin : voir `FadingTwinDiscLifeTicks`.
    pool_.Add(ro::render::SowEffectFadingTwinDiscNode(node_pattern, ground_position,
                                                      ro::render::EffectNodeLifeFor(effect_id)));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayRainstormCylinders(std::uint32_t effect_id, glm::vec3 ground_position) {
  // ⚠ TOUTES LES IMAGES DU JEU OU AUCUNE. La figure en choisit une par le
  // compteur de ses pistes : s'il en manque une seule, un tic sur neuf
  // montrerait un trou, et rien ne dirait laquelle manque.
  const std::span<const std::string_view> images = ro::render::RainstormCylinderImages();
  const bool all = std::ranges::all_of(
      images, [this](std::string_view image) { return !TextureFor(std::string(image)).empty(); });
  if (!all) {
    return false;
  }
  bool sown = false;
  // ⚠ QUATRE NŒUDS À L'ÂGE ZÉRO, un par appel du cas : la garde du bâtisseur
  // est l'âge nul de l'instance, que les quatre appels du premier tic voient
  // tous. Aucune horloge.
  for (const ro::render::EffectRainstormCylinderPattern& node_pattern :
       ro::render::EffectRainstormCylinderPatternsFor(effect_id)) {
    pool_.Add(ro::render::SowRainstormCylinderNode(node_pattern, ground_position,
                                                   ro::render::EffectNodeLifeFor(effect_id)));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayCloudRings(std::uint32_t effect_id, glm::vec3 ground_position,
                                       std::int32_t tick) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectCloudRingPattern& node_pattern :
       ro::render::EffectCloudRingPatternsFor(effect_id)) {
    if (!ro::render::EffectNodeScheduleSowsAt(node_pattern.schedule, tick)) {
      continue;
    }
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectCloudRingNode(
        node_pattern, ground_position, ro::render::EffectNodeLifeFor(effect_id), generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayJobAuras(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectJobAuraPattern& node_pattern :
       ro::render::EffectJobAuraPatternsFor(effect_id)) {
    // ⚠ LES CINQ IMAGES SONT EXIGÉES, pas seulement la première : le numéro
    // que la piste porte les désigne toutes tour à tour, et il se retire au
    // hasard. N'en vérifier qu'une laisserait l'aura clignoter.
    const bool all = std::ranges::all_of(node_pattern.textures, [this](std::string_view image) {
      return !TextureFor(std::string(image)).empty();
    });
    if (!all) {
      continue;
    }
    pool_.Add(ro::render::SowEffectJobAuraNode(
        node_pattern, ground_position, ro::render::EffectNodeLifeFor(effect_id), generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayWanderingMotes(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectWanderingMotePattern& node_pattern :
       ro::render::EffectWanderingMotePatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectWanderingMoteNode(
        node_pattern, ground_position, ro::render::EffectNodeLifeFor(effect_id), generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlaySpinningRounds(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectSpinningRoundPattern& node_pattern :
       ro::render::EffectSpinningRoundPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectSpinningRoundNode(
        node_pattern, ground_position, ro::render::EffectNodeLifeFor(effect_id), generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlaySpinRibbons(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectSpinRibbonPattern& node_pattern :
       ro::render::EffectSpinRibbonPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    // UN NŒUD PAR APPEL DU SITE, chacun incliné d'un cran de plus et tirant ses
    // pistes à la suite du précédent : voir `EffectSpinRibbonPattern::copies`.
    for (std::uint8_t copie = 0; copie < node_pattern.copies; ++copie) {
      ro::render::EffectSpinRibbonPattern incline = node_pattern;
      incline.base_tilt_degrees = static_cast<std::int16_t>(
          node_pattern.base_tilt_degrees + node_pattern.tilt_step_per_copy_degrees * copie);
      pool_.Add(ro::render::SowEffectSpinRibbonNode(
          incline, ground_position, ro::render::EffectNodeLifeFor(effect_id), generator));
    }
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayShieldBoomerangs(std::uint32_t effect_id, glm::vec3 ground_position,
                                             const std::optional<glm::vec3>& target_position,
                                             std::int32_t param) {
  bool sown = false;
  for (const ro::render::EffectShieldBoomerangPattern& node_pattern :
       ro::render::EffectShieldBoomerangPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    // ⚠ SANS CIBLE, RIEN N'EST SEMÉ, et le semis le dit lui-même en rendant un
    // optionnel vide. Poser le nœud quand même donnerait un carré immobile là
    // où le client ne montre rien.
    if (std::optional<ro::render::PooledEffectNode> node = ro::render::SowEffectShieldBoomerangNode(
            node_pattern, ground_position, target_position, param)) {
      pool_.Add(std::move(*node));
      sown = true;
    }
  }
  return sown;
}

bool EffectNodesInPlay::PlayThunderScatters(std::uint32_t effect_id, glm::vec3 ground_position,
                                            const std::optional<glm::vec3>& target_position) {
  if (ro::render::EffectThunderScatterPatternsFor(effect_id).empty() ||
      TextureFor(std::string(ro::render::kThunderScatterTexture)).empty()) {
    return false;
  }
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  // ⚠ UN NŒUD AU PREMIER TIC, par la garde de l'âge nul, et RIEN
  // sans point visé : le semis le dit en rendant un optionnel vide.
  std::optional<ro::render::PooledEffectNode> node = ro::render::SowThunderScatterNode(
      ground_position, target_position, ro::render::EffectNodeLifeFor(effect_id), generator);
  if (!node.has_value()) {
    return false;
  }
  pool_.Add(std::move(*node));
  return true;
}

bool EffectNodesInPlay::PlayThunderFreezes(std::uint32_t effect_id, glm::vec3 ground_position) {
  // ⚠ LES DEUX IMAGES SONT EXIGÉES : sans la seconde, les éclats sortiraient des
  // quads qu'aucun lot ne peindrait.
  if (ro::render::EffectThunderFreezePatternsFor(effect_id).empty() ||
      TextureFor(std::string(ro::render::kThunderFreezeCoreImage)).empty() ||
      TextureFor(std::string(ro::render::kThunderFreezeShardImage)).empty()) {
    return false;
  }
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  // ⚠ LE NOMBRE DE NŒUDS EST CELUI QUE LES TROIS LANCEURS DU CLIENT POSENT par
  // la commande 14 (triplet `(2, 0, 0)`), et non le paramètre de
  // l'instance : voir `effect_thunder_freeze_seed.h`.
  std::vector<ro::render::PooledEffectNode> nodes =
      ro::render::SowThunderFreezeNodes(ground_position, ro::render::kThunderFreezeLaunchedCount,
                                        ro::render::EffectNodeLifeFor(effect_id), generator);
  const bool sown = !nodes.empty();
  for (ro::render::PooledEffectNode& node : nodes) {
    pool_.Add(std::move(node));
  }
  return sown;
}

bool EffectNodesInPlay::PlayAimedSpokes(std::uint32_t effect_id, glm::vec3 ground_position,
                                        const std::optional<glm::vec3>& target_position) {
  // UN NŒUD PAR SITE, au premier tic seulement. ⚠ DEUX LIGNES POUR 1584, une
  // par branche du réglage des effets allégés : l'image qui ne se lit pas écarte
  // la sienne. Sans cible, les rayons pointent au cap zéro.
  bool sown = false;
  for (const ro::render::EffectAimedSpokePattern& node_pattern :
       ro::render::EffectAimedSpokePatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowAimedSpokeNode(node_pattern, ground_position, target_position,
                                            ro::render::EffectNodeLifeFor(effect_id)));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayShockwaveRays(std::uint32_t effect_id, glm::vec3 ground_position) {
  // UN NŒUD PAR APPEL, à la création de l'effet : deux pour le 300, à deux
  // angles.
  bool sown = false;
  for (const ro::render::EffectShockwaveRayPattern& node_pattern :
       ro::render::EffectShockwaveRayPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowShockwaveRayNode(node_pattern, ground_position,
                                              ro::render::EffectNodeLifeFor(effect_id)));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayTaperedSheets(std::uint32_t effect_id, glm::vec3 ground_position) {
  // UN NŒUD PAR SITE, au premier tic seulement ; il suit ensuite son porteur.
  bool sown = false;
  for (const ro::render::EffectTaperedSheetPattern& node_pattern :
       ro::render::EffectTaperedSheetPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowTaperedSheetNode(node_pattern, ground_position,
                                              ro::render::EffectNodeLifeFor(effect_id)));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayTetheredBeams(std::uint32_t effect_id, glm::vec3 ground_position,
                                          const std::optional<glm::vec3>& target_position,
                                          std::uint32_t target_actor_id) {
  bool sown = false;
  for (const ro::render::EffectTetheredBeamPattern& node_pattern :
       ro::render::EffectTetheredBeamPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    ro::render::PooledEffectNode node = ro::render::SowEffectTetheredBeamNode(
        node_pattern, ground_position, target_position, ro::render::EffectNodeLifeFor(effect_id));
    // 🔴 LE BOUT LOINTAIN SUIT LA CIBLE : le client la retrouve par son
    // identifiant à chaque tic et en recopie la place. Voir
    // `PooledEffectNode::target_actor_id`.
    node.target_actor_id = target_actor_id;
    pool_.Add(std::move(node));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayThrownItems(std::uint32_t effect_id, glm::vec3 ground_position,
                                        std::int32_t tick,
                                        const std::optional<glm::vec3>& target_position,
                                        std::int32_t param) {
  // ⚠ SANS CIBLE, RIEN NE SE SÈME, et ce n'est pas une prudence : le bâtisseur
  // tire du couple lanceur-cible le pas d avance, la pente et le nombre de tics
  // du vol. Retomber sur le point du semis donnerait une distance nulle, donc
  // une pente infinie — un objet qui part droit vers le ciel.
  if (!target_position.has_value()) {
    return false;
  }
  bool sown = false;
  for (const ro::render::EffectThrownItemPattern& node_pattern :
       ro::render::EffectThrownItemPatternsFor(effect_id)) {
    // 🔴 PAR ÉGALITÉ, ET À CE TIC SEULEMENT : le client compare l'âge de
    // l'instance au tic de naissance du style. Les deux flacons d'un même effet
    // partent donc l'un après l'autre, et rien ne se dessine avant leur tic.
    if (tick != ro::render::EffectThrownItemBirthTick(node_pattern.builder_argument)) {
      continue;
    }
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectThrownItemNode(node_pattern, ground_position, *target_position,
                                                  ro::render::EffectNodeLifeFor(effect_id), param));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayCyclingSkirts(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  for (const ro::render::EffectCyclingSkirtPattern& node_pattern :
       ro::render::EffectCyclingSkirtPatternsFor(effect_id)) {
    ro::render::PooledEffectNode node = ro::render::SowEffectCyclingSkirtNode(
        node_pattern, ground_position, ro::render::EffectNodeLifeFor(effect_id));
    // ⚠ TOUTES LES IMAGES DU JEU OU AUCUNE. La figure en choisit une par le
    // compteur de tics : s'il en manque une seule, un tic sur vingt-sept
    // montrerait un trou, et rien ne dirait laquelle manque.
    if (node.resource_paths.empty()) {
      continue;
    }
    const bool all = std::ranges::all_of(
        node.resource_paths, [this](const std::string& p) { return !TextureFor(p).empty(); });
    if (!all) {
      continue;
    }
    pool_.Add(std::move(node));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayGridPlanes(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  for (const ro::render::EffectGridPlanePattern& node_pattern :
       ro::render::EffectGridPlanePatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectGridPlaneNode(node_pattern, ground_position,
                                                 ro::render::EffectNodeLifeFor(effect_id)));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayGrowingSprites(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectGrowingSpritePattern& node_pattern :
       ro::render::EffectGrowingSpritePatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    // ⚠ LES SIX NAISSENT AU MEME TIC, et c'est leur RETARD qui les echelonne :
    // les semer a six tics differents ferait le meme effet par un autre chemin,
    // et ce chemin-la n'est pas celui du client.
    pool_.Add(ro::render::SowEffectGrowingSpriteNode(
        node_pattern, ground_position, ro::render::EffectNodeLifeFor(effect_id), generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayAuraColumns(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  for (const ro::render::EffectAuraColumnPattern& node_pattern :
       ro::render::EffectAuraColumnPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectAuraColumnNode(node_pattern, ground_position,
                                                  ro::render::EffectNodeLifeFor(effect_id)));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayFadingStreaks(std::uint32_t effect_id, glm::vec3 ground_position,
                                          const std::optional<glm::vec3>& target_position) {
  // ⚠ SANS CIBLE, RIEN A SEMER, et c est le client qui le veut : la traînee vole
  // du lanceur vers un point vise, et l ancre de chaque piste EST ce point. La
  // semer sur le lanceur donnerait une portee nulle, donc quatre flocons figes.
  if (!target_position.has_value()) {
    return false;
  }
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectFadingStreakPattern& node_pattern :
       ro::render::EffectFadingStreakPatternsFor(effect_id)) {
    if (node_pattern.textures.empty() ||
        TextureFor(std::string(node_pattern.textures.back())).empty()) {
      continue;
    }
    // ⚠ TROIS NOEUDS, ET ILS NAISSENT AU MEME TIC : le cas appelle son batisseur
    // trois fois de suite. Ce qui les distingue est leurs tirages, pas leur age.
    for (std::uint16_t node = 0; node < node_pattern.node_count; ++node) {
      pool_.Add(ro::render::SowEffectFadingStreakNode(
          node_pattern, ground_position, *target_position, ro::render::EffectNodeLifeFor(effect_id),
          generator));
    }
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayRainbowTrails(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  for (const ro::render::EffectRainbowTrailPattern& node_pattern :
       ro::render::EffectRainbowTrailPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    // ⚠ LE CAP DE LA VUE SE FIGE ICI, au tic du semis : c est le client qui le
    // veut, et la troupe le retient pour cela. Le lire au moment de composer
    // ferait pivoter l arc avec l ecran.
    pool_.Add(ro::render::SowEffectRainbowTrailNode(node_pattern, ground_position,
                                                    ro::render::EffectNodeLifeFor(effect_id),
                                                    pool_.billboard_turn_degrees()));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlaySpiralTrails(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectSpiralTrailPattern& node_pattern :
       ro::render::EffectSpiralTrailPatternsFor(effect_id)) {
    if (node_pattern.textures.empty() ||
        TextureFor(std::string(node_pattern.textures.front())).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectSpiralTrailNode(
        node_pattern, ground_position, ro::render::EffectNodeLifeFor(effect_id), generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayDoubleRibbons(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  for (const ro::render::EffectDoubleRibbonPattern& node_pattern :
       ro::render::EffectDoubleRibbonPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    // ⚠ DEUX NOEUDS, ET LE SECOND NAIT EN RETARD : la garde du batisseur le fait
    // batir au tic zero ET au tic quatre. Nous les semons au meme tic et c'est le
    // COMPTEUR NEGATIF qui les decale, comme pour les six boules d'EF_STASIS.
    pool_.Add(ro::render::SowEffectDoubleRibbonNode(node_pattern, ground_position, 0,
                                                    ro::render::EffectNodeLifeFor(effect_id)));
    pool_.Add(ro::render::SowEffectDoubleRibbonNode(node_pattern, ground_position,
                                                    ro::render::kDoubleRibbonSecondNodeDelayTicks,
                                                    ro::render::EffectNodeLifeFor(effect_id)));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlaySpikeBeams(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  for (const ro::render::EffectSpikeBeamPattern& node_pattern :
       ro::render::EffectSpikeBeamPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    // ⚠ TROIS NŒUDS, UN PAR TIC, comme les quartiers de tarte : chacun balaie un
    // quart de tour, et les semer au même tic les superposerait.
    for (std::int32_t tick_index = 0; tick_index < ro::render::kPieWedgeSowingTicks; ++tick_index) {
      pool_.Add(ro::render::SowEffectSpikeBeamNode(node_pattern, ground_position, tick_index,
                                                   ro::render::EffectNodeLifeFor(effect_id)));
    }
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayPieWedges(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  for (const ro::render::EffectPieWedgePattern& node_pattern :
       ro::render::EffectPieWedgePatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    // ⚠ TROIS NŒUDS, UN PAR TIC, et c'est ce qui ferme le cercle : chacun couvre
    // un quart de tour, et les semer au même tic les superposerait.
    for (std::int32_t tick_index = 0; tick_index < ro::render::kPieWedgeSowingTicks; ++tick_index) {
      pool_.Add(ro::render::SowEffectPieWedgeNode(node_pattern, ground_position, tick_index,
                                                  ro::render::EffectNodeLifeFor(effect_id)));
    }
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayGroundGusts(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectGroundGustPattern& node_pattern :
       ro::render::EffectGroundGustPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    // ⚠ LA CIBLE EST INCONNUE ICI, et le zéro le dit : `render` ne sait pas
    // où vise un sort. Une bourrasque qui ne vise rien part cap zéro et file
    // quand même — c'est la même convention que pour l'essaim.
    pool_.Add(ro::render::SowEffectGroundGustNode(
        node_pattern, ground_position, glm::vec3{0.0F, 0.0F, 0.0F},
        ro::render::EffectNodeLifeFor(effect_id), generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayHomingGusts(std::uint32_t effect_id, glm::vec3 ground_position,
                                        const std::optional<glm::vec3>& target_position,
                                        std::int32_t param) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectHomingGustPattern& node_pattern :
       ro::render::EffectHomingGustPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    // ⚠ SANS CIBLE, ELLES SONT SEMÉES QUAND MÊME : le client sème avant de
    // chercher l'acteur, et c'est son mouvement qui décide — le 122 rentre, le
    // 125 s'éteint. Ne rien semer effacerait ce qu'il montre.
    // ⚠ UN NŒUD PAR COUP pour le 621, à l'octet `rang % 2` : c'est le site qui
    // boucle, et un paramètre nul ou négatif n'en sème aucun.
    const std::int32_t how_many = node_pattern.one_node_per_hit ? param : 1;
    for (std::int32_t rang = 0; rang < how_many; ++rang) {
      const auto octet = node_pattern.one_node_per_hit
                             ? static_cast<std::uint8_t>(static_cast<std::uint32_t>(rang) %
                                                         ro::render::kHomingGustSpreadSides)
                             : node_pattern.octet;
      pool_.Add(ro::render::SowEffectHomingGustNode(node_pattern, ground_position, target_position,
                                                    octet, ro::render::EffectNodeLifeFor(effect_id),
                                                    generator));
      sown = true;
    }
  }
  return sown;
}

bool EffectNodesInPlay::PlayRollingGusts(std::uint32_t effect_id, glm::vec3 ground_position,
                                         const std::optional<glm::vec3>& target_position) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  // ⚠ SANS CIBLE, LE CAP EST ZÉRO, comme pour la bourrasque du 119. Le client
  // en a toujours une — le gestionnaire de coup lui passe la place de la
  // victime —, c'est notre appelant qui peut l'ignorer.
  const glm::vec3 vers =
      target_position.has_value() ? *target_position - ground_position : glm::vec3{0.0F};
  for (const ro::render::EffectRollingGustPattern& node_pattern :
       ro::render::EffectRollingGustPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    // ⚠ CINQ NŒUDS, UN PAR TIC : semés ensemble, chacun retardé du sien.
    for (std::int32_t tick_index = 0; tick_index < ro::render::kRollingGustSowingTicks;
         ++tick_index) {
      pool_.Add(ro::render::SowEffectRollingGustNode(
          node_pattern, ground_position, vers, tick_index, ro::render::EffectNodeLifeFor(effect_id),
          generator));
    }
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayDriftingIcons(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectDriftingIconPattern& node_pattern :
       ro::render::EffectDriftingIconPatternsFor(effect_id)) {
    // ⚠ TOUTES LES IMAGES D'UN COUP, avant le premier nœud : l'octet deux en
    // tire une par nœud, et une seule illisible ferait disparaître un nœud sur
    // trois sans que rien ne le dise. Une image absente écarte le site entier.
    bool lisibles = true;
    for (std::size_t image = 0; image < ro::render::EffectDriftingIconTextureCount(node_pattern);
         ++image) {
      lisibles = lisibles && !TextureFor(std::string(node_pattern.textures[image])).empty();
    }
    if (!lisibles) {
      continue;
    }
    // ⚠ QUATRE-VINGTS NŒUDS AU TIC ZÉRO : le site boucle quatre-vingts fois,
    // et le bâtisseur ne sème qu'à l'âge zéro.
    //
    // ⚠ AUTOUR DU JOUEUR LOCAL, et non de la place de l'effet : l'instance
    // recopie la place du joueur avant de semer, quel que soit l'acteur qui a
    // reçu l'effet. Sans joueur — le banc d'essai —, la place de l'effet.
    const glm::vec3 centre = local_player_position_.value_or(ground_position);
    for (std::int32_t node = 0; node < ro::render::kDriftingIconNodesPerPlay; ++node) {
      pool_.Add(ro::render::SowEffectDriftingIconNode(node_pattern, centre,
                                                      ro::render::EffectNodeLifeFor(effect_id),
                                                      pool_.world(), generator));
    }
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayElectricArcs(std::uint32_t effect_id, glm::vec3 ground_position,
                                         float owner_facing_degrees) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectElectricArcPattern& node_pattern :
       ro::render::EffectElectricArcPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    // ⚠ À L'ÂGE ZÉRO SEULEMENT, et deux nœuds : le cas de l'effet appelle le
    // bâtisseur deux fois de suite à chaque tic, mais il
    // ne bâtit qu'à l'âge nul. Aucune horloge. Le cap
    // vient du PORTEUR, lu au semis.
    pool_.Add(
        ro::render::SowEffectElectricArcNode(node_pattern, ground_position, owner_facing_degrees,
                                             ro::render::EffectNodeLifeFor(effect_id), generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayFrameAndBars(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  for (const ro::render::EffectFrameAndBarPattern& node_pattern :
       ro::render::EffectFrameAndBarPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    // ⚠ DEUX NŒUDS PAR EFFET, et la table en porte DEUX LIGNES : le site
    // appelle son bâtisseur deux fois de suite, à deux octets consécutifs.
    // N'en poser qu'un donnerait la moitié de la figure.
    //
    // ⚠ ET LA VIE ENTRE DANS LA DURÉE pour les octets de la dizaine : le
    // bâtisseur y range la vie de l'effet moins quatre-vingt-dix.
    pool_.Add(ro::render::SowEffectFrameAndBarNode(node_pattern, ground_position,
                                                   ro::render::EffectNodeLifeFor(effect_id)));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayRibbonWaves(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  // La graine vient de l'instant et de ce qui joue déjà : deux anneaux lancés
  // ensemble ne respirent donc pas en phase.
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectRibbonWavePattern& node_pattern :
       ro::render::EffectRibbonWavePatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectRibbonWaveNode(node_pattern, ground_position, generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayTurningRings(std::uint32_t effect_id, glm::vec3 ground_position) {
  bool sown = false;
  // La graine vient de l'instant et de ce qui joue déjà : deux anneaux lancés
  // ensemble ne pulsent donc pas d'accord.
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectTurningRingPattern& node_pattern :
       ro::render::EffectTurningRingPatternsFor(effect_id)) {
    if (TextureFor(std::string(node_pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectTurningRingNode(node_pattern, ground_position, generator));
    sown = true;
  }
  return sown;
}

}  // namespace moonclient
