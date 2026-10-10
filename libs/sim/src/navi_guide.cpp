#include "ro/sim/navi_guide.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace ro::sim {

namespace {

[[nodiscard]] bool SameCell(const CellPosition& left, const CellPosition& right) noexcept {
  return left.x == right.x && left.y == right.y;
}

[[nodiscard]] CellPosition Cell(std::int32_t x, std::int32_t y) noexcept {
  return {static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y), 0};
}

// La case que la marche vise vraiment : la cible elle-même si elle se franchit,
// sinon la plus proche qui se franchit.
[[nodiscard]] std::optional<CellPosition> Reachable(const WalkMap& walk, std::int32_t x,
                                                    std::int32_t y) {
  if (walk.Passable(x, y)) return Cell(x, y);
  return NearestPassable(walk, x, y, kNaviTargetRing);
}

}  // namespace

void NaviGuide::SetData(std::shared_ptr<const formats::NaviData> data) { data_ = std::move(data); }

void NaviGuide::Request(NaviGuideRequest request) {
  Stop();
  request_ = std::move(request);
  status_ = NaviGuideStatus::kPending;
}

void NaviGuide::Stop() {
  status_ = NaviGuideStatus::kIdle;
  goal_map_ = formats::kNoNaviMap;
  steps_.clear();
  current_step_ = 0;
  estimated_cost_ = 0;
  field_map_ = formats::kNoNaviMap;
  field_walk_ = nullptr;
  field_ = WalkField{};
  local_target_.reset();
  traced_from_.reset();
  path_cells_.clear();
  path_corners_.clear();
}

void NaviGuide::Update(std::string_view map_name, const WalkMap* walk, CellPosition player) {
  if (!data_ || !active()) return;
  const formats::NaviMapIndex here = data_->FindMap(map_name);
  if (status_ == NaviGuideStatus::kPending) {
    // La carte doit être connue du graphe ; tant qu'elle ne l'est pas — un écran
    // de chargement, une carte d'instance hors génération —, on attend.
    if (here != formats::kNoNaviMap) Plan(here, walk, player);
    return;
  }

  if (here == formats::kNoNaviMap) {
    // Une carte que le graphe ne connaît pas — une instance, un décor : rien à
    // tracer ici, mais l'itinéraire reprend dès qu'on en sort.
    path_cells_.clear();
    path_corners_.clear();
    local_target_.reset();
    field_map_ = here;
    return;
  }

  if (here != field_map_) {
    if (current_step_ < steps_.size() && steps_[current_step_].to_map == here) {
      ++current_step_;
      AimLocally(here, walk);
    } else if (current_step_ < steps_.size() && steps_[current_step_].from_map == here) {
      AimLocally(here, walk);
    } else if (here == goal_map_ && current_step_ >= steps_.size()) {
      AimLocally(here, walk);
    } else {
      // Le joueur n'est pas là où l'itinéraire l'attendait : on repart d'ici.
      Plan(here, walk, player);
      return;
    }
  } else if (walk != field_walk_) {
    // La grille arrive après le nom de la carte.
    AimLocally(here, walk);
  }
  Trace(player);

  if (current_step_ < steps_.size()) return;
  if (request_.kind == NaviGoalKind::kMap) {
    status_ = NaviGuideStatus::kArrived;
    return;
  }
  if (const auto left = field_.CostFrom(player.x, player.y); left && *left <= kNaviArrivalCost)
    status_ = NaviGuideStatus::kArrived;
}

void NaviGuide::Plan(formats::NaviMapIndex here, const WalkMap* walk, CellPosition player) {
  NaviGoal goal;
  goal.kind = request_.kind;
  goal.cell_x = request_.cell_x;
  goal.cell_y = request_.cell_y;
  goal.npc_id = request_.npc_id;
  if (goal.kind == NaviGoalKind::kNpc) {
    const formats::NaviNpc* npc = data_->FindNpc(request_.npc_id);
    goal.map = npc != nullptr ? npc->map : formats::kNoNaviMap;
  } else {
    goal.map = data_->FindMap(request_.map_name);
  }
  goal_map_ = goal.map;
  if (goal.map == formats::kNoNaviMap) {
    status_ = NaviGuideStatus::kNoRoute;
    return;
  }
  if (goal.kind == NaviGoalKind::kMap && goal.map == here) {
    status_ = NaviGuideStatus::kAlreadyHere;
    return;
  }

  // Le départ se mesure sur la grille du joueur : une seule inondation depuis
  // sa case répond pour toutes les sorties de la carte.
  WalkField from_player;
  NaviStartCost start_cost;
  if (walk != nullptr) {
    const std::array<CellPosition, 1> origin{player};
    from_player = WalkField::Flood(*walk, origin);
    start_cost = [&](std::int32_t x, std::int32_t y) -> std::optional<std::uint32_t> {
      const auto target = Reachable(*walk, x, y);
      if (!target) return std::nullopt;
      return from_player.CostFrom(target->x, target->y);
    };
  }
  const NaviStart start{here, player.x, player.y};
  const auto route = PlanNaviRoute(*data_, start, goal, request_.options, start_cost);
  if (!route) {
    status_ = NaviGuideStatus::kNoRoute;
    return;
  }

  steps_.clear();
  for (const std::uint32_t link_id : route->link_ids) {
    const formats::NaviLink* link = data_->FindLink(link_id);
    if (link == nullptr) continue;
    steps_.push_back({link_id, link->map, link->dest_map, link->cell_x, link->cell_y, link->kind});
  }
  current_step_ = 0;
  estimated_cost_ = route->total_cost;
  status_ = NaviGuideStatus::kGuiding;
  AimLocally(here, walk);
  Trace(player);
  if (steps_.empty()) {
    if (const auto left = field_.CostFrom(player.x, player.y); left && *left <= kNaviArrivalCost)
      status_ = NaviGuideStatus::kAlreadyHere;
  }
}

void NaviGuide::AimLocally(formats::NaviMapIndex here, const WalkMap* walk) {
  field_map_ = here;
  field_walk_ = walk;
  field_ = WalkField{};
  traced_from_.reset();
  path_cells_.clear();
  path_corners_.clear();
  local_target_.reset();

  if (current_step_ < steps_.size()) {
    local_target_ = Cell(steps_[current_step_].exit_x, steps_[current_step_].exit_y);
  } else if (request_.kind == NaviGoalKind::kNpc) {
    if (const formats::NaviNpc* npc = data_->FindNpc(request_.npc_id); npc != nullptr)
      local_target_ = Cell(npc->cell_x, npc->cell_y);
  } else if (request_.kind == NaviGoalKind::kCell) {
    local_target_ = Cell(request_.cell_x, request_.cell_y);
  }
  if (!local_target_ || walk == nullptr) return;
  if (const auto source = Reachable(*walk, local_target_->x, local_target_->y)) {
    const std::array<CellPosition, 1> sources{*source};
    field_ = WalkField::Flood(*walk, sources);
  }
}

void NaviGuide::Trace(CellPosition player) {
  if (traced_from_ && SameCell(*traced_from_, player)) return;
  traced_from_ = player;
  path_cells_ = field_.PathFrom(player.x, player.y);
  // Sans grille ni chemin connu, une DIRECTION vaut mieux que rien : le segment
  // droit jusqu'à la cible.
  if (path_cells_.empty() && local_target_) path_cells_ = {player, *local_target_};
  path_corners_ = CornersOf(path_cells_);
}

}  // namespace ro::sim
