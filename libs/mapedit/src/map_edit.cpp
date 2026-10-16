#include "ro/mapedit/map_edit.h"

#include <algorithm>
#include <cmath>
#include <format>
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
