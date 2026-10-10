// LA LISTE DES EFFETS PORTÉS PAR UN ACTEUR : ce qui y entre, ce que le plafond
// en chasse, et le retrait qui l'efface et l'inscrit au journal des retraits.
//
// Elle vit à part de `world.cpp` pour tenir le plafond de huit cents lignes.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "ro/sim/movement.h"
#include "ro/sim/world.h"

namespace ro::sim {
namespace {

// Combien de retraits le journal d'un acteur garde : autant que d'effets.
constexpr std::size_t kRemovalsKeptPerActor = kEffectsKeptPerActor;

// Note l'effet, ou le repart de zéro s'il est déjà là. Un effet d'office le
// reste quand il est renoté.
void Note(std::vector<PlayedEffect>& effects, PlayedEffect played) {
  const auto already = std::ranges::find(effects, played.id, &PlayedEffect::id);
  if (already != effects.end()) {
    played.lasting = played.lasting || already->lasting;
    *already = played;
    return;
  }
  // 🔴 LE PLUS ANCIEN EFFET ORDINAIRE s'en va, jamais un effet d'office : le
  // client les garde tant que l'acteur vit, et les reposer les ferait repartir
  // de zéro au milieu d'un combat.
  if (effects.size() >= kEffectsKeptPerActor) {
    const auto ordinary = std::ranges::find(effects, false, &PlayedEffect::lasting);
    effects.erase(ordinary != effects.end() ? ordinary : effects.begin());
  }
  effects.push_back(played);
}

}  // namespace

bool World::RecordEffect(std::uint32_t id, std::uint32_t effect_id, std::uint32_t since_ms,
                         std::optional<std::int32_t> value,
                         std::optional<StatusEffectId> posed_by_state) {
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return false;
  }
  Note(found->second.effects, PlayedEffect{effect_id, since_ms, value, posed_by_state});
  return true;
}

bool World::RecordLastingEffect(std::uint32_t id, std::uint32_t effect_id, std::uint32_t since_ms) {
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return false;
  }
  Note(found->second.effects, PlayedEffect{.id = effect_id, .since_ms = since_ms, .lasting = true});
  return true;
}

bool World::NoteDeparture(std::uint32_t id, std::uint32_t effect_id, std::uint32_t now_ms) {
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return false;
  }
  departures_.Note(effect_id, PositionAt(found->second, walk_map_, now_ms), now_ms);
  return true;
}

bool World::RemoveEffect(std::uint32_t id, std::uint32_t effect_id) {
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return false;
  }
  Actor& actor = found->second;
  if (actor.removed_effects.size() >= kRemovalsKeptPerActor) {
    actor.removed_effects.erase(actor.removed_effects.begin());
  }
  actor.removed_effects.push_back(effect_id);
  ++actor.removed_effects_total;
  return ForgetEffect(id, effect_id);
}

bool World::ForgetEffect(std::uint32_t id, std::uint32_t effect_id) {
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return false;
  }
  return std::erase_if(found->second.effects, [effect_id](const PlayedEffect& effect) {
           return effect.id == effect_id;
         }) != 0;
}

}  // namespace ro::sim
