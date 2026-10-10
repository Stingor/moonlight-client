// LE CHEMIN DES EFFETS NOMMÉS : ceux que le client désigne par une CHAÎNE, et
// non par un identifiant.
//
// Une famille entière vit ici — 142 compétences, 38 états, 8 unités au sol —,
// et elle ne ressemble à aucune autre : le nom ne désigne aucun fichier par
// lui-même, c'est une table de DONNÉES de l'installation qui le résout. Voir
// docs/adr/0027, et la table scripts/client_named_effect_actions.json.
//
// Elle est SÉPARÉE de effect_view.cpp parce qu'elle se tient seule : ces
// méthodes lisent la même table, inscrivent dans la même file, et rien d'autre
// du fichier voisin n'y touche.

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>

#include "ro/core/log.h"
#include "ro/render/actor_effect_table.h"
#include "ro/render/effect_class.h"
#include "ro/render/effect_duration.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_table.h"
#include "ro/render/skill_named_table.h"
#include "ro/render/state_effect.h"
#include "ro/sim/movement.h"

// ⚠ LA TROUPE PROCÉDURALE EN ENTIER, et non la seule déclaration qu'`effect_view.h`
// porte : `StopById` coupe les deux côtés d'un effet, et le procédural demande
// le type complet.
#include "ro/sim/world.h"

#include "effects/effect_actor_change.h"
#include "effects/effect_layers.h"
#include "effects/effect_named_sounds.h"
#include "effects/effect_node_view.h"
#include "effects/effect_view.h"

