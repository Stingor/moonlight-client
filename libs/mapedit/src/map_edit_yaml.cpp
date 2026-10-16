#include <expected>
#include <format>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/mapedit/map_edit.h"

namespace ro::mapedit {
namespace {

constexpr int kSupportedVersion = 1;
constexpr std::size_t kVectorSize = 3;

// L'EN-TÊTE QUE L'ÉDITEUR ÉCRIT : la mention d'origine, le format, et les
// commandes qui servent à écrire et à vérifier un document.
constexpr std::string_view kMapEditHeader =
    R"(# ─────────────────────────────────────────────────────────────────────────────
#  L'édition d'une carte
#
#  Donnée de moonclient, tenue par le staff (docs/adr/0118). La carte reste
#  celle de l'installation : le client applique ces opérations au chargement,
#  avant de bâtir ses maillages. Le visuel seulement : la marchabilité (GAT)
#  n'est jamais touchée.
#
#  Écrit par l'éditeur de carte du jeu ; il se relit et se corrige aussi à la
#  main.
#
#  map    : la carte, son nom sans extension — celui du fichier ;
#  models : les retouches de modèles du RSW, chacune :
#    index    : le rang du modèle dans le RSW, compté à partir de zéro ;
#    file     : son fichier, relatif à data\model\ ;
#    origin   : sa position dans le RSW d'origine, [x, y, z] ;
#    position, rotation, scale : ce qui change, [x, y, z], dans le repère du
#               fichier — un champ absent garde la valeur du RSW ;
#    hidden   : vrai pour ne plus le dessiner ;
#    why      : pourquoi, obligatoire.
#  added  : les modèles ajoutés, qui prennent rang après ceux du RSW, chacun :
#    file     : son fichier, relatif à data\model\ ;
#    position, rotation, scale : son placement, [x, y, z], dans le repère du
#               fichier — position est obligatoire ;
#    hidden   : vrai pour ne pas le dessiner ;
#    why      : pourquoi, obligatoire.
#  Une opération dont la cible ne correspond plus n'est pas posée, et le
#  journal le dit.
#
#  Les modèles d'une carte, leur rang et leur placement :
#      roc-cli map models <dossier client> <carte> [<case x> <case y> <rayon>]
#  Vérifier un document sans l'installer (le journal dit ce qui est posé et ce
#  qui est refusé) :
#      roc-cli map view <dossier client> <carte> <sortie.png> <x> <y> \
#          --edition dist/data/map_edits/<carte>.yml
# ─────────────────────────────────────────────────────────────────────────────
)";

core::Error Refused(std::string_view map_name, std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData,
                     MapEditPath(map_name) + " : " + std::move(detail)};
}

// Un vecteur de trois réels. Deux composantes ne font pas une position : mieux
// vaut refuser le document que d'en compléter une à zéro.
core::Result<std::array<float, 3>> VectorFrom(const YAML::Node& node, std::string_view map_name,
                                              const std::string& where) {
  if (!node.IsSequence() || node.size() != kVectorSize) {
    return std::unexpected(Refused(map_name, where + " n'a pas trois composantes"));
  }
  std::array<float, 3> vector{};
  for (std::size_t axis = 0; axis < kVectorSize; ++axis) {
    vector[axis] = node[axis].as<float>();
  }
  return vector;
}

core::Result<std::optional<std::array<float, 3>>> OptionalVectorFrom(const YAML::Node& entry,
                                                                     std::string_view field,
                                                                     std::string_view map_name,
                                                                     const std::string& where) {
  const YAML::Node node = entry[std::string(field)];
  if (!node) {
    return std::optional<std::array<float, 3>>();
  }
  core::Result<std::array<float, 3>> vector =
      VectorFrom(node, map_name, where + " : « " + std::string(field) + " »");
  if (!vector) {
    return std::unexpected(vector.error());
  }
  return std::optional<std::array<float, 3>>(*vector);
}

core::Result<ModelEdit> ModelEditFrom(const YAML::Node& entry, std::string_view map_name,
                                      const std::string& where) {
  if (!entry.IsMap()) {
    return std::unexpected(Refused(map_name, where + " n'est pas une table"));
  }
  const YAML::Node index = entry["index"];
  const YAML::Node file = entry["file"];
  const YAML::Node origin = entry["origin"];
  const YAML::Node why = entry["why"];
  if (!index || !file || !origin || !why) {
    return std::unexpected(
        Refused(map_name, where + " : « index », « file », « origin » et « why » sont dus"));
  }
  ModelEdit edit;
  edit.model_index = index.as<std::size_t>();
  edit.model_file = file.as<std::string>();
  edit.why = why.as<std::string>();
  if (edit.why.empty()) {
    return std::unexpected(Refused(map_name, where + " : « why » est vide"));
  }
  core::Result<std::array<float, 3>> origin_vector =
      VectorFrom(origin, map_name, where + " : « origin »");
  if (!origin_vector) {
    return std::unexpected(origin_vector.error());
  }
  edit.origin = *origin_vector;
  auto position = OptionalVectorFrom(entry, "position", map_name, where);
  auto rotation = OptionalVectorFrom(entry, "rotation", map_name, where);
  auto scale = OptionalVectorFrom(entry, "scale", map_name, where);
  for (const auto* field : {&position, &rotation, &scale}) {
    if (!*field) {
      return std::unexpected(field->error());
    }
  }
  edit.position = *position;
  edit.rotation = *rotation;
  edit.scale = *scale;
  if (const YAML::Node hidden = entry["hidden"]) {
    edit.hidden = hidden.as<bool>();
  }
  return edit;
}

