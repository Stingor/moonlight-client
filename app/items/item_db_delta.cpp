#include "items/item_db_delta.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/file_write.h"
#include "ro/core/log.h"
#include "ro/core/utf8_path.h"
#include "ro/ui/item_sheet_editor.h"

#include "game/save_folder.h"
#include "game/yaml_load.h"

namespace moonclient {
namespace {

// Le nom du fichier, sa clé racine et ses champs. ⚠ GRAVÉS : ce sont ceux que
// le script lit.
constexpr const char* kFileName = "item_db_delta.yml";
constexpr const char* kRootKey = "item_db_delta";
constexpr const char* kItemField = "item";

// L'en-tête est écrit à la main : l'émetteur ne porte pas de commentaires, et un
// bon de travail qu'on ouvre sans savoir ce qu'il commande s'applique de travers.
constexpr const char* kHeader =
    "# Bons de travail « item_db », posés EN JEU par le staff.\n"
    "#\n"
    "# Ce fichier ne dit PAS ce qu'un item est pour le serveur : il dit ce qu'on\n"
    "# veut changer de son entrée. Les clés sont celles de l'item_db ; un script\n"
    "# vide retire le script.\n"
    "#\n"
    "# La vérité est serveur : moonlight/db/import/items/*.yml. La table que lit\n"
    "# moonclient, data/yaml_files/item_db.yml, en est EXPORTÉE.\n"
    "#\n"
    "# À appliquer, depuis le dépôt moonclient :\n"
    "#   python scripts/yaml_tables/apply_item_db_delta.py\n"
    "#       --serveur <moonlight> --installation <ce jeu> [--apply]\n"
    "# Il patche l'import du serveur, vérifie la base rechargée, réexporte les\n"
    "# tables du client, puis vide ce fichier. Ensuite : @reloaditemdb.\n";

[[nodiscard]] auto SameItem(std::uint32_t item_id) {
  return [item_id](const ItemDbDeltaEntry& entry) { return entry.item_id == item_id; };
}

// Une clé que le bon a le droit de porter.
[[nodiscard]] bool KnownKey(std::string_view key) {
  const auto named = [key](const auto& field) { return field.key == key; };
  return key == ro::ui::kItemSheetRefineableKey ||
         std::ranges::any_of(ro::ui::kItemSheetServerNumbers, named) ||
         std::ranges::any_of(ro::ui::kItemSheetServerScripts, named);
}

}  // namespace

bool ItemDbFieldIsText(std::string_view key) {
  return std::ranges::any_of(ro::ui::kItemSheetServerScripts,
                             [key](const auto& script) { return script.key == key; });
}

ItemDbDelta::ItemDbDelta(std::filesystem::path client_root)
    : path_(std::move(client_root) / kSaveFolder / kFileName) {}

void ItemDbDelta::Load() {
  if (loaded_) {
    return;
  }
  // Même sur un échec : ne pas retourner au disque à chaque image.
  loaded_ = true;
  std::error_code missing;
  if (!std::filesystem::exists(path_, missing)) {
    return;
  }
  YAML::Node root;
  try {
    root = LoadYamlFile(path_);
  } catch (const YAML::Exception& broken) {
    // Un document abîmé se traite comme un fichier absent : repartir de rien
    // vaut mieux qu'appliquer la moitié d'une intention.
    ro::core::LogWarning("item_db : bons de travail illisibles ({})", broken.what());
    return;
  }
  const YAML::Node list = root.IsMap() ? root[kRootKey] : YAML::Node();
  if (!list || !list.IsSequence()) {
    return;
  }
  for (const YAML::Node& node : list) {
    if (!node.IsMap()) {
      continue;
    }
    ItemDbDeltaEntry entry;
    entry.item_id = node[kItemField].as<std::uint32_t>(0U);
    for (const auto& field : node) {
      const std::string key = field.first.as<std::string>();
      if (key == kItemField || !KnownKey(key) || !field.second.IsScalar()) {
        continue;
      }
      entry.fields.emplace_back(key, field.second.as<std::string>());
    }
    if (entry.item_id == 0 || entry.fields.empty()) {
      continue;
    }
    entries_.push_back(std::move(entry));
  }
}

bool ItemDbDelta::Flush() const {
  YAML::Emitter out;
  out << YAML::BeginMap << YAML::Key << kRootKey << YAML::Value << YAML::BeginSeq;
  for (const ItemDbDeltaEntry& entry : entries_) {
    out << YAML::BeginMap;
    out << YAML::Key << kItemField << YAML::Value << entry.item_id;
    for (const auto& [key, value] : entry.fields) {
      out << YAML::Key << key << YAML::Value;
      if (ItemDbFieldIsText(key)) {
        out << YAML::DoubleQuoted << value;
      } else {
        out << value;
      }
    }
    out << YAML::EndMap;
  }
  out << YAML::EndSeq << YAML::EndMap;
  std::error_code ignored;
  std::filesystem::create_directories(path_.parent_path(), ignored);
  const std::string text = std::string(kHeader) + out.c_str() + "\n";
  if (auto written = ro::core::WriteFileAtomically(path_, text); !written) {
    ro::core::LogWarning("item_db : bon de travail non écrit ({}) : {}",
                         ro::core::Utf8FromPath(path_), written.error().detail);
    return false;
  }
  return true;
}

const std::vector<ItemDbDeltaEntry>& ItemDbDelta::entries() {
  Load();
  return entries_;
}

const ItemDbDeltaEntry* ItemDbDelta::Find(std::uint32_t item_id) {
  Load();
  const auto found = std::find_if(entries_.begin(), entries_.end(), SameItem(item_id));
  return found == entries_.end() ? nullptr : &*found;
}

bool ItemDbDelta::Set(const ItemDbDeltaEntry& entry) {
  Load();
  if (entry.item_id == 0) {
    return false;
  }
  if (entry.fields.empty()) {
    return Clear(entry.item_id);
  }
  const std::vector<ItemDbDeltaEntry> before = entries_;
  const auto found = std::find_if(entries_.begin(), entries_.end(), SameItem(entry.item_id));
  if (found == entries_.end()) {
    entries_.push_back(entry);
  } else {
    *found = entry;
  }
  // 🔴 L'ÉCRITURE D'ABORD, L'ÉTAT ENSUITE.
  if (!Flush()) {
    entries_ = before;
    return false;
  }
  return true;
}

bool ItemDbDelta::Clear(std::uint32_t item_id) {
  Load();
  const auto found = std::find_if(entries_.begin(), entries_.end(), SameItem(item_id));
  if (found == entries_.end()) {
    return true;
  }
  const std::vector<ItemDbDeltaEntry> before = entries_;
  entries_.erase(found);
  if (!Flush()) {
    entries_ = before;
    return false;
  }
  return true;
}

}  // namespace moonclient
