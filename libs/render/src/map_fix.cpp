#include "ro/render/map_fix.h"

#include <algorithm>
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

// L'EN-TÊTE QUE L'ÉTABLI ÉCRIT : la mention d'origine, le format, et les
// commandes qui servent à écrire et à vérifier la table.
constexpr std::string_view kMapFixHeader =
    R"(# ─────────────────────────────────────────────────────────────────────────────
#  Les retouches de carte du staff
#
#  Donnée de moonclient, tenue par le staff (docs/adr/0117). Certaines cartes
#  officielles portent des défauts que le client d'origine montre aussi, parce
#  qu'ils sont dans leurs données ; la carte reste celle de l'installation, et
#  le client la retouche au chargement, avant de bâtir ses maillages.
#
#  Le visuel seulement : la marchabilité (GAT) n'est jamais touchée.
#
#  Écrite par l'établi du staff (section « Retouches de carte ») ; elle se
#  relit et se corrige aussi à la main.
#
#  maps : carte (son nom, sans extension) ->
#    models : les retouches de modèles du RSW, chacune :
#      index    : le rang du modèle dans le RSW, compté à partir de zéro ;
#      file     : son fichier, relatif à data\model\ ;
#      origin   : sa position dans le RSW d'origine, [x, y, z] ;
#      position, rotation, scale : ce qui change, [x, y, z], dans le repère
#                 du fichier — un champ absent garde la valeur du RSW ;
#      hidden   : vrai pour ne plus le dessiner ;
#      why      : pourquoi, obligatoire.
#  Une retouche dont le rang, le fichier ou la position d'origine ne
#  correspondent plus n'est pas posée, et le journal le dit.
#
#  Les modèles d'une carte, leur rang et leur placement :
#      roc-cli map models <dossier client> <carte> [<case x> <case y> <rayon>]
#  Vérifier la table sur une carte, sans l'installer (le journal dit ce qui
#  est posé et ce qui est refusé) :
#      roc-cli map view <dossier client> <carte> <sortie.png> <x> <y> \
#          --retouches dist/data/yaml_files/map_fix.yml
#
#  Lu par moonclient (ro/render/map_fix.h). Absent, aucune carte n'est
#  retouchée.
# ─────────────────────────────────────────────────────────────────────────────
)";

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

// Une chaîne entre apostrophes : rien ne s'y échappe, sauf l'apostrophe, qui
// se double. C'est ce qui garde lisibles les barres obliques inverses d'un
// chemin.
[[nodiscard]] std::string SingleQuoted(std::string_view text) {
  std::string quoted = "'";
  for (const char letter : text) {
    quoted += letter;
    if (letter == '\'') {
      quoted += '\'';
    }
  }
  return quoted + "'";
}

// Une chaîne entre guillemets, ses guillemets, barres obliques inverses et
// sauts de ligne échappés.
[[nodiscard]] std::string DoubleQuoted(std::string_view text) {
  std::string quoted = "\"";
  for (const char letter : text) {
    if (letter == '"' || letter == '\\') {
      quoted += '\\';
      quoted += letter;
    } else if (letter == '\n') {
      quoted += "\\n";
    } else {
      quoted += letter;
    }
  }
  return quoted + "\"";
}

// Un vecteur, chaque composante à son écriture la plus courte qui se relit à
// l'identique.
[[nodiscard]] std::string VectorText(const std::array<float, 3>& vector) {
  return std::format("[{}, {}, {}]", vector[0], vector[1], vector[2]);
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

void MapFixTable::SetModelFix(std::string_view map_name, ModelFix fix) {
  auto found = by_map_.find(map_name);
  if (found == by_map_.end()) {
    found = by_map_.emplace(std::string(map_name), MapFixes()).first;
  }
  std::vector<ModelFix>& models = found->second.models;
  const auto place = std::ranges::lower_bound(models, fix.model_index, {}, &ModelFix::model_index);
  if (place != models.end() && place->model_index == fix.model_index) {
    *place = std::move(fix);
  } else {
    models.insert(place, std::move(fix));
  }
}

void MapFixTable::RemoveModelFix(std::string_view map_name, std::size_t model_index) {
  const auto found = by_map_.find(map_name);
  if (found == by_map_.end()) {
    return;
  }
  std::erase_if(found->second.models,
                [model_index](const ModelFix& fix) { return fix.model_index == model_index; });
  if (found->second.models.empty()) {
    by_map_.erase(found);
  }
}

std::string MapFixTable::ToYaml() const {
  std::string text(kMapFixHeader);
  text += std::format("version: {}\nmaps:", kSupportedVersion);
  if (by_map_.empty()) {
    text += " {}";
  }
  text += "\n";
  for (const auto& [map_name, fixes] : by_map_) {
    text += std::format("  {}:\n    models:\n", map_name);
    for (const ModelFix& fix : fixes.models) {
      text += std::format("      - index: {}\n", fix.model_index);
      text += std::format("        file: {}\n", SingleQuoted(fix.model_file));
      text += std::format("        origin: {}\n", VectorText(fix.origin));
      if (fix.position.has_value()) {
        text += std::format("        position: {}\n", VectorText(*fix.position));
      }
      if (fix.rotation.has_value()) {
        text += std::format("        rotation: {}\n", VectorText(*fix.rotation));
      }
      if (fix.scale.has_value()) {
        text += std::format("        scale: {}\n", VectorText(*fix.scale));
      }
      if (fix.hidden) {
        text += "        hidden: true\n";
      }
      text += std::format("        why: {}\n", DoubleQuoted(fix.why));
    }
  }
  return text;
}

MapFixReport ApplyMapFixes(const MapFixes& fixes, formats::RswScene& rsw) {
  MapFixReport report;
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
      report.hidden.push_back(fix.model_index);
    } else {
      rsw.PlaceModel(fix.model_index, fix.position.value_or(model.position),
                     fix.rotation.value_or(model.rotation), fix.scale.value_or(model.scale));
    }
    ++report.applied;
  }
  std::ranges::sort(report.hidden);
  return report;
}

MapFixTable ReadMapFixTable(vfs::Vfs& vfs) {
  auto bytes = vfs.Read(kMapFixPath);
  if (!bytes) {
    core::LogInfo("{} absente — aucune carte n'est retouchée", kMapFixPath);
    return {};
  }
  auto table = MapFixTable::Parse(
      std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
  if (!table) {
    core::LogWarning("{} — aucune carte n'est retouchée", core::Describe(table.error()));
    return {};
  }
  return std::move(*table);
}

MapFixes LoadMapFixes(vfs::Vfs& vfs, std::string_view map_name) {
  return ReadMapFixTable(vfs).FixesOf(map_name);
}

}  // namespace ro::render
