#include "ro/render/map_fix.h"

#include <cmath>
#include <expected>
#include <format>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/log.h"
#include "ro/vfs/encoding.h"

namespace ro::render {
namespace {

constexpr int kSupportedVersion = 1;
constexpr std::size_t kVectorSize = 3;

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData,
                     std::string(kMapFixPath) + " : " + std::move(detail)};
}

// Un vecteur de trois réels. Deux composantes ne font pas une position : mieux
// vaut refuser la table que d'en compléter une à zéro.
core::Result<std::array<float, 3>> VectorFrom(const YAML::Node& node, const std::string& where) {
  if (!node.IsSequence() || node.size() != kVectorSize) {
    return std::unexpected(Refused(where + " n'a pas trois composantes"));
  }
  std::array<float, 3> vector{};
  for (std::size_t axis = 0; axis < kVectorSize; ++axis) {
    vector[axis] = node[axis].as<float>();
  }
  return vector;
}

core::Result<std::optional<std::array<float, 3>>> OptionalVectorFrom(const YAML::Node& entry,
                                                                     std::string_view field,
                                                                     const std::string& where) {
  const YAML::Node node = entry[std::string(field)];
  if (!node) {
    return std::optional<std::array<float, 3>>();
  }
  core::Result<std::array<float, 3>> vector =
      VectorFrom(node, where + " : « " + std::string(field) + " »");
  if (!vector) {
    return std::unexpected(vector.error());
  }
  return std::optional<std::array<float, 3>>(*vector);
}

core::Result<ModelFix> ModelFixFrom(const YAML::Node& entry, const std::string& where) {
  if (!entry.IsMap()) {
    return std::unexpected(Refused(where + " n'est pas une table"));
  }
  const YAML::Node index = entry["index"];
  const YAML::Node file = entry["file"];
  const YAML::Node origin = entry["origin"];
  const YAML::Node why = entry["why"];
  if (!index || !file || !origin || !why) {
    return std::unexpected(
        Refused(where + " : « index », « file », « origin » et « why » sont dus"));
  }
  ModelFix fix;
  fix.model_index = index.as<std::size_t>();
  fix.model_file = file.as<std::string>();
  fix.why = why.as<std::string>();
  if (fix.why.empty()) {
    return std::unexpected(Refused(where + " : « why » est vide"));
  }
  core::Result<std::array<float, 3>> origin_vector = VectorFrom(origin, where + " : « origin »");
  if (!origin_vector) {
    return std::unexpected(origin_vector.error());
  }
  fix.origin = *origin_vector;
  auto position = OptionalVectorFrom(entry, "position", where);
  auto rotation = OptionalVectorFrom(entry, "rotation", where);
  auto scale = OptionalVectorFrom(entry, "scale", where);
  for (const auto* field : {&position, &rotation, &scale}) {
    if (!*field) {
      return std::unexpected(field->error());
    }
  }
  fix.position = *position;
  fix.rotation = *rotation;
  fix.scale = *scale;
  if (const YAML::Node hidden = entry["hidden"]) {
    fix.hidden = hidden.as<bool>();
  }
  return fix;
}

[[nodiscard]] bool NearOrigin(const std::array<float, 3>& expected,
                              const std::array<float, 3>& found) {
  for (std::size_t axis = 0; axis < kVectorSize; ++axis) {
    if (std::abs(expected[axis] - found[axis]) > kMapFixOriginTolerance) {
      return false;
    }
  }
  return true;
}

}  // namespace

core::Result<MapFixTable> MapFixTable::Parse(std::string_view yaml_text) {
  MapFixTable table;
  // yaml-cpp signale par exception : elle ne franchit pas cette fonction.
  try {
    const YAML::Node whole = YAML::Load(std::string(yaml_text));
    if (!whole.IsMap()) {
      return std::unexpected(Refused("le texte n'est pas une table YAML"));
    }
    const YAML::Node version = whole["version"];
    if (!version || version.as<int>() != kSupportedVersion) {
      return std::unexpected(Refused("version non gérée"));
    }
    const YAML::Node maps = whole["maps"];
    if (!maps || !maps.IsMap()) {
      return std::unexpected(Refused("« maps » n'est pas une table"));
    }
    for (const auto& pair : maps) {
      auto map_name = pair.first.as<std::string>();
      MapFixes fixes;
      if (const YAML::Node models = pair.second["models"]) {
        if (!models.IsSequence()) {
          return std::unexpected(Refused(map_name + " : « models » n'est pas une liste"));
        }
        for (std::size_t rank = 0; rank < models.size(); ++rank) {
          core::Result<ModelFix> fix =
              ModelFixFrom(models[rank], std::format("{}, retouche de modèle {}", map_name, rank));
          if (!fix) {
            return std::unexpected(fix.error());
          }
          fixes.models.push_back(std::move(*fix));
        }
      }
      table.by_map_[std::move(map_name)] = std::move(fixes);
    }
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
  }
  return table;
}

MapFixes MapFixTable::FixesOf(std::string_view map_name) const {
  const auto found = by_map_.find(map_name);
  return found == by_map_.end() ? MapFixes() : found->second;
}

MapFixReport ApplyMapFixes(const MapFixes& fixes, formats::RswScene& rsw) {
  MapFixReport report;
  std::vector<std::size_t> hidden;
  for (const ModelFix& fix : fixes.models) {
    if (fix.model_index >= rsw.models().size()) {
      report.refused.push_back(
          std::format("modèle {} : le RSW n'en a que {}", fix.model_index, rsw.models().size()));
      continue;
    }
    const formats::RswModel& model = rsw.models()[fix.model_index];
    const std::string file = vfs::Cp949ToUtf8Lossy(model.model_name);
    if (file != fix.model_file) {
      report.refused.push_back(std::format("modèle {} : « {} » attendu, « {} » trouvé",
                                           fix.model_index, fix.model_file, file));
      continue;
    }
    if (!NearOrigin(fix.origin, model.position)) {
      report.refused.push_back(std::format(
          "modèle {} : attendu en {} {} {}, trouvé en {} {} {}", fix.model_index, fix.origin[0],
          fix.origin[1], fix.origin[2], model.position[0], model.position[1], model.position[2]));
      continue;
    }
    if (fix.hidden) {
      hidden.push_back(fix.model_index);
    } else {
      rsw.PlaceModel(fix.model_index, fix.position.value_or(model.position),
                     fix.rotation.value_or(model.rotation), fix.scale.value_or(model.scale));
    }
    ++report.applied;
  }
  rsw.RemoveModels(hidden);
  return report;
}

MapFixes LoadMapFixes(vfs::Vfs& vfs, std::string_view map_name) {
  auto bytes = vfs.Read(kMapFixPath);
  if (!bytes) {
    core::LogInfo("{} absente — {} s'ouvre sans retouche", kMapFixPath, map_name);
    return {};
  }
  auto table = MapFixTable::Parse(
      std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
  if (!table) {
    core::LogWarning("{} — {} s'ouvre sans retouche", core::Describe(table.error()), map_name);
    return {};
  }
  return table->FixesOf(map_name);
}

}  // namespace ro::render
