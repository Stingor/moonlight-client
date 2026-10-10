// UNE ÉTAPE DE LA SUITE D'UNE COMPÉTENCE : l'effet qu'elle fait naître, où, sur
// qui, et le bruit qui part avec lui. `PlaySkillOn` parcourt la suite ; chaque
// étape qui pose un effet passe par ici.

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "ro/core/log.h"
#include "ro/render/actor_effect_table.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_launch.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_spawn_request.h"
#include "ro/render/effect_table.h"
#include "ro/render/effect_tethered_beam_seed.h"
#include "ro/render/skill_suite_table.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "effects/effect_class_view.h"
#include "effects/effect_node_view.h"
#include "effects/effect_str_look.h"
#include "effects/effect_view.h"
#include "world/map_stage.h"

namespace moonclient {

void EffectsInPlay::PlaySkillEffect(const ro::sim::World& world, const MapStage& stage,
                                    ro::sim::WalkPlanner& planner, const ro::sim::Actor& target,
                                    std::uint16_t skill_id, std::uint32_t caster_id,
                                    std::uint32_t started_ms, std::int32_t effect_param,
                                    std::int32_t skill_level, SkillMoment moment,
                                    const ro::render::SkillSuiteStep& step) {
  const std::uint32_t effect = step.effect_id;
  // LE RETARD D'UNE ÉTAPE QUI SUIT LA CADENCE DU LANCEUR : cent fois la durée
  // d'image de son action, plus cent — voir `SkillSuiteStep`.
  std::uint32_t delay_ms = step.start_delay_ms;
  if (const ro::sim::Actor* shooter = world.Find(caster_id);
      step.delay_from_caster_cadence && shooter != nullptr && actor_view_.pace_of) {
    if (const auto allure = actor_view_.pace_of(*shooter, skill_id, std::nullopt)) {
      delay_ms =
          static_cast<std::uint32_t>(ro::render::SkillSuiteCadenceDelayMs(allure->cadence_steps));
    }
  }
  // UNE CLASSE INSCRITE passe devant le fichier et le patron.
  if (SpawnSkillClass(classes_, world, stage, planner, target, caster_id, started_ms, step,
                      delay_ms, skill_level)) {
    return;
  }
  const std::string file = effect != 0 ? AskedEffectFile(effect, started_ms) : std::string();
  // LE PATRON DE NŒUDS ET LE FICHIER SE JOUENT TOUS LES DEUX, comme par `PlayById` : un bâtisseur
  // peut semer ses nœuds ET faire défiler son `.str`. Signum Crucis déroule sa croix et fait
  // jaillir, au trentième tic, la couronne et les éclats d'un coup de Bash ; ne jouer que le
  // fichier perdait la gerbe. Sans l'un ni l'autre, il n'y a rien à montrer.
  const bool played_by_nodes = EffectNodesInPlay::KnowsProcedural(effect);
  // UN EFFET QUI CHANGE L'ACTEUR ne se dessine pas, il se pose sur son porteur
  // — que la table d'acteur teint, fait clignoter ou masque —, mais il SONNE.
  if ((file.empty() && !played_by_nodes && ro::render::ActorEffectChangeFor(effect) != nullptr) ||
      effect == ro::render::kSpinningTopEffect) {
    const bool on_caster_only = step.carrier == ro::render::SkillSuiteCarrier::kCaster ||
                                step.carrier == ro::render::SkillSuiteCarrier::kCasterTowardTarget;
    if (const ro::sim::Actor* bearer = on_caster_only ? world.Find(caster_id) : &target) {
      const auto cell = ro::sim::InterpolatedCell(*bearer, planner.PathFor(*bearer), started_ms);
      actor_effects_to_record_.push_back({bearer->id, effect, started_ms});
      NoteLaunched(effect, GroundPositionAt(stage, cell.x, cell.y), bearer->id);
      // UNE DEMANDE DE BRUIT PAR INSTANCE : `Notice`, qui fait sonner celles
      // que le serveur annonce, tient celle-ci pour déjà entendue.
      per_map_.noticed.insert_or_assign(std::pair{bearer->id, effect}, started_ms);
    }
    return;
  }
  if (file.empty() && !played_by_nodes) {
    // ⚠ UN EFFET QUI NE SE MONTRE PAS LE DIT, UNE FOIS. « Je ne vois rien en
    // jeu » ne distingue pas une compétence sans effet relevé d'un effet relevé
    // qui ne se dessine pas, et les deux se corrigent à des endroits différents.
    if (std::ranges::find(reported_, effect) == reported_.end()) {
      reported_.push_back(effect);
      ro::core::LogWarning("compétence {} : effet {} de sa suite sans dessin", skill_id, effect);
    }
    return;
  }
  // SUR LE LANCEUR, SUR LA CIBLE, OU DANS LE MONDE PRÈS D'ELLE : c'est l'étape
  // qui le dit. Sonic Blow s'entaille sur l'assassin, Cold Bolt tombe sur ce
  // qu'il gèle, Tetra Vortex pose ses quatre éclats autour de sa cible.
  const ro::sim::Actor* caster = world.Find(caster_id);
  const auto ground_of = [&](ro::sim::FractionalCell cell) {
    const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
    return glm::vec3(ground[0], ground[1], ground[2]);
  };
  const bool on_caster = step.carrier == ro::render::SkillSuiteCarrier::kCaster ||
                         step.carrier == ro::render::SkillSuiteCarrier::kCasterTowardTarget ||
                         step.carrier == ro::render::SkillSuiteCarrier::kWorldAtCaster;
  // 🔴 LA SOURCE D'UN COUP N'EST PAS TOUJOURS UN ACTEUR. Pour une compétence
  // posée au sol, le serveur nomme l'UNITÉ qui frappe, et c'est de sa case que
  // l'effet naît : le blizzard de Storm Gust se lève là où le joueur a cliqué,
  // et non sur chacune de ses victimes. Faute de l'un et de l'autre, l'effet
  // revient sur la cible — mieux vaut le montrer au mauvais endroit que pas.
  std::optional<ro::sim::FractionalCell> from;
  if (on_caster) {
    if (caster != nullptr) {
      from = ro::sim::InterpolatedCell(*caster, planner.PathFor(*caster), started_ms);
    } else if (const auto skill_unit = world.skill_units().find(caster_id);
               skill_unit != world.skill_units().end()) {
      from = ro::sim::FractionalCell{static_cast<float>(skill_unit->second.cell.x),
                                     static_cast<float>(skill_unit->second.cell.y)};
    }
  }
  const ro::sim::FractionalCell here =
      ro::sim::InterpolatedCell(target, planner.PathFor(target), started_ms);
  const ro::sim::Actor& born = from.has_value() && caster != nullptr ? *caster : target;
  glm::vec3 place = ground_of(from.value_or(here));
  // DANS LE MONDE : le décalage relevé est dans le repère du client, dont la
  // profondeur est l'opposée de la nôtre.
  if (step.carrier == ro::render::SkillSuiteCarrier::kWorldNearTarget ||
      step.carrier == ro::render::SkillSuiteCarrier::kWorldAtCaster) {
    place += glm::vec3(step.offset_x, 0.0F, -step.offset_z);
  }
  // UNE ÉTAPE DISPERSÉE tire son point et son délai, ou reprend ceux de la
  // précédente.
  if (!step.reuse_previous_draw) {
    last_scatter_ = ro::render::SkillSuiteDrawScatter(step, aim_generator_);
  }
  const ro::render::SkillSuiteScatter dispersion =
      step.scatter_radius_max > 0 ? last_scatter_ : ro::render::SkillSuiteScatter{};
  place += dispersion.offset;
  // ⚠ ET D'OÙ L'EFFET EST NÉ SE DIT, une fois par compétence et par moment.
  // « Il se recopie sur chaque cible » ne distingue pas un effet que la table
  // met sur la cible d'un effet dont la SOURCE est introuvable, et les deux se
  // corrigent à des endroits opposés de la chaîne.
  if (reported_births_.insert({skill_id, static_cast<std::uint8_t>(moment)}).second) {
    // ⚠ « NÉ » PORTE SON ACCENT : sans lui, « ne » se lit comme la négation, et
    // la phrase dit le contraire de ce qui s'est passé.
    ro::core::LogDebug("compétence {} {} : effet {} né sur {} en ({:.1f}, {:.1f})", skill_id,
                       moment == SkillMoment::kImpact ? "au coup" : "à l'annonce", effect,
                       !from.has_value()   ? "LA CIBLE"
                       : caster != nullptr ? "le lanceur"
                                           : "l'unité au sol",
                       place.x, place.z);
  }
  // LE SECOND POINT, pour les figures qui tendent quelque chose entre deux
  // bouts. ⚠ C'EST CELUI QUI NE PORTE PAS L'EFFET : la cible quand il naît sur
  // le lanceur, le lanceur sinon. Donner deux fois le même point tendrait le
  // trait sur lui-même, et il est vide quand l'autre bout est introuvable.
  std::optional<glm::vec3> aim;
  if (from.has_value()) {
    aim = ground_of(here);
  } else if (caster != nullptr && caster != &target) {
    aim = ground_of(ro::sim::InterpolatedCell(*caster, planner.PathFor(*caster), started_ms));
  }
  // ⚠ SAUF QUAND L'ÉTAPE NOMME SON POINT : il peut être la place même où
  // l'instance naît.
  std::optional<glm::vec3> caster_position;
  if (caster != nullptr) {
    caster_position =
        ground_of(ro::sim::InterpolatedCell(*caster, planner.PathFor(*caster), started_ms));
  }
  aim = ro::render::SkillSuiteAimPoint(step, aim, ground_of(here), caster_position, aim_generator_);
  // 🔴 ET SON BRUIT PART AVEC LUI. Le client fait sonner l'INSTANCE d'effet —
  // son bâtisseur au tic zéro, sa mise à jour aux tics suivants.
  // ⚠ ET CELA NE DOUBLE RIEN : le son d'une COMPÉTENCE et celui de l'EFFET
  // qu'elle pose sont deux étages distincts du client.
  NoteLaunched(effect, {place.x, place.y, place.z}, born.id);
  // ⚠ SON DÉLAI SE DÉCOMPTE PAR TIC ENTIER : le client en retranche seize à
  // chaque passage, et l'instance ne vit qu'une fois le compte épuisé.
  constexpr auto kTickMs = static_cast<std::uint32_t>(ro::render::kEffectTickMs);
  const std::uint32_t lives_from_ms =
      started_ms + (delay_ms + dispersion.delay_ms + kTickMs - 1) / kTickMs * kTickMs;
  QueueUpdateQuakes(per_map_.pending_quakes, effect, lives_from_ms);
  if (played_by_nodes) {
    // ⚠ CONNAÎTRE UN EFFET N'EST PAS LE SEMER : une texture qui ne se lit pas
    // écarte son nœud en silence. ⚠ LE SUCCÈS SE DIT AUSSI, une fois par effet.
    // ⚠ ET L'ACTEUR QUE L'ÉTAPE VISE, quand elle en vise un : c'est sur lui qu'un
    // nœud fait naître l'effet qui suit — voir `effect_spawn_view.cpp`.
    // ⚠ CET IDENTIFIANT VOYAGE EN FLOTTANT jusqu'à l'instance, et c'est sous
    // cette forme qu'elle le cherche : voir `ro::render::StoredDesignatedActor`.
    const std::uint32_t named_id = step.aim == ro::render::SkillSuiteAim::kTarget   ? target.id
                                   : step.aim == ro::render::SkillSuiteAim::kCaster ? caster_id
                                                                                    : 0;
    const std::uint32_t designated_id =
        ro::render::DesignatedActorAfterStorage(named_id, world.player_id());
    // 🔴 ET LE BOUT LOINTAIN D'UN RAYON ATTACHÉ EST CET ACTEUR-LÀ, et non le
    // point que l'étape vise : le bâtisseur le cherche par l'identifiant relu,
    // et le rayon naît éteint s'il ne trouve personne. Le fichier de l'effet
    // garde, lui, le point visé.
    std::optional<glm::vec3> node_aim = aim;
    if (!ro::render::EffectTetheredBeamPatternsFor(effect).empty()) {
      const ro::sim::Actor* held = world.Find(designated_id);
      node_aim = held != nullptr ? std::optional<glm::vec3>(ground_of(ro::sim::InterpolatedCell(
                                       *held, planner.PathFor(*held), started_ms)))
                                 : std::nullopt;
    }
    // 🔴 ET L'ACTEUR SUR LEQUEL L'EFFET NAÎT, qui le PORTE : les nœuds d'écran
    // se reprojettent sur lui à chaque tic, et la gerbe de Bash suit la cible
    // qui marche. Une unité au sol n'est pas un acteur : rien ne la suit.
    const std::uint32_t bearer_id = !from.has_value()   ? target.id
                                    : caster != nullptr ? caster_id
                                                        : 0;
    const bool was_sown =
        nodes_->PlayProcedural(effect, place, node_aim,
                               ro::render::EffectOwnerFacingDegrees(
                                   ro::sim::FacingAt(born, planner.PathFor(born), started_ms)),
                               bearer_id, effect_param, 0, 0, 0.0F, designated_id);
    if (std::ranges::find(reported_, effect) == reported_.end()) {
      reported_.push_back(effect);
      if (was_sown) {
        ro::core::LogDebug("compétence {} : effet {} SEMÉ en ({:.1f}, {:.1f}, {:.1f})", skill_id,
                           effect, place.x, place.y, place.z);
      } else {
        ro::core::LogWarning(
            "compétence {} : effet {} EN TABLE MAIS RIEN SEMÉ (texture introuvable ?) en "
            "({:.1f}, {:.1f}, {:.1f})",
            skill_id, effect, place.x, place.y, place.z);
      }
    }
    if (file.empty()) {
      return;
    }
  }
  // 🔴 UN FICHIER NÉ SUR UN ACTEUR SE REPLACE SUR LUI À CHAQUE TIC : Provoke
  // suit le monstre qui marche. ⚠ Sauf posé À CÔTÉ — décalage dans le monde,
  // point tiré — : le suivre perdrait l'écart qui le place.
  const bool placed_beside = step.carrier == ro::render::SkillSuiteCarrier::kWorldNearTarget ||
                             step.carrier == ro::render::SkillSuiteCarrier::kWorldAtCaster ||
                             step.scatter_radius_max > 0;
  const std::uint32_t file_bearer_id = placed_beside       ? 0
                                       : !from.has_value() ? target.id
                                       : caster != nullptr ? caster_id
                                                           : 0;
  if (!Play(std::string(ro::render::kEffectFolder) + file, place, lives_from_ms, file_bearer_id)) {
    return;
  }
  // CE QUE L'ÉTAPE FAIT DE L'IMAGE : une échelle, un angle tiré.
  const StrLook look{.spin_degrees = step.spin_drawn
                                         ? ro::render::SkillSuiteDrawSpinDegrees(
                                               ro::render::NextEffectNodeDraw(aim_generator_))
                                         : 0.0F,
                     .scale = step.scale};
  QueueQuakes(
      per_map_.pending_quakes,
      ShapeStrPlaying(per_map_.playing.back(), effect, file_bearer_id,
                      StrTowardTargetDegrees(effect, camera_ ? &*camera_ : nullptr, place, aim)));
  LookStrPlaying(per_map_.playing.back(), effect, look, camera_ ? camera_->yaw_degrees : 0.0F);
}

}  // namespace moonclient
