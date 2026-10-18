#include "items/storage_tab_names.h"

#include <optional>

#include <yaml-cpp/yaml.h>

#include "ro/core/text.h"

#include "game/yaml_load.h"

namespace moonclient {
namespace {

// Les champs du fichier. ⚠ GRAVÉS : les changer rend à tous les coffres le nom
// du serveur, sans que rien ne le dise.
constexpr const char* kTabsField = "tabs";
constexpr const char* kShelfField = "shelf";
constexpr const char* kNameField = "name";
constexpr const char* kIconField = "icon";

// LA LONGUEUR D'UN NOM, celle du serveur (`NAME_LENGTH` moins le NUL).
//
// ⚠ ON BORNE CE QUI ENTRE, et pas seulement ce qui se tape : un fichier corrigé
// à la main peut porter n'importe quoi, et un nom démesuré chasserait les
// boutons voisins hors de la fenêtre.
constexpr std::size_t kLongestName = 23;

constexpr const char* kFileHeader =
    "# Le nom et l'icône que le joueur donne à ses coffres.\n"
    "#\n"
    "# « shelf » est le NUMÉRO du coffre, celui des commandes @storage et\n"
    "# @storagealtN. « name » remplace celui que le serveur envoie ; vide, ou\n"
    "# absent, on retombe sur le sien. « icon » est l'identifiant de l'objet\n"
    "# dont l'icône décore l'onglet ; zéro, ou absent, pour aucune.\n"
    "#\n"
    "# ⚠ TOUT CECI EST AU CLIENT. Le serveur ne connaît ni ces noms ni ces\n"
    "# icônes — il nomme ses coffres « Storage Alt 3 », et rien de plus.\n";

}  // namespace

const StorageTabName* StorageTabNames::Of(std::uint8_t storage_id) const {
  const auto found = told_.find(storage_id);
  return found == told_.end() ? nullptr : &found->second;
}

void StorageTabNames::Name(std::uint8_t storage_id, std::string_view name) {
  StorageTabName told = Of(storage_id) == nullptr ? StorageTabName{} : *Of(storage_id);
  told.name = std::string(ro::core::WithoutDanglingUtf8(name.substr(0, kLongestName)));
  told_[storage_id] = std::move(told);
  Forget(storage_id);
}

void StorageTabNames::Decorate(std::uint8_t storage_id, std::uint32_t item_id) {
  StorageTabName told = Of(storage_id) == nullptr ? StorageTabName{} : *Of(storage_id);
  told.icon_item_id = item_id;
  told_[storage_id] = std::move(told);
  Forget(storage_id);
}

void StorageTabNames::Put(std::uint8_t storage_id, StorageTabName told) {
  // ⚠ À LA FRONTIÈRE D'UN CARACTÈRE : un nom corrigé à la main peut porter un
  // accent juste à la coupe, et l'onglet montrerait un glyphe abîmé — que
  // l'écriture suivante graverait dans le fichier.
  told.name = std::string(
      ro::core::WithoutDanglingUtf8(std::string_view(told.name).substr(0, kLongestName)));
  told_[storage_id] = std::move(told);
  Forget(storage_id);
}

void StorageTabNames::Forget(std::uint8_t storage_id) {
  if (const auto found = told_.find(storage_id); found != told_.end() && !found->second.told()) {
    told_.erase(found);
  }
}

std::filesystem::path StorageTabNamesPath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kStorageTabNamesFileName;
}

void LoadStorageTabNames(const std::filesystem::path& client_root, StorageTabNames& into) {
  std::optional<YAML::Node> whole =
      LoadYamlMap(StorageTabNamesPath(client_root), "onglets du coffre",
                  "tous les coffres reprendront le nom du serveur");
  if (!whole.has_value()) {
    return;
  }
  const YAML::Node listed = (*whole)[kTabsField];
  if (!listed || !listed.IsSequence()) {
    return;
  }
  for (const YAML::Node& entry : listed) {
    if (!entry.IsMap()) {
      continue;
    }
    const YAML::Node shelf = entry[kShelfField];
    if (!shelf || !shelf.IsScalar()) {
      continue;
    }
    StorageTabName told;
    told.name = entry[kNameField].as<std::string>(std::string());
    told.icon_item_id = entry[kIconField].as<std::uint32_t>(0U);
    if (told.told()) {
      into.Put(shelf.as<std::uint32_t>(0U) & 0xFFU, std::move(told));
    }
  }
}

bool SaveStorageTabNames(const std::filesystem::path& client_root, const StorageTabNames& from) {
  YAML::Emitter out;
  out << YAML::BeginMap << YAML::Key << kTabsField << YAML::Value << YAML::BeginSeq;
  for (const auto& [storage_id, told] : from.all()) {
    out << YAML::Flow << YAML::BeginMap;
    out << YAML::Key << kShelfField << YAML::Value << static_cast<int>(storage_id);
    // ⚠ ON N'ÉCRIT QUE CE QUI EST DIT : un « nom: "" » sous chaque coffre ferait
    // croire à un réglage posé là où il n'y en a pas.
    if (!told.name.empty()) {
      out << YAML::Key << kNameField << YAML::Value << told.name;
    }
    if (told.icon_item_id != 0) {
      out << YAML::Key << kIconField << YAML::Value << told.icon_item_id;
    }
    out << YAML::EndMap;
  }
  out << YAML::EndSeq << YAML::EndMap;

  return SaveYamlFile(StorageTabNamesPath(client_root), kFileHeader, out, "onglets du coffre",
                      "les noms donnés cette partie ne seront pas retrouvés");
}

}  // namespace moonclient
