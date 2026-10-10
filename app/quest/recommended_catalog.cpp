#include "quest/recommended_catalog.h"

#include <algorithm>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "quest/quest_yaml_text.h"

namespace moonclient {
namespace {

using quest_yaml::Text;

constexpr int kSupportedVersion = 1;

RecommendedQuest ReadEntry(std::uint32_t entry_id, const YAML::Node& node,
                           std::string_view language, std::string_view file_language) {
  if (!node.IsMap() || !node["title"]) {
    throw YAML::BadConversion(node.Mark());
  }
  RecommendedQuest entry;
  entry.entry_id = entry_id;
  entry.title = Text(node["title"], language, file_language);
  entry.summary = Text(node["summary"], language, file_language);
  entry.icon = node["icon"].as<std::string>(std::string());
  entry.image = node["image"].as<std::string>(std::string());
  if (const YAML::Node npc = node["npc"]; npc && npc.IsMap()) {
    QuestNpc place;
    place.sprite = npc["sprite"].as<std::string>(std::string());
    place.map_name = npc["map"].as<std::string>(std::string());
    place.cell_x = npc["x"].as<int>(0);
    place.cell_y = npc["y"].as<int>(0);
    entry.npc = std::move(place);
  }
  if (const YAML::Node blocks = node["blocks"]; blocks && blocks.IsSequence()) {
    for (const YAML::Node& block : blocks) {
      entry.blocks.push_back(Text(block, language, file_language));
    }
  }
  return entry;
}

}  // namespace

ro::core::Result<RecommendedCatalog> RecommendedCatalog::Parse(std::string_view yaml_text,
                                                               std::string_view language) {
  YAML::Node whole;
  try {
    whole = YAML::Load(std::string(yaml_text));
  } catch (const YAML::Exception& failure) {
    return ro::core::Fail(ro::core::ErrorCode::kCorruptData,
                          std::string("recommandations : YAML illisible : ") + failure.what());
  }
  if (!whole.IsMap()) {
    return ro::core::Fail(ro::core::ErrorCode::kCorruptData,
                          "recommandations : le fichier n'est pas un objet");
  }
  const int read_version = quest_yaml::SchemaVersion(whole);
  if (read_version != kSupportedVersion) {
    return ro::core::Fail(
        ro::core::ErrorCode::kUnsupportedVersion,
        "recommandations : version " + std::to_string(read_version) + " non gérée");
  }
  const std::string file_language = quest_yaml::FileLanguage(whole);
  RecommendedCatalog catalog;
  if (const YAML::Node entries = whole["recommended"]; entries && entries.IsMap()) {
    for (const auto& entry : entries) {
      try {
        catalog.entries_.push_back(
            ReadEntry(entry.first.as<std::uint32_t>(), entry.second, language, file_language));
      } catch (const YAML::Exception&) {
        ++catalog.skipped_;
      }
    }
  }
  return catalog;
}

const RecommendedQuest* RecommendedCatalog::Find(std::uint32_t entry_id) const {
  const auto found = std::ranges::find(entries_, entry_id, &RecommendedQuest::entry_id);
  return found != entries_.end() ? &*found : nullptr;
}

}  // namespace moonclient
