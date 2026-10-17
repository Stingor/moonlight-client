// L'OUTIL SOMMETS DE L'ÉDITEUR DE CARTE : voir docs/adr/0118, phase 2.
//
// Un sommet se désigne d'un clic sur son point, se déplace à la flèche comme
// une pose, et s'écrit dans le repère de son nœud : le monde redescend dans le
// repère du maillage par la matrice de la pose, puis dans celui du nœud par la
// sienne.

#include <array>
#include <cmath>
#include <cstdio>
#include <utility>

#include "ro/core/i18n.h"

#include "world/map_editor_reasons.h"
#include "world/map_editor_session.h"

namespace moonclient {
namespace {

// La place d'un nom de geste, zéro final compris.
constexpr std::size_t kLabelCapacity = 192;
// En deçà, un sommet est tenu pour revenu à sa position d'origine, en unités
// du nœud : `kOriginTolerance` du document, pour qu'il le reconnaisse.
constexpr float kBackHome = ro::mapedit::kOriginTolerance;

}  // namespace

std::vector<ro::ui::MapEditorLine> MapEditorSession::VertexTrace() const {
  std::vector<ro::ui::MapEditorLine> dots;
  dots.reserve(vertex_dots_.size());
  for (const VertexDot& dot : vertex_dots_) {
    const bool chosen = vertex_ == std::pair(dot.node, dot.vertex);
    dots.push_back(ro::ui::MapEditorLine{.from = dot.at,
                                         .to = dot.at,
                                         .kind = chosen ? ro::ui::MapEditorLineKind::kVertexSelected
                                                        : ro::ui::MapEditorLineKind::kVertex});
  }
  return dots;
}

bool MapEditorSession::GrabVertex(glm::vec2 pointer) {
  // LA FLÈCHE D'ABORD, quand un sommet est désigné : elle le déplace.
  if (vertex_.has_value() && vertex_context_.has_value() &&
      vertex_->first < vertex_context_->nodes.size() &&
      vertex_->second < vertex_context_->nodes[vertex_->first].world.size()) {
    if (const std::optional<std::size_t> grabbed = GrabbedHandle(handles_, pointer)) {
      const glm::vec3 world = vertex_context_->nodes[vertex_->first].world[vertex_->second];
      const glm::vec3 native = vertex_context_->map_frame.ToNative(world);
      const ro::ui::map_editor::Place start{.position = {native.x, native.y, native.z},
                                            .released = false};
      drag_ = Drag{.handle = *grabbed,
                   .grabbed = handles_[*grabbed],
                   .frame = gizmo_frame_,
                   .from = pointer,
                   .start = start,
                   .last = start,
                   .vertex = vertex_};
      return true;
    }
  }
  // SINON LE POINT LE PLUS PROCHE, à portée de saisie.
  float nearest = kGizmoGrabPixels;
  std::optional<std::pair<std::size_t, std::size_t>> picked;
  for (const VertexDot& dot : vertex_dots_) {
    const float distance = glm::length(dot.at - pointer);
    if (distance <= nearest) {
      nearest = distance;
      picked = std::pair(dot.node, dot.vertex);
    }
  }
  if (!picked.has_value()) {
    return false;
  }
  vertex_ = picked;
  return true;
}

ro::mapedit::MeshEdit MapEditorSession::MeshFor(const ro::mapedit::MapEdit& document) const {
  const std::size_t rank = *selection_.model_index;
  if (const ro::mapedit::MeshEdit* existing = document.MeshAt(rank)) {
    return *existing;
  }
  ro::mapedit::MeshEdit fresh;
  fresh.model_index = rank;
  fresh.model_file = vertex_context_->model_file;
  fresh.origin = vertex_context_->pose_origin;
  fresh.all_poses = all_poses_;
  if (const auto pending = pending_mesh_why_.find(rank); pending != pending_mesh_why_.end()) {
    fresh.why = pending->second;
  }
  return fresh;
}

MapEditorSession::Outcome MapEditorSession::MoveVertex(const ro::ui::map_editor::MoveVertex& move,
                                                       MapEditStore& store) {
  if (selection_.empty() || !vertex_context_.has_value() ||
      vertex_context_->model_index != *selection_.model_index ||
      move.node >= vertex_context_->nodes.size()) {
    return {};
  }
  const VertexNode& node = vertex_context_->nodes[move.node];
  if (move.vertex >= node.origins.size()) {
    return {};
  }
  // DU MONDE AU NŒUD : par la pose, puis par le nœud.
  const glm::vec3 world =
      vertex_context_->map_frame.ToWorld(glm::vec3(move.native[0], move.native[1], move.native[2]));
  const glm::vec3 local =
      glm::vec3(glm::inverse(node.to_mesh) * glm::inverse(vertex_context_->to_world) *
                glm::vec4(world, 1.0F));
  const std::array<float, 3>& origin = node.origins[move.vertex];
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  if (!gesture_before_.has_value()) {
    gesture_before_ = before;
  }
  ro::mapedit::MeshEdit mesh = MeshFor(before);
  const bool home = std::abs(local.x - origin[0]) < kBackHome &&
                    std::abs(local.y - origin[1]) < kBackHome &&
                    std::abs(local.z - origin[2]) < kBackHome;
  if (home) {
    mesh.RemoveVertex(node.name, move.vertex);
  } else {
    mesh.SetVertex(ro::mapedit::VertexMove{.node = node.name,
                                           .vertex = move.vertex,
                                           .origin = origin,
                                           .position = {local.x, local.y, local.z}});
  }
  ro::mapedit::MapEdit after = before;
  if (mesh.vertices.empty()) {
    after.RemoveMesh(mesh.model_index);
  } else {
    after.SetMesh(std::move(mesh));
  }
  Outcome outcome;
  if (!(after == before)) {
    store.Set(map_name_, WithReasons(after));
    outcome.replace_models = true;
  }
  if (move.released) {
    std::array<char, kLabelCapacity> label{};
    std::snprintf(label.data(), label.size(), ro::core::Tr("Déplacer un sommet du modèle %zu"),
                  *selection_.model_index);
    Record(label.data(), *gesture_before_, store.Document(map_name_));
    gesture_before_.reset();
  }
  return outcome;
}

MapEditorSession::Outcome MapEditorSession::RevertVertex(MapEditStore& store) {
  if (selection_.empty() || !vertex_.has_value() || !vertex_context_.has_value() ||
      vertex_->first >= vertex_context_->nodes.size()) {
    return {};
  }
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  const ro::mapedit::MeshEdit* existing = before.MeshAt(*selection_.model_index);
  const std::string& node = vertex_context_->nodes[vertex_->first].name;
  if (existing == nullptr || existing->VertexAt(node, vertex_->second) == nullptr) {
    return {};
  }
  ro::mapedit::MeshEdit mesh = *existing;
  mesh.RemoveVertex(node, vertex_->second);
  ro::mapedit::MapEdit after = before;
  if (mesh.vertices.empty()) {
    after.RemoveMesh(mesh.model_index);
  } else {
    after.SetMesh(std::move(mesh));
  }
  store.Set(map_name_, WithReasons(after));
  std::array<char, kLabelCapacity> label{};
  std::snprintf(label.data(), label.size(), ro::core::Tr("Rétablir un sommet du modèle %zu"),
                *selection_.model_index);
  Record(label.data(), before, after);
  return {.replace_models = true};
}

MapEditorSession::Outcome MapEditorSession::SetAllPoses(bool all, MapEditStore& store) {
  all_poses_ = all;
  if (selection_.empty()) {
    return {};
  }
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  const ro::mapedit::MeshEdit* existing = before.MeshAt(*selection_.model_index);
  if (existing == nullptr || existing->all_poses == all) {
    return {};
  }
  ro::mapedit::MeshEdit mesh = *existing;
  mesh.all_poses = all;
  ro::mapedit::MapEdit after = before;
  after.SetMesh(std::move(mesh));
  store.Set(map_name_, WithReasons(after));
  std::array<char, kLabelCapacity> label{};
  std::snprintf(label.data(), label.size(), ro::core::Tr("Étendre le maillage du modèle %zu"),
                *selection_.model_index);
  Record(label.data(), before, after);
  return {.replace_models = true};
}

}  // namespace moonclient
