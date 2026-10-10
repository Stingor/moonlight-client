// CE QU'UN ÉTAT POSE SUR SON PORTEUR QUAND CE N'EST NI UN FICHIER NI DES NŒUDS :
// une classe du gestionnaire d'effets, qui reçoit la valeur de l'état, ou un
// effet dont le cas ne fait que sonner.

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <utility>

#include "ro/render/actor_effect_table.h"
#include "ro/render/effect_case_table.h"
#include "ro/render/effect_class.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_sound_table.h"
#include "ro/render/effect_table.h"
#include "ro/render/state_effect.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "effects/effect_view.h"

namespace moonclient {
namespace {

// Le paramètre qu'une classe reçoit quand l'état n'a pas dit sa valeur.
constexpr std::int32_t kNoStateValue = -1;

// Vrai quand l'état pose au moins une classe portée.
[[nodiscard]] bool PosesPortedClass(ro::sim::StatusEffectId state) {
  for (const ro::render::StateEffectEntry& entry : ro::render::StateEffectsFor(state)) {
    if (entry.moment != ro::render::StateEffectMoment::kOnRelease &&
        ro::render::IsPortedEffectClass(entry.effect_id)) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] std::optional<std::int32_t> StateValueOf(const ro::sim::Actor& bearer,
                                                       ro::sim::StatusEffectId state) {
  const auto found = bearer.status_values.find(state);
  return found == bearer.status_values.end() ? std::nullopt : std::optional{found->second};
}

}  // namespace

// 🔴 L'INSTANCE QUI DESSINE EST AUSSI CELLE QUI TEINT. Le client ne fait naître
// qu'un effet : sa mise à jour fait défiler son fichier ou ses nœuds ET réécrit la
// couleur de son porteur à chaque tic — Concentration du Lord Knight (369) joue
// `TwoHand.str` une fois et garde le corps jaune pâle tant que l'état tient.
// Le dessin part par le chemin ordinaire ; le changement s'inscrit sur l'acteur,
// que la table des effets d'acteur teint, et `Notice` le tient pour déjà vu —
// sinon il rejouerait le fichier et son bruit une seconde fois.
//
// ⚠ UN EFFET QUI NE FAIT QUE CHANGER SON PORTEUR N'EST PAS DE CEUX-LÀ : il passe
// par `QueueActorChange`, et une classe tient son porteur elle-même.
void EffectsInPlay::AlsoChangeBearer(std::uint32_t effect_id, std::uint32_t bearer_id,
                                     std::uint32_t started_ms, bool withdrawn) {
  const bool draws =
      !ro::render::EffectFile(effect_id).empty() || ro::render::KnowsProceduralEffect(effect_id);
  if (bearer_id == 0 || !draws || ro::render::IsPortedEffectClass(effect_id) ||
      ro::render::ActorEffectChangeFor(effect_id) == nullptr) {
    return;
  }
  actor_effects_to_record_.push_back({.actor_id = bearer_id,
                                      .effect_id = effect_id,
                                      .started_ms = started_ms,
                                      .posed_by_state = std::nullopt,
                                      .withdrawn = withdrawn});
  if (!withdrawn) {
    per_map_.noticed.insert_or_assign(std::pair{bearer_id, effect_id}, started_ms);
  }
}

// 🔴 UNE CLASSE D'ÉTAT REÇOIT LA VALEUR DE L'ÉTAT EN PARAMÈTRE : le niveau de
// Climax choisit le fond, le compte d'Eternal Slash choisit son chiffre.
//
// ⚠ ET UN EFFET SANS FIGURE N'EST PAS UN EFFET MUET. Le cas de Steel Body ne
// dessine rien, il fait entendre son bruit à la naissance : ne rien jouer parce
// qu'il n'y a rien à peindre le rendait silencieux.
//
// ⚠ UNE CLASSE D'ÉCRAN NE VAUT QUE POUR SON PROPRE PERSONNAGE : le givre d'une
// gelure couvre l'écran de qui la subit, et le client ne le pose sur personne
// d'autre. Elle se tient pour jouée sur un autre porteur.
bool EffectsInPlay::PlayStateEffect(std::uint32_t effect_id, std::uint32_t bearer_id,
                                    std::optional<std::int32_t> state_value,
                                    const std::array<float, 3>& ground, std::uint32_t now_ms,
                                    bool endless) {
  const glm::vec3 place(ground[0], ground[1], ground[2]);
  if (ro::render::EffectClassOwnPlayerOnly(effect_id) && bearer_id != player_id_) {
    return true;
  }
  if (ro::render::IsPortedEffectClass(effect_id)) {
    return classes_.Spawn({.effect_id = effect_id,
                           .owner_id = bearer_id,
                           .ground_position = place,
                           .level = state_value.value_or(kNoStateValue)},
                          now_ms);
  }
  // LA TEINTE NE DÉPEND PAS DU FICHIER : le cas la réécrit avant de le faire défiler.
  const bool played = PlayById(effect_id, place, now_ms, bearer_id, endless);
  AlsoChangeBearer(effect_id, bearer_id, now_ms, false);
  if (played) {
    return true;
  }
  // UN CAS SANS FIGURE A POURTANT SON INSTANCE, que la garde d'une suite
  // interroge : la course du Taekwon pose son marqueur d'arrêt ainsi.
  if (ro::render::EffectCaseFor(effect_id) != nullptr) {
    MarkCarried(per_map_, effect_id, bearer_id, now_ms);
  }
  if (ro::render::EffectCaseFor(effect_id) == nullptr ||
      ro::render::EffectSounds(effect_id).empty()) {
    return false;
  }
  NoteLaunched(effect_id, ground, bearer_id);
  return true;
}

// 🔴 LE CLIENT REFAIT NAÎTRE LES CLASSES D'UN ÉTAT À CHAQUE MESSAGE DE POSE,
// après avoir retiré ce que l'état retire à chaque message. Le monde ne garde de
// ces messages que la liste des états et leur valeur : c'est donc une valeur
// nouvelle, l'état tenant toujours, qui les refait naître ici. Le compte d'Eternal Slash monte
// ainsi d'un chiffre à chaque coup ; Climax change de fond sans s'empiler.
void EffectsInPlay::RenewStateClasses(const ro::sim::World& world, const MapStage& stage,
                                      ro::sim::WalkPlanner& planner, std::uint32_t now_ms) {
  player_id_ = world.player_id();
  std::erase_if(per_map_.state_class_values, [&world](const auto& seen) {
    const ro::sim::Actor* bearer = world.Find(seen.first.first);
    return bearer == nullptr || !bearer->status_effects.contains(seen.first.second);
  });
  for (const auto& [id, actor] : world.actors()) {
    for (const ro::sim::StatusEffectId state : actor.status_effects) {
      if (!PosesPortedClass(state)) {
        continue;
      }
      const std::optional<std::int32_t> value = StateValueOf(actor, state);
      const auto [seen, first] = per_map_.state_class_values.try_emplace({id, state}, value);
      // La première valeur est celle de la pose, que le chemin des états a déjà
      // jouée.
      if (first || seen->second == value) {
        continue;
      }
      seen->second = value;
      const ro::sim::FractionalCell cell =
          ro::sim::InterpolatedCell(actor, planner.PathFor(actor), now_ms);
      const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
      for (const ro::render::StateEffectEntry& removal : ro::render::StateRemovalsFor(state)) {
        if (removal.moment == ro::render::StateEffectMoment::kAlways &&
            ro::render::IsPortedEffectClass(removal.effect_id)) {
          classes_.Stop(removal.effect_id, id);
        }
      }
      for (const ro::render::StateEffectEntry& entry : ro::render::StateEffectsFor(state)) {
        if (entry.moment != ro::render::StateEffectMoment::kOnRelease &&
            ro::render::IsPortedEffectClass(entry.effect_id)) {
          static_cast<void>(
              PlayStateEffect(entry.effect_id, id, value, ground, now_ms, entry.endless));
        }
      }
    }
  }
}

}  // namespace moonclient
