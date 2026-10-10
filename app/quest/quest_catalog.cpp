#include "quest/quest_catalog.h"

#include <algorithm>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "quest/quest_yaml_text.h"

namespace moonclient {
namespace {

using quest_yaml::Text;

constexpr int kSupportedVersion = 1;
constexpr std::string_view kEtcCategoryKey = "etc";
constexpr std::string_view kAllCategoryKey = "all";

QuestInfo ReadQuest(std::uint32_t quest_id, const YAML::Node& node, std::string_view language,
                    std::string_view file_language) {
  if (!node.IsMap() || !node["title"]) {
    throw YAML::BadConversion(node.Mark());
  }
  QuestInfo quest;
  quest.quest_id = quest_id;
  quest.title = Text(node["title"], language, file_language);
  quest.summary = Text(node["summary"], language, file_language);
  quest.description = Text(node["description"], language, file_language);
  if (const YAML::Node icon = node["icon"]; icon) {
    quest.icon = icon.as<std::string>();
  }
  if (const YAML::Node npc = node["npc"]; npc && npc.IsMap()) {
    QuestNpc place;
    place.sprite = npc["sprite"].as<std::string>(std::string());
    place.map_name = npc["map"].as<std::string>(std::string());
    place.cell_x = npc["x"].as<int>(0);
    place.cell_y = npc["y"].as<int>(0);
    quest.npc = std::move(place);
  }
  if (const YAML::Node rewards = node["rewards"]; rewards && rewards.IsMap()) {
    quest.reward_base_exp = rewards["base_exp"].as<std::uint64_t>(0);
    quest.reward_job_exp = rewards["job_exp"].as<std::uint64_t>(0);
    if (const YAML::Node items = rewards["items"]; items && items.IsSequence()) {
      for (const YAML::Node& item : items) {
        quest.reward_items.push_back(
            {item["id"].as<std::uint32_t>(), item["count"].as<std::uint32_t>(1)});
      }
    }
  }
  quest.cooldown = node["cooldown"].as<bool>(false);
  return quest;
}

}  // namespace

ro::core::Result<QuestCatalog> QuestCatalog::Parse(std::string_view yaml_text,
                                                   std::string_view language) {
  YAML::Node whole;
  try {
    whole = YAML::Load(std::string(yaml_text));
  } catch (const YAML::Exception& failure) {
    return ro::core::Fail(ro::core::ErrorCode::kCorruptData,
                          std::string("quêtes : YAML illisible : ") + failure.what());
  }
  if (!whole.IsMap()) {
    return ro::core::Fail(ro::core::ErrorCode::kCorruptData,
                          "quêtes : le fichier n'est pas un objet");
  }
  const int read_version = quest_yaml::SchemaVersion(whole);
  if (read_version != kSupportedVersion) {
    return ro::core::Fail(ro::core::ErrorCode::kUnsupportedVersion,
                          "quêtes : version " + std::to_string(read_version) + " non gérée");
  }

  const std::string file_language = quest_yaml::FileLanguage(whole);

  QuestCatalog catalog;
  try {
    if (const YAML::Node icons = whole["icons"]; icons && icons.IsMap()) {
      for (const auto& entry : icons) {
        QuestIcon icon;
        icon.key = entry.first.as<std::string>();
        icon.image = entry.second["image"].as<std::string>(std::string());
        icon.label = Text(entry.second["label"], language, file_language);
        catalog.icons_.push_back(std::move(icon));
      }
    }
    if (const YAML::Node categories = whole["categories"]; categories && categories.IsSequence()) {
      for (const YAML::Node& entry : categories) {
        QuestCategory category;
        category.key = entry["key"].as<std::string>();
        category.label = Text(entry["label"], language, file_language);
        if (const YAML::Node icons = entry["icons"]; icons && icons.IsSequence()) {
          for (const YAML::Node& icon : icons) {
            category.icons.push_back(icon.as<std::string>());
          }
        }
        catalog.categories_.push_back(std::move(category));
      }
    }
  } catch (const YAML::Exception& failure) {
    return ro::core::Fail(
        ro::core::ErrorCode::kCorruptData,
        std::string("quêtes : icônes ou catégories mal formées : ") + failure.what());
  }

  if (const YAML::Node quests = whole["quests"]; quests && quests.IsMap()) {
    for (const auto& entry : quests) {
      try {
        const auto quest_id = entry.first.as<std::uint32_t>();
        catalog.quests_.push_back(ReadQuest(quest_id, entry.second, language, file_language));
      } catch (const YAML::Exception&) {
        ++catalog.skipped_;
      }
    }
  }
  std::stable_sort(
      catalog.quests_.begin(), catalog.quests_.end(),
      [](const QuestInfo& left, const QuestInfo& right) { return left.quest_id < right.quest_id; });
  return catalog;
}

const QuestInfo* QuestCatalog::Find(std::uint32_t quest_id) const {
  const auto found = std::lower_bound(
      quests_.begin(), quests_.end(), quest_id,
      [](const QuestInfo& quest, std::uint32_t wanted) { return quest.quest_id < wanted; });
  return found != quests_.end() && found->quest_id == quest_id ? &*found : nullptr;
}

const QuestIcon* QuestCatalog::FindIcon(std::string_view key) const {
  for (const QuestIcon& icon : icons_) {
    if (icon.key == key) {
      return &icon;
    }
  }
  return nullptr;
}

std::string_view QuestCatalog::CategoryOf(const QuestInfo& quest) const {
  if (!quest.icon.empty()) {
    for (const QuestCategory& category : categories_) {
      if (category.key == kAllCategoryKey) {
        continue;
      }
      if (std::find(category.icons.begin(), category.icons.end(), quest.icon) !=
          category.icons.end()) {
        return category.key;
      }
    }
  }
  return kEtcCategoryKey;
}

}  // namespace moonclient
