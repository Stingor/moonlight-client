#include "ro/sim/navi_route.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <queue>
#include <utility>
#include <vector>

#include "ro/sim/walk_field.h"
#include "ro/sim/walk_path.h"

namespace ro::sim {

namespace {

using formats::NaviData;
using formats::NaviLink;
namespace link_kind = formats::navi_link_kind;

constexpr std::uint32_t kUnreached = std::numeric_limits<std::uint32_t>::max();
constexpr std::uint32_t kNoPrevious = std::numeric_limits<std::uint32_t>::max();

// La table de navigation compte la marche en PAS ; le planificateur compte en
// unités de pas, dix par case droite, comme la recherche de chemin.
[[nodiscard]] std::uint32_t StepsToCost(std::int32_t steps) noexcept {
  return steps <= 0 ? 0 : static_cast<std::uint32_t>(steps) * kMoveCost;
}

[[nodiscard]] std::uint32_t Saturating(std::uint32_t left, std::uint32_t right) noexcept {
  return left > kUnreached - right ? kUnreached : left + right;
}

// La destination précisée depuis la table quand c'est un PNJ : sa carte et sa case
// font foi, pas celles que l'appelant a pu recopier.
[[nodiscard]] NaviGoal Resolved(const NaviData& data, NaviGoal goal) {
  if (goal.kind != NaviGoalKind::kNpc) return goal;
  if (const formats::NaviNpc* npc = data.FindNpc(goal.npc_id); npc != nullptr) {
    goal.map = npc->map;
    goal.cell_x = npc->cell_x;
    goal.cell_y = npc->cell_y;
  }
  return goal;
}

}  // namespace

bool NaviLinkAllowed(const NaviLink& link, const NaviRouteOptions& options) noexcept {
  switch (link.kind) {
    case link_kind::kKafraDungeonWarp:
    case link_kind::kEventDungeonWarp:
    case link_kind::kTransportService:
      return options.allow_transport_services;
    case link_kind::kAirport:
      return options.allow_airport;
    default:
      return true;
  }
}

std::uint32_t NaviHopCost(const NaviLink& link) noexcept {
  return link.kind == link_kind::kWarpPortal ? kPortalHopCost : kDialogHopCost;
}

std::optional<NaviRoute> PlanNaviRoute(const NaviData& data, const NaviStart& start,
                                       const NaviGoal& wanted, const NaviRouteOptions& options,
                                       const NaviStartCost& start_cost,
                                       const NaviGoalCost& goal_cost) {
  if (start.map >= data.maps.size()) return std::nullopt;
  const NaviGoal goal = Resolved(data, wanted);
  if (goal.map >= data.maps.size()) return std::nullopt;

  const auto walk_from_start = [&](std::int32_t x, std::int32_t y) -> std::optional<std::uint32_t> {
    if (start_cost) return start_cost(x, y);
    return OctileCost(start.cell_x, start.cell_y, x, y);
  };
  // Ce qu'il reste à marcher une fois sorti d'une liaison sur la carte visée ;
  // rien quand cette sortie ne mène pas à la destination, ou que rien ne dit
  // la marche jusqu'à elle.
  const auto leg_after = [&](const NaviLink& link) -> std::optional<std::uint32_t> {
    if (goal.kind == NaviGoalKind::kMap) return 0;
    if (goal_cost) return goal_cost(link);
    return std::nullopt;
  };

  // Les nœuds : une liaison par rang, et un nœud de plus pour l'ARRIVÉE. Le coût
  // d'une liaison est celui d'être à son entrée, prêt à la franchir.
  const std::uint32_t goal_node = static_cast<std::uint32_t>(data.links.size());
  std::vector<std::uint32_t> cost(data.links.size() + 1, kUnreached);
  std::vector<std::uint32_t> previous(data.links.size() + 1, kNoPrevious);
  using Entry = std::pair<std::uint32_t, std::uint32_t>;
  std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;

  const auto offer = [&](std::uint32_t node, std::uint32_t at_cost, std::uint32_t from) {
    if (at_cost >= cost[node]) return;
    cost[node] = at_cost;
    previous[node] = from;
    open.emplace(at_cost, node);
  };

  // L'arrivée directe, sans quitter la carte.
  if (goal.map == start.map) {
    if (goal.kind == NaviGoalKind::kMap) {
      offer(goal_node, 0, kNoPrevious);
    } else if (const auto walk = walk_from_start(goal.cell_x, goal.cell_y)) {
      offer(goal_node, *walk, kNoPrevious);
    }
  }
  for (const std::uint32_t slot : data.LinksLeaving(start.map)) {
    const NaviLink& link = data.links[slot];
    if (!NaviLinkAllowed(link, options)) continue;
    if (const auto walk = walk_from_start(link.cell_x, link.cell_y))
      offer(slot, *walk, kNoPrevious);
  }

  while (!open.empty()) {
    const auto [at_cost, node] = open.top();
    open.pop();
    if (at_cost != cost[node]) continue;
    if (node == goal_node) break;

    const NaviLink& link = data.links[node];
    const std::uint32_t arrived = Saturating(at_cost, NaviHopCost(link));
    if (link.dest_map == goal.map) {
      if (const auto leg = leg_after(link)) offer(goal_node, Saturating(arrived, *leg), node);
    }
    for (const formats::NaviPassRun& run : data.PassesAfter(link.link_id)) {
      for (std::uint32_t offset = 0; offset < run.link_count; ++offset) {
        const auto next = data.LinkSlot(run.first_link_id + offset);
        if (!next || !NaviLinkAllowed(data.links[*next], options)) continue;
        offer(*next, Saturating(arrived, StepsToCost(run.distance_cells)), node);
      }
    }
  }

  if (cost[goal_node] == kUnreached) return std::nullopt;
  NaviRoute route;
  route.total_cost = cost[goal_node];
  for (std::uint32_t node = previous[goal_node]; node != kNoPrevious; node = previous[node])
    route.link_ids.push_back(data.links[node].link_id);
  std::reverse(route.link_ids.begin(), route.link_ids.end());
  return route;
}

}  // namespace ro::sim