core::Result<AddedModel> AddedModelFrom(const YAML::Node& entry, std::string_view map_name,
                                        const std::string& where) {
  if (!entry.IsMap()) {
    return std::unexpected(Refused(map_name, where + " n'est pas une table"));
  }
  const YAML::Node file = entry["file"];
  const YAML::Node position = entry["position"];
  const YAML::Node why = entry["why"];
  if (!file || !position || !why) {
    return std::unexpected(
        Refused(map_name, where + " : « file », « position » et « why » sont dus"));
  }
  AddedModel added;
  added.model_file = file.as<std::string>();
  added.why = why.as<std::string>();
  if (added.why.empty()) {
    return std::unexpected(Refused(map_name, where + " : « why » est vide"));
  }
  core::Result<std::array<float, 3>> placed =
      VectorFrom(position, map_name, where + " : « position »");
  if (!placed) {
    return std::unexpected(placed.error());
  }
  added.position = *placed;
  auto rotation = OptionalVectorFrom(entry, "rotation", map_name, where);
  auto scale = OptionalVectorFrom(entry, "scale", map_name, where);
  if (!rotation) {
    return std::unexpected(rotation.error());
  }
  if (!scale) {
    return std::unexpected(scale.error());
  }
  added.rotation = rotation->value_or(added.rotation);
  added.scale = scale->value_or(added.scale);
  if (const YAML::Node hidden = entry["hidden"]) {
    added.hidden = hidden.as<bool>();
  }
  return added;
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

}  // namespace

core::Result<MapEdit> ParseMapEdit(std::string_view yaml_text, std::string_view map_name) {
  MapEdit edit;
  // yaml-cpp signale par exception : elle ne franchit pas cette fonction.
  try {
    const YAML::Node whole = YAML::Load(std::string(yaml_text));
    if (!whole.IsMap()) {
      return std::unexpected(Refused(map_name, "le texte n'est pas une table YAML"));
    }
    const YAML::Node version = whole["version"];
    if (!version || version.as<int>() != kSupportedVersion) {
      return std::unexpected(Refused(map_name, "version non gérée"));
    }
    const YAML::Node map = whole["map"];
    if (!map || map.as<std::string>() != map_name) {
      return std::unexpected(Refused(map_name, "« map » ne nomme pas cette carte"));
    }
    if (const YAML::Node models = whole["models"]) {
      if (!models.IsSequence()) {
        return std::unexpected(Refused(map_name, "« models » n'est pas une liste"));
      }
      for (std::size_t rank = 0; rank < models.size(); ++rank) {
        core::Result<ModelEdit> model =
            ModelEditFrom(models[rank], map_name, std::format("retouche de modèle {}", rank));
        if (!model) {
          return std::unexpected(model.error());
        }
        edit.SetModel(std::move(*model));
      }
    }
    if (const YAML::Node added = whole["added"]) {
      if (!added.IsSequence()) {
        return std::unexpected(Refused(map_name, "« added » n'est pas une liste"));
      }
      for (std::size_t rank = 0; rank < added.size(); ++rank) {
        core::Result<AddedModel> model =
            AddedModelFrom(added[rank], map_name, std::format("ajout {}", rank));
        if (!model) {
          return std::unexpected(model.error());
        }
        edit.AddModel(std::move(*model));
      }
    }
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(map_name, failure.what()));
  }
  return edit;
}

std::string MapEditToYaml(const MapEdit& edit, std::string_view map_name) {
  std::string text(kMapEditHeader);
  text += std::format("version: {}\nmap: {}\nmodels:", kSupportedVersion, SingleQuoted(map_name));
  if (edit.models().empty()) {
    text += " []";
  }
  text += "\n";
  for (const ModelEdit& model : edit.models()) {
    text += std::format("  - index: {}\n", model.model_index);
    text += std::format("    file: {}\n", SingleQuoted(model.model_file));
    text += std::format("    origin: {}\n", VectorText(model.origin));
    if (model.position.has_value()) {
      text += std::format("    position: {}\n", VectorText(*model.position));
    }
    if (model.rotation.has_value()) {
      text += std::format("    rotation: {}\n", VectorText(*model.rotation));
    }
    if (model.scale.has_value()) {
      text += std::format("    scale: {}\n", VectorText(*model.scale));
    }
    if (model.hidden) {
      text += "    hidden: true\n";
    }
    text += std::format("    why: {}\n", DoubleQuoted(model.why));
  }
  if (!edit.added().empty()) {
    text += "added:\n";
  }
  for (const AddedModel& added : edit.added()) {
    text += std::format("  - file: {}\n", SingleQuoted(added.model_file));
    text += std::format("    position: {}\n", VectorText(added.position));
    text += std::format("    rotation: {}\n", VectorText(added.rotation));
    text += std::format("    scale: {}\n", VectorText(added.scale));
    if (added.hidden) {
      text += "    hidden: true\n";
    }
    text += std::format("    why: {}\n", DoubleQuoted(added.why));
  }
  return text;
}

}  // namespace ro::mapedit
