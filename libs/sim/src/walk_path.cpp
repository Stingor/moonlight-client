#include "ro/sim/walk_path.h"

#include <array>
#include <cstdlib>
#include <utility>

#include "ro/sim/movement.h"

namespace ro::sim {
namespace {

// dirx et diry, dans src/map/unit.cpp. Zéro pointe au nord.
constexpr std::array<int, kDirectionCount> kDeltaX = {0, -1, -1, -1, 0, 1, 1, 1};
constexpr std::array<int, kDirectionCount> kDeltaY = {1, 1, 0, -1, -1, -1, 0, 1};

// walk_choices : la direction d'un pas d'après le signe de chaque écart,
// rangée par -dy + 1 puis dx + 1. Le centre n'est jamais un pas.
constexpr std::uint8_t kNoStep = 0xFF;
constexpr std::array<std::array<std::uint8_t, 3>, 3> kWalkChoices = {
    {{1, 0, 7}, {2, kNoStep, 6}, {3, 4, 5}}};

std::uint8_t ChoiceFor(int dx, int dy) {
  return kWalkChoices[static_cast<std::size_t>(-dy + 1)][static_cast<std::size_t>(dx + 1)];
}

int Sign(int value) { return value > 0 ? 1 : (value < 0 ? -1 : 0); }

// La table de nœuds de path_search : MAX_WALKPATH² entrées, indexées modulo.
// C'est cette indexation qui peut faire se heurter deux cases (calc_index).
constexpr std::size_t kNodePoolSize = kMaxWalkSteps * kMaxWalkSteps;

std::size_t NodeIndex(int x, int y) {
  return (static_cast<std::size_t>(x) + static_cast<std::size_t>(y) * kMaxWalkSteps) &
         (kNodePoolSize - 1);
}

constexpr std::uint8_t kSetOpen = 0;
constexpr std::uint8_t kSetClosed = 1;

// PATH_DIR_* : les directions orthogonales permises depuis la case courante.
constexpr int kDirNorth = 1;
constexpr int kDirWest = 2;
constexpr int kDirSouth = 4;
constexpr int kDirEast = 8;

// L'heuristique du client : la distance de Manhattan, qui SURESTIME. C'est elle
// qui fait préférer la diagonale, dont un pas coûte 14 mais en retranche 20.
int Heuristic(int x0, int y0, int x1, int y1) {
  return static_cast<int>(kMoveCost) * (std::abs(x1 - x0) + std::abs(y1 - y0));
}

struct PathNode {
  int parent = -1;
  int x = 0;
  int y = 0;
  int g_cost = 0;
  int f_cost = 0;
  std::uint8_t flag = kSetOpen;
};

using NodePool = std::array<PathNode, kNodePoolSize>;

// Le tas binaire de rAthena — BHEAP_PUSH2, BHEAP_POP2, BHEAP_UPDATE dans
// src/common/db.hpp — reproduit opération par opération. Ce n'est pas un
// détail : quand deux nœuds ont le même coût, c'est lui qui décide lequel
// s'explore d'abord, donc lequel des deux chemins de même longueur le serveur
// retient.
class NodeHeap {
 public:
  explicit NodeHeap(const NodePool& pool) : pool_(pool) {}

  [[nodiscard]] bool empty() const noexcept { return heap_.empty(); }
  [[nodiscard]] std::size_t Top() const noexcept { return heap_.front(); }

  void Push(std::size_t node) {
    heap_.push_back(node);
    SiftDown(0, heap_.size() - 1);
  }

  void Pop() {
    heap_.front() = heap_.back();
    heap_.pop_back();
    if (!heap_.empty()) {
      SiftUp(0);
    }
  }

  // Faux si le nœud n'est pas dans le tas : le serveur tient cela pour une
  // erreur qui fait échouer la recherche.
  [[nodiscard]] bool Update(std::size_t node) {
    for (std::size_t index = 0; index < heap_.size(); ++index) {
      if (heap_[index] == node) {
        SiftDown(0, index);
        SiftUp(index);
        return true;
      }
    }
    return false;
  }

 private:
  [[nodiscard]] int Compare(std::size_t left, std::size_t right) const noexcept {
    return pool_[left].f_cost - pool_[right].f_cost;
  }

  // Remonte l'élément tant que son parent lui est supérieur.
  void SiftDown(std::size_t start, std::size_t index) {
    while (index > start) {
      const std::size_t parent = (index - 1) / 2;
      if (Compare(heap_[parent], heap_[index]) <= 0) {
        break;
      }
      std::swap(heap_[parent], heap_[index]);
      index = parent;
    }
  }

