#include "ro/render/model_pose.h"

#include <cmath>
#include <string>
#include <unordered_map>
#include <utility>

#include <glm/gtc/matrix_transform.hpp>

namespace ro::render {
namespace {

using formats::RsmMesh;
using formats::RsmModel;
using formats::RsmRotationKey;

// Composantes d'un quaternion, dans l'ordre du fichier.
constexpr std::size_t kX = 0;
constexpr std::size_t kY = 1;
constexpr std::size_t kZ = 2;
constexpr std::size_t kW = 3;

// Sous cet écart à un, les deux quaternions sont jugés confondus et
// l'interpolation redevient linéaire.
constexpr float kSlerpLinearThreshold = 1e-6F;

// Le cran d'une image par pas vaut `vitesse / 3 × 100`.
constexpr float kSpeedDivisor = 3.0F;
constexpr float kFramesPerSpeedUnit = 100.0F;

}  // namespace

std::vector<NodePlacement> PlaceNodes(const RsmModel& model) {
  const std::vector<RsmMesh>& meshes = model.meshes();

  // 🔴 LE PARENT EST LE PREMIER MAILLAGE DU FICHIER QUI PORTE LE NOM DEMANDÉ,
  // et c'est la règle du client : sa recherche parcourt les nœuds dans l'ordre
  // et rend le premier homonyme. Des modèles convertis nomment TOUS leurs
  // maillages comme le modèle, et chacun se dit enfant de ce nom : ils
  // s'accrochent au premier, et non à eux-mêmes. En faire des racines rompt la
  // maison — ses façades partent ailleurs, parfois au milieu de la route.
  //
  // ⚠ UN MAILLAGE DONT LE PREMIER HOMONYME EST LUI-MÊME reste une racine : il ne
  // peut pas être son propre enfant.
  std::unordered_map<std::string, std::size_t> first_named;
  for (std::size_t index = 0; index < meshes.size(); ++index) {
    first_named.try_emplace(meshes[index].name, index);
  }
  std::vector<std::vector<std::size_t>> children(meshes.size());
  std::vector<std::size_t> roots;
  std::vector<std::size_t> orphans;
  for (std::size_t index = 0; index < meshes.size(); ++index) {
    const RsmMesh& mesh = meshes[index];
    if (mesh.parent_name.empty()) {
      roots.push_back(index);
      continue;
    }
    const auto parent = first_named.find(mesh.parent_name);
    if (parent == first_named.end()) {
      orphans.push_back(index);
    } else if (parent->second == index) {
      roots.push_back(index);
    } else {
      children[parent->second].push_back(index);
    }
  }
  // Les orphelins APRÈS les vraies racines : c'est l'ordre dans lequel leurs
  // sommets s'émettent, et tout le reste du moteur l'a toujours vu ainsi.
  roots.insert(roots.end(), orphans.begin(), orphans.end());

  // Parcours en profondeur, parent d'abord. La profondeur est bornée par le
  // nombre de maillages : un arbre légitime ne peut pas descendre plus bas, et
  // deux homonymes qui se désignent l'un l'autre bouclent sans cette borne.
  struct Pending {
    std::size_t mesh_index;
    std::int32_t parent;
    std::size_t depth;
  };
  std::vector<NodePlacement> placements;
  for (const std::size_t root : roots) {
    std::vector<Pending> stack{{root, -1, 0}};
    while (!stack.empty()) {
      const Pending pending = stack.back();
      stack.pop_back();
      const auto placed = static_cast<std::int32_t>(placements.size());
      placements.push_back({pending.mesh_index, pending.parent});
      if (pending.depth >= meshes.size()) {
        continue;
      }
      const std::vector<std::size_t>& found = children[pending.mesh_index];
      // À l'envers sur la pile, pour les visiter dans l'ordre du fichier.
      for (auto child = found.rbegin(); child != found.rend(); ++child) {
        stack.push_back({*child, placed, pending.depth + 1});
      }
    }
  }
  return placements;
}

std::vector<FirstFamilyNode> FirstFamilyNodes(const RsmModel& model) {
  std::vector<FirstFamilyNode> nodes;
  for (const NodePlacement& placement : PlaceNodes(model)) {
    const RsmMesh& mesh = model.meshes()[placement.mesh_index];
    FirstFamilyNode node;
    node.mesh_index = placement.mesh_index;
    node.parent = placement.parent;
    node.position = {mesh.global_position[0], mesh.global_position[1], mesh.global_position[2]};
    node.scale = {mesh.global_scale[0], mesh.global_scale[1], mesh.global_scale[2]};
    node.rest_rotation = AxisAngleQuaternion(mesh.global_rotation_axis, mesh.global_rotation_angle);
    node.rotation_keys = mesh.rotation_keys;
    nodes.push_back(std::move(node));
  }
  return nodes;
}

std::array<float, 4> AxisAngleQuaternion(const std::array<float, 3>& axis, float angle_radians) {
  const float half = angle_radians * 0.5F;
  const float sine = std::sin(half);
  return {sine * axis[0], sine * axis[1], sine * axis[2], std::cos(half)};
}

glm::mat4 QuaternionMatrix(const std::array<float, 4>& quaternion) {
  const float x = quaternion[kX];
  const float y = quaternion[kY];
  const float z = quaternion[kZ];
  const float w = quaternion[kW];
  // La formule du natif, en vecteur-LIGNE : `natif[ligne][colonne]`. glm range
  // par colonnes et multiplie en vecteur-colonne ; les deux transpositions
  // s'annulent, et `matrice[i][j]` reçoit directement `natif[i][j]`.
  glm::mat4 matrix{1.0F};
  matrix[0][0] = 1.0F - 2.0F * (z * z + y * y);
  matrix[0][1] = 2.0F * x * y + 2.0F * w * z;
  matrix[0][2] = 2.0F * x * z - 2.0F * w * y;
  matrix[1][0] = 2.0F * x * y - 2.0F * w * z;
  matrix[1][1] = 1.0F - 2.0F * (z * z + x * x);
  matrix[1][2] = 2.0F * y * z + 2.0F * w * x;
  matrix[2][0] = 2.0F * x * z + 2.0F * w * y;
  matrix[2][1] = 2.0F * y * z - 2.0F * w * x;
  matrix[2][2] = 1.0F - 2.0F * (y * y + x * x);
  return matrix;
}

std::array<float, 4> SlerpQuaternion(const std::array<float, 4>& from,
                                     const std::array<float, 4>& to, float amount) {
  const float cosine =
      from[kX] * to[kX] + from[kY] * to[kY] + from[kZ] * to[kZ] + from[kW] * to[kW];
  const float absolute = std::abs(cosine);
  float from_weight = 1.0F - amount;
  float to_weight = amount;
  if (1.0F - absolute >= kSlerpLinearThreshold) {
    const float angle = std::acos(absolute);
    const float sine = std::sin(angle);
    from_weight = std::sin(angle - angle * amount) / sine;
    to_weight = std::sin(angle * amount) / sine;
  }
  // Le chemin le plus court : un cosinus négatif retourne l'arrivée.
  if (cosine < 0.0F) {
    to_weight = -to_weight;
  }
  return {from[kX] * from_weight + to[kX] * to_weight, from[kY] * from_weight + to[kY] * to_weight,
          from[kZ] * from_weight + to[kZ] * to_weight, from[kW] * from_weight + to[kW] * to_weight};
}

glm::mat4 FirstFamilyRotationAt(const FirstFamilyNode& node, std::int32_t frame) {
  const std::vector<RsmRotationKey>& keys = node.rotation_keys;
  // Sans image-clé, le natif en fabrique une de la rotation au repos.
  if (keys.empty()) {
    return QuaternionMatrix(node.rest_rotation);
  }
  if (keys.size() == 1) {
    return QuaternionMatrix(keys.front().quaternion);
  }
  std::size_t next = 1;
  while (next < keys.size() && keys[next].frame <= frame) {
    ++next;
  }
  if (next == keys.size()) {
    return glm::mat4{1.0F};
  }
  const RsmRotationKey& before = keys[next - 1];
  const RsmRotationKey& after = keys[next];
  // Deux quaternions de partie réelle UN : le natif rend l'identité sans les
  // interpoler, quel que soit le reste de leurs composantes.
  if (before.quaternion[kW] == 1.0F && after.quaternion[kW] == 1.0F) {
    return glm::mat4{1.0F};
  }
  const auto span = static_cast<std::int64_t>(after.frame) - before.frame;
  // Deux images-clés au même numéro donneraient une division par zéro : on
  // reste sur la première plutôt que de propager un nombre non défini.
  const float amount = span == 0
                           ? 0.0F
                           : static_cast<float>(static_cast<std::int64_t>(frame) - before.frame) /
                                 static_cast<float>(span);
  return QuaternionMatrix(SlerpQuaternion(before.quaternion, after.quaternion, amount));
}

std::vector<glm::mat4> PoseFirstFamily(std::span<const FirstFamilyNode> nodes, std::int32_t frame) {
  std::vector<glm::mat4> poses;
  poses.reserve(nodes.size());
  for (const FirstFamilyNode& node : nodes) {
    const glm::mat4 own = glm::translate(glm::mat4{1.0F}, node.position) *
                          FirstFamilyRotationAt(node, frame) *
                          glm::scale(glm::mat4{1.0F}, node.scale);
    const glm::mat4 inherited =
        node.parent >= 0 ? poses[static_cast<std::size_t>(node.parent)] : glm::mat4{1.0F};
    poses.push_back(inherited * own);
  }
  return poses;
}

bool FirstFamilyMoves(std::span<const FirstFamilyNode> nodes) {
  for (const FirstFamilyNode& node : nodes) {
    if (node.rotation_keys.size() >= 2) {
      return true;
    }
  }
  return false;
}

std::int32_t NextFirstFamilyFrame(std::int32_t frame, std::int32_t animation_type,
                                  float animation_speed, std::int32_t animation_length) {
  if (animation_type != kModelPlayOnce && animation_type != kModelPlayLooped) {
    return frame;
  }
  // Les opérations du natif, dans son ordre et en simple précision : la
  // troncature porte sur la SOMME, pas sur le cran.
  const float per_step = animation_speed / kSpeedDivisor;
  const float reached = per_step * kFramesPerSpeedUnit + static_cast<float>(frame);
  const auto next = static_cast<std::int64_t>(reached);
  if (next >= animation_length) {
    return animation_type == kModelPlayLooped ? 0 : animation_length - 1;
  }
  return static_cast<std::int32_t>(next);
}

bool FirstFamilyStaysStill(std::int32_t animation_type, float animation_speed,
                           std::int32_t animation_length) {
  return NextFirstFamilyFrame(0, animation_type, animation_speed, animation_length) == 0;
}

}  // namespace ro::render