namespace moonclient {

const ro::formats::NamedEffect* EffectsInPlay::NamedEffectFor(std::string_view name) {
  if (!named_effects_read_) {
    named_effects_read_ = true;
    auto bytes = vfs_->Read(ro::formats::kNamedEffectsPath);
    if (!bytes) {
      ro::core::LogWarning("effets nommés : {} illisible ({})", ro::formats::kNamedEffectsPath,
                           ro::core::Describe(bytes.error()));
      return nullptr;
    }
    auto table = ro::formats::NamedEffectTable::Parse(*bytes);
    if (!table) {
      ro::core::LogWarning(
          "effets nommés : {} illisible à l'analyse ({}) — aucun effet nommé ne "
          "paraîtra",
          ro::formats::kNamedEffectsPath, ro::core::Describe(table.error()));
      return nullptr;
    }
    named_effects_ = std::move(*table);
  }
  if (!named_effects_.has_value()) {
    return nullptr;
  }
  return named_effects_->Find(name);
}

void EffectsInPlay::PlayNamedEntry(std::string_view name, ro::render::NamedEffectVerb verb,
                                   std::uint32_t owner_id, bool from_unit,
                                   const ro::sim::CellPosition& cell,
                                   ro::render::NamedEffectAnchor anchor, bool after_vanish,
                                   std::uint32_t now_ms, std::int32_t start_delay_ms,
                                   const glm::vec3& offset, const ro::render::NamedSowing& sowing) {
  // ⚠ CESSER N'EST PAS SEMER, et le même nom sert aux deux. Couper vide ce qui
  // joue sous ce nom ; arrêter laisse l'image en cours finir son cycle et se
  // contente de ne plus la réinscrire — c'est ce que fait le drapeau que le
  // client lève sur l'effet vivant.
  if (verb != ro::render::NamedEffectVerb::kSow) {
    std::erase_if(per_map_.pending_named, [&](const PendingNamed& waiting) {
      return waiting.owner_id == owner_id && waiting.from_unit == from_unit && waiting.name == name;
    });
    if (verb == ro::render::NamedEffectVerb::kCut) {
      std::erase_if(per_map_.playing, [&](const Playing& shown) {
        return shown.named_owner_id == owner_id && shown.named_as == name;
      });
    }
    return;
  }
  const ro::formats::NamedEffect* record = NamedEffectFor(name);
  // ⚠ UN NOM QUE LE CODE DEMANDE PEUT MANQUER AUX DONNÉES, et ce n'est pas un
  // défaut de lecture : la table de l'installation porte 223 fiches pour 411
  // noms demandés, et le client natif ne montre pas davantage ceux qui manquent.
  if (record == nullptr || record->file.empty()) {
    if (std::ranges::find(reported_names_, name) == reported_names_.end()) {
      reported_names_.push_back(name);
      ro::core::LogWarning("effet nommé {} : pas dans les données de cette installation", name);
    }
    return;
  }
  // LE DÉLAI DE DÉPART EST CE QUI ENCHAÎNE les fiches d'une même action : le
  // `_start` part tout de suite, le `_loop` quand le premier a fini.
  PendingNamed waiting;
  waiting.owner_id = owner_id;
  waiting.from_unit = from_unit;
  waiting.name = name;
  waiting.cell = cell;
  // ⚠ UNE MÉTHODE PEUT RÉÉCRIRE CE DÉLAI après la création, zéro compris.
  const std::int32_t delay_ms = start_delay_ms < 0 ? record->start_delay_ms : start_delay_ms;
  waiting.at_ms = now_ms + static_cast<std::uint32_t>(std::max(0, delay_ms));
  waiting.after_vanish = after_vanish;
  waiting.on_actor = anchor == ro::render::NamedEffectAnchor::kActor && !from_unit;
  waiting.offset = offset;
  waiting.turn_degrees = sowing.turn_degrees.value_or(0.0F);
  waiting.scale = sowing.scale;
  waiting.flip_x = sowing.flip_x;
  waiting.flip_y = sowing.flip_y;
  waiting.laid_on_ground = sowing.laid_on_ground;
  per_map_.pending_named.push_back(waiting);
}

namespace {

// Le niveau qu'une classe d'unité reçoit quand sa fabrique ne le lui passe pas.
constexpr std::int32_t kNoUnitLevel = -1;

}  // namespace

void EffectsInPlay::PlayNamedUnit(std::uint32_t unit_instance_id, std::uint16_t unit_id,
                                  const ro::sim::CellPosition& cell,
                                  ro::render::SkillUnitNamedMoment moment, std::uint32_t now_ms,
                                  std::int32_t level, const MapStage* stage) {
  const bool at_vanish = moment == ro::render::SkillUnitNamedMoment::kOnVanish;
  for (const ro::render::SkillUnitNamedEffectEntry& entry :
       ro::render::SkillUnitNamedEffects(unit_id, moment)) {
    // UN EFFET PAR NUMÉRO : une classe du gestionnaire, attachée à l'unité et
    // née sur sa case, le niveau de la compétence en paramètre quand la
    // fabrique le lui passe. Une classe que nous ne portons pas ne montre rien,
    // comme chez le client sans fabrique inscrite pour son numéro ; celle qui
    // naît chez le poseur de l'unité attend sa classe.
    if (entry.effect_id != 0) {
      if (stage != nullptr && ro::render::IsPortedEffectClass(entry.effect_id)) {
        const std::array<float, 3> ground = GroundPositionAt(*stage, cell.x, cell.y);
        const glm::vec3 place(ground[0], ground[1], ground[2]);
        static_cast<void>(classes_.Spawn({.effect_id = entry.effect_id,
                                          .owner_id = unit_instance_id,
                                          .ground_position = place,
                                          .bound_id = unit_instance_id,
                                          .bound_ground_position = place,
                                          .level = entry.level_as_param ? level : kNoUnitLevel},
                                         now_ms));
      }
      continue;
    }
    // ⚠ UNE UNITÉ RESTE À SA CASE, quoi que dise l'ancrage de sa fiche. Le
    // client attache ces effets à l'ACTEUR de l'unité — les unités au sol en
    // sont — mais cet acteur ne marche jamais, et le nôtre n'existe pas dans le
    // monde des acteurs : c'est la case qui les porte.
    PlayNamedEntry(entry.name, entry.verb, unit_instance_id, true, cell,
                   ro::render::NamedEffectAnchor::kPosition, at_vanish, now_ms);
  }
}

namespace {

// ⚠ LA CASE, ET NON LA POSITION FLOTTANTE. Un semis nommé se rejoue tant qu'il
// dure, et il se rejoue là où il est né : garder la case fige le point comme le
// client le fige sur son acteur.
[[nodiscard]] ro::sim::CellPosition CellOf(ro::sim::WalkPlanner& planner,
                                           const ro::sim::Actor& actor, std::uint32_t at_ms) {
  const ro::sim::FractionalCell cell =
      ro::sim::InterpolatedCell(actor, planner.PathFor(actor), at_ms);
  return ro::sim::CellPosition{static_cast<std::uint16_t>(cell.x),
                               static_cast<std::uint16_t>(cell.y)};
}

}  // namespace

bool EffectsInPlay::PlayNamedSkill(const MapStage& stage, ro::sim::WalkPlanner& planner,
                                   std::uint32_t actor_id, const ro::sim::Actor& actor,
                                   std::uint16_t skill_id, std::uint8_t slot,
                                   std::uint32_t started_ms, const ro::sim::Actor* source,
                                   std::int32_t level) {
  (void)actor_id;
  NamedSkillMoment moment;
  moment.skill_id = skill_id;
  moment.slot = slot;
  moment.source = source;
  moment.target = &actor;
  moment.level = level;
  moment.started_ms = started_ms;
  // L'ALLURE DU LANCEUR est celle de l'action que son état d'emploi vient de lui
  // faire prendre, sans facteur de coup : voir `NamedEffectPaceOf`.
  if (!PlayNamedMoment(stage, planner, moment)) {
    return false;
  }
  // CE QUE LA MÉTHODE FAIT ENTENDRE TOUT DE SUITE, pourvu que le lanceur soit
  // connu : voir `PlayNamedMethodSounds`. À la pose d'une incantation, le
  // lanceur est l'acteur du moment.
  const bool cast_moment =
      slot == ro::render::kSkillNamedOnCastSpawn || slot == ro::render::kSkillNamedOnCast;
  const ro::sim::Actor* bearer = source != nullptr ? source : cast_moment ? &actor : nullptr;
  if (bearer != nullptr) {
    PlayNamedMethodSounds(NamedMethodFamily::kSkill, skill_id, slot, bearer->id,
                          CellOf(planner, *bearer, started_ms), CellOf(planner, actor, started_ms),
                          per_map_.just_sown_named_sounds);
  }
  return true;
}

void EffectsInPlay::PlayNamedSkillEntry(const ro::render::SkillNamedEffectEntry& entry,
                                        std::string_view name, std::uint32_t owner_id,
                                        const ro::sim::CellPosition& cell,
                                        ro::render::NamedEffectAnchor anchor, std::int32_t level,
                                        std::uint32_t started_ms, const glm::vec3& offset,
                                        const ro::render::NamedSowing& sowing) {
  PlayNamedEntry(name, entry.verb, owner_id, false, cell, anchor, false, started_ms,
                 sowing.start_delay_ms, offset, sowing);
  // LES BRUITS QUE LA MÉTHODE EMPILE SUR L'EFFET SEMÉ, qui vivent autant que
  // lui : voir `QueueNamedMethodSounds`.
  if (const ro::formats::NamedEffect* semee = NamedEffectFor(name);
      semee != nullptr && !semee->file.empty() && entry.verb == ro::render::NamedEffectVerb::kSow) {
    const ro::render::LoadedEffect* effect =
        catalog_->Load(std::string(ro::render::kEffectFolder) + semee->file);
    const std::int32_t delay_ms =
        sowing.start_delay_ms < 0 ? semee->start_delay_ms : sowing.start_delay_ms;
    QueueNamedMethodSounds(entry.skill_id, entry.slot, level, owner_id, cell, started_ms,
                           NamedEffectLifeTicks(delay_ms, effect ? effect->prepared.last_frame : 0),
                           per_map_.just_sown_named_sounds);
  }
}

void EffectsInPlay::SowDueNamed(const MapStage& stage, std::uint32_t now_ms) {
  // Un semis peut en réinscrire un autre : on parcourt par rang, et les
  // réinscriptions écrasent l'entrée à sa place plutôt que d'en ajouter.
  for (std::size_t rank = 0; rank < per_map_.pending_named.size();) {
    PendingNamed& pending = per_map_.pending_named[rank];
    if (now_ms < pending.at_ms) {
      ++rank;
      continue;
    }
    // LA CASE VIENT DU SEMIS, non du monde : un effet de LEVÉE se sème
    // précisément quand son unité n'y est plus.
    const ro::formats::NamedEffect* record = NamedEffectFor(pending.name);
    const ro::render::LoadedEffect* effect =
        record == nullptr ? nullptr
                          : catalog_->Load(std::string(ro::render::kEffectFolder) + record->file);
    if (effect == nullptr) {
      per_map_.pending_named.erase(per_map_.pending_named.begin() +
                                   static_cast<std::ptrdiff_t>(rank));
      continue;
    }
    const std::array<float, 3> ground = GroundPositionAt(stage, pending.cell.x, pending.cell.y);
    // ⚠ LE DÉCALAGE DE LA FICHE N'EST PAS APPLIQUÉ, et c'est délibéré : ce que
    // PosY mesure n'est pas tranché. Une fiche COUCHÉE AU SOL porte le même
    // PosY que la fiche debout qui lui répond — douze pour les deux moitiés de
    // Jack Frost Nova —, ce qui interdit de le lire comme une hauteur
    // au-dessus du point d'ancrage. La case est juste ; le décalage attend une
    // mesure.
    // ⚠ UN EFFET ATTACHÉ SUIT SON PORTEUR, et c'est la fiche qui le dit. Une
    // boucle d'état posée sur quelqu'un marche avec lui ; la laisser à sa case
    // la ferait rester au sol dès le premier pas. La position reste donnée : un
    // porteur disparu y laisse l'effet finir.
    Playing sown{effect,
                 pending.on_actor ? pending.owner_id : 0,
                 glm::vec3(ground[0], ground[1], ground[2]) + pending.offset,
                 now_ms,
                 pending.owner_id,
                 pending.name};
    sown.turn_degrees = pending.turn_degrees;
    sown.scale = pending.scale;
    sown.flip_x = pending.flip_x;
    sown.flip_y = pending.flip_y;
    sown.laid_on_ground = pending.laid_on_ground;
    per_map_.playing.push_back(sown);
    // LE BRUIT PART AVEC L'IMAGE — un tic après elle chez le client, ce qui ne
    // s'entend pas —, et il ne repart à chaque tour que si la fiche BOUCLE son
    // son. Neuf fiches durent sans boucler le leur, et le rejouer pour elles
    // ferait battre un bruit que le client ne fait entendre qu'une fois.
    if (!record->sound.empty() && (!pending.sown_once || record->is_sound_loop)) {
      per_map_.just_sown_named_sounds.push_back(SownNamedSound{record->sound, pending.cell});
    }
    pending.sown_once = true;
    // CE QUI N'EST PAS INFINI NE SE REJOUE PAS. Le drapeau est le seul qui le
    // dise : le client garde son nœud vivant, et le nôtre meurt au bout de sa
    // vie — on le relance donc à sa durée, sans recouvrement.
    if (!record->is_infinite) {
      per_map_.pending_named.erase(per_map_.pending_named.begin() +
                                   static_cast<std::ptrdiff_t>(rank));
      continue;
    }
    pending.at_ms = now_ms + PeriodMs(effect->prepared.last_frame, 0);
    ++rank;
  }
}
namespace {

// LA GARDE QUE LE CLIENT NATIF TIENT AUTREMENT. Il vérifie que l'acteur ne
// porte pas déjà l'effet avant de le créer — ou il retire avant de reposer —
// pour qu'un même effet ne paraisse qu'une fois quand deux de ses quatre
// chemins le nomment. Nous n'avons pas besoin de lire l'écran pour cela : la
// table de la POSE dit ce qui sera déjà là.
[[nodiscard]] bool AlreadyPosedByState(ro::sim::StatusEffectId state, std::uint16_t effect_id) {
  return std::ranges::any_of(ro::render::StateEffectsFor(state),
                             [effect_id](const ro::render::StateEffectEntry& pose) {
                               return pose.effect_id == effect_id;
                             });
}

// L'EFFET D'ACTEUR QU'UN ÉTAT POSE OU RETIRE va sur son porteur, avec l'état
// qui l'a posé ; la levée de n'importe lequel des états qui le posent l'en
// retire, comme le client qui éteint toutes les instances de ce numéro. Vrai
// quand l'effet est de ceux-là.
bool QueueActorChange(EffectMapMemory& memory,
                      std::vector<EffectsInPlay::ActorEffectToRecord>& queue,
                      std::uint32_t effect_id, std::uint32_t actor_id,
                      const std::array<float, 3>& ground, std::uint32_t now_ms,
                      ro::sim::StatusEffectId state, bool withdrawn) {
  if (!ChangesOnlyItsBearer(effect_id)) {
    return false;
  }
  queue.push_back({.actor_id = actor_id,
                   .effect_id = effect_id,
                   .started_ms = now_ms,
                   .posed_by_state = state,
                   .withdrawn = withdrawn});
  const auto key = std::pair{actor_id, effect_id};
  if (withdrawn) {
    memory.state_actor_effects.erase(key);
    return true;
  }
  if (OwnLifeMs(effect_id).has_value()) {
    memory.state_actor_effects.insert_or_assign(key, now_ms);
  } else {
    memory.state_actor_effects.erase(key);
  }
  // Son bruit, s'il en a un, part avec l'instance — et une seule fois : `Notice`
  // la tient pour déjà entendue.
  memory.just_launched.push_back({effect_id, ground, actor_id});
  memory.noticed.insert_or_assign(key, now_ms);
  return true;
}

// LES EFFETS D'ACTEUR QU'UN ÉTAT A POSÉS ET QUI ONT FINI LEUR VIE partent de
// leur porteur, même si l'état tient encore. Seule l'instance que l'état a
// posée s'en va : un même numéro reposé depuis, par l'état ou par le serveur,
// a un autre instant de naissance et reste.
void ExpireStateActorEffects(const ro::sim::World& world, EffectMapMemory& memory,
                             std::vector<EffectsInPlay::ActorEffectToRecord>& queue,
                             std::uint32_t now_ms) {
  for (auto seen = memory.state_actor_effects.begin(); seen != memory.state_actor_effects.end();) {
    const auto [actor_id, effect_id] = seen->first;
    const std::uint32_t posed_ms = seen->second;
    const ro::sim::Actor* bearer = world.Find(actor_id);
    if (bearer == nullptr) {
      seen = memory.state_actor_effects.erase(seen);
      continue;
    }
    if (static_cast<std::uint64_t>(now_ms - posed_ms) < OwnLifeMs(effect_id).value_or(0)) {
      ++seen;
      continue;
    }
    const auto worn = std::ranges::find(bearer->effects, effect_id, &ro::sim::PlayedEffect::id);
    if (worn != bearer->effects.end() && worn->since_ms == posed_ms &&
        worn->posed_by_state.has_value()) {
      queue.push_back({.actor_id = actor_id,
                       .effect_id = effect_id,
                       .started_ms = now_ms,
                       .posed_by_state = worn->posed_by_state,
                       .withdrawn = true});
    }
    seen = memory.state_actor_effects.erase(seen);
  }
}

}  // namespace

void SettleActorEffects(ro::sim::World& world,
                        const std::vector<EffectsInPlay::ActorEffectToRecord>& poses) {
  // ⚠ UN RETRAIT RÉGLÉ ICI NE S'INSCRIT PAS AU JOURNAL DES RETRAITS. Les
  // effets l'ont décidé et ont déjà coupé leur instance ; inscrit, il se
  // relirait à l'image suivante comme un ordre du serveur et couperait
  // l'instance reposée depuis — une suite qui retire puis repose son effet,
  // Concentration du Lord Knight en tête, n'en garderait rien.
  for (const EffectsInPlay::ActorEffectToRecord& pose : poses) {
    if (pose.withdrawn) {
      static_cast<void>(world.ForgetEffect(pose.actor_id, pose.effect_id));
    }
  }
  for (const EffectsInPlay::ActorEffectToRecord& pose : poses) {
    if (!pose.withdrawn) {
      static_cast<void>(world.RecordEffect(pose.actor_id, pose.effect_id, pose.started_ms,
                                           std::nullopt, pose.posed_by_state));
    }
  }
}

void EffectsInPlay::StopById(std::uint32_t effect_id, std::uint32_t actor_id) {
  // 🔴 ON DÉSIGNE L'INSTANCE PAR L'IDENTIFIANT QUI L'A SEMÉE, et non par le
  // fichier qu'elle joue. `EffectFile` choisit parmi les variantes d'un effet
  // qui en a plusieurs, et le tirage vient de l'INSTANT : rechercher le fichier
  // au moment de couper ne rend pas celui qu'on a semé, et la coupe manquait
  // alors sa cible sans rien dire.
  std::erase_if(per_map_.playing, [effect_id, actor_id](const Playing& shown) {
    return shown.from_effect_id == effect_id &&
           (shown.actor_id == actor_id || shown.loops_with_actor == actor_id);
  });
  // 🔴 ET LE CÔTÉ PROCÉDURAL, que cette coupe ne touchait pas. Un effet peut
  // n'avoir aucun fichier — la nuée de Sight Blaster est semée nœud par nœud —,
  // et ne retirer que `per_map_.playing` la laissait tourner jusqu'au
  // changement de carte. Le client, lui, détruit l'instance sans savoir de
  // quoi elle est faite.
  nodes_->StopEffect(effect_id, actor_id);
  // Et le côté des classes : une classe qu'un état a posée part à sa levée.
  classes_.Stop(effect_id, actor_id);
  // Et la teinte de celui qui dessinait en teignant : voir `AlsoChangeBearer`.
  AlsoChangeBearer(effect_id, actor_id, 0, true);
}

bool EffectsInPlay::Carries(std::uint32_t effect_id, std::uint32_t actor_id) const {
  return std::ranges::any_of(
             per_map_.playing,
             [effect_id, actor_id](const Playing& shown) {
               return shown.from_effect_id == effect_id &&
                      (shown.actor_id == actor_id || shown.loops_with_actor == actor_id);
             }) ||
         nodes_->CarriesEffect(effect_id, actor_id) || classes_.Carries(effect_id, actor_id);
}

bool SkillSuiteConditionHolds(const EffectsInPlay* effects, const ro::render::SkillSuiteStep& step,
                              std::uint32_t caster_id, std::uint32_t target_id) {
  if (step.condition_effect_id == 0) {
    return true;
  }
  const std::uint32_t bearer_id =
      step.condition_carrier == ro::render::SkillSuiteCarrier::kCaster ? caster_id : target_id;
  const auto carries = [&](std::uint32_t effect_id) {
    return effects != nullptr && effects->Carries(effect_id, bearer_id);
  };
  return carries(step.condition_effect_id) == step.condition_carried &&
         (step.condition2_effect_id == 0 ||
          carries(step.condition2_effect_id) == step.condition2_carried);
}

void EffectsInPlay::PlayRebuildStateEffects(ro::sim::StatusEffectId state, std::uint32_t actor_id,
                                            std::optional<std::int32_t> state_value,
                                            bool sprite_rebuilt, const std::array<float, 3>& ground,
                                            std::uint32_t now_ms) {
  if (!sprite_rebuilt) {
    return;
  }
  for (const ro::render::StateEffectEntry& entry : ro::render::StateRebuildEffectsFor(state)) {
    if (AlreadyPosedByState(state, entry.effect_id) ||
        QueueActorChange(per_map_, actor_effects_to_record_, entry.effect_id, actor_id, ground,
                         now_ms, state, false)) {
      continue;
    }
    static_cast<void>(
        PlayStateEffect(entry.effect_id, actor_id, state_value, ground, now_ms, entry.endless));
  }
}

void EffectsInPlay::PlayStateValueEffects(const ro::sim::World& world, const MapStage& stage,
                                          ro::sim::WalkPlanner& planner, std::uint32_t now_ms) {
  std::erase_if(per_map_.state_values,
                [&world](const auto& vu) { return !world.actors().contains(vu.first.first); });
  for (const ro::render::StateValueEffectEntry& record : ro::render::StateValueEffectTable()) {
    for (const auto& [id, actor] : world.actors()) {
      if (record.own_player_only && id != world.player_id()) {
        continue;
      }
      const auto carries = actor.status_values.find(record.state_id);
      const std::optional<std::int32_t> value =
          carries == actor.status_values.end() ? std::nullopt : std::optional{carries->second};
      const auto map_key = std::pair{id, record.state_id};
      const auto vu = per_map_.state_values.find(map_key);
      const std::optional<std::int32_t> ancienne =
          vu == per_map_.state_values.end() ? std::nullopt : std::optional{vu->second};
      if (value == ancienne) {
        continue;
      }
      // ⚠ TOUTE LA FAMILLE PART D'ABORD, ET LES DEUX FAMILLES : c'est ce que le
      // client fait, et c'est ce qui empêche les rangs de s'empiler. Balayer la
      // seule famille en vigueur laisserait l'anneau de l'autre à l'écran pour
      // qui bascule `/mineffect` en plein combat.
      for (std::int32_t rang = record.min_value; rang <= record.max_value; ++rang) {
        StopById(static_cast<std::uint32_t>(rang + record.offset), id);
        if (record.minimal_offset != 0) {
          StopById(static_cast<std::uint32_t>(rang + record.minimal_offset), id);
        }
      }
      if (!value.has_value()) {
        per_map_.state_values.erase(map_key);
        continue;
      }
      per_map_.state_values.insert_or_assign(map_key, *value);
      // ⚠ HORS DES BORNES, LE CLIENT NE MONTRE RIEN — et il ne montre pas non
      // plus le rang le plus proche. Un serveur qui annoncerait onze tours
      // laisserait donc l'anneau éteint, comme chez lui.
      if (*value < record.min_value || *value > record.max_value) {
        continue;
      }
      const ro::sim::FractionalCell cell =
          ro::sim::InterpolatedCell(actor, planner.PathFor(actor), now_ms);
      const std::array<float, 3> sol = GroundPositionAt(stage, cell.x, cell.y);
      // 🔴 C'EST ICI QUE `/mineffect` MORD, au calcul même de l'identifiant, et
      // non au chargement du fichier : le client lit son réglage 229 à l'endroit
      // où il choisit le nombre. Rolling Cutter pose 1300..1309 en plein et
      // 1467..1476 en allégé — deux dossiers, les mêmes noms de fichiers.
      const std::int32_t family =
          minimal_effects_ && record.minimal_offset != 0 ? record.minimal_offset : record.offset;
      PlayById(static_cast<std::uint32_t>(*value + family), glm::vec3(sol[0], sol[1], sol[2]),
               now_ms, id, record.endless);
    }
  }
}

void EffectsInPlay::NoticeStateEffects(const ro::sim::World& world, const MapStage& stage,
                                       ro::sim::WalkPlanner& planner, std::uint32_t now_ms) {
  player_id_ = world.player_id();
  ExpireStateActorEffects(world, per_map_, actor_effects_to_record_, now_ms);
  // UN ACTEUR PARTI EMPORTE SES ÉTATS, et ses semis avec eux : sans cela, un
  // effet infini tournerait pour toujours sur une case vide, et le premier état
  // de qui reprendrait son identifiant se croirait déjà posé.
  for (auto known = per_map_.named_states.begin(); known != per_map_.named_states.end();) {
    if (world.actors().contains(known->first)) {
      ++known;
      continue;
    }
    const std::uint32_t gone_id = known->first;
    known = per_map_.named_states.erase(known);
    std::erase_if(per_map_.pending_named, [gone_id](const PendingNamed& waiting) {
      return waiting.owner_id == gone_id && !waiting.from_unit;
    });
  }

  for (const auto& [id, actor] : world.actors()) {
    std::set<ro::sim::StatusEffectId> shown_states;
    for (const ro::sim::StatusEffectId state : actor.status_effects) {
      // On ne retient que ce qui a quelque chose à montrer : les états sont des
      // centaines, et les suivre tous coûterait sans rien montrer de plus.
      //
      // ⚠ SIX TABLES, ET UN ÉTAT PEUT N'ÊTRE QUE DANS LA DERNIÈRE. Les effets
      // NOMMÉS donnent des fichiers ; les quatre fonctions d'état donnent des
      // identifiants, à trois instants différents ; et trois états portent un
      // SON. Ne filtrer que sur la première laisserait ces tables sans effet.
      if (!ro::render::StateNamedEffects(state, ro::render::kStateNamedOnGained).empty() ||
          !ro::render::StateNamedEffects(state, ro::render::kStateNamedOnLost).empty() ||
          !ro::render::StateEffectsFor(state).empty() ||
          !ro::render::StatePacketEffectsFor(state).empty() ||
          !ro::render::StateRebuildEffectsFor(state).empty() ||
          !ro::render::StateRemovalsFor(state).empty() ||
          !ro::render::StateSoundFor(state).empty()) {
        shown_states.insert(state);
      }
    }
    // LE PREMIER PASSAGE SUR UN ACTEUR est le seul où son sprite naît sous nos
    // yeux ; c'est là, et seulement là, que le rebâtissage a un sens.
    const bool first_time = !per_map_.named_states.contains(id);
    std::set<ro::sim::StatusEffectId>& seen_states = per_map_.named_states[id];
    if (seen_states == shown_states) {
      continue;
    }
    // ⚠ LE SERVEUR NE DIT PAS CE QUI VIENT DE CHANGER : il donne la liste de ce
    // qui tient. La pose et la levée ne se lisent qu'en comparant au passage
    // précédent.
    const ro::sim::FractionalCell cell =
        ro::sim::InterpolatedCell(actor, planner.PathFor(actor), now_ms);
    const ro::sim::CellPosition posed_cell{static_cast<std::uint16_t>(cell.x),
                                           static_cast<std::uint16_t>(cell.y)};
    const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
    // UN EFFET PAR NUMÉRO d'une case d'état, posé ou retiré comme ceux des
    // tables de l'état : sur l'acteur s'il ne fait que le changer, joué ou
    // coupé sinon — avec le paramètre de la case, ou la valeur de l'état.
    const auto play_numbered = [&](const ro::render::StateNamedEffectEntry& entry,
                                   ro::sim::StatusEffectId state,
                                   std::optional<std::int32_t> state_value) {
      const bool withdrawn = entry.verb != ro::render::NamedEffectVerb::kSow;
      if (QueueActorChange(per_map_, actor_effects_to_record_, entry.effect_id, id, ground, now_ms,
                           state, withdrawn)) {
        return;
      }
      if (withdrawn) {
        StopById(entry.effect_id, id);
        return;
      }
      static_cast<void>(
          PlayStateEffect(entry.effect_id, id,
                          entry.effect_param >= 0 ? std::optional{entry.effect_param} : state_value,
                          ground, now_ms, entry.endless));
    };
    for (const ro::sim::StatusEffectId state : shown_states) {
      if (seen_states.contains(state)) {
        continue;
      }
      const std::optional<std::int32_t> state_value =
          actor.status_values.contains(state) ? std::optional{actor.status_values.at(state)}
                                              : std::nullopt;
      // ⚠ UN ÉTAT QUI LIT SA VALEUR joue ses noms de pose ailleurs, à chaque
      // valeur nouvelle, bruit compris : voir `PlayStateNamedValues`. Ses
      // numéros, eux, partent ici, une fois.
      const std::span<const ro::render::StateNamedEffectEntry> on_gained =
          ro::render::StateNamedEffects(state, ro::render::kStateNamedOnGained);
      const bool reads_value =
          std::ranges::any_of(on_gained, [](const ro::render::StateNamedEffectEntry& entry) {
            return ro::render::StateNamedEntryReadsValue(entry);
          });
      for (const ro::render::StateNamedEffectEntry& entry : on_gained) {
        if (entry.effect_id != 0) {
          play_numbered(entry, state, state_value);
        } else if (!reads_value) {
          PlayNamedEntry(entry.name, entry.verb, id, false, posed_cell, entry.anchor, false,
                         now_ms);
        }
      }
      if (!reads_value) {
        PlayNamedMethodSounds(NamedMethodFamily::kState, state, ro::render::kStateNamedOnGained, id,
                              posed_cell, posed_cell, per_map_.just_sown_named_sounds);
      }
      // 🔴 ET LES EFFETS PAR IDENTIFIANT, À LA POSE. Le client les fait naître
      // en traitant le message 110 ; la table dit lesquels sont gardés par le
      // drapeau « posé », et ceux-là seuls sont ici. Les deux qui ne naissent
      // qu'à la LEVÉE sont plus bas.
      for (const ro::render::StateEffectEntry& entry : ro::render::StateEffectsFor(state)) {
        if (entry.moment == ro::render::StateEffectMoment::kOnRelease ||
            QueueActorChange(per_map_, actor_effects_to_record_, entry.effect_id, id, ground,
                             now_ms, state, false)) {
          continue;
        }
        // 🔴 UNE AURA REBOUCLE, UN ÉCLAT NON. Le client déclare la durée de
        // chaque effet et `99999999` y est son infini : ces effets-là vivent
        // jusqu'à ce qu'on les retire. Nous les jouions une fois, c'est-à-dire
        // deux secondes pour un Steel Body qui dure une minute.
        static_cast<void>(
            PlayStateEffect(entry.effect_id, id, state_value, ground, now_ms, entry.endless));
      }
      // ⚠ ET CE QUE LES DEUX RECEVEUSES DU PAQUET CRÉENT ELLES-MÊMES, avant de
      // poster ce message. C'est le MÊME instant — un seul paquet, deux
      // fonctions — et six de leurs dix effets sont ceux que la table de la
      // pose porte déjà : le client les RETIRE PUIS LES REPOSE, ce qui fait une
      // instance et non deux. Les rejouer par-dessus en ferait deux chez nous.
      for (const ro::render::StateEffectEntry& entry : ro::render::StatePacketEffectsFor(state)) {
        if (AlreadyPosedByState(state, entry.effect_id) ||
            QueueActorChange(per_map_, actor_effects_to_record_, entry.effect_id, id, ground,
                             now_ms, state, false)) {
          continue;
        }
        static_cast<void>(
            PlayStateEffect(entry.effect_id, id, state_value, ground, now_ms, entry.endless));
      }
      // LE SON DU CHANGEMENT D'ÉTAT. Trois états en portent un, et il passe par
      // le même canal que les sons des effets nommés : `app` les entend après
      // coup, parce que le rendu ne connaît pas l'audio.
      if (const std::string_view sound = ro::render::StateSoundFor(state); !sound.empty()) {
        per_map_.just_sown_named_sounds.push_back(SownNamedSound{sound, posed_cell});
      }
      PlayRebuildStateEffects(state, id, state_value, first_time && id == world.player_id(), ground,
                              now_ms);
    }
    for (const ro::sim::StatusEffectId state : seen_states) {
      if (shown_states.contains(state)) {
        continue;
      }
      for (const ro::render::StateNamedEffectEntry& entry :
           ro::render::StateNamedEffects(state, ro::render::kStateNamedOnLost)) {
        if (entry.effect_id != 0) {
          play_numbered(entry, state, std::nullopt);
          continue;
        }
        PlayNamedEntry(entry.name, entry.verb, id, false, posed_cell, entry.anchor, false, now_ms);
      }
      // 🔴 CE QUE LA LEVÉE EMPORTE. Le client retire par deux voies — l'appel
      // direct, ou le message 109 envoyé à l'acteur —, dans la branche « non
      // posé » ou sans garde du tout : un retrait sans garde tombe aussi à la
      // levée.
      for (const ro::render::StateEffectEntry& entry : ro::render::StateRemovalsFor(state)) {
        if (!QueueActorChange(per_map_, actor_effects_to_record_, entry.effect_id, id, ground,
                              now_ms, state, true)) {
          StopById(entry.effect_id, id);
        }
      }
      const auto removed_on_release = [state](std::uint16_t effect_id) {
        return std::ranges::any_of(ro::render::StateRemovalsFor(state),
                                   [effect_id](const ro::render::StateEffectEntry& removal) {
                                     return removal.effect_id == effect_id;
                                   });
      };
      // ⚠ ET TOUT CE QUE CET ÉTAT TENAIT EN VIE. Les retraits couvrent
      // quarante-sept des cinquante auras ; cette boucle éteint les trois
      // autres. Deux ne naissent qu'au rebâtissage (586 et 673), où aucun
      // retrait n'est relevé, et l'état 1205 ne retire 164 qu'à la pose, avant
      // de le reposer. Que le client les éteigne à la levée n'est PAS mesuré :
      // sans cette boucle, elles tourneraient jusqu'à la déconnexion.
      for (const std::span<const ro::render::StateEffectEntry> table :
           {ro::render::StateEffectsFor(state), ro::render::StatePacketEffectsFor(state),
            ro::render::StateRebuildEffectsFor(state)}) {
        for (const ro::render::StateEffectEntry& entry : table) {
          if (!entry.endless) {
            continue;
          }
          // Un effet d'acteur que les retraits n'ont pas déjà emporté part
          // de son porteur, et non de l'écran.
          if (ChangesOnlyItsBearer(entry.effect_id)) {
            if (!removed_on_release(entry.effect_id)) {
              static_cast<void>(QueueActorChange(per_map_, actor_effects_to_record_,
                                                 entry.effect_id, id, ground, now_ms, state, true));
            }
            continue;
          }
          StopById(entry.effect_id, id);
        }
      }
      // Et les deux effets qui ne naissent qu'À LA LEVÉE : un éclat de fin, et
      // non une aura. Les jouer à la pose les montrerait à l'envers.
      for (const ro::render::StateEffectEntry& entry : ro::render::StateEffectsFor(state)) {
        if (entry.moment != ro::render::StateEffectMoment::kOnRelease ||
            QueueActorChange(per_map_, actor_effects_to_record_, entry.effect_id, id, ground,
                             now_ms, state, false)) {
          continue;
        }
        static_cast<void>(
            PlayStateEffect(entry.effect_id, id, std::nullopt, ground, now_ms, false));
      }
      if (const std::string_view sound = ro::render::StateSoundFor(state); !sound.empty()) {
        per_map_.just_sown_named_sounds.push_back(SownNamedSound{sound, posed_cell});
      }
    }
    seen_states = std::move(shown_states);
  }
  RenewStateClasses(world, stage, planner, now_ms);
  PlayStateValueEffects(world, stage, planner, now_ms);
  PlayStateNamedValues(world, stage, planner, now_ms);
}

}  // namespace moonclient
