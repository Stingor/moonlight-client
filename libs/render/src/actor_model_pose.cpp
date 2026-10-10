#include "ro/render/actor_model_pose.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <glm/gtc/type_ptr.hpp>

#include "ro/core/ascii.h"

namespace ro::render {
namespace {

using formats::GrannyCurve;
using formats::GrannyTransform;

constexpr std::size_t kPositionSize = 3;
constexpr std::size_t kOrientationSize = 4;
constexpr std::size_t kScaleShearSize = 9;
constexpr std::size_t kDiagonalScaleSize = 3;
// En deçà, un dénominateur tient pour nul et le coefficient qu'il porte aussi.
constexpr double kNegligible = 1e-12;
// Le poids d'un os se range sur un octet.
constexpr float kWeightScale = 255.0F;

double SafeDivide(double numerator, double denominator) {
  return std::abs(denominator) < kNegligible ? 0.0 : numerator / denominator;
}

// Le premier nœud STRICTEMENT après l'instant, ou le dernier.
std::size_t FindKnot(const std::vector<float>& knots, double seconds) {
  for (std::size_t index = 0; index < knots.size(); ++index) {
    if (static_cast<double>(knots[index]) > seconds) {
      return index;
    }
  }
  return knots.empty() ? 0 : knots.size() - 1;
}

std::vector<double> ControlAt(const GrannyCurve& curve, std::size_t knot) {
  const auto dimension = static_cast<std::size_t>(curve.dimension);
  const std::size_t count = curve.controls.size() / dimension;
  const std::size_t index = std::min(knot, count - 1);
  std::vector<double> control(dimension);
  for (std::size_t lane = 0; lane < dimension; ++lane) {
    control[lane] = curve.controls[index * dimension + lane];
  }
  return control;
}

bool EqualIgnoringCase(std::string_view left, std::string_view right) {
  return core::AsciiLower(left) == core::AsciiLower(right);
}

}  // namespace

std::optional<std::vector<float>> EvaluateGrannyCurve(const GrannyCurve& curve, float seconds) {
  if (curve.knots.empty() || curve.controls.empty() || curve.dimension <= 0) {
    return std::nullopt;
  }
  const auto dimension = static_cast<std::size_t>(curve.dimension);
  const std::vector<float>& knots = curve.knots;
  const double time = seconds;
  const std::size_t knot = FindKnot(knots, time);
  std::vector<double> result;
  const std::int32_t degree = std::max(curve.degree, 0);
  if (degree == 0 || knots.size() == 1) {
    result = ControlAt(curve, knot);
  } else {
    // La fenêtre de la B-spline : `degré` nœuds de part et d'autre, bornés aux
    // extrémités.
    const auto window = static_cast<std::size_t>(2 * degree);
    const auto first = static_cast<std::int64_t>(knot) - degree;
    std::vector<double> times(window);
    std::vector<std::vector<double>> points(window);
    for (std::size_t local = 0; local < window; ++local) {
      const std::int64_t source = std::clamp<std::int64_t>(
          first + static_cast<std::int64_t>(local), 0, static_cast<std::int64_t>(knots.size()) - 1);
      times[local] = knots[static_cast<std::size_t>(source)];
      points[local] = ControlAt(curve, static_cast<std::size_t>(source));
    }
    const auto center = static_cast<std::size_t>(degree);
    result.assign(dimension, 0.0);
    if (degree == 1) {
      const double blend = SafeDivide(time - times[center - 1], times[center] - times[center - 1]);
      for (std::size_t lane = 0; lane < dimension; ++lane) {
        result[lane] = (1.0 - blend) * points[center - 1][lane] + blend * points[center][lane];
      }
    } else if (degree == 2) {
      const double before = times[center - 2];
      const double previous = times[center - 1];
      const double here = times[center];
      const double next = times[center + 1];
      const double l0 = SafeDivide(time - previous, here - previous);
      const double l1_first = SafeDivide(time - before, here - before);
      const double l1_second = SafeDivide(time - previous, next - previous);
      const double two_first = (l1_first + l0) - l0 * l1_first;
      const double weight_here = l0 * l1_second;
      const double weight_previous = two_first - weight_here;
      const double weight_before = 1.0 - two_first;
      for (std::size_t lane = 0; lane < dimension; ++lane) {
        result[lane] = weight_before * points[center - 2][lane] +
                       weight_previous * points[center - 1][lane] +
                       weight_here * points[center][lane];
      }
    } else {
      result = points[center];
    }
  }
  if (dimension == kOrientationSize) {
    // La renormalisation RAPIDE, au premier ordre, et non la division exacte.
    double squared = 0.0;
    for (const double lane : result) {
      squared += lane * lane;
    }
    const double factor = (3.0 - squared) * 0.5;
    for (double& lane : result) {
      lane *= factor;
    }
  }
  std::vector<float> values(result.size());
  std::transform(result.begin(), result.end(), values.begin(),
                 [](double lane) { return static_cast<float>(lane); });
  return values;
}

std::optional<ActorModelAnimation> BindActorModelAnimation(
    const formats::GrannyScene& model, const formats::GrannyAnimation& animation) {
  const formats::GrannyTrackGroup* group = nullptr;
  for (const formats::GrannyTrackGroup& candidate : animation.track_groups) {
    if (EqualIgnoringCase(candidate.name, model.model_name)) {
      group = &candidate;
      break;
    }
  }
  if (group == nullptr) {
    return std::nullopt;
  }
  ActorModelAnimation bound;
  bound.animation = &animation;
  bound.track_of_bone.assign(model.bones.size(), nullptr);
  for (std::size_t bone = 0; bone < model.bones.size(); ++bone) {
    for (const formats::GrannyTrack& track : group->tracks) {
      if (EqualIgnoringCase(track.name, model.bones[bone].name)) {
        bound.track_of_bone[bone] = &track;
        break;
      }
    }
  }
  return bound;
}

std::vector<GrannyTransform> SampleLocalPose(const formats::GrannyScene& model,
                                             const ActorModelAnimation* animation,
                                             float local_seconds) {
  std::vector<GrannyTransform> pose;
  pose.reserve(model.bones.size());
  for (std::size_t bone = 0; bone < model.bones.size(); ++bone) {
    const formats::GrannyTrack* track =
        animation != nullptr && bone < animation->track_of_bone.size()
            ? animation->track_of_bone[bone]
            : nullptr;
    if (track == nullptr) {
      pose.push_back(model.bones[bone].local);
      continue;
    }
    GrannyTransform sampled;
    if (const auto position = EvaluateGrannyCurve(track->position, local_seconds);
        position.has_value() && position->size() == kPositionSize) {
      std::copy(position->begin(), position->end(), sampled.position.begin());
    }
    if (const auto orientation = EvaluateGrannyCurve(track->orientation, local_seconds);
        orientation.has_value() && orientation->size() == kOrientationSize) {
      std::copy(orientation->begin(), orientation->end(), sampled.orientation.begin());
    }
    if (const auto scale = EvaluateGrannyCurve(track->scale_shear, local_seconds);
        scale.has_value()) {
      if (scale->size() == kScaleShearSize) {
        std::copy(scale->begin(), scale->end(), sampled.scale_shear.begin());
      } else if (scale->size() == kDiagonalScaleSize) {
        sampled.scale_shear = {(*scale)[0], 0.0F, 0.0F, 0.0F,       (*scale)[1],
                               0.0F,        0.0F, 0.0F, (*scale)[2]};
      }
    }
    pose.push_back(sampled);
  }
  return pose;
}

namespace {

glm::dmat4 TransformMatrixDouble(const GrannyTransform& transform) {
  const double qx = transform.orientation[0];
  const double qy = transform.orientation[1];
  const double qz = transform.orientation[2];
  const double qw = transform.orientation[3];
  const double xx = qx * qx;
  const double yy = qy * qy;
  const double zz = qz * qz;
  const double xy = qx * qy;
  const double xz = qx * qz;
  const double yz = qy * qz;
  const double wx = qw * qx;
  const double wy = qw * qy;
  const double wz = qw * qz;
  // La rotation, ligne par ligne.
  const std::array<std::array<double, 3>, 3> rotation = {{
      {1.0 - 2.0 * (yy + zz), 2.0 * (xy - wz), 2.0 * (xz + wy)},
      {2.0 * (xy + wz), 1.0 - 2.0 * (xx + zz), 2.0 * (yz - wx)},
      {2.0 * (xz - wy), 2.0 * (yz + wx), 1.0 - 2.0 * (xx + yy)},
  }};
  glm::dmat4 matrix(1.0);
  for (std::size_t row = 0; row < 3; ++row) {
    for (std::size_t column = 0; column < 3; ++column) {
      double sum = 0.0;
      for (std::size_t inner = 0; inner < 3; ++inner) {
        sum +=
            rotation[row][inner] * static_cast<double>(transform.scale_shear[inner * 3 + column]);
      }
      matrix[static_cast<glm::length_t>(column)][static_cast<glm::length_t>(row)] = sum;
    }
  }
  matrix[3] = glm::dvec4(transform.position[0], transform.position[1], transform.position[2], 1.0);
  return matrix;
}

}  // namespace

glm::mat4 TransformMatrix(const GrannyTransform& transform) {
  return glm::mat4(TransformMatrixDouble(transform));
}

std::vector<glm::mat4> CompositeMatrices(const formats::GrannyScene& model,
                                         std::span<const GrannyTransform> local_pose) {
  const std::size_t count = std::min(model.bones.size(), local_pose.size());
  std::vector<glm::dmat4> world(count);
  for (std::size_t bone = 0; bone < count; ++bone) {
    const glm::dmat4 local = TransformMatrixDouble(local_pose[bone]);
    const std::int32_t parent = model.bones[bone].parent_index;
    world[bone] = parent < 0 || static_cast<std::size_t>(parent) >= bone
                      ? local
                      : world[static_cast<std::size_t>(parent)] * local;
  }
  std::vector<glm::mat4> composites(count);
  for (std::size_t bone = 0; bone < count; ++bone) {
    // Le monde s'arrondit d'abord aux flottants, puis se compose avec l'inverse
    // du repos.
    const glm::dmat4 rounded{glm::mat4{world[bone]}};
    const glm::dmat4 inverse{glm::make_mat4(model.bones[bone].inverse_world.data())};
    composites[bone] = glm::mat4(rounded * inverse);
  }
  return composites;
}

std::vector<std::int32_t> MeshBoneMap(const formats::GrannyScene& model,
                                      const formats::GrannyMesh& mesh) {
  std::vector<std::int32_t> map(mesh.bone_names.size(), -1);
  for (std::size_t index = 0; index < mesh.bone_names.size(); ++index) {
    for (std::size_t bone = 0; bone < model.bones.size(); ++bone) {
      if (EqualIgnoringCase(mesh.bone_names[index], model.bones[bone].name)) {
        map[index] = static_cast<std::int32_t>(bone);
        break;
      }
    }
  }
  return map;
}

PosedMesh PoseMesh(const formats::GrannyMesh& mesh, std::span<const std::int32_t> bone_map,
                   std::span<const glm::mat4> composites) {
  const auto composite_of = [&](std::size_t mesh_bone) -> const glm::mat4* {
    if (mesh_bone >= bone_map.size() || bone_map[mesh_bone] < 0 ||
        static_cast<std::size_t>(bone_map[mesh_bone]) >= composites.size()) {
      return nullptr;
    }
    return &composites[static_cast<std::size_t>(bone_map[mesh_bone])];
  };
  PosedMesh posed;
  posed.vertices.reserve(mesh.vertices.size());
  if (mesh.rigid) {
    if (const glm::mat4* matrix = composite_of(0)) {
      posed.mesh_matrix = *matrix;
    }
    for (const formats::GrannyVertex& vertex : mesh.vertices) {
      posed.vertices.push_back(PosedVertex{glm::make_vec3(vertex.position.data()),
                                           glm::make_vec3(vertex.normal.data()),
                                           glm::make_vec2(vertex.uv.data())});
    }
    return posed;
  }
  for (const formats::GrannyVertex& vertex : mesh.vertices) {
    const glm::vec4 rest_position(glm::make_vec3(vertex.position.data()), 1.0F);
    const glm::vec3 rest_normal = glm::make_vec3(vertex.normal.data());
    PosedVertex out;
    out.uv = glm::make_vec2(vertex.uv.data());
    for (std::size_t lane = 0; lane < vertex.bone_indices.size(); ++lane) {
      if (vertex.bone_weights[lane] == 0) {
        continue;
      }
      const glm::mat4* matrix = composite_of(vertex.bone_indices[lane]);
      if (matrix == nullptr) {
        continue;
      }
      const float weight = static_cast<float>(vertex.bone_weights[lane]) / kWeightScale;
      out.position += weight * glm::vec3(*matrix * rest_position);
      out.normal += weight * (glm::mat3(*matrix) * rest_normal);
    }
    posed.vertices.push_back(out);
  }
  return posed;
}

float AnimationLocalSeconds(float clock_seconds, float duration_seconds, bool play_once) {
  if (duration_seconds <= 0.0F) {
    return 0.0F;
  }
  if (play_once) {
    return std::clamp(clock_seconds, 0.0F, duration_seconds);
  }
  const float wrapped = std::fmod(clock_seconds, duration_seconds);
  return wrapped < 0.0F ? wrapped + duration_seconds : wrapped;
}

}  // namespace ro::render
