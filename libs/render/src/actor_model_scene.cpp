#include "ro/render/actor_model_scene.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

#include "ro/render/map_effect_math.h"

namespace ro::render {
namespace {

// Les constantes d'angle du client, en simple précision telles qu'il les écrit.
constexpr float kClientPi = 3.141592F;
constexpr float kClientHalfPi = 1.570796F;
constexpr float kHalfTurnDegrees = 180.0F;

// Le nom d'un fichier exporté debout.
constexpr std::string_view kQuarterTurnMarker = "90";

// Qui se tient debout est posé deux unités en deçà de sa case, sur les deux
// axes du sol.
constexpr float kUprightOffset = 2.0F;

// La lumière : la part variable, puis le plancher, pour chacune des deux tenues.
constexpr float kUprightLightRange = 128.0F;
constexpr float kUprightLightFloor = 255.0F - kUprightLightRange;
constexpr float kGroundedLightRange = 100.0F;
constexpr float kGroundedLightFloor = 155.0F;
constexpr float kMaxLuminance = 255.0F;

// Le haut de la caméra du client, dans son repère.
constexpr glm::vec3 kClientCameraUp{0.0F, 1.0F, 0.0F};

// Les classes dont la tenue ou la boîte de clic diffère.
constexpr std::uint32_t kGuildFlagJob = 722;
constexpr std::uint32_t kZombieDragonJob = 1181;
constexpr std::uint32_t kHugelingJob = 1284;

// La vue du client : l'œil, puis ses trois axes dans son monde.
struct ClientView {
  glm::vec3 eye{0.0F};
  glm::vec3 right{1.0F, 0.0F, 0.0F};
  glm::vec3 down{0.0F, 1.0F, 0.0F};
  glm::vec3 forward{0.0F, 0.0F, 1.0F};

