#include "ro/render/sight_quad_tree.h"

#include <algorithm>
#include <cmath>

#include "ro/render/terrain_mesh.h"

namespace ro::render {
namespace {

// L'étendue qu'un nœud prend en hauteur tant que rien ne la borne, et la
// longueur de la droite qui les éprouve.
constexpr float kUnbounded = 999999.0F;
// En deçà, la droite est tenue pour parallèle à la tranche.
constexpr float kParallel = 0.00001F;
// La part d'un nœud qu'une droite parallèle doit viser.
constexpr float kParallelShare = 0.7F;
// Au-delà, deux projections se séparent.
constexpr float kSeparation = 0.000001F;
// Le bord haut de la racine s'arrête une unité avant celui du sol.
constexpr float kRootInset = 1.0F;

constexpr std::size_t kChildren = 4;
constexpr std::uint32_t kGatQuarter = 4;
constexpr std::int32_t kSplitAxes = 2;

// Les hauteurs basse et haute d'un morceau de terrain.
struct HeightRange {
  float lowest = kUnbounded;
  float highest = -kUnbounded;

  void Take(const std::array<float, 4>& corners) {
    for (const float corner : corners) {
      highest = std::max(highest, corner);
      lowest = std::min(lowest, corner);
    }
  }
};

// La case du sol qui contient ce point : le client la compte depuis le centre.
[[nodiscard]] std::array<std::int32_t, 2> GroundCellOf(const formats::GndMap& gnd, float x,
                                                       float z) {
  const float zoom = gnd.proportion_ratio();
  return {gnd.width() / kSplitAxes + static_cast<std::int32_t>(std::floor(x / zoom)),
          gnd.height() / kSplitAxes + static_cast<std::int32_t>(std::floor(z / zoom))};
}

// Et celle du relief de marche, deux fois plus fine.
[[nodiscard]] std::array<std::int32_t, 2> WalkCellOf(const formats::GatMap& gat, float x, float z) {
  const auto quarter = static_cast<std::int32_t>(kGatQuarter);
  return {static_cast<std::int32_t>(std::floor(x / kWorldUnitsPerCell)) +
              kSplitAxes * (gat.width() / quarter),
          static_cast<std::int32_t>(std::floor(z / kWorldUnitsPerCell)) +
              kSplitAxes * (gat.height() / quarter)};
}

// Les hauteurs du sol entre deux cases, bornes comprises : celles des coins de
// chaque case qui porte une face du dessus.
//
// ⚠ UNE PLAGE QUI DÉBORDE DE LA CARTE, OU QUI NE S'ÉTEND PAS, rend zéro et zéro :
// c'est la règle du client, et elle vaut pour toute feuille du bord.
[[nodiscard]] HeightRange GroundRangeOf(const formats::GndMap& gnd,
                                        const std::array<std::int32_t, 2>& first,
                                        const std::array<std::int32_t, 2>& last) {
  const auto within = [](std::int32_t value, std::int32_t size) {
    return value >= 0 && value < size;
  };
  if (!within(first[0], gnd.width()) || !within(last[0], gnd.width()) ||
      !within(first[1], gnd.height()) || !within(last[1], gnd.height()) || last[0] <= first[0] ||
      last[1] <= first[1]) {
    return HeightRange{.lowest = 0.0F, .highest = 0.0F};
  }
  HeightRange range;
  for (std::int32_t x = first[0]; x <= last[0]; ++x) {
    for (std::int32_t y = first[1]; y <= last[1]; ++y) {
      const formats::GndCube& cube = gnd.at(x, y);
      if (cube.tile_up >= 0) {
        range.Take(cube.corner_height);
      }
    }
  }
  return range;
}

// Les hauteurs du relief de marche entre deux cases, en ne gardant que celles
// qui tombent sur la carte.
[[nodiscard]] HeightRange WalkRangeOf(const formats::GatMap& gat,
                                      const std::array<std::int32_t, 2>& first,
                                      const std::array<std::int32_t, 2>& last) {
  HeightRange range;
  for (std::int32_t x = first[0]; x <= last[0]; ++x) {
    for (std::int32_t y = first[1]; y <= last[1]; ++y) {
      if (gat.InBounds(x, y)) {
        range.Take(gat.at(x, y).corner_height);
      }
    }
  }
  return range;
}

}  // namespace

bool RayMeetsNode(const glm::vec3& origin, const glm::vec3& direction, const glm::vec3& lowest,
                  const glm::vec3& highest) {
  float entry = -kUnbounded;
  float exit = kUnbounded;
  for (glm::length_t axis = 0; axis < 3; ++axis) {
    const float center = (highest[axis] + lowest[axis]) * 0.5F;
    const float half = highest[axis] - center;
    const float offset = center - origin[axis];
    const float slope = direction[axis];
    if (std::abs(slope) <= kParallel) {
      const float share = half * kParallelShare;
      if (-offset - share > 0.0F || share - offset < 0.0F) {
        return false;
      }
      continue;
    }
    const float inverse = 1.0F / slope;
    const float first = (offset + half) * inverse;
    const float second = (offset - half) * inverse;
    if (first <= second) {
      entry = std::fmax(first, entry);
      exit = std::fmin(second, exit);
    } else {
      entry = std::fmax(second, entry);
      exit = std::fmin(first, exit);
    }
    if (entry > exit || exit < 0.0F) {
      return false;
    }
  }
  if (entry <= 0.0F) {
    return exit <= kUnbounded;
  }
  return entry <= kUnbounded;
}

bool BoxMeetsNode(const SightBox& box, const glm::vec3& lowest, const glm::vec3& highest) {
  const std::array<float, 3>& a = box.half_extents;
  const glm::vec3& u = box.axes[0];
  const glm::vec3& v = box.axes[1];
  const glm::vec3& w = box.axes[2];
  const glm::vec3 b = (highest - lowest) * 0.5F;
  const glm::vec3 t = box.center - (lowest + highest) * 0.5F;
  const glm::vec3 au = glm::abs(u);
  const glm::vec3 av = glm::abs(v);
  const glm::vec3 aw = glm::abs(w);
  const float tu = u.y * t.y + u.x * t.x + u.z * t.z;
  const float tv = v.y * t.y + v.x * t.x + v.z * t.z;
  const float tw = w.y * t.y + w.x * t.x + w.z * t.z;
  const auto apart = [](float distance, float reach) { return distance - reach > kSeparation; };

  // Les trois axes de la boîte, puis les trois du nœud.
  if (apart(std::abs(tu), au.y * b.y + au.x * b.x + au.z * b.z + a[0]) ||
      apart(std::abs(tv), av.y * b.y + av.x * b.x + av.z * b.z + a[1]) ||
      apart(std::abs(tw), aw.y * b.y + aw.x * b.x + aw.z * b.z + a[2]) ||
      apart(std::abs(t.x), a[1] * av.x + a[0] * au.x + a[2] * aw.x + b.x) ||
      apart(std::abs(t.y), av.y * a[1] + au.y * a[0] + aw.y * a[2] + b.y) ||
      apart(std::abs(t.z), av.z * a[1] + au.z * a[0] + aw.z * a[2] + b.z)) {
    return false;
  }
  // Les neuf axes croisés, avec les rayons tels que le client les compose.
  return !(apart(std::abs(v.x * tw - w.x * tv),
                 (a[2] * av.x + a[1] * aw.x) + (au.y * b.z + au.z * b.y)) ||
           apart(std::abs(v.y * tw - w.y * tv),
                 (aw.y * a[1] + av.y * a[2]) + (au.z * b.x + au.x * b.z)) ||
           apart(std::abs(v.z * tw - w.z * tv),
                 (aw.z * a[1] + av.z * a[2]) + (au.y * b.x + au.x * b.y)) ||
           apart(std::abs(w.x * tu - u.x * tw),
                 (av.y * b.z + av.z * b.y) + (a[0] * aw.x + a[2] * au.x)) ||
           apart(std::abs(w.y * tu - u.y * tw),
                 (av.z * b.x + av.x * b.z) + (aw.y * a[0] + au.y * a[2])) ||
           apart(std::abs(w.z * tu - u.z * tw),
                 (aw.z * a[0] + au.z * a[2]) + (av.y * b.x + b.y * av.x)) ||
           apart(std::abs(u.x * tv - v.x * tu),
                 (a[0] * av.x + au.x * a[1]) + (aw.z * b.y + aw.y * b.z)) ||
           apart(std::abs(u.y * tv - v.y * tu),
                 (aw.z * b.x + aw.x * b.z) + (au.y * a[1] + av.y * a[0])) ||
           apart(std::abs(u.z * tv - v.z * tu),
                 (aw.y * b.x + aw.x * b.y) + (au.z * a[1] + av.z * a[0])));
}

SightQuadTree SightQuadTree::Build(const formats::RswScene& rsw, const formats::GndMap& gnd,
                                   const formats::GatMap* gat, std::span<const SightBox> boxes) {
  SightQuadTree tree;
  const float zoom = gnd.proportion_ratio();
  const std::int32_t width = gnd.width();
  const std::int32_t height = gnd.height();
  const glm::vec3 lowest{static_cast<float>(width / -kSplitAxes) * zoom, -kUnbounded,
                         static_cast<float>(height / -kSplitAxes) * zoom};
  const glm::vec3 highest{static_cast<float>(width - width / kSplitAxes) * zoom - kRootInset,
                          kUnbounded,
                          static_cast<float>(height - height / kSplitAxes) * zoom - kRootInset};
  tree.nodes_.reserve(formats::kRswQuadTreeNodeCount);
  static_cast<void>(tree.Subdivide(lowest, highest, 0));

  // LE RANGEMENT, sur le découpage calculé : la racine n'éprouve pas, elle
  // confie chaque boîte aux enfants qu'elle touche.
  for (std::size_t item = 0; item < boxes.size(); ++item) {
    for (const std::uint32_t child : tree.nodes_.front().children) {
      const Node& node = tree.nodes_[child];
      if (BoxMeetsNode(boxes[item], node.lowest, node.highest)) {
        tree.Insert(child, static_cast<std::uint32_t>(item), boxes[item]);
      }
    }
  }

  // PUIS LES BORNES QUE LA DROITE ÉPROUVERA.
  const std::vector<formats::RswQuadTreeNode>& stored = rsw.quad_tree();
  if (stored.size() == tree.nodes_.size()) {
    for (std::size_t rank = 0; rank < stored.size(); ++rank) {
      tree.nodes_[rank].lowest =
          glm::vec3(stored[rank].min[0], stored[rank].min[1], stored[rank].min[2]);
      tree.nodes_[rank].highest =
          glm::vec3(stored[rank].max[0], stored[rank].max[1], stored[rank].max[2]);
    }
  } else {
    tree.RaiseLeaves(gnd, gat, boxes);
  }
  return tree;
}

std::uint32_t SightQuadTree::Subdivide(const glm::vec3& lowest, const glm::vec3& highest,
                                       std::int32_t depth) {
  const auto rank = static_cast<std::uint32_t>(nodes_.size());
  nodes_.push_back(Node{.lowest = {lowest.x, -kUnbounded, lowest.z},
                        .highest = {highest.x, kUnbounded, highest.z},
                        .leaf = depth == kLeafDepth});
  if (depth == kLeafDepth) {
    return rank;
  }
  const float middle_x = (lowest.x + highest.x) * 0.5F;
  const float middle_z = (lowest.z + highest.z) * 0.5F;
  const std::array<std::array<float, 4>, kChildren> quarters = {{
      {lowest.x, middle_x, lowest.z, middle_z},
      {middle_x, highest.x, lowest.z, middle_z},
      {lowest.x, middle_x, middle_z, highest.z},
      {middle_x, highest.x, middle_z, highest.z},
  }};
  for (std::size_t child = 0; child < kChildren; ++child) {
    const std::array<float, 4>& quarter = quarters[child];
    const std::uint32_t built = Subdivide(glm::vec3(quarter[0], 0.0F, quarter[2]),
                                          glm::vec3(quarter[1], 0.0F, quarter[3]), depth + 1);
    nodes_[rank].children[child] = built;
  }
  return rank;
}

void SightQuadTree::Insert(std::uint32_t node, std::uint32_t item, const SightBox& box) {
  if (nodes_[node].leaf) {
    nodes_[node].items.push_back(item);
    return;
  }
  for (const std::uint32_t child : nodes_[node].children) {
    if (BoxMeetsNode(box, nodes_[child].lowest, nodes_[child].highest)) {
      Insert(child, item, box);
    }
  }
}

void SightQuadTree::RaiseLeaves(const formats::GndMap& gnd, const formats::GatMap* gat,
                                std::span<const SightBox> boxes) {
  for (Node& node : nodes_) {
    if (!node.leaf) {
      continue;
    }
    const HeightRange ground = GroundRangeOf(gnd, GroundCellOf(gnd, node.lowest.x, node.lowest.z),
                                             GroundCellOf(gnd, node.highest.x, node.highest.z));
    const HeightRange walk = gat == nullptr
                                 ? HeightRange()
                                 : WalkRangeOf(*gat, WalkCellOf(*gat, node.lowest.x, node.lowest.z),
                                               WalkCellOf(*gat, node.highest.x, node.highest.z));
    node.highest.y = std::fmax(walk.highest, ground.highest);
    node.lowest.y = std::fmin(walk.lowest, ground.lowest);
    for (const std::uint32_t item : node.items) {
      node.lowest.y = std::min(node.lowest.y, boxes[item].lowest.y);
      node.highest.y = std::max(node.highest.y, boxes[item].highest.y);
    }
  }
}

void SightQuadTree::CollectAlong(const glm::vec3& origin, const glm::vec3& direction,
                                 std::vector<std::uint32_t>& found) const {
  if (nodes_.empty()) {
    return;
  }
  for (const std::uint32_t child : nodes_.front().children) {
    if (RayMeetsNode(origin, direction, nodes_[child].lowest, nodes_[child].highest)) {
      Collect(child, origin, direction, found);
    }
  }
}

void SightQuadTree::Collect(std::uint32_t node, const glm::vec3& origin, const glm::vec3& direction,
                            std::vector<std::uint32_t>& found) const {
  const Node& here = nodes_[node];
  if (here.leaf) {
    found.insert(found.end(), here.items.begin(), here.items.end());
    return;
  }
  for (const std::uint32_t child : here.children) {
    if (RayMeetsNode(origin, direction, nodes_[child].lowest, nodes_[child].highest)) {
      Collect(child, origin, direction, found);
    }
  }
}

}  // namespace ro::render
