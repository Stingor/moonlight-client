#include "ro/render/veiled_prims.h"

#include <algorithm>
#include <array>

namespace ro::render {
namespace {

constexpr std::uint32_t kVerticesPerFace = 3;

// Ce que le client sait d'un sommet projeté : sa place à l'écran, dans les
// coordonnées normalisées où l'ordonnée MONTE, et l'inverse de sa profondeur.
struct Projected {
  glm::vec2 screen{};
  float inverse_depth = 0.0F;
};

[[nodiscard]] Projected Project(const glm::mat4& view_projection, const glm::vec3& world) {
  const glm::vec4 clip = view_projection * glm::vec4(world, 1.0F);
  if (clip.w == 0.0F) {
    return Projected{};
  }
  const float inverse = 1.0F / clip.w;
  return Projected{.screen = glm::vec2(clip.x, clip.y) * inverse, .inverse_depth = inverse};
}

// UNE FACE ENTIÈREMENT HORS DE L'ÉCRAN, ou dont un sommet passe derrière l'œil.
// Le client ne l'écarte que pour une pose à cheval sur un bord de la vue.
[[nodiscard]] bool OffScreen(const std::array<Projected, 3>& corners) {
  const auto x = [&corners](std::size_t corner) { return corners[corner].screen.x; };
  const auto y = [&corners](std::size_t corner) { return corners[corner].screen.y; };
  const float left = std::min({x(0), x(1), x(2)});
  const float right = std::max({x(0), x(1), x(2)});
  const float bottom = std::min({y(0), y(1), y(2)});
  const float top = std::max({y(0), y(1), y(2)});
  constexpr float kEdge = 1.0F;
  return left >= kEdge || right < -kEdge || top <= -kEdge || bottom > kEdge ||
         corners[0].inverse_depth < 0.0F || corners[1].inverse_depth < 0.0F ||
         corners[2].inverse_depth < 0.0F;
}

// CE QUI DÉCIDE DU CÔTÉ QU'UNE FACE SIMPLE MONTRE.
struct FaceRule {
  // Un nombre impair d'échelles négatives au .rsw.
  bool mirrored = false;
  bool second_family = false;
};

// LE CÔTÉ QUE LA FACE MONTRE, lu au sens où ses sommets tournent vus de l'œil.
//
// D'une pose ordinaire de la première famille, le client dessine les faces qui
// tournent dans le sens contraire des aiguilles d'une montre ; de la seconde,
// les autres. Une pose en miroir retourne la règle, et c'est alors elle qui
// garde la face dégénérée.
[[nodiscard]] bool ShowsFront(const std::array<Projected, 3>& corners, const FaceRule& rule) {
  const glm::vec2 first = corners[1].screen - corners[0].screen;
  const glm::vec2 second = corners[2].screen - corners[0].screen;
  const float turn = first.x * second.y - first.y * second.x;
  if (rule.second_family) {
    return rule.mirrored ? turn >= 0.0F : turn < 0.0F;
  }
  return rule.mirrored ? turn <= 0.0F : turn > 0.0F;
}

// Les sommets, la parité des faces et les suites d'une pose : celles du
// maillage posé pour une pose figée, celles d'une part du gréement sinon.
struct PrimSource {
  std::span<const ModelVertex> vertices;
  std::span<const std::uint8_t> two_sided;
  std::span<const ModelPrim> prims;
  glm::mat4 transform{1.0F};
  // Le rang des grandeurs de texture de la part : vide pour un maillage posé.
  std::optional<std::size_t> lot_base;
};

void EmitPrims(const PrimSource& source, const VeiledPose& pose, const FaceRule& rule,
               const glm::mat3& normal_transform, const glm::mat4& view_projection,
               std::vector<ModelVertex>& vertices, std::vector<VeiledPrim>& prims) {
  for (const ModelPrim& prim : source.prims) {
    const auto first = static_cast<std::uint32_t>(vertices.size());
    float key = 0.0F;
    for (std::uint32_t offset = 0; offset + kVerticesPerFace <= prim.vertex_count;
         offset += kVerticesPerFace) {
      const std::uint32_t start = prim.first_vertex + offset;
      const std::uint32_t face = start / kVerticesPerFace;
      if (start + kVerticesPerFace > source.vertices.size() || face >= source.two_sided.size()) {
        continue;
      }
      std::array<glm::vec3, 3> world{};
      std::array<Projected, 3> corners{};
      for (std::uint32_t corner = 0; corner < kVerticesPerFace; ++corner) {
        const std::array<float, 3>& position = source.vertices[start + corner].position;
        world[corner] =
            glm::vec3(source.transform * glm::vec4(position[0], position[1], position[2], 1.0F));
        corners[corner] = Project(view_projection, world[corner]);
      }
      if (pose.across && OffScreen(corners)) {
        continue;
      }
      if (source.two_sided[face] == 0 && !ShowsFront(corners, rule)) {
        continue;
      }
      if (vertices.size() == first) {
        key = corners[0].inverse_depth;
      }
      for (std::uint32_t corner = 0; corner < kVerticesPerFace; ++corner) {
        const ModelVertex& local = source.vertices[start + corner];
        const glm::vec3 normal =
            normal_transform * glm::vec3(local.normal[0], local.normal[1], local.normal[2]);
        vertices.push_back(
            ModelVertex{.position = {world[corner].x, world[corner].y, world[corner].z},
                        .texture_uv = local.texture_uv,
                        .normal = {normal.x, normal.y, normal.z}});
      }
    }
    const auto count = static_cast<std::uint32_t>(vertices.size()) - first;
    if (count == 0) {
      continue;
    }
    prims.push_back(
        VeiledPrim{.model_index = pose.model_index,
                   .texture_index = prim.texture_index,
                   .motion = source.lot_base.has_value()
                                 ? std::optional<std::size_t>(*source.lot_base + prim.lot)
                                 : std::nullopt,
                   .group = pose.group,
                   .opacity = pose.opacity,
                   .sort_key = key,
                   .first_vertex = first,
                   .vertex_count = count});
  }
}

}  // namespace

void EmitVeiledPose(const VeiledPose& pose, const ModelInstances& model,
                    std::span<const glm::mat4> node_poses, const glm::mat4& view_projection,
                    std::vector<ModelVertex>& vertices, std::vector<VeiledPrim>& prims) {
  const bool animated = pose.group.has_value();
  const std::vector<glm::mat4>* transforms =
      animated ? (*pose.group < model.animated.size() ? &model.animated[*pose.group].transforms
                                                      : nullptr)
               : &model.transforms;
  if (transforms == nullptr || pose.index >= transforms->size()) {
    return;
  }
  const glm::mat4& instance = (*transforms)[pose.index];
  const FaceRule rule{.mirrored = InstanceMirrored(instance, model.rsm_version),
                      .second_family = IsSecondFamily(model.rsm_version)};
  const glm::mat3 normal_transform(instance);
  if (!animated) {
    EmitPrims(PrimSource{.vertices = model.mesh.vertices,
                         .two_sided = model.mesh.two_sided,
                         .prims = model.mesh.prims,
                         .transform = instance},
              pose, rule, normal_transform, view_projection, vertices, prims);
    return;
  }
  std::size_t lot_base = 0;
  for (const ModelRigPart& part : model.rig.parts) {
    const glm::mat4 node = part.node < node_poses.size() ? node_poses[part.node] : glm::mat4(1.0F);
    EmitPrims(PrimSource{.vertices = model.rig.vertices,
                         .two_sided = model.rig.two_sided,
                         .prims = part.prims,
                         .transform = instance * node,
                         .lot_base = lot_base},
              pose, rule, normal_transform, view_projection, vertices, prims);
    lot_base += part.surfaces.size();
  }
}

void SortVeiledPrims(std::vector<VeiledPrim>& prims) {
  std::ranges::stable_sort(prims, {}, &VeiledPrim::sort_key);
}

}  // namespace ro::render
