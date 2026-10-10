#include "world/monster_model.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <glm/glm.hpp>

#include "ro/core/ascii.h"
#include "ro/core/log.h"
#include "ro/render/actor_model_motion.h"
#include "ro/render/model_vignette.h"

namespace moonclient {
namespace {

// 🔴 LE CLIENT JOUE CES ANIMATIONS À VITESSE DOUBLE : son horloge avance de
// deux fois le temps écoulé. À vitesse réelle, la fiche « rame » à côté du jeu.
constexpr float kClientSpeed = 2.0F;
// Le pas visé entre deux images, et leur plafond : une animation plus longue
// s'échantillonne plus large plutôt que d'être tronquée — couper l'Emperium
// escamoterait la moitié de son cycle.
constexpr float kTargetStepMs = 40.0F;
constexpr int kMostFrames = 96;
constexpr float kMillisecondsPerSecond = 1000.0F;
// La durée d'une image d'un modèle sans animation : il n'en a qu'une.
constexpr float kStillFrameMs = 1000.0F;
// Deux orientations plus proches se confondent.
constexpr float kSameYaw = 1e-4F;

}  // namespace

bool MonsterModel::Load(std::string_view resource_name) {
  if (ro::core::AsciiLower(resource_name) == ro::core::AsciiLower(name_)) {
    return scene_.has_value();
  }
  name_ = std::string(resource_name);
  scene_.reset();
  animation_.reset();
  bone_maps_.clear();
  frames_.clear();
  rendered_.clear();
  const std::string path = std::string(ro::render::kActorModelFolder) + name_;
  auto bytes = vfs_->Read(path);
  if (!bytes) {
    ro::core::LogWarning("fiche de monstre : modèle {} absent", path);
    return false;
  }
  auto scene = ro::formats::ReadGrannyScene(*bytes);
  if (!scene) {
    ro::core::LogWarning("fiche de monstre : modèle {} illisible — {}", path,
                         ro::core::Describe(scene.error()));
    return false;
  }
  scene_ = std::move(*scene);
  for (const ro::formats::GrannyMesh& mesh : scene_->meshes) {
    bone_maps_.push_back(ro::render::MeshBoneMap(*scene_, mesh));
  }
  // L'ANIMATION EMBARQUÉE, jouée en boucle : les modèles livrés en ont une.
  if (!scene_->animations.empty()) {
    animation_ = ro::render::BindActorModelAnimation(*scene_, scene_->animations.front());
  }
  return true;
}

void MonsterModel::ResetSeries() {
  // La durée telle que le JOUEUR la voit : à la vitesse du client.
  const float duration_s =
      animation_.has_value() && scene_.has_value() && !scene_->animations.empty()
          ? scene_->animations.front().duration_seconds / kClientSpeed
          : 0.0F;
  int count = 1;
  if (duration_s > 0.0F) {
    count = static_cast<int>(std::lround(duration_s * kMillisecondsPerSecond / kTargetStepMs));
    count = std::clamp(count, 1, kMostFrames);
  }
  ms_per_frame_ = duration_s > 0.0F
                      ? duration_s * kMillisecondsPerSecond / static_cast<float>(count)
                      : kStillFrameMs;
  frames_.clear();
  frames_.resize(static_cast<std::size_t>(count));
  rendered_.assign(static_cast<std::size_t>(count), false);
}

const ro::render::IconTexture* MonsterModel::EnsureFrame(std::size_t index) {
  if (index >= frames_.size() || !scene_.has_value()) {
    return nullptr;
  }
  if (rendered_[index]) {
    return frames_[index].usable() ? &frames_[index] : nullptr;
  }
  rendered_[index] = true;
  // L'instant DU FICHIER de cette image : la lecture est accélérée, et
  // l'échantillonnage reparcourt donc toute l'animation.
  const float file_seconds = animation_.has_value() ? scene_->animations.front().duration_seconds *
                                                          static_cast<float>(index) /
                                                          static_cast<float>(frames_.size())
                                                    : 0.0F;
  const std::vector<ro::formats::GrannyTransform> local = ro::render::SampleLocalPose(
      *scene_, animation_.has_value() ? &*animation_ : nullptr, file_seconds);
  const std::vector<glm::mat4> composites = ro::render::CompositeMatrices(*scene_, local);

  std::vector<ro::render::PosedMesh> posed;
  posed.reserve(scene_->meshes.size());
  std::vector<ro::render::ModelVignetteMesh> meshes;
  meshes.reserve(scene_->meshes.size());
  for (std::size_t rank = 0; rank < scene_->meshes.size(); ++rank) {
    const ro::formats::GrannyMesh& mesh = scene_->meshes[rank];
    ro::render::PosedMesh pose = ro::render::PoseMesh(mesh, bone_maps_[rank], composites);
    // ⚠ UN MAILLAGE RIGIDE SUIT SON OS : sa matrice le porte en entier.
    const glm::mat3 turn(pose.mesh_matrix);
    for (ro::render::PosedVertex& vertex : pose.vertices) {
      vertex.position = glm::vec3(pose.mesh_matrix * glm::vec4(vertex.position, 1.0F));
      vertex.normal = turn * vertex.normal;
    }
    posed.push_back(std::move(pose));
    const bool textured =
        mesh.texture >= 0 && static_cast<std::size_t>(mesh.texture) < scene_->textures.size();
    meshes.push_back(ro::render::ModelVignetteMesh{
        .vertices = posed.back().vertices,
        .indices = mesh.indices,
        .texture = textured ? &scene_->textures[static_cast<std::size_t>(mesh.texture)] : nullptr});
  }
  const ro::formats::Image image = ro::render::RasterModelVignette(
      meshes, ro::render::ModelVignetteView{
                  .width = width_, .height = height_, .yaw_radians = yaw_radians_});
  if (image.empty()) {
    return nullptr;
  }
  auto texture = ro::render::GlTexture::Upload(image, ro::render::GlTexture::Filtering::kSprite);
  if (!texture) {
    return nullptr;
  }
  frames_[index] = ro::render::IconTexture{
      .texture = std::move(*texture), .width_px = width_, .height_px = height_};
  return &frames_[index];
}

const ro::render::IconTexture* MonsterModel::FrameAt(const Framing& framing,
                                                     std::uint32_t clock_ms) {
  if (framing.resource_name.empty() || framing.box_width <= 0 || framing.box_height <= 0) {
    return nullptr;
  }
  if (!Load(framing.resource_name)) {
    return nullptr;
  }
  // LA SÉRIE SE REFAIT quand le cadre ou l'orientation changent : ses images
  // ont été rendues pour eux.
  if (frames_.empty() || framing.box_width != width_ || framing.box_height != height_ ||
      std::fabs(framing.yaw_radians - yaw_radians_) >= kSameYaw) {
    width_ = framing.box_width;
    height_ = framing.box_height;
    yaw_radians_ = framing.yaw_radians;
    ResetSeries();
  }
  std::size_t index = 0;
  if (frames_.size() > 1 && ms_per_frame_ > 0.0F) {
    index = static_cast<std::size_t>(static_cast<float>(clock_ms) / ms_per_frame_) % frames_.size();
  }
  return EnsureFrame(index);
}

void MonsterModel::Forget() {
  name_.clear();
  scene_.reset();
  animation_.reset();
  bone_maps_.clear();
  frames_.clear();
  rendered_.clear();
}

}  // namespace moonclient
