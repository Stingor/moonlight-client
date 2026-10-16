#include "ro/mapedit/map_edit.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <tuple>
#include <utility>

namespace ro::mapedit {
namespace {

constexpr std::string_view kMapEditFolder = "data\\map_edits\\";
// Le nom que porte un modèle ajouté dans le RSW : celui du fichier ne sert
// qu'aux éditeurs de cartes, et dit d'où il vient.
constexpr std::string_view kAddedModelName = "map_edit";
constexpr std::string_view kMapEditExtension = ".yml";

[[nodiscard]] bool NearOrigin(const std::array<float, 3>& expected,
                              const std::array<float, 3>& found) {
  for (std::size_t axis = 0; axis < expected.size(); ++axis) {
    if (std::abs(expected[axis] - found[axis]) > kOriginTolerance) {
      return false;
    }
  }
  return true;
}

}  // namespace

std::string MapEditPath(std::string_view map_name) {
  return std::string(kMapEditFolder) + std::string(map_name) + std::string(kMapEditExtension);
}

const ModelEdit* MapEdit::ModelAt(std::size_t model_index) const {
  const auto found = std::ranges::find(models_, model_index, &ModelEdit::model_index);
  return found == models_.end() ? nullptr : &*found;
}

void MapEdit::SetModel(ModelEdit edit) {
  const auto place =
      std::ranges::lower_bound(models_, edit.model_index, {}, &ModelEdit::model_index);
  if (place != models_.end() && place->model_index == edit.model_index) {
    *place = std::move(edit);
  } else {
    models_.insert(place, std::move(edit));
  }
}

void MapEdit::RemoveModel(std::size_t model_index) {
  std::erase_if(models_,
                [model_index](const ModelEdit& edit) { return edit.model_index == model_index; });
}

const VertexMove* MeshEdit::VertexAt(std::string_view node, std::size_t vertex) const {
  const auto found = std::ranges::find_if(
      vertices, [&](const VertexMove& move) { return move.node == node && move.vertex == vertex; });
  return found == vertices.end() ? nullptr : &*found;
}

void MeshEdit::SetVertex(VertexMove move) {
  const auto before = [](const VertexMove& left, const VertexMove& right) {
    return std::tie(left.node, left.vertex) < std::tie(right.node, right.vertex);
  };
  const auto place = std::ranges::lower_bound(vertices, move, before);
  if (place != vertices.end() && place->node == move.node && place->vertex == move.vertex) {
    *place = std::move(move);
  } else {
    vertices.insert(place, std::move(move));
  }
}

void MeshEdit::RemoveVertex(std::string_view node, std::size_t vertex) {
  std::erase_if(vertices,
                [&](const VertexMove& move) { return move.node == node && move.vertex == vertex; });
}

const MeshEdit* MapEdit::MeshAt(std::size_t model_index) const {
  const auto found = std::ranges::find(meshes_, model_index, &MeshEdit::model_index);
  return found == meshes_.end() ? nullptr : &*found;
}

void MapEdit::SetMesh(MeshEdit edit) {
  const auto place =
      std::ranges::lower_bound(meshes_, edit.model_index, {}, &MeshEdit::model_index);
  if (place != meshes_.end() && place->model_index == edit.model_index) {
    *place = std::move(edit);
  } else {
    meshes_.insert(place, std::move(edit));
  }
}

void MapEdit::RemoveMesh(std::size_t model_index) {
  std::erase_if(meshes_,
                [model_index](const MeshEdit& edit) { return edit.model_index == model_index; });
}

std::string MeshTargetRefusal(const MeshEdit& edit, const formats::RswScene& rsw,
                              const FileNameCodec& names) {
  if (edit.model_index >= rsw.models().size()) {
    return std::format("maillage {} : le RSW n'a que {} modèles", edit.model_index,
                       rsw.models().size());
  }
  const formats::RswModel& model = rsw.models()[edit.model_index];
  const std::string file = names.to_utf8(model.model_name);
  if (file != edit.model_file) {
    return std::format("maillage {} : « {} » attendu, « {} » trouvé", edit.model_index,
                       edit.model_file, file);
  }
  if (!NearOrigin(edit.origin, model.position)) {
    return std::format("maillage {} : attendu en {} {} {}, trouvé en {} {} {}", edit.model_index,
                       edit.origin[0], edit.origin[1], edit.origin[2], model.position[0],
                       model.position[1], model.position[2]);
  }
  return {};
}

MapEditReport ApplyMeshEdit(const MeshEdit& edit, formats::RsmModel& model,
                            const FileNameCodec& names) {
  MapEditReport report;
  for (const VertexMove& move : edit.vertices) {
    const std::string raw_node = names.to_raw(move.node);
    const auto& meshes = model.meshes();
    const auto node = std::ranges::find(meshes, raw_node, &formats::RsmMesh::name);
    if (node == meshes.end()) {
      report.refused.push_back(
          std::format("maillage {} : pas de nœud « {} »", edit.model_index, move.node));
      continue;
    }
    if (move.vertex >= node->vertices.size()) {
      report.refused.push_back(std::format("maillage {} : le nœud « {} » n'a que {} sommets",
                                           edit.model_index, move.node, node->vertices.size()));
      continue;
    }
    if (!NearOrigin(move.origin, node->vertices[move.vertex])) {
      report.refused.push_back(std::format("maillage {} : le sommet {} du nœud « {} » a bougé",
                                           edit.model_index, move.vertex, move.node));
      continue;
    }
    model.MoveVertex(static_cast<std::size_t>(node - meshes.begin()), move.vertex, move.position);
    ++report.applied;
  }
  return report;
}

void MapEdit::SetAdded(std::size_t slot, AddedModel model) {
  if (slot < added_.size()) {
    added_[slot] = std::move(model);
  }
}

void MapEdit::RemoveAdded(std::size_t slot) {
  if (slot < added_.size()) {
    added_.erase(added_.begin() + static_cast<std::ptrdiff_t>(slot));
  }
}

MapEditReport ApplyMapEdit(const MapEdit& edit, formats::RswScene& rsw,
                           const FileNameCodec& names) {
  MapEditReport report;
  for (const ModelEdit& model_edit : edit.models()) {
    if (model_edit.model_index >= rsw.models().size()) {
      report.refused.push_back(std::format("modèle {} : le RSW n'en a que {}",
                                           model_edit.model_index, rsw.models().size()));
      continue;
    }
    const formats::RswModel& model = rsw.models()[model_edit.model_index];
    const std::string file = names.to_utf8(model.model_name);
    if (file != model_edit.model_file) {
      report.refused.push_back(std::format("modèle {} : « {} » attendu, « {} » trouvé",
                                           model_edit.model_index, model_edit.model_file, file));
      continue;
    }
    if (!NearOrigin(model_edit.origin, model.position)) {
      report.refused.push_back(
          std::format("modèle {} : attendu en {} {} {}, trouvé en {} {} {}", model_edit.model_index,
                      model_edit.origin[0], model_edit.origin[1], model_edit.origin[2],
                      model.position[0], model.position[1], model.position[2]));
      continue;
    }
    if (model_edit.hidden) {
      report.hidden.push_back(model_edit.model_index);
    } else {
      rsw.PlaceModel(model_edit.model_index, model_edit.position.value_or(model.position),
                     model_edit.rotation.value_or(model.rotation),
                     model_edit.scale.value_or(model.scale));
    }
    ++report.applied;
  }
  // LES AJOUTS, au bout de la liste, dans l'ordre du document.
  for (const AddedModel& added : edit.added()) {
    formats::RswModel model;
    model.name = std::string(kAddedModelName);
    model.model_name = names.to_raw(added.model_file);
    model.position = added.position;
    model.rotation = added.rotation;
    model.scale = added.scale;
    const std::size_t rank = rsw.AddModel(std::move(model));
    if (added.hidden) {
      report.hidden.push_back(rank);
    }
    ++report.applied;
  }
  std::ranges::sort(report.hidden);
  return report;
}

}  // namespace ro::mapedit
