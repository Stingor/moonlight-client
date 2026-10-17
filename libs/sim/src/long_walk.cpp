// La marche par étapes. Voir l'en-tête.

#include "ro/sim/long_walk.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <functional>
#include <limits>
#include <queue>
#include <utility>

#include "ro/sim/movement.h"
#include "ro/sim/pending_action.h"
#include "ro/sim/world.h"

namespace ro::sim {
namespace {

// Les huit voisins, et le coût d'un pas vers chacun.
struct Neighbor {
  int dx = 0;
  int dy = 0;
  std::uint32_t cost = 0;
};
constexpr std::array<Neighbor, 8> kNeighbors = {
    Neighbor{1, -1, kMoveDiagonalCost},  Neighbor{1, 0, kMoveCost},
    Neighbor{1, 1, kMoveDiagonalCost},   Neighbor{0, 1, kMoveCost},
    Neighbor{-1, 1, kMoveDiagonalCost},  Neighbor{-1, 0, kMoveCost},
    Neighbor{-1, -1, kMoveDiagonalCost}, Neighbor{0, -1, kMoveCost}};

constexpr int kNoParent = -1;

// L'heuristique octile, en coûts de pas : elle ne surestime jamais, donc le
// fil trouvé est un plus court chemin.
int Octile(int x0, int y0, int x1, int y1) {
  const int dx = std::abs(x1 - x0);
  const int dy = std::abs(y1 - y0);
  return static_cast<int>(kMoveCost) * std::max(dx, dy) +
         static_cast<int>(kMoveDiagonalCost - kMoveCost) * std::min(dx, dy);
}

bool SameCell(CellPosition a, CellPosition b) { return a.x == b.x && a.y == b.y; }

// Le pas de (x, y) vers ce voisin est-il permis ? Une diagonale demande ses
// DEUX côtés, comme chez le serveur.
bool StepAllowed(const WalkMap& map, int x, int y, const Neighbor& step) {
  if (!map.Passable(x + step.dx, y + step.dy)) {
    return false;
  }
  if (step.dx != 0 && step.dy != 0) {
    return map.Passable(x + step.dx, y) && map.Passable(x, y + step.dy);
  }
  return true;
}

std::uint32_t RemainingWalkMs(const Actor& actor, const WalkPath& path, std::uint32_t now_ms) {
  const std::uint32_t total_ms = MovementDurationMs(actor, path);
  const std::uint32_t elapsed_ms = now_ms - actor.movement_started_ms;
  return elapsed_ms >= total_ms ? 0 : total_ms - elapsed_ms;
}

}  // namespace

std::optional<std::vector<CellPosition>> FindRoute(const WalkMap& map, CellPosition from,
                                                   CellPosition to) {
  const int width = map.width();
  const int height = map.height();
  if (from.x >= width || from.y >= height || !map.Passable(to.x, to.y)) {
    return std::nullopt;
  }
  const auto index_of = [width](int x, int y) {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
           static_cast<std::size_t>(x);
  };
  const std::size_t cells = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
  std::vector<int> g_cost(cells, std::numeric_limits<int>::max());
  std::vector<int> parent(cells, kNoParent);
  std::vector<bool> closed(cells, false);

  using Entry = std::pair<int, std::size_t>;  // f, case
  std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;
  const std::size_t start = index_of(from.x, from.y);
  const std::size_t goal = index_of(to.x, to.y);
  g_cost[start] = 0;
  open.emplace(Octile(from.x, from.y, to.x, to.y), start);

  while (!open.empty()) {
    const std::size_t current = open.top().second;
    open.pop();
    if (closed[current]) {
      continue;
    }
    closed[current] = true;
    if (current == goal) {
      break;
    }
    const int x = static_cast<int>(current % static_cast<std::size_t>(width));
    const int y = static_cast<int>(current / static_cast<std::size_t>(width));
    for (const Neighbor& step : kNeighbors) {
      if (!StepAllowed(map, x, y, step)) {
        continue;
      }
      const std::size_t next = index_of(x + step.dx, y + step.dy);
      const int cost = g_cost[current] + static_cast<int>(step.cost);
      if (closed[next] || cost >= g_cost[next]) {
        continue;
      }
      g_cost[next] = cost;
      parent[next] = static_cast<int>(current);
      open.emplace(cost + Octile(x + step.dx, y + step.dy, to.x, to.y), next);
    }
  }
  if (!closed[goal]) {
    return std::nullopt;
  }

  std::vector<CellPosition> route;
  for (std::size_t at = goal; at != start; at = static_cast<std::size_t>(parent[at])) {
    CellPosition cell;
    cell.x = static_cast<std::uint16_t>(at % static_cast<std::size_t>(width));
    cell.y = static_cast<std::uint16_t>(at / static_cast<std::size_t>(width));
    route.push_back(cell);
  }
  std::ranges::reverse(route);
  return route;
}

CellPosition WalkStopToward(const WalkMap& map, CellPosition from, CellPosition to,
                            std::size_t max_steps) {
  const auto accepted = [&](CellPosition cell) {
    const std::optional<WalkPath> path = FindWalkPath(map, from, cell);
    return path.has_value() && path->directions.size() <= max_steps;
  };
  if (SameCell(from, to) || (StepsApart(from, to) <= max_steps && accepted(to))) {
    return to;
  }
  const std::optional<std::vector<CellPosition>> route = FindRoute(map, from, to);
  if (!route.has_value()) {
    return to;
  }
  // Le fil compte un pas par case : au-delà de `max_steps` cases, aucune n'est
  // à portée d'une marche.
  for (std::size_t reach = std::min(route->size(), max_steps); reach > 0; --reach) {
    if (accepted((*route)[reach - 1])) {
      return (*route)[reach - 1];
    }
  }
  return to;
}

std::size_t LegSteps(const World& world) noexcept {
  const int steps = world.session().max_walk_path_steps;
  return steps > 1 ? static_cast<std::size_t>(steps - 1) : 1;
}

std::size_t StepsApart(CellPosition from, CellPosition to) noexcept {
  return static_cast<std::size_t>(
      std::max(std::abs(static_cast<int>(to.x) - static_cast<int>(from.x)),
               std::abs(static_cast<int>(to.y) - static_cast<int>(from.y))));
}

bool LegTaken(const Actor& actor, CellPosition stop, std::uint32_t sent_ms) noexcept {
  return actor.movement.has_value() && SameCell(actor.movement->to, stop) &&
         actor.movement_started_ms >= sent_ms;
}

bool LegStillRunning(const Actor& actor, const WalkPath& path, std::uint32_t now_ms) {
  if (!IsWalking(actor, path, now_ms)) {
    return false;
  }
  const std::uint32_t lead_ms =
      static_cast<std::uint32_t>(kNextLegLeadSteps) * MovingSpeedOf(actor);
  return RemainingWalkMs(actor, path, now_ms) > lead_ms;
}

CellPosition BeginLongWalk(const World& world, std::optional<LongWalk>& walk,
                           CellPosition destination, std::uint32_t now_ms) {
  walk.reset();
  const Actor* self = world.player();
  const WalkMap* map = world.walk_map();
  if (self == nullptr || map == nullptr) {
    return destination;
  }
  const CellPosition stop =
      WalkStopToward(*map, StandingCell(*self), destination,
                     static_cast<std::size_t>(std::max(1, world.session().max_walk_path_steps)));
  if (!SameCell(stop, destination)) {
    walk = LongWalk{.destination = destination,
                    .stop = stop,
                    .sent_ms = now_ms,
                    .map_entry = world.map_entries()};
  }
  return stop;
}

std::optional<CellPosition> AdvanceLongWalk(const World& world, WalkPlanner& planner,
                                            std::optional<LongWalk>& walk, ApproachWalkClock& clock,
                                            std::uint32_t now_ms) {
  if (!walk.has_value()) {
    return std::nullopt;
  }
  const Actor* self = world.player();
  const WalkMap* map = world.walk_map();
  if (self == nullptr || map == nullptr || walk->map_entry != world.map_entries()) {
    walk.reset();
    return std::nullopt;
  }
  // L'ÉTAPE EST-ELLE PARTIE ? Le serveur a fait marcher le personnage vers
  // elle depuis la demande. Sinon, ou bien il ne l'a pas encore dit, ou bien il
  // l'a refusée, ou bien une autre marche a pris sa place.
  if (!LegTaken(*self, walk->stop, walk->sent_ms)) {
    if (now_ms - walk->sent_ms > kLegConfirmMs) {
      walk.reset();
    }
    return std::nullopt;
  }
  if (SameCell(walk->stop, walk->destination)) {
    walk.reset();
    return std::nullopt;
  }
  const WalkPath& path = planner.PathFor(*self);
  if (LegStillRunning(*self, path, now_ms)) {
    return std::nullopt;
  }
  const CellPosition here = StoppedCell(*self, path, now_ms);
  const CellPosition next = WalkStopToward(*map, here, walk->destination, LegSteps(world));
  // Plus d'avance possible : arrivé à l'étape sans pouvoir aller au-delà, on
  // s'arrête là.
  if (SameCell(next, walk->stop)) {
    if (!IsWalking(*self, path, now_ms)) {
      walk.reset();
    }
    return std::nullopt;
  }
  if (!AdmitWalkRequest(world, clock, now_ms)) {
    return std::nullopt;
  }
  walk->stop = next;
  walk->sent_ms = now_ms;
  return next;
}

}  // namespace ro::sim
