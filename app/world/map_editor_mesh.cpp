#include "world/map_editor_mesh.h"

#include <string_view>
#include <utility>

#include "ro/render/actor_pick.h"
#include "ro/render/model_mesh.h"
#include "ro/render/model_pick.h"
#include "ro/vfs/encoding.h"

namespace moonclient {
namespace {

std::string CitedName(std::string_view raw) { return ro::vfs::Cp949ToUtf8Lossy(raw); }

std::string RawName(std::string_view utf8) {
  auto raw = ro::vfs::Utf8ToCp949(utf8);
  return raw ? std::move(*raw) : std::string(utf8);
}

constexpr ro::mapedit::FileNameCodec kNames{.to_utf8 = &CitedName, .to_raw = &RawName};

// La retouche de maillage que dessine la pose `rank` : la sienne, ou celle qui
// vaut pour toutes les poses de son fichier.
const ro::mapedit::MeshEdit* DrawnMeshEdit(const ro::mapedit::MapEdit& document, std::size_t rank,
                                           const std::string& file) {
  if (const ro::mapedit::MeshEdit* own = document.MeshAt(rank); own != nullptr && !own->all_poses) {
    return own;
  }
  for (const ro::mapedit::MeshEdit& mesh : document.meshes()) {
    if (mesh.all_poses && mesh.model_file == file) {
      return &mesh;
    }
  }
  return nullptr;
}

}  // namespace

std::shared_ptr<const ro::formats::RsmModel> RsmCache::Get(ro::vfs::Vfs& vfs,
                                                           const std::string& raw_file) {
  if (const auto found = by_file_.find(raw_file); found != by_file_.end()) {
    return found->second;
  }
  std::shared_ptr<const ro::formats::RsmModel> model;
  if (auto bytes = vfs.ReadRaw("data\\model\\" + raw_file)) {
    if (auto parsed = ro::formats::RsmModel::Parse(*bytes)) {
      model = std::make_shared<const ro::formats::RsmModel>(std::move(*parsed));
    }
  }
  by_file_.emplace(raw_file, model);
  return model;
}

std::optional<MapEditorSession::VertexContext> BuildVertexContext(
    const ro::render::MapScene& scene, std::size_t rank, const ro::mapedit::MapEdit& document,
    RsmCache& cache, ro::vfs::Vfs& vfs) {
  if (rank >= scene.rsw.models().size()) {
    return std::nullopt;
  }
  const std::optional<ro::render::ScenePose> pose = ro::render::FindScenePose(scene, rank);
  if (!pose.has_value()) {
    return std::nullopt;
  }
  const ro::formats::RswModel& placement = scene.rsw.models()[rank];
  const std::shared_ptr<const ro::formats::RsmModel> original =
      cache.Get(vfs, placement.model_name);
  if (original == nullptr) {
    return std::nullopt;
  }
  MapEditorSession::VertexContext context;
  context.model_index = rank;
  context.model_file = CitedName(placement.model_name);
  context.pose_origin =
      rank < scene.unfixed_models.size() ? scene.unfixed_models[rank].position : placement.position;
  context.to_world = pose->transform;
  context.map_frame = ro::render::MapEffectFrame{scene.gnd.width(), scene.gnd.height()};
  // LES SOMMETS LÀ OÙ LE MAILLAGE DESSINÉ LES MET : l'édition s'applique à une
  // copie du fichier, posée comme le décor la pose.
  ro::formats::RsmModel edited = *original;
  if (const ro::mapedit::MeshEdit* mesh = DrawnMeshEdit(document, rank, context.model_file)) {
    static_cast<void>(ro::mapedit::ApplyMeshEdit(*mesh, edited, kNames));
  }
  for (const ro::render::PosedNode& node : ro::render::PosedNodes(edited)) {
    MapEditorSession::VertexNode described;
    described.name = CitedName(original->meshes()[node.mesh_index].name);
    described.to_mesh = node.to_mesh;
    described.origins = original->meshes()[node.mesh_index].vertices;
    for (const glm::vec3& vertex : node.vertices) {
      described.world.emplace_back(context.to_world * glm::vec4(vertex, 1.0F));
    }
    context.nodes.push_back(std::move(described));
  }
  return context;
}

bool VertexContextCache::Refresh(const ro::render::MapScene& scene, std::size_t rank,
                                 const ro::mapedit::MapEdit& document, ro::vfs::Vfs& vfs) {
  std::optional<Key> key;
  if (const std::optional<ro::render::ScenePose> pose = ro::render::FindScenePose(scene, rank);
      pose.has_value() && rank < scene.rsw.models().size()) {
    const std::string& raw_file = scene.rsw.models()[rank].model_name;
    const ro::mapedit::MeshEdit* mesh = DrawnMeshEdit(document, rank, CitedName(raw_file));
    key = Key{.rank = rank,
              .raw_file = raw_file,
              .to_world = pose->transform,
              .mesh = mesh != nullptr ? std::optional(*mesh) : std::nullopt};
  }
  if (key == key_ && (key_.has_value() || !context_.has_value())) {
    return false;
  }
  key_ = std::move(key);
  context_ =
      key_.has_value() ? BuildVertexContext(scene, rank, document, files_, vfs) : std::nullopt;
  return true;
}

std::vector<MapEditorSession::VertexDot> VertexDots(const MapEditorSession::VertexContext& context,
                                                    const ro::render::Camera& camera,
                                                    float aspect_ratio, float viewport_width,
                                                    float viewport_height) {
  std::vector<MapEditorSession::VertexDot> dots;
  for (std::size_t node = 0; node < context.nodes.size(); ++node) {
    const std::vector<glm::vec3>& world = context.nodes[node].world;
    for (std::size_t vertex = 0; vertex < world.size(); ++vertex) {
      if (const std::optional<glm::vec2> at = ro::render::ProjectToScreen(
              camera, aspect_ratio, world[vertex], viewport_width, viewport_height)) {
        dots.push_back(MapEditorSession::VertexDot{.at = *at, .node = node, .vertex = vertex});
      }
    }
  }
  return dots;
}

}  // namespace moonclient
