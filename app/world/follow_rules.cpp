#include "world/follow_rules.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string_view>
#include <utility>

#include "ro/sim/movement.h"
#include "ro/sim/walk_field.h"
#include "ro/sim/walk_map.h"
#include "ro/sim/world.h"

namespace moonclient {
namespace {

constexpr std::string_view kFollowCommand = "/follow";

[[nodiscard]] bool SameWord(std::string_view typed, std::string_view word) {
  return std::ranges::equal(typed, word, [](char left, char right) {
    return std::tolower(static_cast<unsigned char>(left)) ==
           std::tolower(static_cast<unsigned char>(right));
  });
}

[[nodiscard]] std::string_view Trimmed(std::string_view text) {
  const std::size_t first = text.find_first_not_of(' ');
  if (first == std::string_view::npos) {
    return {};
  }
  return text.substr(first, text.find_last_not_of(' ') - first + 1);
}

// La case où l'acteur se trouve à cet instant : celle de sa position, arrondie,
// au milieu d'un pas s'il marche.
[[nodiscard]] ro::sim::CellPosition CurrentCell(const ro::sim::Actor& actor,
                                                const ro::sim::WalkMap* walk,
                                                std::uint32_t now_ms) {
  const ro::sim::FractionalCell at = ro::sim::PositionAt(actor, walk, now_ms);
  ro::sim::CellPosition cell;
  cell.x = static_cast<std::uint16_t>(std::max(0L, std::lround(at.x)));
  cell.y = static_cast<std::uint16_t>(std::max(0L, std::lround(at.y)));
  return cell;
}

}  // namespace

bool CanFollow(const ro::sim::Actor& actor, std::uint32_t own_id) {
  return actor.kind == ro::sim::ActorKind::kPlayer && actor.id != own_id;
}

void BeginFollow(FollowState& state, const ro::sim::Actor& target) {
  state = FollowState();
  state.followed_id = target.id;
  state.followed_name = target.name;
}

void EndFollow(FollowState& state) { state = FollowState(); }

const ro::sim::Actor* FollowedPlayer(const ro::sim::World& world, std::uint32_t followed_id) {
  if (followed_id == 0) {
    return nullptr;
  }
  const ro::sim::Actor* actor = world.Find(followed_id);
  return actor != nullptr && actor->kind == ro::sim::ActorKind::kPlayer ? actor : nullptr;
}

const ro::sim::Actor* PlayerInViewNamed(const ro::sim::World& world, std::string_view name) {
  // Deux homonymes en vue : le plus petit identifiant, pour que la même
  // commande désigne toujours le même.
  const ro::sim::Actor* found = nullptr;
  for (const auto& [id, actor] : world.actors()) {
    if (CanFollow(actor, world.player_id()) && actor.name == name &&
        (found == nullptr || id < found->id)) {
      found = &actor;
    }
  }
  return found;
}

FollowDecision AdvanceFollow(FollowState& state, const ro::sim::World& world,
                             const ro::sim::WalkMap* walk, const FollowTick& tick) {
  const bool left_pressed = std::exchange(state.left_pressed, false);
  if (!state.active()) {
    return {};
  }
  if (left_pressed || tick.left_held) {
    return {.step = FollowStep::kStop, .end = FollowEnd::kLetGo};
  }
  const ro::sim::Actor* followed = FollowedPlayer(world, state.followed_id);
  if (followed == nullptr) {
    if (!state.lost_since_ms.has_value()) {
      state.lost_since_ms = tick.now_ms;
    }
    if (tick.now_ms - *state.lost_since_ms > kFollowLostMs) {
      return {.step = FollowStep::kStop, .end = FollowEnd::kOutOfSight};
    }
    return {};
  }
  state.lost_since_ms.reset();
  if (state.last_step_ms.has_value() && tick.now_ms - *state.last_step_ms <= kFollowStepMs) {
    return {};
  }
  state.last_step_ms = tick.now_ms;
  ro::sim::CellPosition cell = CurrentCell(*followed, walk, tick.now_ms);
  const ro::sim::Actor* self = world.player();
  const bool sitting = self != nullptr && self->posture == ro::sim::Posture::kSitting;
  if (sitting || tick.in_chat_room) {
    return {.step = FollowStep::kTurn, .cell = cell};
  }
  if (walk != nullptr && !walk->Passable(cell.x, cell.y)) {
    const std::optional<ro::sim::CellPosition> nearest =
        ro::sim::NearestPassable(*walk, cell.x, cell.y, kFollowNearestRing);
    if (!nearest.has_value()) {
      return {};
    }
    cell = *nearest;
  }
  return {.step = FollowStep::kWalk, .cell = cell};
}

std::optional<std::string> ReadFollowCommand(std::string_view line) {
  const std::size_t space = line.find(' ');
  if (!SameWord(line.substr(0, space), kFollowCommand)) {
    return std::nullopt;
  }
  return std::string(space == std::string_view::npos ? std::string_view()
                                                     : Trimmed(line.substr(space + 1)));
}

}  // namespace moonclient
