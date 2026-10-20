#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>

#include "ro/render/effect_frame.h"
#include "ro/render/effect_node.h"
#include "ro/render/ground_skill_visual.h"
#include "ro/render/map_effect_math.h"
#include "ro/render/monster_skill_motion.h"
#include "ro/render/skill_impl.h"
#include "ro/render/skill_named_table.h"
#include "ro/sim/movement.h"

#include "effects/effect_arrow_shower.h"
#include "effects/effect_named_sounds.h"
#include "effects/effect_node_view.h"
#include "effects/effect_view.h"
#include "world/hit_popup_rules.h"

// CE QUE MONTRE UNE COMPÉTENCE POSÉE SUR UNE CASE : le chemin du paquet, du
// répartiteur du lanceur jusqu'au registre des effets nommés.
//
// À part de `effect_skill_view.cpp`, qui touchait son plafond de lignes, et
// parce que ce chemin a DEUX entrées : un lanceur en vue, et un lanceur hors de
// vue dont la case se montre quand même.

namespace moonclient {

void EffectsInPlay::PlayGroundSkillPacket(const MapStage& stage, ro::sim::WalkPlanner& planner,
                                          const ro::sim::Actor* caster, std::uint32_t caster_id,
                                          const ro::sim::GroundSkillUse& use) {
  // LA VOLÉE D'ARROW SHOWER au lancer : une flèche qui monte au-dessus du
  // lanceur, puis huit vers les cases voisines de la case visée. Sans lanceur,
  // le client n'en tire aucune.
  if (use.skill_id == kArrowShowerSkill && caster != nullptr) {
    const ro::sim::FractionalCell caster_cell =
        ro::sim::InterpolatedCell(*caster, planner.PathFor(*caster), use.started_ms);
    const std::array<float, 3> caster_feet = GroundPositionAt(stage, caster_cell.x, caster_cell.y);
    const glm::vec3 caster_origin(caster_feet[0], caster_feet[1], caster_feet[2]);
    QueueArrowShower(per_map_.shower_arrows, stage, caster_id, caster_origin, caster_origin,
                     static_cast<float>(use.cell.x), static_cast<float>(use.cell.y),
                     use.started_ms);
  }
  // 🔴 DANS L'ORDRE DU CLIENT, ET SANS QU'UN CHEMIN EXCLUE L'AUTRE : le
  // message au lanceur (le répartiteur), puis les gestes du gestionnaire
  // du paquet, puis — s'il ne sort pas avant — le registre des effets
  // nommés. Voir `PlanGroundSkillPacket`.
  //
  // Sur la CASE, quoi que la compétence dise de sa cible : c'est
  // l'endroit visé qui reçoit, pas quelqu'un.
  //
  // 🔴 ET LE SON SE DEMANDE, LUI AUSSI. Poser le fichier sans `NoteLaunched`
  // laissait un sort de zone se voir sans s'entendre : le vent de Storm
  // Gust est le son de son EFFET, joué une fois à la pose, et personne ne
  // le réclamait.
  //
  // ⚠ LE FICHIER ET LES NŒUDS, parce que le bâtisseur monte les DEUX. Celui
  // de Storm Gust fait défiler son `.str` ET sème ses cristaux
  // de glace, et `PlayById` est le seul chemin qui joue les deux.
  //
  // 🔴 ET CE N'EST PAS TOUJOURS LA CASE QUI PORTE. Le client pose l'effet
  // par un LANCEUR sans corps qui le porte et le coupe en mourant — Ice
  // Wall déclare dix mille tics et quitte la case en six secondes —, sur
  // la case sans porteur, ou sur le lanceur du sort. Il en pose parfois
  // plusieurs d'un coup, et en pose même pour des compétences que nos
  // tables fondues ne connaissent pas.
  const ro::render::GroundSkillPacketCaster packet_caster =
      caster == nullptr ? ro::render::GroundSkillPacketCaster::kUnseen
      : ro::render::IsMonsterOrNpcClass(caster->look.job)
          ? ro::render::GroundSkillPacketCaster::kCreature
          : ro::render::GroundSkillPacketCaster::kPlayer;
  const ro::render::GroundSkillPacketPlan plan = ro::render::PlanGroundSkillPacket(
      ro::render::GeneratedGroundSkillPacketTables(), use.skill_id,
      ro::render::SkillImplFor(use.skill_id).EffectId(), use.level, minimal_effects_, packet_caster,
      !ro::render::SkillNamedEffects(use.skill_id, ro::render::kSkillNamedOnGround).empty());
  const std::array<float, 3> ground = GroundPositionAt(stage, use.cell.x, use.cell.y);
  const glm::vec3 aimed_cell(ground[0], ground[1], ground[2]);
  // LE LANCEUR : ses pieds, et l'angle d'Overbrand — la caméra plus son regard,
  // que le paquet vient de tourner vers la case. Rien à lire sans lui : le plan
  // n'a alors aucune étape qui s'en serve.
  std::array<float, 3> caster_feet = ground;
  float angle = 0.0F;
  if (caster != nullptr) {
    const ro::sim::FractionalCell caster_cell =
        ro::sim::InterpolatedCell(*caster, planner.PathFor(*caster), use.started_ms);
    caster_feet = GroundPositionAt(stage, caster_cell.x, caster_cell.y);
    const float body_degrees = ClientBodyDegrees(
        *caster, ro::sim::FacingAt(*caster, planner.PathFor(*caster), use.started_ms));
    angle = ro::render::GroundSkillViewAngleDegrees(camera_ ? camera_->yaw_degrees : 0.0F,
                                                    body_degrees);
  }
  // LES TIRAGES DU RAYON, un par rang, venus de l'instant comme les autres
  // tirages du client : un paquet rejoué se rejoue à l'identique.
  std::uint32_t draw_state = use.started_ms;
  std::map<std::uint8_t, float> drawn_radii;
  const ro::render::MapEffectFrame client_frame{stage.scene.gnd.width(), stage.scene.gnd.height()};
  for (const ro::render::GroundSkillPacketStep& step : plan.steps) {
    if (!ro::render::GroundSkillStepFitsViewAngle(step, angle)) {
      continue;
    }
    switch (step.gesture) {
      case ro::render::GroundSkillPacketGesture::kSound: {
        // À LA POSITION POUSSÉE, qui n'est pas la case : voir `GroundSkillSound`.
        const glm::vec3 point = client_frame.ToWorld(glm::vec3(step.sound_client_position[0],
                                                               step.sound_client_position[1],
                                                               step.sound_client_position[2]));
        SownNamedSound heard{step.sound, use.cell};
        heard.point = std::array<float, 3>{point.x, point.y, point.z};
        heard.hearing_range = step.hearing_range;
        heard.near_distance = step.near_distance;
        heard.volume = step.volume;
        per_map_.just_sown_named_sounds.push_back(heard);
        continue;
      }
      case ro::render::GroundSkillPacketGesture::kScreenQuake:
        screen_quake_until_ms_ =
            std::max(screen_quake_until_ms_.value_or(0U), use.started_ms + step.quake_ms);
        continue;
      case ro::render::GroundSkillPacketGesture::kOnCaster: {
        // LA CASE POUR POINT VISÉ, quand le client la lui donne.
        const std::size_t files_before = per_map_.playing.size();
        PlayById(step.effect_id, glm::vec3(caster_feet[0], caster_feet[1], caster_feet[2]),
                 use.started_ms, caster_id,
                 /*endless=*/false, /*life_floor_ticks=*/0, /*announce_sound=*/true,
                 /*life_exact_ticks=*/0, /*instance_scale=*/0.0F, /*instance_second=*/0.0F,
                 step.aimed_at_cell ? std::optional<glm::vec3>(aimed_cell) : std::nullopt);
        if (per_map_.playing.size() > files_before && step.str_scale != 1.0F) {
          per_map_.playing.back().scale = step.str_scale;
        }
        continue;
      }
      case ro::render::GroundSkillPacketGesture::kOnCell:
      case ro::render::GroundSkillPacketGesture::kLauncher:
        break;
    }
    // La hauteur se donne dans le repère du client, dont l'axe vertical
    // descend : elle change de signe ici. L'écart autour de la case aussi,
    // en profondeur.
    glm::vec3 spot(ground[0], ground[1] - step.client_height_offset, ground[2]);
    if (step.ring_draw_modulo > 0) {
      auto [radius, fresh] = drawn_radii.try_emplace(step.ring_draw_rank, 0.0F);
      if (fresh) {
        radius->second = static_cast<float>(ro::render::NextEffectNodeDraw(draw_state) %
                                            static_cast<std::uint32_t>(step.ring_draw_modulo)) +
                         step.ring_draw_base;
      }
      spot.x += step.client_ring_direction[0] * radius->second;
      spot.z -= step.client_ring_direction[1] * radius->second;
    }
    const std::uint32_t bearer_death_ms = ro::render::GroundSkillStepLauncherCarries(step)
                                              ? static_cast<std::uint32_t>(step.launcher_life_ms)
                                              : 0U;
    // Le porteur meurt pour les nœuds comme pour le fichier : la troupe
    // le lit au semis, le fichier le garde avec lui.
    nodes_->DieWithBearerAtNext(ro::render::BearerDeathTick(bearer_death_ms));
    const std::size_t files_before = per_map_.playing.size();
    PlayById(step.effect_id, spot, use.started_ms);
    nodes_->DieWithBearerAtNext(0);
    if (per_map_.playing.size() > files_before) {
      EffectMapMemory::Playing& played = per_map_.playing.back();
      played.bearer_dies_after_ms = bearer_death_ms;
      // LE DÉPART DU FICHIER, que les commandes du client déplacent : un
      // délai le retarde, une part sautée le fait naître plus loin.
      played.started_ms += static_cast<std::uint32_t>(step.str_start_delay_ms);
      played.started_ms -= static_cast<std::uint32_t>(
          ro::render::StrSkippedFrames(played.effect->prepared.last_frame, step.str_skipped_share) *
          ro::render::kEffectTickMs);
      if (step.str_scale != 1.0F) {
        played.scale = step.str_scale;
      }
    }
  }
  if (!plan.reaches_registry) {
    return;
  }
  // ⚠ LE CHEMIN NOMMÉ SÈME SUR LA CASE VISÉE, et non sous le lanceur : ce
  // slot est le seul dont les effets soient ancrés à une POSITION — dix-
  // huit sur dix-neuf —, là où tous les autres le sont à l'acteur. C'est
  // ce qui distingue Wild Fire d'un effet porté.
  // ⚠ ET CERTAINS NAISSENT SUR LE LANCEUR, tournés vers la case : la
  // table le dit entrée par entrée. Sans lanceur, ceux-là ne naissent pas.
  NamedSkillMoment moment;
  moment.skill_id = use.skill_id;
  moment.slot = ro::render::kSkillNamedOnGround;
  moment.source = caster;
  moment.target_cell = use.cell;
  moment.level = use.level;
  moment.started_ms = use.started_ms;
  static_cast<void>(PlayNamedMoment(stage, planner, moment));
}

}  // namespace moonclient
