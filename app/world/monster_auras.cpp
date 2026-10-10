#include "world/monster_auras.h"

#include <algorithm>
#include <vector>

#include "ro/sim/world.h"

#include "world/actor_visibility.h"

namespace moonclient {
namespace {

// Vrai quand l'acteur porte l'un au moins de ses effets d'office.
[[nodiscard]] bool WearsAny(const ro::sim::Actor& actor,
                            const ro::vfs::MonsterSizeEffects::Entry& entry) {
  return std::ranges::any_of(actor.effects, [&entry](const ro::sim::PlayedEffect& played) {
    return std::ranges::find(entry.effect_ids, played.id) != entry.effect_ids.end();
  });
}

}  // namespace

void DressMonsters(ro::sim::World& world, const ro::vfs::MonsterSizeEffects* table,
                   std::uint32_t now_ms) {
  if (table == nullptr) {
    return;
  }
  // ⚠ COLLECTÉ D'ABORD : on ne note pas d'effet sur ce qu'on parcourt.
  struct Change {
    std::uint32_t actor_id = 0;
    const ro::vfs::MonsterSizeEffects::Entry* entry = nullptr;
    bool dress = false;
  };
  std::vector<Change> changes;
  for (const auto& [id, actor] : world.actors()) {
    // Un joueur ne passe pas par ce chemin chez le client.
    if (actor.kind == ro::sim::ActorKind::kPlayer) {
      continue;
    }
    const ro::vfs::MonsterSizeEffects::Entry* entry = table->For(actor.look.job);
    if (entry == nullptr || entry->effect_ids.empty()) {
      continue;
    }
    // Caché, dissimulé ou camouflé, le client retire ses effets ; visible, il
    // les pose.
    const bool shown = ConcealmentOf(actor, world) == Concealment::kVisible;
    if (shown != WearsAny(actor, *entry)) {
      changes.push_back(Change{.actor_id = id, .entry = entry, .dress = shown});
    }
  }
  for (const Change& change : changes) {
    for (const std::uint32_t effect : change.entry->effect_ids) {
      if (change.dress) {
        static_cast<void>(world.RecordLastingEffect(change.actor_id, effect, now_ms));
      } else {
        static_cast<void>(world.RemoveEffect(change.actor_id, effect));
      }
    }
  }
}

}  // namespace moonclient
