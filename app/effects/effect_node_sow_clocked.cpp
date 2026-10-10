// LES SEMIS QUI ATTENDENT LEUR TIC.
//
// Sortis d'`effect_node_sow.cpp` au plafond de huit cents lignes, et la coupure
// suit une frontiere reelle : les familles rassemblees ici ne posent PAS tout au
// premier tic. Le client rappelle leur batisseur a chaque tic et garde son
// semis derriere une condition sur le compteur ; c'est `EffectClock` qui les
// rappelle chez nous, et c'est pourquoi elles seules recoivent un tic.
//
// Les autres posent tout d'un coup et n'ont pas besoin de savoir quand.

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ro/render/actor_sprite.h"
#include "ro/render/effect_anchored_skirt_seed.h"
#include "ro/render/effect_bearer_follow.h"
#include "ro/render/effect_break_ring_seed.h"
#include "ro/render/effect_camera_fan_seed.h"
#include "ro/render/effect_cloud_ring_seed.h"
#include "ro/render/effect_drifting_mote_seed.h"
#include "ro/render/effect_expanding_ring_seed.h"
#include "ro/render/effect_fading_sprite_seed.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_frame_beat.h"
#include "ro/render/effect_glow_sprite_seed.h"
#include "ro/render/effect_life_table.h"
#include "ro/render/effect_lightning_fan_seed.h"
#include "ro/render/effect_marching_sower.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_palette_billboard_seed.h"
#include "ro/render/effect_radial_petal_seed.h"
#include "ro/render/effect_rain_particle_seed.h"
#include "ro/render/effect_ring_starburst_seed.h"
#include "ro/render/effect_rising_spark_seed.h"
#include "ro/render/effect_screen_burst_seed.h"
#include "ro/render/effect_seeded_ring.h"
#include "ro/render/effect_sheathed_bolt_seed.h"
#include "ro/render/effect_snaking_arc_seed.h"
#include "ro/render/effect_spark_tube_seed.h"
#include "ro/render/effect_sprite_fan_seed.h"
#include "ro/render/effect_swarm_seed.h"
#include "ro/render/effect_swirl_trail_seed.h"
#include "ro/render/effect_tetra_vortex_seed.h"
#include "ro/render/effect_thrown_item_seed.h"
#include "ro/render/effect_tri_plane_seed.h"
#include "ro/render/effect_triple_wall_seed.h"
#include "ro/render/effect_white_burst_seed.h"

#include "effects/effect_node_view.h"

