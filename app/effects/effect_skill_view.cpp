#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <map>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "ro/core/log.h"
#include "ro/render/actor_effect_table.h"
#include "ro/render/auto_counter.h"
#include "ro/render/effect_class.h"
#include "ro/render/effect_class_registry.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_launch.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_table.h"
#include "ro/render/floating_number.h"
#include "ro/render/ground_skill_visual.h"
#include "ro/render/led_pose.h"
#include "ro/render/map_effect_math.h"
#include "ro/render/monster_skill_motion.h"
#include "ro/render/option_effect_table.h"
#include "ro/render/pvp_rank_tint.h"
#include "ro/render/skill_cast_extra.h"
#include "ro/render/skill_effect_table.h"
#include "ro/render/skill_hit_effect.h"
#include "ro/render/skill_lua_effect.h"
#include "ro/render/skill_unit_pose.h"
#include "ro/render/skill_unit_primer.h"
#include "ro/render/skill_use_motion.h"
#include "ro/render/sprite_billboard.h"
#include "ro/sim/ground_skill_log.h"
#include "ro/sim/inventory.h"
#include "ro/sim/movement.h"
#include "ro/sim/status_effects.h"
#include "ro/sim/strike_log.h"
#include "ro/sim/world.h"

#include "effects/effect_node_view.h"
#include "effects/effect_param.h"
#include "effects/effect_requests.h"
#include "effects/effect_str_look.h"
#include "effects/effect_view.h"
#include "world/actor_view.h"
#include "world/hit_popup_rules.h"
#include "world/struck_place.h"

// CE QU'UNE COMPÉTENCE MONTRE, des trois côtés à la fois : l'unité qu'elle pose
// au sol, l'INCANTATION sur son lanceur, et la case qu'elle vise.
//
// ⚠ TROIS CHEMINS ET NON UN, et c'est le client qui les sépare : l'annonce, le
// coup et l'unité au sol passent par des répartiteurs différents. Les fondre
// rejouerait l'effet de la compétence sur chaque victime.
//
// Sorti de `effect_view.cpp`, qui touchait son plafond de lignes. La frontière
// est celle que la classe emploie déjà — voir `effect_named.cpp`,
// `effect_hit_view.cpp`, `effect_looping.cpp`.

