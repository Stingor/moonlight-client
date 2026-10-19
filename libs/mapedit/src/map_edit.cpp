#include "ro/mapedit/map_edit.h"

#include <algorithm>
#include <cmath>
#include <expected>
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

// Note l'opacité d'un modèle posé, s'il n'est pas plein.
void NoteOpacity(std::size_t rank, const std::optional<float>& opacity, MapEditReport& report) {
  if (opacity.has_value() && *opacity < kFullOpacity) {
    report.opacities.push_back(ModelOpacity{.model_index = rank, .opacity = *opacity});
  }
}

}  // namespace

std::string MapEditPath(std::string_view map_name) {
  return std::string(kMapEditFolder) + std::string(map_name) + std::string(kMapEditExtension);
}

std::optional<float> DocumentOpacity(float opacity) noexcept {
  if (!(opacity < kFullOpacity)) {
    return std::nullopt;
  }
  return std::max(opacity, kNoOpacity);
}

const ModelEdit* MapEdit::ModelAt(std::size_t model_index) const {
  const auto found = std::ranges::find(models_, model_index, &ModelEdit::model_index);
  return found == models_.end() ? nullptr : &*found;
}

bool EffectEditTargets(const EffectEdit& edit, const formats::RswEffect& object) {
  return edit.number == object.number && NearOrigin(edit.origin, object.position);
}

const EffectEdit* MapEdit::EffectEditAt(std::size_t effect_index) const {
  const auto found =
      std::ranges::lower_bound(effect_edits_, effect_index, {}, &EffectEdit::effect_index);
  return found != effect_edits_.end() && found->effect_index == effect_index ? &*found : nullptr;
}

void MapEdit::SetEffectEdit(EffectEdit edit) {
  const auto place =
      std::ranges::lower_bound(effect_edits_, edit.effect_index, {}, &EffectEdit::effect_index);
  if (place != effect_edits_.end() && place->effect_index == edit.effect_index) {
    *place = std::move(edit);
  } else {
    effect_edits_.insert(place, std::move(edit));
  }
}

void MapEdit::RemoveEffectEdit(std::size_t effect_index) {
  std::erase_if(effect_edits_, [effect_index](const EffectEdit& edit) {
    return edit.effect_index == effect_index;
  });
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

void MapEdit::SetEffect(std::size_t slot, AddedEffect effect) {
  if (slot < effects_.size()) {
    effects_[slot] = std::move(effect);
  }
}

void MapEdit::RemoveEffect(std::size_t slot) {
  if (slot < effects_.size()) {
    effects_.erase(effects_.begin() + static_cast<std::ptrdiff_t>(slot));
  }
}

void MapEdit::SetLight(std::size_t slot, AddedLight light) {
  if (slot < lights_.size()) {
    lights_[slot] = std::move(light);
  }
}

void MapEdit::RemoveLight(std::size_t slot) {
  if (slot < lights_.size()) {
    lights_.erase(lights_.begin() + static_cast<std::ptrdiff_t>(slot));
  }
}

void MapEdit::SetSprite(std::size_t slot, MapSprite sprite) {
  if (slot < sprites_.size()) {
    sprites_[slot] = std::move(sprite);
  }
}

void MapEdit::RemoveSprite(std::size_t slot) {
  if (slot < sprites_.size()) {
    sprites_.erase(sprites_.begin() + static_cast<std::ptrdiff_t>(slot));
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
      NoteOpacity(model_edit.model_index, model_edit.opacity, report);
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
    model.animation_type = added.animation_type;
    model.animation_speed = added.animation_speed;
    const std::size_t rank = rsw.AddModel(std::move(model));
    if (added.hidden) {
      report.hidden.push_back(rank);
    } else {
      NoteOpacity(rank, added.opacity, report);
    }
    ++report.applied;
  }
  std::ranges::sort(report.hidden);
  std::ranges::sort(report.opacities, {}, &ModelOpacity::model_index);
  return report;
}

namespace {

// L'heure ramenée dans [0, 24).
float WrappedHour(float hour) {
  const float wrapped = std::fmod(hour, kLitHoursPerDay);
  return wrapped < 0.0F ? wrapped + kLitHoursPerDay : wrapped;
}

// La longueur de la plage, en heures ; deux bornes égales, tout le jour.
float SpanHours(const LitHours& hours) {
  const float span = WrappedHour(hours.to_hour - hours.from_hour);
  return span == 0.0F ? kLitHoursPerDay : span;
}

}  // namespace

bool LitHoursCover(const LitHours& hours, float hour) {
  return WrappedHour(hour - hours.from_hour) <= SpanHours(hours);
}

float LitHoursStrength(const LitHours& hours, float hour) {
  const float span = SpanHours(hours);
  if (span >= kLitHoursPerDay) {
    return 1.0F;
  }
  const float since_start = WrappedHour(hour - hours.from_hour);
  if (since_start > span) {
    return 0.0F;
  }
  const float fade = std::min(kLitHoursFadeHours, span / 2.0F);
  return std::clamp(std::min(since_start, span - since_start) / fade, 0.0F, 1.0F);
}

core::Result<MapEdit> ParseMapEdit(std::string_view yaml_text, std::string_view map_name) {
  core::Result<RevisedMapEdit> revised = ParseRevisedMapEdit(yaml_text, map_name);
  if (!revised) {
    return std::unexpected(revised.error());
  }
  return std::move(revised->edit);
}

std::string MapEditToYaml(const MapEdit& edit, std::string_view map_name) {
  return RevisedMapEditToYaml(edit, map_name, 0);
}

}  // namespace ro::mapedit