  // Un point du monde du client, dans l'espace de la vue.
  [[nodiscard]] glm::vec3 ToView(const glm::vec3& point) const {
    const glm::vec3 relative = point - eye;
    return {glm::dot(right, relative), glm::dot(down, relative), glm::dot(forward, relative)};
  }
  // Un point de l'espace de la vue, dans le monde du client.
  [[nodiscard]] glm::vec3 FromView(const glm::vec3& point) const {
    return eye + point.x * right + point.y * down + point.z * forward;
  }
};

// La vue que le client bâtit : de l'œil vers le point visé, le haut étant
// l'ordonnée de son monde.
ClientView ClientViewOf(const Camera& camera, const MapEffectFrame& map) {
  ClientView view;
  view.eye = map.ToNative(camera.Position());
  const glm::vec3 target = map.ToNative(camera.look_at);
  view.forward = glm::normalize(target - view.eye);
  view.right = glm::normalize(glm::cross(kClientCameraUp, view.forward));
  view.down = glm::cross(view.forward, view.right);
  return view;
}

std::uint8_t Saturate(float value) {
  return static_cast<std::uint8_t>(std::clamp(value, 0.0F, kMaxLuminance));
}

}  // namespace

ActorModelResource MakeActorModelResource(formats::GrannyScene scene, std::string_view file_name) {
  ActorModelResource resource;
  resource.bone_maps.reserve(scene.meshes.size());
  for (const formats::GrannyMesh& mesh : scene.meshes) {
    resource.bone_maps.push_back(MeshBoneMap(scene, mesh));
  }
  resource.scene = std::move(scene);
  resource.quarter_turn_upright = file_name.find(kQuarterTurnMarker) != std::string_view::npos;
  return resource;
}

float ActorModelPickBoxSide(std::uint32_t job) {
  return job == kZombieDragonJob || job == kHugelingJob ? kLargeActorModelPickBoxSide
                                                        : kActorModelPickBoxSide;
}

ActorModelStance ActorModelStanceOf(std::uint32_t job) {
  return job == kGuildFlagJob ? ActorModelStance::kGrounded : ActorModelStance::kUpright;
}

glm::vec3 ClientFromWorld(const glm::vec3& world) { return {world.x, -world.y, -world.z}; }

glm::vec3 WorldFromClient(const glm::vec3& client) { return {client.x, -client.y, -client.z}; }

glm::mat4 ActorModelToClientWorld(const ActorModelResource& resource,
                                  const ActorModelPlacement& placement) {
  const float upright = resource.quarter_turn_upright ? kClientHalfPi : kClientPi;
  const float yaw = (placement.yaw_degrees + kHalfTurnDegrees) / kHalfTurnDegrees * kClientPi;
  glm::vec3 position = ClientFromWorld(placement.ground_position);
  if (placement.stance == ActorModelStance::kUpright) {
    position.x -= kUprightOffset;
    position.z -= kUprightOffset;
  }
  glm::mat4 matrix = glm::translate(glm::mat4(1.0F), position);
  matrix = glm::rotate(matrix, yaw, glm::vec3{0.0F, 1.0F, 0.0F});
  matrix = glm::rotate(matrix, upright, glm::vec3{1.0F, 0.0F, 0.0F});
  return matrix;
}

glm::vec3 UprightLightDirection(const Camera& camera, const MapEffectFrame& map) {
  return glm::normalize(ClientViewOf(camera, map).FromView(kActorModelViewLight));
}

glm::vec3 GroundedLightDirection(const Camera& camera, const MapEffectFrame& map) {
  const ClientView view = ClientViewOf(camera, map);
  return glm::normalize(view.ToView(view.forward));
}

glm::vec3 RemnantLightDirection(const Camera& camera, const MapEffectFrame& map) {
  return glm::normalize(ClientViewOf(camera, map).FromView(kActorModelRemnantViewLight));
}

std::uint8_t ActorModelLuminance(ActorModelStance stance, const glm::vec3& normal,
                                 const glm::vec3& light) {
  const float lit = std::clamp(glm::dot(light, normal), 0.0F, 1.0F);
  if (stance == ActorModelStance::kUpright) {
    return Saturate(lit * kUprightLightRange + kUprightLightFloor);
  }
  // Le drapeau et le reste partagent la même rampe, plus claire.
  return Saturate(lit * kGroundedLightRange + kGroundedLightFloor);
}

DrawnActorModel ComposeActorModel(const ActorModelResource& resource,
                                  const ActorModelAnimation* animation, float local_seconds,
                                  const ActorModelPlacement& placement, const Camera& camera,
                                  const MapEffectFrame& map, const formats::Image* emblem) {
  DrawnActorModel drawn;
  const formats::GrannyScene& scene = resource.scene;
  const std::vector<formats::GrannyTransform> local =
      SampleLocalPose(scene, animation, local_seconds);
  const std::vector<glm::mat4> composites = CompositeMatrices(scene, local);
  const glm::mat4 to_client = ActorModelToClientWorld(resource, placement);
  const glm::vec3 light =
      placement.stance == ActorModelStance::kGrounded  ? GroundedLightDirection(camera, map)
      : placement.stance == ActorModelStance::kRemnant ? RemnantLightDirection(camera, map)
                                                       : UprightLightDirection(camera, map);
  for (std::size_t index = 0; index < scene.meshes.size(); ++index) {
    const formats::GrannyMesh& mesh = scene.meshes[index];
    ActorModelTexture texture;
    texture.owner = &resource;
    texture.key = index;
    bool textured = false;
    if (placement.stance == ActorModelStance::kGrounded && index == kGuildFlagEmblemMesh) {
      textured = emblem != nullptr && !emblem->empty();
      texture.owner = emblem;
      texture.emblem = emblem;
    } else if (mesh.texture >= 0 &&
               static_cast<std::size_t>(mesh.texture) < scene.textures.size() &&
               !scene.textures[static_cast<std::size_t>(mesh.texture)].rgba.empty()) {
      textured = true;
      texture.key = static_cast<std::uint64_t>(mesh.texture);
      texture.image = &scene.textures[static_cast<std::size_t>(mesh.texture)];
    }
    // ⚠ DEBOUT, UN MAILLAGE SANS TEXTURE NE SE POSE MÊME PAS : il ne fait donc
    // naître aucun effet. Sur le drapeau, il se pose, puis ne se peint pas.
    if (!textured && placement.stance == ActorModelStance::kUpright) {
      continue;
    }
    const PosedMesh posed = PoseMesh(mesh, resource.bone_maps[index], composites);
    if (placement.stance != ActorModelStance::kRemnant && placement.job == kZombieDragonJob &&
        index == kZombieDragonBreathMesh && posed.vertices.size() > kZombieDragonBreathVertex) {
      const glm::vec3 offset = RotatedSpawnOffset(
          posed.vertices[kZombieDragonBreathVertex].position, -placement.yaw_degrees);
      drawn.mesh_events.push_back(ActorModelMeshEvent{
          kZombieDragonBreathEffect, placement.ground_position + WorldFromClient(offset)});
    }
    if (!textured) {
      continue;
    }
    const glm::mat4 to_world = to_client * posed.mesh_matrix;
    DrawnActorModelMesh out;
    out.texture = texture;
    out.vertices.reserve(mesh.indices.size());
    for (const std::uint16_t vertex_index : mesh.indices) {
      if (vertex_index >= posed.vertices.size()) {
        continue;
      }
      const PosedVertex& vertex = posed.vertices[vertex_index];
      const std::uint8_t luminance = ActorModelLuminance(placement.stance, vertex.normal, light);
      ActorModelVertex drawn_vertex;
      drawn_vertex.position =
          WorldFromClient(glm::vec3(to_world * glm::vec4(vertex.position, 1.0F)));
      drawn_vertex.uv = vertex.uv;
      drawn_vertex.color = {luminance, luminance, luminance, placement.alpha};
      out.vertices.push_back(drawn_vertex);
    }
    drawn.meshes.push_back(std::move(out));
  }
  return drawn;
}

glm::vec3 RotatedSpawnOffset(const glm::vec3& client_offset, float angle_degrees) {
  const float radians = NativeTableRadians(static_cast<std::int32_t>(angle_degrees));
  const float sine = std::sin(radians);
  const float cosine = std::cos(radians);
  return {client_offset.x * cosine - client_offset.z * sine, client_offset.y,
          client_offset.z * cosine + client_offset.x * sine};
}

glm::vec3 ActorModelPickCenter(const ActorModelPlacement& placement) {
  glm::vec3 client = ClientFromWorld(placement.ground_position);
  if (placement.stance == ActorModelStance::kUpright) {
    client.x -= kUprightOffset;
    client.z -= kUprightOffset;
  }
  return WorldFromClient(client);
}

}  // namespace ro::render