namespace moonclient {
namespace {}  // namespace

bool EffectNodesInPlay::PlayRisingSparks(std::uint32_t effect_id, glm::vec3 ground_position,
                                         std::int32_t tick) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectRisingSparkPattern& pattern :
       ro::render::EffectRisingSparkPatternsFor(effect_id)) {
    if (!ro::render::EffectNodeScheduleSowsAt(pattern.schedule, tick)) {
      continue;
    }
    // Une texture qui ne se lit pas laisserait quatre carrés blancs, ce qui
    // ressemble à une figure ratée plutôt qu'à une absence.
    if (TextureFor(std::string(pattern.texture)).empty()) {
      continue;
    }
    if (!pattern.flash_texture.empty()) {
      (void)TextureFor(std::string(pattern.flash_texture));
    }
    // UN NŒUD PAR APPEL DU SITE : voir `EffectRisingSparkPattern::copies`.
    for (std::uint16_t copie = 0; copie < pattern.copies; ++copie) {
      pool_.Add(ro::render::SowEffectRisingSparkNode(pattern, ground_position, generator));
    }
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlaySeededRings(std::uint32_t effect_id, glm::vec3 ground_position,
                                        std::int32_t tick) {
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  bool sown = false;
  for (const ro::render::EffectSeededRingPattern& pattern :
       ro::render::EffectSeededRingPatternsFor(effect_id)) {
    if (!ro::render::EffectNodeScheduleSowsAt(pattern.schedule, tick)) {
      continue;
    }
    if (TextureFor(std::string(pattern.texture)).empty()) {
      continue;
    }
    // LA VIE DE L'INSTANCE, que certains bâtisseurs relèvent puis jugent : voir
    // `EffectSeededRingPattern::instance_life_floor_ticks`.
    pool_.Add(ro::render::SowEffectSeededRingNode(
        pattern, ground_position, generator,
        pool_.InstanceLifeTicks(ro::render::EffectNodeLifeFor(effect_id))));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayTripleWalls(std::uint32_t effect_id, glm::vec3 ground_position,
                                        std::int32_t tick) {
  bool sown = false;
  for (const ro::render::EffectTripleWallPattern& pattern :
       ro::render::EffectTripleWallPatternsFor(effect_id)) {
    // ⚠ LE MUR VERT ATTEND LE CINQUIÈME TIC : son bâtisseur compare le compteur
    // de l'instance à cinq, là où les deux autres sèment au premier. Le semer
    // d'emblée le montrerait un quart de seconde avant le reste de l'effet.
    if (!ro::render::EffectNodeScheduleSowsAt(pattern.schedule, tick)) {
      continue;
    }
    if (TextureFor(std::string(pattern.texture)).empty()) {
      continue;
    }
    // ⚠ DEUX NŒUDS PAR EFFET, aux azimuts zéro et quarante-cinq : le site
    // appelle son bâtisseur deux fois de suite. N'en poser qu'un donnerait la
    // moitié des murs, entrelacés autrement.
    for (const std::int16_t azimuth : ro::render::kTripleWallNodeAzimuths) {
      pool_.Add(ro::render::SowEffectTripleWallNode(pattern, azimuth, ground_position,
                                                    ro::render::EffectNodeLifeFor(effect_id)));
    }
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayLightningFans(std::uint32_t effect_id, glm::vec3 ground_position,
                                          std::int32_t tick) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectLightningFanPattern& pattern :
       ro::render::EffectLightningFanPatternsFor(effect_id)) {
    // ⚠ LES TROIS BÂTISSEURS SÈMENT AUX QUATRE PREMIERS TICS, et chaque
    // identifiant en appelle DEUX : huit nœuds en tout. N'en semer qu'un
    // donnerait le huitième de l'éclair.
    if (!ro::render::EffectNodeScheduleSowsAt(pattern.schedule, tick)) {
      continue;
    }
    if (TextureFor(std::string(pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectLightningFanNode(
        pattern, ground_position, ro::render::EffectNodeLifeFor(effect_id), generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayAnchoredSkirts(std::uint32_t effect_id, glm::vec3 ground_position,
                                           std::int32_t tick) {
  bool sown = false;
  for (const ro::render::EffectAnchoredSkirtPattern& pattern :
       ro::render::EffectAnchoredSkirtPatternsFor(effect_id)) {
    // ⚠ LE SECOND BÂTISSEUR SÈME TROIS FOIS, aux tics zéro, vingt et
    // quarante. N'en semer qu'un donnerait le tiers de la gerbe.
    if (!ro::render::EffectNodeScheduleSowsAt(pattern.schedule, tick)) {
      continue;
    }
    if (pattern.textures.empty() || TextureFor(std::string(pattern.textures.front())).empty()) {
      continue;
    }
    // ⚠ LA CIBLE EST INCONNUE ICI, et le zéro le dit : `render` ne sait pas où
    // vise un sort. Une gerbe sans cible part cap cent quatre-vingt-quinze.
    pool_.Add(ro::render::SowEffectAnchoredSkirtNode(pattern, ground_position,
                                                     glm::vec3{0.0F, 0.0F, 0.0F},
                                                     ro::render::EffectNodeLifeFor(effect_id)));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlaySnakingArcs(std::uint32_t effect_id, glm::vec3 ground_position,
                                        std::int32_t tick) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectSnakingArcPattern& pattern :
       ro::render::EffectSnakingArcPatternsFor(effect_id)) {
    // ⚠ QUATRE NŒUDS, AUX QUATRE PREMIERS TICS : le bâtisseur du client ne
    // teste pas « au premier tic » mais « tant que le tic ne dépasse pas
    // trois ». N'en semer qu'un donnerait le quart de l'orage.
    if (!ro::render::EffectNodeScheduleSowsAt(pattern.schedule, tick)) {
      continue;
    }
    if (TextureFor(std::string(pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectSnakingArcNode(
        pattern, ground_position, ro::render::EffectNodeLifeFor(effect_id), generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayBreakRings(std::uint32_t effect_id, glm::vec3 ground_position,
                                       std::int32_t tick) {
  // ⚠ LE BÂTISSEUR EST RAPPELÉ À CHAQUE TIC, et ne sème qu'aux tics de son
  // horaire : dix nœuds, dont la taille dépend du tic où ils naissent.
  bool sown = false;
  if (!ro::render::EffectNodeScheduleSowsAt(ro::render::kBreakRingSchedule, tick)) {
    return sown;
  }
  for (const ro::render::EffectBreakRingPattern& pattern :
       ro::render::EffectBreakRingPatternsFor(effect_id)) {
    if (TextureFor(std::string(pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowBreakRingNode(pattern, ground_position, tick,
                                           ro::render::EffectNodeLifeFor(effect_id)));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlaySwarms(std::uint32_t effect_id, glm::vec3 ground_position,
                                   std::int32_t tick) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectSwarmPattern& pattern :
       ro::render::EffectSwarmPatternsFor(effect_id)) {
    // ⚠ TROIS BOUFFÉES À DIX TICS D'INTERVALLE, et l'horaire est le même pour
    // toute la famille : le client garde son bâtisseur derrière un modulo sur le
    // tic de l'instance. Tout semer au premier tic donnerait un tiers de la
    // durée et trois fois la densité.
    if (!ro::render::EffectNodeScheduleSowsAt(ro::render::kEffectSwarmSchedule, tick)) {
      continue;
    }
    // ⚠ DEUX PATRONS SUR CINQ SONT UNE BRANCHE DE RÉGLAGE, et c'est ici que le
    // choix se fait : l'image qui ne se lit pas écarte sa ligne. Voir
    // scripts/gen_client_effect_swarms.py.
    if (TextureFor(std::string(pattern.texture)).empty()) {
      continue;
    }
    // ⚠ LA CIBLE EST INCONNUE ICI, et le zéro le dit : `render` ne sait pas où
    // vise un sort. Un essaim qui ne vise rien part cap zéro et poursuit
    // quand même — c'est la même convention que pour la colonne.
    pool_.Add(ro::render::SowEffectSwarmNode(pattern, ground_position, glm::vec3{0.0F, 0.0F, 0.0F},
                                             ro::render::EffectNodeLifeFor(effect_id), generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlaySparkTubes(std::uint32_t effect_id, glm::vec3 ground_position,
                                       std::int32_t tick,
                                       const std::optional<glm::vec3>& target_position) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectSparkTubePattern& pattern :
       ro::render::EffectSparkTubePatternsFor(effect_id)) {
    // ⚠ LA FENÊTRE DE TICS EST DANS LE BÂTISSEUR, et elle diffère pour chacun :
    // deux nœuds pour l'un, cinq pour l'autre, et l'un d'eux ne commence qu'au
    // sixième tic. Tout semer au premier donnerait une gerbe au lieu d'une
    // rafale.
    if (!ro::render::EffectNodeScheduleSowsAt(pattern.schedule, tick)) {
      continue;
    }
    if (TextureFor(std::string(ro::render::kSparkTubeTexture)).empty()) {
      continue;
    }
    // LE VECTEUR VERS LA CIBLE DE L'EFFET, ou nul sans cible. Seul le bâtisseur
    // dirigé le lit ; les deux autres tirent leur cap au sort.
    const glm::vec3 toward_target =
        target_position.has_value() ? *target_position - ground_position : glm::vec3{0.0F};
    pool_.Add(ro::render::SowEffectSparkTubeNode(pattern, ground_position, toward_target,
                                                 ro::render::EffectNodeLifeFor(effect_id),
                                                 generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayDriftingMotes(std::uint32_t effect_id, glm::vec3 ground_position,
                                          std::int32_t tick) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectDriftingMotePattern& pattern :
       ro::render::EffectDriftingMotePatternsFor(effect_id)) {
    // ⚠ L'ENTIER DU SITE EST AUSSI LE TIC DU SEMIS : l'un des quatre attend le
    // TRENTE-CINQUIÈME tic de son effet. Le semer au premier le montrerait une
    // seconde et demie trop tôt.
    if (!ro::render::EffectNodeScheduleSowsAt(pattern.schedule, tick)) {
      continue;
    }
    if (TextureFor(std::string(pattern.texture)).empty()) {
      continue;
    }
    // UN NŒUD PAR TOUR DE LA BOUCLE DU SITE : voir
    // `EffectDriftingMotePattern::copies`.
    for (std::uint16_t copie = 0; copie < pattern.copies; ++copie) {
      pool_.Add(ro::render::SowEffectDriftingMoteNode(
          pattern, ground_position, ro::render::EffectNodeLifeFor(effect_id), generator));
    }
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayRadialPetals(std::uint32_t effect_id, glm::vec3 ground_position,
                                         std::int32_t tick) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectRadialPetalPattern& pattern :
       ro::render::EffectRadialPetalPatternsFor(effect_id)) {
    // ⚠ SEIZE NŒUDS PAR EFFET, et ceux de l'effet 411 attendent le TRENTE-
    // CINQUIÈME tic : le troisième argument du site est aussi l'horaire.
    if (!ro::render::EffectNodeScheduleSowsAt(pattern.schedule, tick) ||
        TextureFor(std::string(pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowRadialPetalNode(pattern, ground_position,
                                             ro::render::EffectNodeLifeFor(effect_id), generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayFadingSprites(std::uint32_t effect_id, glm::vec3 ground_position,
                                          std::int32_t tick) {
  bool sown = false;
  for (const ro::render::EffectFadingSpritePattern& pattern :
       ro::render::EffectFadingSpritePatternsFor(effect_id)) {
    // ⚠ LES AILES SE RESÈMENT TOUS LES TROIS TICS jusqu'au dix-huitième, et le
    // reste ne naît qu'au tic zéro. N'en semer qu'un ferait un ange dont les
    // ailes ne battent pas.
    if (!ro::render::EffectNodeScheduleSowsAt(pattern.schedule, tick)) {
      continue;
    }
    // Le couple .spr/.act se charge par le même chemin que les autres sprites
    // d'effet : c'est `Sow` qui sait le faire, et lui seul.
    const std::string base(pattern.sprite_base);
    if (!sprites_.contains(base)) {
      const std::array<ro::render::ActorPieceRequest, 1> request = {
          ro::render::ActorPieceRequest{ro::render::ActorPart::kBody, base}};
      auto charge = ro::render::LoadActorSprites(*vfs_, request);
      if (!charge || charge->empty()) {
        continue;
      }
      sprites_.emplace(base, std::move(charge->front()));
    }
    pool_.Add(ro::render::SowEffectFadingSpriteNode(
        pattern, ground_position, ro::render::EffectNodeLifeFor(effect_id), tick));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayCameraFans(std::uint32_t effect_id, glm::vec3 ground_position,
                                       std::int32_t tick) {
  bool sown = false;
  for (const ro::render::EffectCameraFanPattern& pattern :
       ro::render::EffectCameraFanPatternsFor(effect_id)) {
    // ⚠ CHAQUE NŒUD A SON TIC, et ils ne sont pas au même : le client en bâtit
    // un au premier tic et un autre au vingt et unième. Les semer ensemble
    // donnerait l'éventail entier d'un coup, au lieu de le voir s'ouvrir.
    if (!ro::render::EffectNodeScheduleSowsAt(pattern.schedule, tick)) {
      continue;
    }
    // ⚠ UNE IMAGE SUFFIT POUR SEMER : les trois traînées se peignent en trois
    // lots, et celle dont l'image manque se tait sans emporter les autres.
    if (TextureFor(std::string(pattern.textures.front())).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectCameraFanNode(pattern, ground_position,
                                                 ro::render::EffectNodeLifeFor(effect_id)));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlaySheathedBolts(std::uint32_t effect_id, glm::vec3 ground_position,
                                          std::int32_t tick,
                                          const std::optional<glm::vec3>& target_position) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectSheathedBoltPattern& pattern :
       ro::render::EffectSheathedBoltPatternsFor(effect_id)) {
    // ⚠ UN NŒUD PAR TIC, et non un seul : le client rebâtit le sien à chaque
    // tic tant que le tic de l'instance reste sous la borne du bâtisseur. N'en
    // semer qu'un montrerait un trait là où il en jaillit neuf.
    if (!ro::render::EffectNodeScheduleSowsAt(pattern.schedule, tick)) {
      continue;
    }
    if (TextureFor(std::string(pattern.texture)).empty()) {
      continue;
    }
    // 🔴 LE TRAIT VISE LA CIBLE DE L'EFFET : son cap et sa longueur se prennent
    // sur le vecteur qui y mène, depuis la place où l'effet sème à ce tic. Sans
    // cible, le vecteur nul donne un cap de zéro et une distance nulle — le
    // trait part vers l'est et ne doit sa longueur qu'à son tirage.
    const glm::vec3 toward_target =
        target_position.has_value() ? *target_position - ground_position : glm::vec3{0.0F};
    pool_.Add(ro::render::SowEffectSheathedBoltNode(pattern, ground_position, toward_target,
                                                    ro::render::EffectNodeLifeFor(effect_id),
                                                    generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayTriPlanes(std::uint32_t effect_id, glm::vec3 ground_position,
                                      std::int32_t tick) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectTriPlanePattern& pattern :
       ro::render::EffectTriPlanePatternsFor(effect_id)) {
    // ⚠ L'UN DES DEUX BÂTISSEURS ATTEND LE TRENTIÈME TIC. Le semer d'emblée
    // montrerait EF_CARTTER une seconde et demie trop tôt.
    if (!ro::render::EffectNodeScheduleSowsAt(pattern.schedule, tick)) {
      continue;
    }
    if (TextureFor(std::string(pattern.texture)).empty()) {
      continue;
    }
    // UN NŒUD PAR APPEL DU SITE : voir `EffectTriPlanePattern::copies`.
    for (std::uint16_t copie = 0; copie < pattern.copies; ++copie) {
      pool_.Add(ro::render::SowEffectTriPlaneNode(
          pattern, ground_position, ro::render::EffectNodeLifeFor(effect_id), generator));
    }
    sown = true;
  }
  return sown;
}

// JUSQU'À QUEL TIC UN EFFET SÈME ENCORE.
//
// 🔴 C'EST CE NOMBRE QUI DÉCIDE SI UNE HORLOGE SE POSE, et jusqu'où elle bat.
// Chaque famille de semis a son propre horaire, et aucune ne connaît celui des
// autres : l'horizon est le PLUS TARDIF de tous. En oublier une arrête
// l'horloge avant son dernier semis, et ce qui manque alors ne manque qu'à la
// fin. Son seul consommateur est `StepClocks`.
std::int32_t EffectNodesInPlay::LastSowTick(std::uint32_t effect_id,
                                            std::span<const ro::render::EffectNodePattern> patterns,
                                            glm::vec3 ground_position,
                                            const std::optional<glm::vec3>& target_position) const {
  std::int32_t last_tick = 0;
  for (const ro::render::EffectNodePattern& pattern : patterns) {
    last_tick = std::max(last_tick, ro::render::EffectNodePatternLastSowTick(pattern));
  }
  // ⚠ ET L'HORLOGE COUVRE AUSSI LES BOUFFÉES D'ÉTINCELLES. Ne la borner que sur
  // les patrons de nœud l'arrêterait avant la dernière bouffée, et le vert
  // s'éteindrait au milieu du soin.
  for (const ro::render::EffectRisingSparkPattern& pattern :
       ro::render::EffectRisingSparkPatternsFor(effect_id)) {
    last_tick = std::max(last_tick, ro::render::EffectNodeScheduleLastSowTick(pattern.schedule));
  }
  for (const ro::render::EffectSeededRingPattern& pattern :
       ro::render::EffectSeededRingPatternsFor(effect_id)) {
    last_tick = std::max(last_tick, ro::render::EffectNodeScheduleLastSowTick(pattern.schedule));
  }
  // ⚠ ET LES ANNEAUX DE NUAGE AUSSI, dont celui du portail attend le tic DEUX :
  // sans horloge, il ne serait jamais semé.
  for (const ro::render::EffectCloudRingPattern& pattern :
       ro::render::EffectCloudRingPatternsFor(effect_id)) {
    last_tick = std::max(last_tick, ro::render::EffectNodeScheduleLastSowTick(pattern.schedule));
  }
  // ⚠ ET LES ESSAIMS AUSSI, dont les trois bouffées s'étalent sur vingt tics.
  // Leur horaire est le même pour toute la famille : il ne dépend pas du patron,
  // mais il faut quand même qu'un patron existe pour que l'horloge se pose.
  for (const ro::render::EffectSparkTubePattern& pattern :
       ro::render::EffectSparkTubePatternsFor(effect_id)) {
    last_tick = std::max(last_tick, ro::render::EffectNodeScheduleLastSowTick(pattern.schedule));
  }
  for (const ro::render::EffectDriftingMotePattern& pattern :
       ro::render::EffectDriftingMotePatternsFor(effect_id)) {
    last_tick = std::max(last_tick, ro::render::EffectNodeScheduleLastSowTick(pattern.schedule));
  }
  for (const ro::render::EffectRadialPetalPattern& pattern :
       ro::render::EffectRadialPetalPatternsFor(effect_id)) {
    last_tick = std::max(last_tick, ro::render::EffectNodeScheduleLastSowTick(pattern.schedule));
  }
  for (const ro::render::EffectSheathedBoltPattern& pattern :
       ro::render::EffectSheathedBoltPatternsFor(effect_id)) {
    last_tick = std::max(last_tick, ro::render::EffectNodeScheduleLastSowTick(pattern.schedule));
  }
  // ⚠ ET L'ÉVENTAIL DE L'ASURA AUSSI, dont AUCUN des deux nœuds ne naît au tic
  // zéro : le premier attend le tic un, le second le vingt et unième.
  for (const ro::render::EffectCameraFanPattern& pattern :
       ro::render::EffectCameraFanPatternsFor(effect_id)) {
    last_tick = std::max(last_tick, ro::render::EffectNodeScheduleLastSowTick(pattern.schedule));
  }
  // ⚠ ET LES AILES DE L'ANGE, qui se resèment tous les trois tics jusqu'au
  // dix-huitième : sans horloge, l'ange n'en battrait qu'une fois.
  for (const ro::render::EffectFadingSpritePattern& pattern :
       ro::render::EffectFadingSpritePatternsFor(effect_id)) {
    last_tick = std::max(last_tick, ro::render::EffectNodeScheduleLastSowTick(pattern.schedule));
  }
  // ⚠ ET LES TROIS PLANS AUSSI, dont l'un attend le trentième tic.
  for (const ro::render::EffectTriPlanePattern& pattern :
       ro::render::EffectTriPlanePatternsFor(effect_id)) {
    last_tick = std::max(last_tick, ro::render::EffectNodeScheduleLastSowTick(pattern.schedule));
  }
  // ⚠ ET LA GERBE D'ANNEAUX AUSSI, dont AUCUN des deux nœuds ne naît au tic
  // zéro : le premier attend le tic un, le second le tic deux.
  last_tick = std::max(last_tick, ro::render::EffectRingStarburstLastSowTick(effect_id));
  // ⚠ ET LES MURS TRIPLES AUSSI, dont le vert attend le cinquième tic.
  for (const ro::render::EffectTripleWallPattern& pattern :
       ro::render::EffectTripleWallPatternsFor(effect_id)) {
    last_tick = std::max(last_tick, ro::render::EffectNodeScheduleLastSowTick(pattern.schedule));
  }
  // ⚠ ET LES ARCS QUI SERPENTENT AUSSI : leur bâtisseur sème aux QUATRE
  // premiers tics, pas au seul premier.
  for (const ro::render::EffectSnakingArcPattern& pattern :
       ro::render::EffectSnakingArcPatternsFor(effect_id)) {
    last_tick = std::max(last_tick, ro::render::EffectNodeScheduleLastSowTick(pattern.schedule));
  }
  // ⚠ ET LES GERBES, dont l'une sème jusqu'au quarantième tic.
  for (const ro::render::EffectAnchoredSkirtPattern& pattern :
       ro::render::EffectAnchoredSkirtPatternsFor(effect_id)) {
    last_tick = std::max(last_tick, ro::render::EffectNodeScheduleLastSowTick(pattern.schedule));
  }
  // ⚠ ET LES OBJETS LANCÉS, dont le second flacon d'EF_THROWITEM4 attend le
  // dixième tic. Sans horloge, aucun des deux ne partirait.
  for (const ro::render::EffectThrownItemPattern& pattern :
       ro::render::EffectThrownItemPatternsFor(effect_id)) {
    last_tick =
        std::max(last_tick, ro::render::EffectThrownItemBirthTick(pattern.builder_argument));
  }
  // ⚠ ET LES ÉCLAIRS, qui sèment aux quatre premiers tics.
  for (const ro::render::EffectLightningFanPattern& pattern :
       ro::render::EffectLightningFanPatternsFor(effect_id)) {
    last_tick = std::max(last_tick, ro::render::EffectNodeScheduleLastSowTick(pattern.schedule));
  }
  // ⚠ ET LE CARRÉ QUI FILE, dont les cinq objets s'étalent sur douze tics.
  if (!ro::render::EffectSwirlTrailPatternsFor(effect_id).empty()) {
    last_tick = std::max(
        last_tick, ro::render::EffectNodeScheduleLastSowTick(ro::render::kSwirlTrailSchedule));
  }
  // ⚠ ET LES ÉCLATS DE GEL, dont la seconde vague attend le dixième tic.
  if (!ro::render::EffectPaletteBillboardPatternsFor(effect_id).empty()) {
    last_tick =
        std::max(last_tick,
                 ro::render::EffectNodeScheduleLastSowTick(ro::render::kPaletteBillboardSchedule));
  }
  // ⚠ ET LA PLUIE DE GRAINS, qui sème tant que l'effet vit, jusqu'à mille tics.
  if (!ro::render::EffectRainParticlePatternsFor(effect_id).empty()) {
    last_tick = std::max(
        last_tick,
        std::min(ro::render::EffectNodeScheduleLastSowTick(ro::render::kRainParticleSchedule),
                 ro::render::EffectNodeLifeFor(effect_id)));
  }
  // ⚠ ET L'ANNEAU QUI S'ÉLARGIT, dont les quatre nœuds naissent aux quatre
  // premiers tics.
  if (!ro::render::EffectExpandingRingPatternsFor(effect_id).empty()) {
    last_tick = std::max(
        last_tick, ro::render::EffectNodeScheduleLastSowTick(ro::render::kExpandingRingSchedule));
  }
  // ⚠ ET LE TOURBILLON DE TETRA VORTEX, dont les quatre nœuds s'étalent sur
  // soixante-quinze tics.
  if (!ro::render::EffectTetraVortexPatternsFor(effect_id).empty()) {
    last_tick = std::max(
        last_tick, ro::render::EffectNodeScheduleLastSowTick(ro::render::kTetraVortexSchedule));
  }
  if (!ro::render::EffectSwarmPatternsFor(effect_id).empty()) {
    last_tick = std::max(
        last_tick, ro::render::EffectNodeScheduleLastSowTick(ro::render::kEffectSwarmSchedule));
  }
  // ⚠ UNE HORLOGE POUR RIEN COÛTERAIT À CHAQUE IMAGE : un effet dont tout naît
  // au premier tic n'a rien à attendre, et n'entre pas dans la liste.
  // ⚠ ET LES ÉVENTAILS DE SPRITES, qui sèment AUSSI au second tic.
  for (const ro::render::EffectSpriteFanPattern& pattern :
       ro::render::EffectSpriteFanPatternsFor(effect_id)) {
    static_cast<void>(pattern);
    last_tick = std::max(last_tick, ro::render::kSpriteFanLastSowTick);
  }

  // ⚠ ET LES ÉCLATS D'ÉCRAN, dont l'un attend le trentième tic. Ceux-ci ne
  // portent pas d'horaire mais un TIC UNIQUE : le bâtisseur du client teste
  // l'âge de l'effet par égalité, et sème une seule fois.
  for (const ro::render::EffectScreenBurstPattern& pattern :
       ro::render::EffectScreenBurstPatternsFor(effect_id)) {
    last_tick = std::max(last_tick, pattern.sow_tick);
  }
  // ⚠ ET LES MONSTRES, un par tic tant que leur style sème.
  for (const ro::render::EffectGlowSpriteNodePattern& pattern :
       ro::render::EffectGlowSpriteNodePatternsFor(effect_id)) {
    last_tick = std::max(last_tick, ro::render::EffectGlowSpriteSowingTicks(pattern.style) - 1);
  }
  // ⚠ ET LES COURONNES DE MAGNUM BREAK, qui ne commencent qu'au vingt-septième.
  if (!ro::render::EffectBreakRingPatternsFor(effect_id).empty()) {
    last_tick = std::max(last_tick,
                         ro::render::EffectNodeScheduleLastSowTick(ro::render::kBreakRingSchedule));
  }

  // ⚠ ET LES NŒUDS D'ÉCRAN, dont la moitié des bâtisseurs sème à RÉPÉTITION :
  // la poussière d'EF_ENDURE jusqu'au quarante-neuvième tic, le sable d'EF_SAND
  // jusqu'au cinquante-huitième.
  for (const ro::render::EffectScreenPattern& pattern :
       ro::render::EffectScreenPatternsFor(effect_id)) {
    last_tick = std::max(last_tick, pattern.birth_last_tick);
  }

  // 🔴 ET LE SEMEUR QUI MARCHE, dont l'horaire ne vit dans aucun patron : il
  // sème à chaque tic jusqu'à son arrivée, et c'est la CIBLE qui dit quand.
  // Sans cette ligne l'horloge ne se poserait pas du tout — le patron du trait
  // de Frost Diver dit « une seule fois » —, et le trait se réduirait à la
  // pointe de son premier pas.
  if (const ro::render::EffectMarchingSower* march = ro::render::EffectMarchingSowerFor(effect_id);
      march != nullptr && target_position.has_value()) {
    last_tick =
        std::max(last_tick,
                 ro::render::EffectMarchingSowerLastSowTick(
                     *march, ro::render::EffectGroundDistance(ground_position, *target_position)));
  }
  return last_tick;
}

void EffectNodesInPlay::FollowActor(std::uint32_t actor_id, glm::vec3 ground_position,
                                    float owner_facing_degrees,
                                    std::optional<float> owner_body_degrees) {
  if (actor_id == 0) {
    return;
  }
  for (EffectClock& clock : clocks_) {
    if (clock.attached_actor_id == actor_id) {
      clock.bearer_ground_position = ground_position;
      clock.owner_facing_degrees = owner_facing_degrees;
      // ⚠ SES SEMIS À VENIR NE SUIVENT QUE SI L'EFFET SUIT : un effet qui reste
      // où il est né sème jusqu'au bout à sa place de naissance, et un effet qui
      // ne rejoint son porteur qu'à un tic précis s'y recale à ce tic-là — voir
      // `StepClocks`.
      if (ro::render::EffectFollowsBearerStepByStep(
              ro::render::EffectBearerFollowOf(clock.effect_id), clock.age_ticks)) {
        clock.ground_position = ground_position;
      }
    }
  }
  // 🔴 ET LES NŒUDS DÉJÀ NÉS AVEC. L'horloge ne commande que les semis À VENIR :
  // un effet qui sème une fois puis vit dix mille tics n'a plus d'horloge du
  // tout, et restait planté là où son porteur se tenait à la seconde où il l'a
  // reçu. C'est ce que montrait l'anneau du classement PVP, abandonné derrière
  // le joueur au premier pas.
  pool_.MoveAnchored(actor_id, ground_position, owner_body_degrees);
  // ET CE QUI LE VISE : un trait tendu vers lui reprend sa place, comme le
  // client retrouve sa cible par son identifiant à chaque tic.
  pool_.MoveTargeted(actor_id, ground_position);
  // 🔴 ET LES NŒUDS D'ÉCRAN, SANS CONSULTER LA TABLE DE SUIVI : le client
  // reprojette la place de leur porteur à chaque tic, même pour un impact qui
  // reste où il est né dans le monde. La gerbe d'un Bash reçu par un monstre
  // qui marche suit le monstre, et ne reste pas à la case du coup.
  for (ScreenNodeInPlay& live : screen_nodes_) {
    if (live.owner_actor_id == actor_id) {
      live.owner_position = ground_position;
    }
  }
}

bool EffectNodesInPlay::HasClocksAttachedTo(std::uint32_t actor_id) const {
  if (actor_id == 0) {
    return false;
  }
  // ⚠ L'HORLOGE OU LE NŒUD, et la seconde moitié compte autant que la première :
  // l'horloge d'un effet permanent meurt au premier tic, son nœud lui survit de
  // dix mille. Ne demander que les horloges cessait de le faire suivre juste
  // après l'avoir posé. Un nœud d'écran compte aussi, comme la gerbe d'un coup.
  return std::ranges::any_of(clocks_,
                             [actor_id](const EffectClock& clock) {
                               return clock.attached_actor_id == actor_id;
                             }) ||
         pool_.HasAnchored(actor_id) || pool_.HasTargeted(actor_id) ||
         std::ranges::any_of(screen_nodes_, [actor_id](const ScreenNodeInPlay& live) {
           return live.owner_actor_id == actor_id;
         });
}

// L'HORAIRE LUI-MÊME : ce qui fait battre les horloges et sème ce que chacune
// doit à ce tic.
//
// ⚠ VENUE DE `effect_node_view.cpp` AU PLAFOND DE HUIT CENTS LIGNES, et la
// couture est celle que le nom de ce fichier annonçait : tout le reste ici est
// un semis COMMANDÉ PAR L'HORLOGE, et celle-ci est l'horloge.
void EffectNodesInPlay::StepClocks(bool single_frame) {
  if (clocks_.empty()) {
    return;
  }
  // ⚠ UNE IMAGE SANS TIC DÛ fait quand même battre une fois les instances qui
  // battent à chaque image : voir `effect_frame_beat.h`.
  if (single_frame && std::ranges::none_of(clocks_, [](const EffectClock& clock) {
        return ro::render::EffectInstanceBeatsEveryFrame(clock.effect_id);
      })) {
    return;
  }
  // Une copie, parce que semer AJOUTE à `clocks_` : un effet qui en lancerait
  // un autre invaliderait le parcours.
  std::vector<EffectClock> courantes = clocks_;
  clocks_.clear();
  for (EffectClock& clock : courantes) {
    if (single_frame && !ro::render::EffectInstanceBeatsEveryFrame(clock.effect_id)) {
      clocks_.push_back(clock);
      continue;
    }
    // Tout ce que cette horloge sème hérite de son porteur, comme le premier
    // semis. Par horloge et non par tic : elle ne change pas d'un tic à l'autre.
    // ⚠ ET SA PLACE COURANTE, non celle du premier semis : l'horloge suit son
    // porteur (`FollowActor`), et l'écart d'un nœud se prend contre le point où
    // l'ancre est AU MOMENT où il naît.
    const ro::render::EffectNodePool::AnchoredSowing anchor{
        pool_, clock.attached_actor_id, clock.ground_position, clock.effect_id, clock.from_hat};
    // ET SA MARQUE DE CHAPEAU, que ses nœuds d'écran prennent aussi.
    sowing_hat_ = clock.from_hat;
    // Et sa vie minimale, pour la même raison : un nœud semé au dernier tic
    // d'une incantation doit tenir aussi longtemps que celui du premier.
    pool_.LiveAtLeast(clock.life_floor_ticks);
    // Et la mort de son porteur, qui borne déjà son dernier tic.
    pool_.DieWithBearerAt(clock.bearer_death_tick);
    // ⚠ UNE HORLOGE COUPÉE VIEILLIT QUAND MÊME, et c'est ce qui la fait se
    // retirer à l'heure : la figer ferait ressemer tout son horaire depuis le
    // début au moment où le joueur rallume les effets.
    const bool shows = ShowsEffect(clock.effect_id);
    const ro::render::EffectBearerFollow follow = ro::render::EffectBearerFollowOf(clock.effect_id);
    // 🔴 LE BÂTISSEUR SÈME À L'ÂGE COURANT, PUIS L'INSTANCE VIEILLIT, comme le
    // client à chaque passage : semé au tic k, un nœud est mis à jour dans le
    // passage k + 1 compté depuis la naissance. Le tic zéro s'est semé à la
    // naissance ; le premier pas ne sème donc rien.
    const std::int32_t sow_tick = clock.age_ticks;
    ++clock.age_ticks;
    const bool sows = sow_tick > 0 && sow_tick <= clock.last_tick;
    if (sows) {
      // 🔴 LE RECALAGE PRÉCÈDE LE SEMIS DE SON TIC : l'effet reprend la place de
      // son porteur, PUIS sème — ce qui naît à ce tic naît sur le porteur, et
      // l'ancre des nouveaux nœuds est cette place-là.
      if (clock.attached_actor_id != 0 && ro::render::EffectRejoinsBearerAt(follow, sow_tick)) {
        clock.ground_position = clock.bearer_ground_position;
        pool_.AnchorNextTo(clock.attached_actor_id, clock.ground_position);
      }
      pool_.SowAtEffectTick(sow_tick);
    }
    if (shows && sows) {
      sowing_clock_ = &clock;
      PlaySeedsDueAt(ro::render::EffectNodePatternsFor(clock.effect_id), clock.ground_position,
                     sow_tick, clock.owner_facing_degrees, clock.target_position, clock.param,
                     clock.instance_scale, clock.instance_second);
      sowing_clock_ = nullptr;
      PlayRisingSparks(clock.effect_id, clock.ground_position, sow_tick);
      PlaySeededRings(clock.effect_id, clock.ground_position, sow_tick);
      PlayCloudRings(clock.effect_id, clock.ground_position, sow_tick);
      PlaySwarms(clock.effect_id, clock.ground_position, sow_tick);
      PlaySparkTubes(clock.effect_id, clock.ground_position, sow_tick, clock.target_position);
      PlayDriftingMotes(clock.effect_id, clock.ground_position, sow_tick);
      PlayRadialPetals(clock.effect_id, clock.ground_position, sow_tick);
      PlaySheathedBolts(clock.effect_id, clock.ground_position, sow_tick, clock.target_position);
      PlayCameraFans(clock.effect_id, clock.ground_position, sow_tick);
      PlayFadingSprites(clock.effect_id, clock.ground_position, sow_tick);
      PlayTriPlanes(clock.effect_id, clock.ground_position, sow_tick);
      PlayBreakRings(clock.effect_id, clock.ground_position, sow_tick);
      PlaySnakingArcs(clock.effect_id, clock.ground_position, sow_tick);
      PlayAnchoredSkirts(clock.effect_id, clock.ground_position, sow_tick);
      PlayLightningFans(clock.effect_id, clock.ground_position, sow_tick);
      PlayThrownItems(clock.effect_id, clock.ground_position, sow_tick, clock.target_position,
                      clock.param);
      PlayScreenBursts(clock.effect_id, clock.ground_position, sow_tick, clock.attached_actor_id);
      PlayScreenNodes(clock.effect_id, clock.ground_position, sow_tick, clock.attached_actor_id);
      PlaySpriteFans(clock.effect_id, clock.ground_position, sow_tick);
      PlayRingStarbursts(clock.effect_id, clock.ground_position, sow_tick);
      PlayTripleWalls(clock.effect_id, clock.ground_position, sow_tick);
      PlaySwirlTrails(clock.effect_id, clock.ground_position, sow_tick);
      PlayPaletteBillboards(clock.effect_id, clock.ground_position, sow_tick);
      PlayRainParticles(clock.effect_id, clock.ground_position, sow_tick);
      // Le client ne resème la bouffée blanche que si l'instance a un porteur.
      if (clock.attached_actor_id != 0) {
        PlayRisingParticles(clock.effect_id, clock.ground_position, sow_tick);
      }
      PlayExpandingRings(clock.effect_id, clock.ground_position, sow_tick);
      PlayTetraVortices(clock.effect_id, clock.ground_position, clock.target_position, sow_tick);
      PlaySownFigures(clock.effect_id, clock.ground_position, sow_tick, clock.instance_scale,
                      clock.instance_second);
    }
    pool_.LiveAtLeast(0);
    pool_.DieWithBearerAt(0);
    sowing_hat_ = false;
    if (clock.age_ticks <= clock.last_tick) {
      clocks_.push_back(clock);
    }
  }
}

}  // namespace moonclient
