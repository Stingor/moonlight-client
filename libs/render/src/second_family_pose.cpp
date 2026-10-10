#include "ro/render/second_family_pose.h"

#include <cmath>
#include <limits>
#include <utility>

#include "ro/render/model_pose.h"

namespace ro::render {
namespace {

using formats::RsmMesh;
using formats::RsmModel;
using formats::RsmPositionKey;
using formats::RsmRotationKey;
using formats::RsmScaleKey;
using formats::RsmTextureAnimation;
using formats::RsmTextureKey;

// Composantes d'un quaternion, dans l'ordre du fichier.
constexpr std::size_t kX = 0;
constexpr std::size_t kY = 1;
constexpr std::size_t kZ = 2;
constexpr std::size_t kW = 3;

// Au-delà de ce cosinus, les deux quaternions sont jugés assez proches pour une
// interpolation LINÉAIRE. La première famille
// place ce seuil bien plus haut.
constexpr float kLinearAbove = 0.95F;

// Les numéros des grandeurs de texture, tels que le fichier les écrit.
constexpr std::int32_t kOffsetU = 0;
constexpr std::int32_t kOffsetV = 1;
constexpr std::int32_t kScaleU = 2;
constexpr std::int32_t kScaleV = 3;
constexpr std::int32_t kAngle = 4;

// Le centre d'une texture, autour duquel elle tourne et s'étire.
constexpr float kTextureCenter = 0.5F;

constexpr float kMillisecondsPerSecond = 1000.0F;

// La part d'interpolation entre deux images-clés, à l'image `frame`. Deux
// images-clés au même numéro donneraient une division par zéro : on reste sur
// la première.
float Amount(float frame, std::int32_t from, std::int32_t to) {
  const auto span = static_cast<std::int64_t>(to) - from;
  return span == 0 ? 0.0F : (frame - static_cast<float>(from)) / static_cast<float>(span);
}

// Le premier index dont l'image-clé DÉPASSE l'image, en partant de zéro.
template <typename Key>
std::size_t FirstKeyAfter(const std::vector<Key>& keys, float frame) {
  std::size_t index = 0;
  while (index < keys.size() && static_cast<float>(keys[index].frame) <= frame) {
    ++index;
  }
  return index;
}

glm::vec3 ToVec3(const std::array<float, 3>& value) { return {value[0], value[1], value[2]}; }

// L'échelle d'un nœud. Avant la première image-clé, elle part de
// l'unité à l'image zéro.
glm::vec3 ScaleAt(const std::vector<RsmScaleKey>& keys, float frame) {
  if (keys.empty()) {
    return glm::vec3{1.0F};
  }
  if (keys.size() == 1 || frame >= static_cast<float>(keys.back().frame)) {
    return ToVec3(keys.size() == 1 ? keys.front().scale : keys.back().scale);
  }
  const std::size_t next = FirstKeyAfter(keys, frame);
  const std::int32_t from_frame = next > 0 ? keys[next - 1].frame : 0;
  const glm::vec3 from = next > 0 ? ToVec3(keys[next - 1].scale) : glm::vec3{1.0F};
  const float amount = Amount(frame, from_frame, keys[next].frame);
  return from * (1.0F - amount) + ToVec3(keys[next].scale) * amount;
}

// Le quaternion d'une rotation au repos.
//
// ⚠ LE NATIF EN REND LE CONJUGUÉ : ses signes sont ceux de la formule D3DX
// inversés, si bien que la matrice qu'on en retire est la rotation INVERSE. Il
// ne s'en sert que pour interpoler vers la première image-clé, avant elle.
std::array<float, 4> RestQuaternion(const glm::mat4& rest) {
  const float root = std::sqrt(rest[0][0] + 1.0F + rest[1][1] + rest[2][2]);
  const float w = root * 0.5F;
  const float divisor = w * 4.0F;
  // Une rotation d'un demi-tour annule la partie réelle : le natif diviserait
  // par zéro, on s'en tient à l'identité.
  if (!(divisor != 0.0F) || !std::isfinite(divisor)) {
    return {0.0F, 0.0F, 0.0F, 1.0F};
  }
  return {(rest[2][1] - rest[1][2]) / divisor, (rest[0][2] - rest[2][0]) / divisor,
          (rest[1][0] - rest[0][1]) / divisor, w};
}

// L'interpolation de la seconde famille : c'est le DÉPART qu'on
// retourne quand le cosinus est négatif, et elle devient linéaire au-dessus de
// 0,95 — sans renormaliser.
std::array<float, 4> Slerp(std::array<float, 4> from, const std::array<float, 4>& to,
                           float amount) {
  float cosine = to[kX] * from[kX] + to[kY] * from[kY] + to[kZ] * from[kZ] + to[kW] * from[kW];
  if (cosine < 0.0F) {
    for (float& component : from) {
      component = -component;
    }
    cosine = -cosine;
  }
  float from_weight = 1.0F - amount;
  float to_weight = amount;
  if (!(cosine > kLinearAbove)) {
    const float angle = std::acos(cosine);
    const float sine = std::sin(angle);
    from_weight = std::sin(from_weight * angle) / sine;
    to_weight = std::sin(angle * amount) / sine;
  }
  return {to[kX] * to_weight + from[kX] * from_weight, to[kY] * to_weight + from[kY] * from_weight,
          to[kZ] * to_weight + from[kZ] * from_weight, to[kW] * to_weight + from[kW] * from_weight};
}

// La rotation d'un nœud, 3 × 3 en convention colonne.
glm::mat3 RotationAt(const SecondFamilyNode& node, float frame) {
  const std::vector<RsmRotationKey>& keys = node.rotation_keys;
  if (keys.empty()) {
    return glm::mat3(node.rest);
  }
  if (keys.size() == 1 || frame >= static_cast<float>(keys.back().frame)) {
    return glm::mat3(
        QuaternionMatrix(keys.size() == 1 ? keys.front().quaternion : keys.back().quaternion));
  }
  const std::size_t next = FirstKeyAfter(keys, frame);
  const std::int32_t from_frame = next > 0 ? keys[next - 1].frame : 0;
  const std::array<float, 4> from =
      next > 0 ? keys[next - 1].quaternion : RestQuaternion(node.rest);
  const float amount = Amount(frame, from_frame, keys[next].frame);
  return glm::mat3(QuaternionMatrix(Slerp(from, keys[next].quaternion, amount)));
}

// La position d'un nœud. Avant la première image-clé, elle part de
// celle du repos à l'image zéro.
glm::vec3 PositionAt(const SecondFamilyNode& node, float frame) {
  const std::vector<RsmPositionKey>& keys = node.position_keys;
  const glm::vec3 rest{node.rest[3]};
  if (keys.empty()) {
    return rest;
  }
  if (keys.size() == 1 || frame >= static_cast<float>(keys.back().frame)) {
    return ToVec3(keys.size() == 1 ? keys.front().position : keys.back().position);
  }
  const std::size_t next = FirstKeyAfter(keys, frame);
  const std::int32_t from_frame = next > 0 ? keys[next - 1].frame : 0;
  const glm::vec3 from = next > 0 ? ToVec3(keys[next - 1].position) : rest;
  const float amount = Amount(frame, from_frame, keys[next].frame);
  return from * (1.0F - amount) + ToVec3(keys[next].position) * amount;
}

// Une grandeur de texture à l'image `frame`.
float ValueAt(const std::vector<RsmTextureKey>& keys, float frame) {
  if (frame >= static_cast<float>(keys.back().frame) || keys.size() == 1) {
    // Seule, une image-clé vaut aussi AVANT son numéro : le natif lirait là
    // au-delà de sa liste.
    return keys.size() == 1 ? keys.front().value : keys.back().value;
  }
  std::size_t next = 1;
  while (next + 1 < keys.size() && static_cast<float>(keys[next].frame) <= frame) {
    ++next;
  }
  const float amount = Amount(frame, keys[next - 1].frame, keys[next].frame);
  return (1.0F - amount) * keys[next - 1].value + amount * keys[next].value;
}

// Conversion saturante d'un flottant en entier non signé, troncature vers zéro
// (ftoui_sat du natif).
std::uint32_t SaturatingUnsigned(double value) {
  constexpr double kCeiling = static_cast<double>(std::numeric_limits<std::uint32_t>::max());
  if (!(value > 0.0)) {
    return 0;
  }
  if (value >= kCeiling) {
    return std::numeric_limits<std::uint32_t>::max();
  }
  return static_cast<std::uint32_t>(value);
}

}  // namespace

glm::mat4 FileNodeMatrix(const RsmMesh& mesh) {
  // Même disposition que la 3 × 3 de la première famille : `matrice[i][j]`
  // reçoit l'élément (i, j) du fichier — rangé pour le vecteur-LIGNE, il
  // devient ainsi la transposée qu'attend le vecteur-colonne — et le décalage
  // fait la translation.
  glm::mat4 matrix{1.0F};
  for (int file_row = 0; file_row < 3; ++file_row) {
    for (int file_column = 0; file_column < 3; ++file_column) {
      matrix[file_row][file_column] =
          mesh.transform[static_cast<std::size_t>(file_row * 3 + file_column)];
    }
  }
  matrix[3] = glm::vec4{ToVec3(mesh.local_position), 1.0F};
  return matrix;
}

std::vector<SecondFamilyNode> SecondFamilyNodes(const RsmModel& model) {
  const std::vector<NodePlacement> placements = PlaceNodes(model);
  std::vector<SecondFamilyNode> nodes;
  nodes.reserve(placements.size());
  for (const NodePlacement& placement : placements) {
    const RsmMesh& mesh = model.meshes()[placement.mesh_index];
    SecondFamilyNode node;
    node.mesh_index = placement.mesh_index;
    node.parent = placement.parent;
    // Le natif écrit « matrice du nœud · inverse de celle du parent » en
    // vecteur-LIGNE ; en vecteur-colonne, les facteurs s'échangent.
    const glm::mat4 own = FileNodeMatrix(mesh);
    node.rest = placement.parent >= 0
                    ? glm::inverse(FileNodeMatrix(
                          model.meshes()[placements[static_cast<std::size_t>(placement.parent)]
                                             .mesh_index])) *
                          own
                    : own;
    node.scale_keys = mesh.scale_keys;
    node.rotation_keys = mesh.rotation_keys;
    node.position_keys = mesh.position_keys;
    node.texture_animations = mesh.texture_animations;
    nodes.push_back(std::move(node));
  }
  return nodes;
}

glm::mat4 SecondFamilyNodePose(const SecondFamilyNode& node, float frame) {
  // Le natif compose « échelle · rotation » en vecteur-LIGNE : l'échelle
  // s'applique d'abord. La position ne se compose pas, elle REMPLACE la
  // translation.
  const glm::vec3 scale = ScaleAt(node.scale_keys, frame);
  glm::mat3 oriented = RotationAt(node, frame);
  for (int column = 0; column < 3; ++column) {
    oriented[column] *= scale[column];
  }
  glm::mat4 pose{oriented};
  pose[3] = glm::vec4{PositionAt(node, frame), 1.0F};
  return pose;
}

std::vector<glm::mat4> PoseSecondFamily(std::span<const SecondFamilyNode> nodes, float frame) {
  std::vector<glm::mat4> poses;
  poses.reserve(nodes.size());
  for (const SecondFamilyNode& node : nodes) {
    const glm::mat4 inherited =
        node.parent >= 0 ? poses[static_cast<std::size_t>(node.parent)] : glm::mat4{1.0F};
    poses.push_back(inherited * SecondFamilyNodePose(node, frame));
  }
  return poses;
}

bool SecondFamilyMoves(std::span<const SecondFamilyNode> nodes) {
  for (const SecondFamilyNode& node : nodes) {
    if (node.scale_keys.size() >= 2 || node.rotation_keys.size() >= 2 ||
        node.position_keys.size() >= 2) {
      return true;
    }
    for (const RsmTextureAnimation& animation : node.texture_animations) {
      if (animation.keys.size() >= 2) {
        return true;
      }
    }
  }
  return false;
}

TextureMotion TextureMotionAt(std::span<const RsmTextureAnimation> animations,
                              std::int32_t texture_index, float frame) {
  TextureMotion motion;
  // Deux animations de la même grandeur pour la même texture : la dernière lue
  // l'emporte, comme dans la table du natif.
  for (const RsmTextureAnimation& animation : animations) {
    if (animation.texture_index != texture_index || animation.keys.empty()) {
      continue;
    }
    const float value = ValueAt(animation.keys, frame);
    switch (animation.motion) {
      case kOffsetU:
        motion.offset_u = value;
        break;
      case kOffsetV:
        motion.offset_v = value;
        break;
      case kScaleU:
        motion.scale_u = value;
        break;
      case kScaleV:
        motion.scale_v = value;
        break;
      case kAngle:
        motion.angle = value;
        break;
      default:
        // Le natif ne connaît que ces cinq grandeurs.
        break;
    }
  }
  return motion;
}

std::array<float, 2> MoveTextureCoordinate(const TextureMotion& motion,
                                           const std::array<float, 2>& uv) {
  const float cosine = std::cos(motion.angle);
  const float sine = std::sin(motion.angle);
  const float u = uv[0] - kTextureCenter;
  const float v = uv[1] - kTextureCenter;
  return {(cosine * u - sine * v) * motion.scale_u + kTextureCenter + motion.offset_u,
          (sine * u + cosine * v) * motion.scale_v + kTextureCenter + motion.offset_v};
}

double SecondFamilyDurationMs(std::int32_t animation_length, float frame_rate) {
  // Le natif multiplie en ENTIER sur 32 bits, puis passe en simple précision.
  const auto thousandfold =
      static_cast<std::int32_t>(static_cast<std::int64_t>(animation_length) * 1000);
  const float inverse_rate = 1.0F / frame_rate;
  return static_cast<double>(static_cast<float>(thousandfold) * inverse_rate);
}

SecondFamilyPlayed SecondFamilyPlayedAt(std::uint32_t elapsed_ms, std::int32_t animation_type,
                                        float animation_speed, double duration_ms) {
  if (animation_type != kModelPlayOnce && animation_type != kModelPlayLooped) {
    return {};
  }
  // L'écoulé passe en double, puis en simple, AVANT la vitesse.
  const float elapsed = static_cast<float>(static_cast<double>(elapsed_ms));
  std::uint32_t played = SaturatingUnsigned(static_cast<double>(elapsed * animation_speed));
  if (static_cast<double>(played) >= duration_ms) {
    if (animation_type == kModelPlayLooped) {
      return {.played_ms = 0, .restart = true};
    }
    played = SaturatingUnsigned(duration_ms - 1.0);
  }
  return {.played_ms = played, .restart = false};
}

float SecondFamilyFrameAt(std::uint32_t played_ms, float frame_rate) {
  return static_cast<float>(static_cast<double>(frame_rate) * static_cast<double>(played_ms) /
                            static_cast<double>(kMillisecondsPerSecond));
}

}  // namespace ro::render
