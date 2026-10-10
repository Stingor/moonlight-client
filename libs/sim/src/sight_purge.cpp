// Les restes du monde : hors de la vue du serveur, ou debout à zéro PV. Voir
// l'en-tête.

#include "ro/sim/sight_purge.h"

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <utility>

#include "ro/sim/actor.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

namespace ro::sim {
namespace {

// Un monstre dont le serveur a dit les PV, à zéro exact, et pas encore couché.
[[nodiscard]] bool StandsAtZeroHealth(const Actor& actor) noexcept {
  return actor.kind == ActorKind::kMonster && actor.health == 0 && actor.max_health > 0 &&
         actor.posture != Posture::kDead;
}

// Depuis quand ce suspect l'est : l'instant noté, ou maintenant s'il est neuf.
[[nodiscard]] std::uint32_t SinceOf(const std::unordered_map<std::uint32_t, std::uint32_t>& noted,
                                    std::uint32_t actor_id, std::uint32_t now_ms) {
  const auto seen = noted.find(actor_id);
  return seen != noted.end() ? seen->second : now_ms;
}

}  // namespace

std::vector<PurgedActor> SightPurge::Sweep(World& world, int view_cells, std::uint32_t now_ms) {
  std::vector<PurgedActor> purged;
  const Actor* player = world.player();
  const std::optional<CellPosition> center =
      player != nullptr ? std::optional<CellPosition>(StandingCell(*player)) : std::nullopt;
  // LA VUE NE SE JUGE QUE SI LE SERVEUR A DIT SON RAYON ; les zéro PV, toujours.
  const bool sight_known = view_cells > 0 && center.has_value();
  const int bound_cells = view_cells + kSightPurgeMarginCells;
  std::unordered_map<std::uint32_t, std::uint32_t> still_beyond;
  std::unordered_map<std::uint32_t, std::uint32_t> still_zero;
  for (const auto& [actor_id, actor] : world.actors()) {
    if (actor_id == world.player_id()) {
      continue;
    }
    const CellPosition cell = StandingCell(actor);
    const int dx_cells = center.has_value() ? static_cast<int>(cell.x) - center->x : 0;
    const int dy_cells = center.has_value() ? static_cast<int>(cell.y) - center->y : 0;
    const PurgedActor gone{
        .actor_id = actor_id, .job = actor.look.job, .dx_cells = dx_cells, .dy_cells = dy_cells};
    if (StandsAtZeroHealth(actor)) {
      const std::uint32_t since_ms = SinceOf(zero_since_ms_, actor_id, now_ms);
      if (now_ms - since_ms >= kZeroHealthGraceMs) {
        purged.push_back(gone);
        purged.back().reason = PurgeReason::kZeroHealth;
        continue;
      }
      still_zero.emplace(actor_id, since_ms);
    }
    if (!sight_known || std::max(std::abs(dx_cells), std::abs(dy_cells)) <= bound_cells) {
      continue;
    }
    const std::uint32_t since_ms = SinceOf(beyond_since_ms_, actor_id, now_ms);
    if (now_ms - since_ms >= kSightPurgeGraceMs) {
      purged.push_back(gone);
      continue;
    }
    still_beyond.emplace(actor_id, since_ms);
  }
  // Un acteur revenu dans le carré, couché, ou parti, repart de zéro.
  beyond_since_ms_ = std::move(still_beyond);
  zero_since_ms_ = std::move(still_zero);
  for (const PurgedActor& each : purged) {
    world.Remove(each.actor_id);
  }
  return purged;
}

}  // namespace ro::sim
