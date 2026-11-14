#include "items/item_info_delta.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/file_write.h"
#include "ro/core/log.h"
#include "ro/core/utf8_path.h"

#include "game/save_folder.h"
#include "game/yaml_load.h"

namespace moonclient {
namespace {

// Le nom du fichier, sa clé racine et ses champs. ⚠ GRAVÉS : ce sont ceux que
// le script lit.
constexpr const char* kFileName = "item_info_delta.yml";
constexpr const char* kRootKey = "item_info_delta";
constexpr const char* kItemField = "item";
constexpr const char* kNameField = "name";
constexpr const char* kResourceField = "resource";
constexpr const char* kTintField = "tint";
constexpr const char* kDescriptionField = "description";
// Le mot qui RETIRE la teinte : un texte vide ne se distinguerait pas, à la
// relecture d'un fichier retouché, d'un champ oublié.
constexpr std::string_view kNoTintWord = "none";

// L'en-tête est écrit à la main : l'émetteur ne porte pas de commentaires, et un
// bon de travail qu'on ouvre sans savoir ce qu'il commande s'applique de travers.
constexpr const char* kHeader =
    "# Bons de travail « fiche d'objet », posés EN JEU par le staff.\n"
    "#\n"
    "# Ce fichier ne dit PAS ce qu'un objet est : il dit ce qu'on veut changer de\n"
    "# sa fiche. Un champ absent n'est pas demandé ; tint: none retire la teinte.\n"
    "#\n"
    "# La vérité est moonclient/dist/data/yaml_files/item_info.yml (ADR 0112).\n"
    "#\n"
    "# À appliquer, depuis le dépôt moonclient :\n"
    "#   python scripts/yaml_tables/apply_item_info_delta.py\n"
    "#       --installation <ce jeu> [--apply]\n"
    "# Il patche la table du dépôt, la relit, la recopie dans ce jeu, puis vide\n"
    "# ce fichier. Ensuite : relancer le client.\n";

[[nodiscard]] std::optional<std::string> ReadText(const YAML::Node& node, const char* field) {
  const YAML::Node value = node[field];
  if (!value || !value.IsScalar()) {
    return std::nullopt;
  }
  return value.as<std::string>();
}

[[nodiscard]] std::optional<std::vector<std::string>> ReadLines(const YAML::Node& node) {
  const YAML::Node value = node[kDescriptionField];
  if (!value || !value.IsSequence()) {
    return std::nullopt;
  }
  std::vector<std::string> lines;
  for (const YAML::Node& line : value) {
    lines.push_back(line.IsScalar() ? line.as<std::string>() : std::string());
  }
  return lines;
}

[[nodiscard]] auto SameItem(std::uint32_t item_id) {
  return [item_id](const ItemInfoDeltaEntry& entry) { return entry.item_id == item_id; };
}

}  // namespace

ItemInfoDelta::ItemInfoDelta(std::filesystem::path client_root)
    : path_(std::move(client_root) / kSaveFolder / kFileName) {}

void ItemInfoDelta::Load() {
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
    ro::core::LogWarning("fiche d'objet : bons de travail illisibles ({})", broken.what());
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
    ItemInfoDeltaEntry entry;
    entry.item_id = node[kItemField].as<std::uint32_t>(0U);
    entry.name = ReadText(node, kNameField);
    entry.resource = ReadText(node, kResourceField);
    entry.tint = ReadText(node, kTintField);
    if (entry.tint.has_value() && *entry.tint == kNoTintWord) {
      entry.tint = std::string();
    }
    entry.description = ReadLines(node);
    if (entry.item_id == 0 || entry.empty()) {
      continue;
    }
    entries_.push_back(std::move(entry));
  }
}

bool ItemInfoDelta::Flush() const {
  YAML::Emitter out;
  out << YAML::BeginMap << YAML::Key << kRootKey << YAML::Value << YAML::BeginSeq;
  for (const ItemInfoDeltaEntry& entry : entries_) {
    out << YAML::BeginMap;
    out << YAML::Key << kItemField << YAML::Value << entry.item_id;
    // ⚠ ENTRE GUILLEMETS : un nom comme « yes » ou « 1 » serait relu autrement.
    if (entry.name.has_value()) {
      out << YAML::Key << kNameField << YAML::Value << YAML::DoubleQuoted << *entry.name;
    }
    if (entry.resource.has_value()) {
      out << YAML::Key << kResourceField << YAML::Value << YAML::DoubleQuoted << *entry.resource;
    }
    if (entry.tint.has_value()) {
      const std::string word = entry.tint->empty() ? std::string(kNoTintWord) : *entry.tint;
      out << YAML::Key << kTintField << YAML::Value << YAML::DoubleQuoted << word;
    }
    if (entry.description.has_value()) {
      out << YAML::Key << kDescriptionField << YAML::Value << YAML::BeginSeq;
      for (const std::string& line : *entry.description) {
        out << YAML::DoubleQuoted << line;
      }
      out << YAML::EndSeq;
    }
    out << YAML::EndMap;
  }
  out << YAML::EndSeq << YAML::EndMap;
  std::error_code ignored;
  std::filesystem::create_directories(path_.parent_path(), ignored);
  const std::string text = std::string(kHeader) + out.c_str() + "\n";
  if (auto written = ro::core::WriteFileAtomically(path_, text); !written) {
    ro::core::LogWarning("fiche d'objet : bon de travail non écrit ({}) : {}",
                         ro::core::Utf8FromPath(path_), written.error().detail);
    return false;
  }
  return true;
}

const std::vector<ItemInfoDeltaEntry>& ItemInfoDelta::entries() {
  Load();
  return entries_;
}

const ItemInfoDeltaEntry* ItemInfoDelta::Find(std::uint32_t item_id) {
  Load();
  const auto found = std::find_if(entries_.begin(), entries_.end(), SameItem(item_id));
  return found == entries_.end() ? nullptr : &*found;
}

bool ItemInfoDelta::Set(const ItemInfoDeltaEntry& entry) {
  Load();
  if (entry.item_id == 0) {
    return false;
  }
  if (entry.empty()) {
    return Clear(entry.item_id);
  }
  const std::vector<ItemInfoDeltaEntry> before = entries_;
  const auto found = std::find_if(entries_.begin(), entries_.end(), SameItem(entry.item_id));
  if (found == entries_.end()) {
    entries_.push_back(entry);
  } else {
    *found = entry;
  }
  // 🔴 L'ÉCRITURE D'ABORD, L'ÉTAT ENSUITE : si le disque refuse, on rend
  // EXACTEMENT ce qu'il y avait — y compris l'intention que celle-ci remplaçait.
  if (!Flush()) {
    entries_ = before;
    return false;
  }
  ++revision_;
  return true;
}

bool ItemInfoDelta::Clear(std::uint32_t item_id) {
  Load();
  const auto found = std::find_if(entries_.begin(), entries_.end(), SameItem(item_id));
  if (found == entries_.end()) {
    return true;
  }
  const std::vector<ItemInfoDeltaEntry> before = entries_;
  entries_.erase(found);
  if (!Flush()) {
    entries_ = before;
    return false;
  }
  ++revision_;
  return true;
}

}  // namespace moonclient