namespace moonclient {
namespace {

// DE COMBIEN DE TICS UN TOUR DU CERCLE D'INCANTATION MORD SUR LE SUIVANT.
//
// Relancer pile à l'extinction laisserait une image de sol nu entre deux tours,
// et l'œil la voit comme un clignotement. C'est le même principe que le retrait
// des effets d'état — voir `PeriodMs`.
constexpr std::uint8_t kCastRingOverlapTicks = 6;

// L'EFFET qu'une incantation interrompue retire en plus, sur un autre que soi.
constexpr std::uint32_t kInterruptedCastMarkEffect = 1114;

}  // namespace

void EffectsInPlay::PlaySkillOn(const ro::sim::World& world, const MapStage& stage,
                                ro::sim::WalkPlanner& planner, const ro::sim::Actor& target,
                                std::uint16_t skill_id, std::uint32_t caster_id,
                                std::uint32_t started_ms, std::int32_t effect_param,
                                SkillMoment moment, std::int32_t argument,
                                std::uint8_t damage_type) {
  // 🔴 TOUTE LA SUITE, et non son premier effet : le client fait naître chacun
  // de ceux que la compétence nomme à ce moment. Une étape qui dépend de
  // l'argument ne joue que dans son intervalle — Heal choisit son effet selon
  // ce qu'il soigne.
  const ro::render::SkillSuiteMoment moment_de_la_suite =
      moment == SkillMoment::kImpact ? ro::render::SkillSuiteMoment::kImpact
                                     : ro::render::SkillSuiteMoment::kAnnounced;
  const std::span<const ro::render::SkillSuiteStep> suite =
      ro::render::SkillSuiteOf(skill_id, moment_de_la_suite);
  // LES QUESTIONS D'ABORD, toutes : semer une étape ne doit pas changer la
  // réponse des suivantes.
  std::vector<bool> retenues;
  retenues.reserve(suite.size());
  for (const ro::render::SkillSuiteStep& step : suite) {
    retenues.push_back(
        ro::render::SkillSuiteStepPlays(step, argument) &&
        ro::render::SkillSuiteStepPlaysForCaster(step, caster_id == world.player_id()) &&
        ro::render::SkillSuiteStepFitsDamageType(step, damage_type) &&
        ro::render::SkillSuiteStepShows(step, minimal_effects_) &&
        SkillSuiteConditionHolds(this, step, caster_id, target.id));
  }
  bool played = false;
  for (std::size_t rang = 0; rang < suite.size(); ++rang) {
    const ro::render::SkillSuiteStep& step = suite[rang];
    if (!retenues[rang]) {
      continue;
    }
    if (step.kind == ro::render::SkillSuiteKind::kScreenQuake) {
      screen_quake_until_ms_ =
          std::max(screen_quake_until_ms_.value_or(0U), started_ms + step.quake_ms);
      continue;
    }
    if (step.kind == ro::render::SkillSuiteKind::kRemoveEffect) {
      // Dans l'ordre de la suite : le retrait précède l'effet neuf qu'il
      // remplace.
      StopById(step.effect_id,
               step.carrier == ro::render::SkillSuiteCarrier::kCaster ? caster_id : target.id);
      continue;
    }
    if (step.kind == ro::render::SkillSuiteKind::kSmokePuff) {
      const ro::sim::Actor* bearer =
          step.carrier == ro::render::SkillSuiteCarrier::kCaster ? world.Find(caster_id) : &target;
      if (bearer != nullptr) {
        const ro::sim::FractionalCell here =
            ro::sim::InterpolatedCell(*bearer, planner.PathFor(*bearer), started_ms);
        const std::array<float, 3> pieds = GroundPositionAt(stage, here.x, here.y);
        static_cast<void>(nodes_->PlayActOnce(ro::render::kSmokePuffSprite,
                                              glm::vec3(pieds[0], pieds[1], pieds[2]),
                                              ro::render::kSmokePuffFrameSteps));
      }
      continue;
    }
    if (step.kind != ro::render::SkillSuiteKind::kEffect) {
      continue;
    }
    played = true;
    // ⚠ LE PARAMÈTRE NE VA QU'À L'INSTANCE QUI LE REÇOIT chez le client ; les
    // autres gardent un.
    // L'ÉCART D'UNE GRILLE QUI S'ÉLARGIT AVEC L'ARGUMENT se résout ici.
    ro::render::SkillSuiteStep etape = step;
    const glm::vec2 offset = ro::render::SkillSuiteOffsetFor(step, argument);
    etape.offset_x = offset.x;
    etape.offset_z = offset.y;
    // SUR LE JOUEUR DE LA CAMÉRA, l'étape se joue comme sur un lanceur qui
    // serait lui. Sans personnage à regarder, elle n'a pas où naître.
    std::uint32_t bearer_id = caster_id;
    if (step.carrier == ro::render::SkillSuiteCarrier::kLocalPlayer) {
      if (world.player() == nullptr) {
        continue;
      }
      bearer_id = world.player_id();
      etape.carrier = ro::render::SkillSuiteCarrier::kCaster;
    }
    // ⚠ ET LE NIVEAU QUE LE PAQUET ANNONCE VA À PART : une classe qui le reçoit
    // le lit tel quel, là où le paramètre d'instance ne va qu'aux nœuds qui le
    // reçoivent.
    PlaySkillEffect(world, stage, planner, target, skill_id, bearer_id, started_ms,
                    step.receives_param ? effect_param : kRawEffectParam, argument, moment, etape);
    // CELUI QUI DESSINE EN TEIGNANT teint aussi son porteur — voir `AlsoChangeBearer`.
    if (step.carrier == ro::render::SkillSuiteCarrier::kCaster ||
        step.carrier == ro::render::SkillSuiteCarrier::kCasterTowardTarget) {
      AlsoChangeBearer(step.effect_id, caster_id, started_ms, false);
    } else if (step.carrier == ro::render::SkillSuiteCarrier::kTarget) {
      AlsoChangeBearer(step.effect_id, target.id, started_ms, false);
    }
  }
  if (!played) {
    // ⚠ CE RETOUR SORTAIT EN SILENCE, et c'est ce qui a rendu Bash impossible à
    // juger : « je ne vois rien » ne distingue pas une compétence sans suite
    // d'un paquet qui n'arrive jamais, et les deux se corrigent à des endroits
    // opposés — un relevé d'un côté, le réseau de l'autre.
    const auto key = std::pair{skill_id, static_cast<std::uint8_t>(moment)};
    if (std::ranges::find(reported_without_table_, key) == reported_without_table_.end()) {
      reported_without_table_.push_back(key);
      ro::core::LogDebug("compétence {} : aucun effet dans sa suite {}", skill_id,
                         moment == SkillMoment::kImpact ? "au coup" : "à l'annonce");
    }
  }
}

void EffectsInPlay::NoticeSkills(const ro::sim::World& world, const MapStage& stage,
                                 ro::sim::WalkPlanner& planner, std::uint32_t now_ms) {
  // 🔴 CE QU'UN ÉTAT FAIT NAÎTRE SUR SON PORTEUR EN SE POSANT, à la motion même
  // — voir `ro::render::StateEffectsOf` : l'incantation, l'emploi et le coup
  // posent leur état par la même motion. Seul un corps de joueur y passe, et ni
  // un mort ni qui fait le mort : leur motion ne change pas.
  const auto play_state_effects = [&](const ro::sim::Actor& bearer, std::int8_t action_state,
                                      std::uint16_t skill_id, std::uint32_t posed_ms,
                                      SkillMoment moment) {
    if (!LookOf(bearer).player || bearer.posture == ro::sim::Posture::kDead ||
        bearer.status_effects.contains(ro::sim::kEfstTrickDead)) {
      return;
    }
    for (const ro::render::StateEffectStep& step :
         ro::render::StateEffectsOf(action_state, ShownJobOf(bearer))) {
      if (step.kind == ro::render::StateEffectKind::kRemove) {
        StopById(step.effect_id, bearer.id);
        // UN EFFET QUI CHANGE SON PORTEUR vit sur lui, et non en instance : il
        // s'en retire aussi, avant que le suivant ne s'y pose.
        if (ro::render::ActorEffectChangeFor(step.effect_id) != nullptr) {
          actor_effects_to_record_.push_back({.actor_id = bearer.id,
                                              .effect_id = step.effect_id,
                                              .started_ms = posed_ms,
                                              .posed_by_state = std::nullopt,
                                              .withdrawn = true});
        }
        continue;
      }
      // Sur le porteur même, sans décalage.
      ro::render::SkillSuiteStep on_bearer;
      on_bearer.skill_id = skill_id;
      on_bearer.carrier = ro::render::SkillSuiteCarrier::kCaster;
      on_bearer.effect_id = step.effect_id;
      PlaySkillEffect(world, stage, planner, bearer, skill_id, bearer.id, posed_ms, kRawEffectParam,
                      0, moment, on_bearer);
    }
  };
  // Les UNITÉS posées au sol d'abord : un piège, un mur, une zone. Leur effet
  // se range sous l'identifiant d'UNITÉ et non sous la compétence, et il
  // ACCOMPAGNE le modèle au lieu de le remplacer — c'est la bouffée qui manque
  // aux pièges, et la seule image que Pneuma ait jamais eue.
  //
  // À la POSE seulement. Une unité vit bien plus longtemps que son effet, et le
  // rejouer à chaque image ferait un clignotement continu.
  for (const auto& [unit_id, unit] : world.skill_units()) {
    const auto seen = per_map_.placed.find(unit_id);
    if (seen != per_map_.placed.end() && seen->second == unit.placed_ms) {
      continue;
    }
    per_map_.placed.insert_or_assign(unit_id, unit.placed_ms);
    const std::array<float, 3> sol = GroundPositionAt(stage, unit.cell.x, unit.cell.y);
    // TOUTE SA SUITE, dans l'ordre : chaque effet naît sur l'unité, qui le
    // porte sous son identifiant — c'est ce qui permet au retrait de le viser.
    const std::span<const ro::render::SkillUnitPoseStep> suite =
        ro::render::SkillUnitPoseOf(unit.unit_id);
    for (const ro::render::SkillUnitPoseStep& step : suite) {
      if (!ro::render::SkillUnitPoseStepPlays(step, unit.skill_level) ||
          (step.unless_carried && Carries(step.effect_id, unit_id))) {
        continue;
      }
      if (step.kind == ro::render::SkillUnitPoseKind::kRemoveEffect) {
        StopById(step.effect_id, unit_id);
      } else if (ro::render::IsPortedEffectClass(step.effect_id)) {
        // UNE CLASSE D'UNITÉ reçoit le NIVEAU de la compétence en paramètre.
        static_cast<void>(classes_.Spawn({.effect_id = step.effect_id,
                                          .owner_id = unit_id,
                                          .ground_position = glm::vec3(sol[0], sol[1], sol[2]),
                                          .level = unit.skill_level},
                                         unit.placed_ms));
      } else if (const std::size_t before = per_map_.playing.size();
                 PlayById(step.effect_id, glm::vec3(sol[0], sol[1], sol[2]), unit.placed_ms,
                          unit_id) &&
                 per_map_.playing.size() > before) {
        // SON FICHIER reçoit la rotation de l'étape et le niveau de l'unité ;
        // `PlayById` y a déjà posé ce que le lecteur fait de plus.
        const StrLook regard{.spin_degrees = step.spin_degrees,
                             .spin_follows_view = step.spin_anchored_to_ground,
                             .level = unit.skill_level};
        LookStrPlaying(per_map_.playing.back(), step.effect_id, regard,
                       camera_ ? camera_->yaw_degrees : 0.0F);
      }
    }
    if (!suite.empty()) {
      continue;
    }
    const std::string file =
        AskedEffectFile(ro::render::SkillUnitEffect(unit.unit_id), unit.placed_ms);
    if (file.empty()) {
      continue;
    }
    Play(std::string(ro::render::kEffectFolder) + file, glm::vec3(sol[0], sol[1], sol[2]),
         unit.placed_ms);
  }
  // L'AMORCE D'UN PIÈGE QUI SE DÉCLENCHE : quand l'unité prend l'apparence du
  // piège usé, elle joue l'effet que son apparence de pose lui a fait retenir —
  // la glace d'un Freezing Trap, l'éclair d'un Flasher. Une fois par changement.
  for (const auto& [unit_id, unit] : world.skill_units()) {
    if (unit.look_since_ms == unit.placed_ms) {
      continue;
    }
    const auto seen = per_map_.looked.find(unit_id);
    if (seen != per_map_.looked.end() && seen->second == unit.look_since_ms) {
      continue;
    }
    per_map_.looked.insert_or_assign(unit_id, unit.look_since_ms);
    const std::uint16_t primer =
        ro::render::SkillUnitPrimerOnLook(unit.placed_unit_id, unit.skill_level, unit.unit_id);
    if (primer == 0) {
      continue;
    }
    // UNE CLASSE naît libre, sur la case ; un PRIMITIF naît sur l'unité, qui
    // l'emporte en quittant le monde.
    const std::array<float, 3> ground = GroundPositionAt(stage, unit.cell.x, unit.cell.y);
    const glm::vec3 position(ground[0], ground[1], ground[2]);
    const std::uint32_t owner_id = ro::render::ClientRegistersEffectClass(primer) ? 0 : unit_id;
    static_cast<void>(PlayById(primer, position, unit.look_since_ms, owner_id, false, 0, true, 0,
                               0.0F, 0.0F, position));
  }
  // 🔴 LE LANCEUR QUI BRONCHE ÉTEINT LE RÉTICULE POSÉ SUR SA CIBLE — voir
  // `EffectMapMemory::lock_on_reticles`. Un coup qui le frappe en pleine
  // incantation, Spell Breaker qui lui prend ses PV : le cercle sous la cible
  // disparaît à l'instant, au lieu de vivre la durée annoncée.
  std::erase_if(per_map_.lock_on_reticles, [&](const auto& entry) {
    const ro::sim::Actor* caster = world.Find(entry.first);
    if (caster == nullptr || now_ms >= entry.second.ends_ms) {
      return true;
    }
    if (!actor_view_.flinches_at || !actor_view_.flinches_at(*caster, now_ms)) {
      return false;
    }
    StopById(ro::render::kLockOnReticleEffect, entry.second.target_id);
    return true;
  });
  for (const auto& [id, actor] : world.actors()) {
    // L'INCANTATION : l'effet qui naît sur le lanceur pendant qu'il prépare son
    // sort, avant tout dégât. C'est un chemin à part — le client le résout en
    // même temps que la pose — et une compétence en a souvent un sans avoir
    // celui des dégâts.
    //
    // L'ÉLÉMENT sert ici, et nulle part ailleurs : quand la table rend l'effet
    // générique, c'est l'élément annoncé qui dit quelle aura tourne sous les
    // pieds du lanceur.
    if (actor.cast.has_value()) {
      const ro::sim::Cast& cast = *actor.cast;
      const auto seen = per_map_.casting.find(id);
      if (seen == per_map_.casting.end() || seen->second != cast.started_ms) {
        per_map_.casting.insert_or_assign(id, cast.started_ms);
        // L'incantation a elle aussi son chemin nommé, et il exclut l'autre pour
        // la même raison : le client le prend au défaut de son répartiteur.
        //
        // ⚠ DEUX SLOTS, ET LE GROS EST DANS LE SECOND. Le visuel d'incantation
        // joue l'un directement, puis appelle un répartiteur qui joue l'autre ;
        // s'arrêter au premier ne montrerait que deux effets sur soixante-sept.
        //
        // ⚠ LA POSE NE VIENT QU'AVEC UNE DURÉE : le client sort avant son
        // répartiteur d'incantation quand la durée annoncée est nulle, et c'est
        // ce répartiteur qui, au défaut, appelle le semeur — avec la durée, que
        // les cercles d'incantation des actions prennent pour vie.
        // ⚠ SAUF UNE COMPÉTENCE, qui saute cette sortie : voir
        // `CastOutlivesInstantExit`.
        const bool resolves =
            cast.duration_ms > 0 || ro::render::CastOutlivesInstantExit(cast.skill_id);
        const bool par_le_semeur =
            resolves && PlayNamedSkill(stage, planner, id, actor, cast.skill_id,
                                       ro::render::kSkillNamedOnCastSpawn, cast.started_ms);
        const bool plays_own = PlayNamedSkill(stage, planner, id, actor, cast.skill_id,
                                              ro::render::kSkillNamedOnCast, cast.started_ms);
        // ⚠ UNE FOIS PAR COMPÉTENCE, à l'ENTRÉE du chemin et non à sa sortie :
        // un échec silencieux en amont — pas de cast dans la sim, une durée
        // nulle, un effet nommé qui intercepte — ne laisse aucune trace, et
        // c'est alors l'absence de ligne qui doit parler.
        if (reported_cast_paths_.insert(cast.skill_id).second) {
          ro::core::LogInfo("incantation {} : durée {} ms, élément {}, nommés {}/{} — le repli {}",
                            cast.skill_id, cast.duration_ms, cast.element, par_le_semeur, plays_own,
                            resolves && !par_le_semeur && !plays_own ? "s'applique" : "est écarté");
        }
        // 🔴 UNE INCANTATION INSTANTANÉE NE MONTRE AUCUN EFFET DE CE CÔTÉ, et
        // c'est une SORTIE du client, pas une table vide : le visuel
        // d'incantation rend la main dès que la durée annoncée vaut zéro — juste
        // après la phrase de chat, la bannière du nom et le registre d'effets,
        // et AVANT la résolution de la pose.
        // Une compétence sans temps d'incantation n'a donc ni effet ni pose
        // d'incantation, quoi que la table en dise : c'est l'autre chemin, celui
        // de l'emploi ou des dégâts, qui la montre.
        //
        // 🔴 SAUF ASURA STRIKE, qui saute la sortie : en combo, sa durée nulle
        // montre quand même l'effet d'incantation. Sa pose, elle, ne se voit
        // pas — le coup arrive dans la même trame et la remplace.
        //
        // ⚠ LES EFFETS NOMMÉS, EUX, RESTENT : le registre tourne AVANT cette
        // sortie, et les écarter ici priverait d'effet les compétences
        // instantanées qui en ont un.
        //
        // ⚠ LA POSE PORTE DÉJÀ CETTE GARDE, par une autre voie : `MotionAt` la
        // borne à la durée annoncée, qu'une durée nulle rend vide d'emblée.
        // LES EXTRAS DE L'INCANTATION : ce que les deux DERNIERS gestes du
        // visuel d'incantation posent, pour sept compétences en tout.
        //
        // ⚠ DERRIÈRE LA MÊME GARDE DE DURÉE — ils vivent au-delà de la sortie
        // sur durée nulle —, mais SANS le crible des effets nommés : le client
        // les pose que le chemin nommé ait répondu ou non. Ce sont ses derniers
        // gestes, pas une solution de repli.
        if (resolves) {
          // L'INCANTATION SE SURVEILLE, cercle semé ou non : une interruption
          // doit éteindre ses extras et sa marque — voir `cast_ring_sown`.
          static_cast<void>(per_map_.cast_ring_sown.try_emplace(id));
          // L'ÉTAT D'INCANTATION, posé avec la barre : ce qu'il fait naître.
          play_state_effects(actor, ro::render::SkillImplFor(cast.skill_id).CastState(),
                             cast.skill_id, cast.started_ms, SkillMoment::kAnnounced);
          // 🔴 LA GRILLE DE VISÉE SOUS UN SORT LANCÉ AU SOL : c'est elle, le
          // cercle qu'on voit se poser pendant l'incantation d'un Storm Gust —
          // `magic_target.tga`, sept cases sur sept peintes à même le terrain.
          //
          // ⚠ ELLE N'A RIEN À VOIR AVEC L'AURA ÉLÉMENTAIRE. Celle-ci naît sous
          // le LANCEUR et dit l'élément du sort ; la grille se pose sur la CASE
          // VISÉE et dit où le sort tombera. Les confondre fait chercher un
          // cercle absent dans la table des auras, qui répondait pourtant.
          //
          // ⚠ ET SA VIE SUFFIT À LA FAIRE DURER, là où l'aura demande une
          // relance : son dessinateur n'utilise AUCUN alpha de piste — les deux
          // teintes sont écrites en dur —, si bien qu'elle apparaît et disparaît
          // avec son nœud au lieu de s'éteindre en chemin.
          if (cast.target_id == 0 && (cast.target_cell.x != 0 || cast.target_cell.y != 0)) {
            const std::array<float, 3> visee =
                GroundPositionAt(stage, cast.target_cell.x, cast.target_cell.y);
            // ⚠ UNE ÉCHÉANCE, ET NON UN PLANCHER. La grille déclare TROIS MILLE
            // tics — quarante-huit secondes —, et un plancher la laisse les
            // vivre : elle restait sur la carte longtemps après l incantation.
            // Le client lui donne une heure de mort, timeGetTime() + durée.
            PlayById(ro::render::kGroundSampleEffect, glm::vec3(visee[0], visee[1], visee[2]),
                     cast.started_ms, /*owner_id=*/0, /*endless=*/false,
                     /*life_floor_ticks=*/0, /*announce_sound=*/true,
                     static_cast<std::int32_t>(cast.duration_ms / ro::render::kEffectTickMs));
          }
          // 🔴 LE RÉTICULE SUR LA CIBLE, et il ne dépend d'AUCUNE compétence :
          // le client l'envoie à la cible avant tout cas particulier, dès que
          // le lancer vise quelqu'un.
          // C'est le cercle qui se referme sous ce qu'on incante.
          //
          // Ses gardes vivent dans `render`, avec ce qu'elles décrivent : voir
          // `ShowsLockOnReticle`.
          if (ro::render::ShowsLockOnReticle(cast.skill_id, id, cast.target_id, cast.duration_ms)) {
            if (const ro::sim::Actor* aimed = world.Find(cast.target_id); aimed != nullptr) {
              const ro::sim::FractionalCell ou =
                  ro::sim::InterpolatedCell(*aimed, planner.PathFor(*aimed), cast.started_ms);
              const std::array<float, 3> assise = GroundPositionAt(stage, ou.x, ou.y);
              // La vie est celle que le message porte, `durée / 16` : le
              // réticule se referme exactement sur la fin de l'incantation.
              //
              // 🔴 UNE ÉCHÉANCE, ET NON UN PLANCHER, comme la grille au sol : en
              // plancher, le réticule vivait la vie que son nœud déclare, et
              // restait sur la cible bien après le sort lancé.
              PlayById(ro::render::kLockOnReticleEffect, glm::vec3(assise[0], assise[1], assise[2]),
                       cast.started_ms, cast.target_id,
                       /*endless=*/false, /*life_floor_ticks=*/0, /*announce_sound=*/true,
                       static_cast<std::int32_t>(cast.duration_ms / ro::render::kEffectTickMs));
              per_map_.lock_on_reticles.insert_or_assign(
                  id, EffectMapMemory::LockOnReticle{cast.target_id,
                                                     cast.started_ms + cast.duration_ms});
            }
          }
          for (const ro::render::SkillCastExtraEntry& extra :
               ro::render::SkillCastExtrasFor(cast.skill_id)) {
            const bool on_target = extra.on == ro::render::SkillEffectOn::kTarget;
            // ⚠ UN LANCER SANS CIBLE PREND L'AUTRE BRANCHE DU CLIENT — celle du
            // repère au sol —, et aucun de ces effets-là n'y naît. Les poser
            // quand même les ferait paraître sur le lanceur, faute de cible.
            if ((on_target || extra.thrown_at_target) && cast.target_id == 0) {
              continue;
            }
            // UN OBJET LANCÉ part du lanceur et vise la place de sa cible.
            std::optional<glm::vec3> aimed;
            if (extra.thrown_at_target) {
              const ro::sim::Actor* aimed_actor = world.Find(cast.target_id);
              if (aimed_actor == nullptr) {
                continue;
              }
              const ro::sim::FractionalCell cell = ro::sim::InterpolatedCell(
                  *aimed_actor, planner.PathFor(*aimed_actor), cast.started_ms);
              const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
              aimed = glm::vec3(ground[0], ground[1], ground[2]);
            }
            const std::uint32_t bearer = on_target ? cast.target_id : id;
            const ro::sim::Actor* found_actor = world.Find(bearer);
            if (found_actor == nullptr) {
              continue;
            }
            const ro::sim::FractionalCell ou = ro::sim::InterpolatedCell(
                *found_actor, planner.PathFor(*found_actor), cast.started_ms);
            const std::array<float, 3> assise = GroundPositionAt(stage, ou.x, ou.y);
            PlayById(extra.effect_id, glm::vec3(assise[0], assise[1], assise[2]), cast.started_ms,
                     bearer, /*endless=*/false, /*life_floor_ticks=*/0, /*announce_sound=*/true,
                     /*life_exact_ticks=*/0, /*instance_scale=*/0.0F, /*instance_second=*/0.0F,
                     aimed);
          }
        }
        // ⚠ QUE LE CHEMIN NOMMÉ AIT RÉPONDU OU NON : le répartiteur d'incantation
        // du client sème l'effet que la résolution de la pose lui donne AVANT
        // son aiguillage par compétence, et c'est au défaut de cet aiguillage
        // que les actions sèment le leur. Une action qui rend « aucun effet »
        // à la résolution n'en montre pas ici ; une qui rend le geste ordinaire
        // le montre, en plus de ce qu'elle nomme.
        //
        // ⚠ SAUF DEUX COMPÉTENCES, Brandish Spear et Bowling Bash, dont le
        // semeur refuse l'effet d'incantation : voir `CastSpawnsEffect`.
        if (resolves && ro::render::SkillImplFor(cast.skill_id).CastSpawnsEffect()) {
          // 🔴 LE LUA PRIME SUR LES DOUZE SWITCHES, et c'est la première marche
          // de la cascade : le client demande d'abord au Lua son
          // `beginEffectID`, et ne descend qu'à son refus. Onze des
          // soixante-six fiches en donnent un.
          const std::span<const ro::render::SkillLuaEffectEntry> du_lua =
              ro::render::SkillLuaEffectsFor(cast.skill_id, ro::render::SkillLuaSlot::kBegin);
          // ⚠ ET LA CLASSE DU LANCEUR PEUT CHANGER L'EFFET : les glyphes d'Asura
          // Strike d'un Champion ne sont pas ceux d'un Monk.
          const std::int16_t from_table = ro::render::CastEffectForJob(
              cast.skill_id, ro::render::SkillImplFor(cast.skill_id).CastEffectId(),
              ShownJobOf(actor));
          const std::int16_t effect =
              du_lua.empty() ? ro::render::CastEffectForElement(from_table, cast.element)
                             : static_cast<std::int16_t>(du_lua.front().effect_id);
          // 🔴 UN EFFET NUL OU NÉGATIF NE SE PLAIGNAIT PAS. Trois chemins mènent
          // là — la table dit « aucun », l'élément annoncé sort de la plage des
          // auras, ou la compétence est inconnue — et tous les trois rendaient
          // un sol nu sans une ligne. Le silence se lisait « notre moteur ne
          // sait pas le dessiner », ce qui envoyait chercher au mauvais endroit.
          if (effect <= 0 && reported_casts_.insert(cast.skill_id).second) {
            ro::core::LogWarning(
                "compétence {} à l'incantation : aucun effet (table {}, élément {})", cast.skill_id,
                ro::render::SkillImplFor(cast.skill_id).CastEffectId(), cast.element);
          }
          if (effect > 0) {
            const ro::sim::FractionalCell cell =
                ro::sim::InterpolatedCell(actor, planner.PathFor(actor), cast.started_ms);
            const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
            // 🔴 PAR `PlayById`, ET NON PAR LE FICHIER SEUL. Les deux effets
            // d'incantation les plus courants — 12 pour un sort, 16 pour un
            // geste — n'ont AUCUN fichier : ils se dessinent par un patron de
            // nœuds. Ne chercher qu'un fichier rendait toute incantation muette
            // et invisible, alors que la table répondait.
            //
            // ⚠ ET C'EST AUSSI CE QUI LA FAIT SONNER : `Play` ne demande aucun
            // bruit, seul `NoteLaunched` en réclame un, et rien d'autre ne sonne
            // à l'incantation — `PlaySkills` ne connaît que le coup reçu et la
            // compétence sans dégâts.
            // ⚠ LA DURÉE DE L'INCANTATION VA AVEC : les anneaux qui recopient la
            // vie de leur instance la vivent d'un seul tenant, les autres se
            // relancent — voir `SowCastRing`.
            const bool sown =
                SowCastRing(*this, per_map_, id, glm::vec3(ground[0], ground[1], ground[2]),
                            static_cast<std::uint32_t>(effect), cast.started_ms, cast.duration_ms);
            if (!sown && reported_casts_.insert(cast.skill_id).second) {
              // Une fois par compétence : « je ne vois rien » ne distingue pas
              // une table muette d'un effet qui ne se dessine pas.
              ro::core::LogWarning(
                  "compétence {} à l'incantation : effet {} ni fichier ni patron (élément {})",
                  cast.skill_id, effect, cast.element);
            }
          }
        }
      }
    }
    // 🔴 CE QUI DOIT DURER SE RELANCE, IL NE S'ÉTIRE PAS. La règle est celle de
    // la maison — elle est écrite au-dessus de `PeriodMs` —, et s'en écarter a
    // coûté une correction inutile : allonger la VIE d'un nœud ne rallonge pas
    // l'alpha de ses pistes. Celles du cercle d'incantation s'éteignent vers le
    // soixante-cinquième tic, soit une seconde, quelle que soit la vie donnée.
    // Une incantation de quatorze secondes montrait donc son cercle une
    // seconde, puis un sol nu — le semis ayant pourtant réussi, rien ne s'en
    // plaignait.
    //
    // ⚠ HORS DU SEMIS UNIQUE, et c'est tout le point : ceci tourne à CHAQUE
    // image tant que le sort se prépare.
    if (const auto rings = per_map_.cast_ring_sown.find(id);
        rings != per_map_.cast_ring_sown.end()) {
      const bool still = actor.cast.has_value() && now_ms >= actor.cast->started_ms &&
                         now_ms - actor.cast->started_ms < actor.cast->duration_ms;
      if (!still) {
        // 🔴 UNE INCANTATION INTERROMPUE ÉTEINT SES EFFETS NET : le client
        // détruit son cercle et le second emplacement — celui des extras —, et,
        // pour un autre que soi, l'effet 1114. La fin normale, elle, les laisse
        // finir d'eux-mêmes.
        if (actor.cast.has_value() && actor.cast->cancelled) {
          for (const CastRing& ring : rings->second) {
            StopById(ring.effect_id, id);
          }
          for (const ro::render::SkillCastExtraEntry& extra :
               ro::render::SkillCastExtrasFor(actor.cast->skill_id)) {
            const bool on_target = extra.on == ro::render::SkillEffectOn::kTarget;
            StopById(extra.effect_id, on_target ? actor.cast->target_id : id);
          }
          if (id != world.player_id()) {
            StopById(kInterruptedCastMarkEffect, id);
          }
        }
        per_map_.cast_ring_sown.erase(rings);
      } else {
        // La période est la VIE VISIBLE de l'effet, moins un recouvrement :
        // relancer pile à l'extinction laisserait une image de sol nu. Chaque
        // cercle du lanceur a la sienne.
        for (CastRing& ring : rings->second) {
          if (!ring.relaunches) {
            continue;
          }
          const std::uint32_t period_ms = PeriodMs(
              EffectNodesInPlay::ProceduralLifeTicks(ring.effect_id), kCastRingOverlapTicks);
          if (now_ms - ring.sown_ms < period_ms) {
            continue;
          }
          ring.sown_ms = now_ms;
          const ro::sim::FractionalCell cell =
              ro::sim::InterpolatedCell(actor, planner.PathFor(actor), now_ms);
          const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
          PlayById(ring.effect_id, glm::vec3(ground[0], ground[1], ground[2]), now_ms, id,
                   /*endless=*/false, /*life_floor_ticks=*/0, /*announce_sound=*/false);
        }
      }
    }

    // L'ÉTAT D'EMPLOI DU LANCEUR — compétence annoncée ou posée sur une case —
    // fait naître ce qu'il fait naître en se posant, AVANT la suite de la
    // compétence : le client pose la motion en tête de ses répartiteurs, que
    // leur chemin nommé réponde ou non. Une créature que sa table fait jouer
    // n'y passe pas.
    if (actor.skill_use.has_value()) {
      const ro::sim::SkillUse& use = *actor.skill_use;
      const auto posed = per_map_.posed_use_states.find(id);
      if (posed == per_map_.posed_use_states.end() || posed->second != use.started_ms) {
        per_map_.posed_use_states.insert_or_assign(id, use.started_ms);
        if (!ro::render::SkillUsePosedByTable(actor.look.job, use.skill_id)) {
          play_state_effects(actor, ro::render::SkillUseStateFor(use.skill_id), use.skill_id,
                             use.started_ms, SkillMoment::kAnnounced);
        }
      }
    }
    // Une compétence posée SUR UNE CASE : son effet se joue là où elle est
    // posée, et non sur quelqu'un. C'est le chemin par défaut du client, celui
    // qu'il prend pour toute compétence dont il n'a pas fait un cas à part.
    if (actor.ground_skill.has_value()) {
      const ro::sim::GroundSkillUse& use = *actor.ground_skill;
      const auto cast = per_map_.ground_cast.find(id);
      if (cast == per_map_.ground_cast.end() || cast->second != use.started_ms) {
        per_map_.ground_cast.insert_or_assign(id, use.started_ms);
        PlayGroundSkillPacket(stage, planner, &actor, id, use);
      }
    }
    // Une compétence SANS DÉGÂTS : un soin, un renfort, un état posé. Le
    // serveur l'annonce par son propre paquet, et c'est un AUTRE répartiteur du
    // client qui en fait naître l'effet. Sans ce passage, toute la famille
    // acolyte et prêtre reste muette à l'écran, quoi que la table dise d'elle.
    if (actor.support.has_value()) {
      const ro::sim::SupportHit& given = *actor.support;
      const auto seen = per_map_.supported.find(id);
      if (seen == per_map_.supported.end() || seen->second != given.started_ms) {
        per_map_.supported.insert_or_assign(id, given.started_ms);
        // ⚠ LE CHEMIN NOMMÉ EXCLUT L'AUTRE, et c'est ce que fait le client : son
        // répartiteur de ZC_USE_SKILL ne consulte le registre des actions qu'au
        // DÉFAUT de son switch — donc pour les compétences dont il n'a pas fait
        // un cas, ce que sont toutes les modernes. Jouer les deux montrerait
        // l'effet d'une compétence par-dessus celui d'une autre.
        // Le paquet ne porte qu'un champ pour le niveau et le montant : c'est
        // lui que la méthode reçoit comme niveau.
        if (!PlayNamedSkill(stage, planner, id, actor, given.skill_id,
                            ro::render::kSkillNamedOnUseSkill, given.started_ms,
                            world.Find(given.source_id), given.amount)) {
          // ⚠ LE PARAMÈTRE DU CHEMIN SANS DÉGÂTS N'EST PAS LU : le paquet ne
          // porte qu'un champ pour le niveau et le montant, et ce que le
          // répartiteur de ZC_USE_SKILL en pose dans l'instance n'est pas
          // connu. Un, comme le chemin nu, plutôt qu'un montant pris pour un
          // compte de boules.
          PlaySkillOn(world, stage, planner, actor, given.skill_id, given.source_id,
                      given.started_ms, kRawEffectParam, SkillMoment::kAnnounced, given.amount);
        }
      }
    }
  }
  // 🔴 UN LANCEUR HORS DE VUE N'ÉTEINT PAS LA CASE : le serveur l'annonce à qui
  // la voit, et ce qui naît sur elle naît sans lui.
  for (const ro::sim::UnseenGroundSkill& unseen :
       unseen_ground_skills_.Take(world.unseen_ground_skills())) {
    PlayGroundSkillPacket(stage, planner, nullptr, unseen.caster_id, unseen.use);
  }
  for (const ro::sim::RecordedStrike& taken : skill_strikes_.Take(world.strikes())) {
    const ro::sim::Strike& strike = taken.strike;
    const std::uint32_t id = strike.target;
    const ro::sim::Actor* const struck = world.Find(id);
    if (strike.skill_id == 0) {
      continue;
    }
    // 🔴 L'EFFET D'EMPLOI D'ABORD, sur le lanceur, À L'ARRIVÉE DU PAQUET : le
    // client le fait naître avant la suite de la compétence et son effet visé,
    // sans attendre que le coup touche. Il ne reçoit pas la place de la cible,
    // seulement le nombre de coups. Voir `ro::render::SkillHitUseEffect`.
    //
    // ⚠ PAS POUR UNE UNITÉ AU SOL : son coup va à un autre répartiteur, qui
    // n'en pose aucun. Et le lanceur doit être là pour le porter.
    if (const ro::sim::Actor* caster = world.Find(strike.attacker);
        caster != nullptr && !StrikeByGroundUnit(world, strike)) {
      // L'ÉTAT QUE LE COUP POSE À SON AUTEUR, par la même motion qu'à l'emploi.
      if (const std::optional<std::int8_t> struck_state =
              ro::render::SkillHitMotionState(strike.skill_id, strike.type)) {
        play_state_effects(*caster, *struck_state, strike.skill_id, strike.started_ms,
                           SkillMoment::kImpact);
      }
      const bool target_known = struck != nullptr || world.skill_units().contains(id);
      const std::int16_t use_effect = ro::render::SkillHitUseEffect(
          strike.skill_id, strike.type, strike.announced_level, target_known);
      if (use_effect >= 0) {
        const ro::sim::FractionalCell cell =
            ro::sim::InterpolatedCell(*caster, planner.PathFor(*caster), strike.started_ms);
        const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
        if (!PlayById(static_cast<std::uint32_t>(use_effect),
                      glm::vec3(ground[0], ground[1], ground[2]), strike.started_ms,
                      strike.attacker, /*endless=*/false, /*life_floor_ticks=*/0,
                      /*announce_sound=*/true, /*life_exact_ticks=*/0, /*instance_scale=*/0.0F,
                      /*instance_second=*/0.0F, std::nullopt, EffectParamOfStrike(strike)) &&
            reported_hit_effects_.insert(static_cast<std::uint32_t>(use_effect)).second) {
          ro::core::LogWarning("compétence {} : effet d'emploi {} ni fichier ni patron",
                               strike.skill_id, use_effect);
        }
      }
    }
    // 🔴 UNE UNITÉ AU SOL NE JOUE PAS L'EFFET DE SA COMPÉTENCE. Le client
    // délivre le message du coup à un AUTRE répartiteur quand la source est une
    // unité, et celui-là ne pose que l'impact. Sans cette porte, le blizzard de
    // Storm Gust renaissait à chaque battement de chacune de ses unités — et il
    // y en a une par case.
    //
    // ⚠ L'IMPACT NOMMÉ N'EST PAS ICI : il part à chaque FRAPPE, avec l'effet du
    // message qu'il remplace — voir `NoticeHitEffects`. Il ne dispense donc pas
    // de ce chemin-ci, que le client suit dans tous les cas.
    //
    // L'ARGUMENT DU COUP est le nombre de coups que le client passe à son
    // répartiteur — le même que le paramètre d'instance.
    if (struck != nullptr && !StrikeByGroundUnit(world, strike)) {
      PlaySkillOn(world, stage, planner, *struck, strike.skill_id, strike.attacker,
                  strike.started_ms, EffectParamOfStrike(strike), SkillMoment::kImpact,
                  EffectParamOfStrike(strike), strike.type);
    }
    // LE MOMENT DU PAQUET, APRÈS la suite du coup : le client consulte son
    // registre des actions une fois le visuel du coup posé. Il part toujours —
    // sur la source ou sur la cible, selon l'effet. ⚠ UNE UNITÉ AU SOL N'EST PAS
    // UNE SOURCE pour lui, mais la cible reçoit quand même ce qui lui revient.
    //
    // ⚠ UNE CIBLE QUI N'EST PAS UN ACTEUR peut être une UNITÉ AU SOL : le
    // moment la reçoit alors en REPLI, et une méthode sème à sa place.
    const auto repli = world.skill_units().find(id);
    PlayNamedSkillOnPair(
        stage, planner, strike.skill_id, ro::render::kSkillNamedOnDamagePacket,
        world.Find(strike.attacker), struck, strike.started_ms, strike.attack_motion_ms,
        struck == nullptr && repli != world.skill_units().end() ? &repli->second : nullptr);
  }
  // 🔴 EN PLUS, ET NON AU DÉFAUT : le client interroge son Lua par des chemins
  // qui lui sont propres, et qui ne remplacent aucun des switches ci-dessus.
  NoticeLuaEffects(world, stage, planner);
}

}  // namespace moonclient