  // Descend l'élément jusqu'à une feuille en prenant toujours le plus petit
  // enfant — le DROIT à égalité —, puis le remonte à sa place.
  void SiftUp(std::size_t index) {
    std::size_t current = index;
    std::size_t left = current * 2 + 1;
    while (left < heap_.size()) {
      const std::size_t right = current * 2 + 2;
      if (right >= heap_.size() || Compare(heap_[left], heap_[right]) < 0) {
        std::swap(heap_[current], heap_[left]);
        current = left;
      } else {
        std::swap(heap_[current], heap_[right]);
        current = right;
      }
      left = current * 2 + 1;
    }
    SiftDown(index, current);
  }

  const NodePool& pool_;
  std::vector<std::size_t> heap_;
};

// add_path : pose ou améliore un nœud. Faux sur la collision d'index de la
// table, que le serveur tient pour un échec de la recherche entière.
[[nodiscard]] bool AddPath(NodeHeap& heap, NodePool& pool, int x, int y, int g_cost, int parent,
                           int h_cost) {
  const std::size_t index = NodeIndex(x, y);
  PathNode& node = pool[index];
  if (node.x == x && node.y == y) {
    if (g_cost < node.g_cost) {
      node.g_cost = g_cost;
      node.parent = parent;
      node.f_cost = g_cost + h_cost;
      if (node.flag == kSetClosed) {
        heap.Push(index);
      } else if (!heap.Update(index)) {
        return false;
      }
      node.flag = kSetOpen;
    }
    return true;
  }
  if (node.x != 0 || node.y != 0) {
    return false;
  }
  node = PathNode{parent, x, y, g_cost, g_cost + h_cost, kSetOpen};
  heap.Push(index);
  return true;
}

bool InMap(const WalkMap& map, CellPosition cell) {
  return cell.x < map.width() && cell.y < map.height();
}

// Le tronc commun du chemin facile et de la ligne droite : en diagonale tant
// que les deux écarts le demandent, puis tout droit. `map` vérifie chaque case
// atteinte, ou ne vérifie rien s'il est nul.
std::optional<WalkPath> DirectPath(const WalkMap* map, CellPosition from, CellPosition to,
                                   bool bounded) {
  int dx = Sign(static_cast<int>(to.x) - static_cast<int>(from.x));
  int dy = Sign(static_cast<int>(to.y) - static_cast<int>(from.y));
  int x = from.x;
  int y = from.y;
  WalkPath path;
  while (dx != 0 || dy != 0) {
    if (bounded && path.directions.size() >= kMaxWalkSteps) {
      break;
    }
    path.directions.push_back(ChoiceFor(dx, dy));
    x += dx;
    y += dy;
    if (x == to.x) {
      dx = 0;
    }
    if (y == to.y) {
      dy = 0;
    }
    if (dx == 0 && dy == 0) {
      break;
    }
    if (map != nullptr && !map->Passable(x, y)) {
      break;
    }
  }
  if (x != to.x || y != to.y) {
    return std::nullopt;
  }
  return path;
}

}  // namespace

bool IsDiagonal(std::uint8_t direction) noexcept {
  return direction < kDirectionCount && kDeltaX[direction] != 0 && kDeltaY[direction] != 0;
}

CellPosition Stepped(CellPosition from, std::uint8_t direction) noexcept {
  if (direction >= kDirectionCount) {
    return from;
  }
  from.x = static_cast<std::uint16_t>(static_cast<int>(from.x) + kDeltaX[direction]);
  from.y = static_cast<std::uint16_t>(static_cast<int>(from.y) + kDeltaY[direction]);
  return from;
}

std::uint32_t StepDurationMs(std::uint16_t speed_ms_per_cell, std::uint8_t direction) noexcept {
  const std::uint32_t speed = speed_ms_per_cell;
  return IsDiagonal(direction) ? speed * kMoveDiagonalCost / kMoveCost : speed;
}

std::optional<WalkPath> FindWalkPath(const WalkMap& map, CellPosition from, CellPosition to) {
  // Le départ n'est pas vérifié : on ne sortirait jamais d'une case bloquée.
  if (!InMap(map, from) || !map.Passable(to.x, to.y)) {
    return std::nullopt;
  }
  const int x1 = to.x;
  const int y1 = to.y;
  const int xs = map.width() - 1;
  const int ys = map.height() - 1;

  NodePool pool{};
  NodeHeap heap(pool);
  const std::size_t start = NodeIndex(from.x, from.y);
  pool[start] = PathNode{-1, from.x, from.y, 0, Heuristic(from.x, from.y, x1, y1), kSetOpen};
  heap.Push(start);

  std::size_t current = start;
  for (;;) {
    if (heap.empty()) {
      return std::nullopt;
    }
    current = heap.Top();
    heap.Pop();
    const int x = pool[current].x;
    const int y = pool[current].y;
    const int g_cost = pool[current].g_cost;
    pool[current].flag = kSetClosed;
    if (x == x1 && y == y1) {
      break;
    }

    // Une diagonale n'est permise que si ses DEUX côtés le sont : sinon on
    // couperait le coin d'une case bloquée, que le client contourne.
    int allowed = 0;
    if (y < ys && map.Passable(x, y + 1)) {
      allowed |= kDirNorth;
    }
    if (y > 0 && map.Passable(x, y - 1)) {
      allowed |= kDirSouth;
    }
    if (x < xs && map.Passable(x + 1, y)) {
      allowed |= kDirEast;
    }
    if (x > 0 && map.Passable(x - 1, y)) {
      allowed |= kDirWest;
    }
    const auto has = [allowed](int bits) { return (allowed & bits) == bits; };
    const auto add = [&](int nx, int ny, std::uint32_t cost) {
      return AddPath(heap, pool, nx, ny, g_cost + static_cast<int>(cost), static_cast<int>(current),
                     Heuristic(nx, ny, x1, y1));
    };

    // Dans l'ordre du serveur : sud-est, est, nord-est, nord, nord-ouest,
    // ouest, sud-ouest, sud. Le changer changerait les ex æquo.
    bool ok = true;
    if (has(kDirSouth | kDirEast) && map.Passable(x + 1, y - 1)) {
      ok = ok && add(x + 1, y - 1, kMoveDiagonalCost);
    }
    if (has(kDirEast)) {
      ok = ok && add(x + 1, y, kMoveCost);
    }
    if (has(kDirNorth | kDirEast) && map.Passable(x + 1, y + 1)) {
      ok = ok && add(x + 1, y + 1, kMoveDiagonalCost);
    }
    if (has(kDirNorth)) {
      ok = ok && add(x, y + 1, kMoveCost);
    }
    if (has(kDirNorth | kDirWest) && map.Passable(x - 1, y + 1)) {
      ok = ok && add(x - 1, y + 1, kMoveDiagonalCost);
    }
    if (has(kDirWest)) {
      ok = ok && add(x - 1, y, kMoveCost);
    }
    if (has(kDirSouth | kDirWest) && map.Passable(x - 1, y - 1)) {
      ok = ok && add(x - 1, y - 1, kMoveDiagonalCost);
    }
    if (has(kDirSouth)) {
      ok = ok && add(x, y - 1, kMoveCost);
    }
    if (!ok) {
      return std::nullopt;
    }
  }

  std::size_t length = 0;
  for (std::size_t it = current; pool[it].parent != -1;
       it = static_cast<std::size_t>(pool[it].parent)) {
    ++length;
  }
  if (length > kMaxWalkSteps) {
    return std::nullopt;
  }
  WalkPath path;
  path.directions.resize(length);
  std::size_t it = current;
  for (std::size_t step = length; step-- > 0; it = static_cast<std::size_t>(pool[it].parent)) {
    const PathNode& node = pool[it];
    const PathNode& parent = pool[static_cast<std::size_t>(node.parent)];
    path.directions[step] = ChoiceFor(node.x - parent.x, node.y - parent.y);
  }
  return path;
}

std::optional<WalkPath> FindEasyWalkPath(const WalkMap& map, CellPosition from, CellPosition to) {
  if (!InMap(map, from) || !map.Passable(to.x, to.y)) {
    return std::nullopt;
  }
  return DirectPath(&map, from, to, true);
}

WalkPath StraightWalkPath(CellPosition from, CellPosition to) {
  // Sans carte rien ne bloque et rien ne borne : le serveur a annoncé cette
  // destination, on y va.
  return *DirectPath(nullptr, from, to, false);
}

const WalkPath& WalkPlanner::PathFor(const Actor& actor) {
  if (!actor.movement.has_value()) {
    planned_.erase(actor.id);
    return no_path_;
  }
  const CellMovement& movement = *actor.movement;
  const auto found = planned_.find(actor.id);
  if (found != planned_.end() && found->second.movement == movement &&
      found->second.started_ms == actor.movement_started_ms) {
    return found->second.path;
  }
  std::optional<WalkPath> path =
      map_ != nullptr ? FindWalkPath(*map_, movement.from, movement.to) : std::nullopt;
  Planned planned{
      movement, actor.movement_started_ms,
      path.has_value() ? std::move(*path) : StraightWalkPath(movement.from, movement.to)};
  return planned_.insert_or_assign(actor.id, std::move(planned)).first->second.path;
}

}  // namespace ro::sim
