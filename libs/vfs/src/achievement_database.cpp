// Voir l'en-tête.

#include "ro/vfs/achievement_database.h"

#include <algorithm>
#include <cstdint>
#include <expected>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "yaml_required.h"

namespace ro::vfs {
namespace {

constexpr int kSupportedVersion = 1;
// Un objectif s'écrit [numéro, compte].
constexpr std::size_t kTargetIndexField = 0;
constexpr std::size_t kTargetCountField = 1;
constexpr std::size_t kTargetFieldCount = 2;
constexpr std::int64_t kMostCount = std::numeric_limits<std::uint32_t>::max();
// La jauge, telle qu'écrite.
constexpr std::string_view kBulletsGauge = "bullets";
constexpr std::string_view kCountsGauge = "counts";

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData, "table des succès : " + std::move(detail)};
}

[[nodiscard]] std::int32_t Number(const YAML::Node& node) {
  return static_cast<std::int32_t>(Whole(node, 0, kMostAchievementNumber));
}

[[nodiscard]] YAML::Node Sequence(const YAML::Node& map, const char* key) {
  YAML::Node list = Required(map, key);
  if (!list.IsSequence()) {
    throw YAML::RepresentationException(list.Mark(), std::string(key) + " n'est pas une liste");
  }
  return list;
}

void ReadTargets(const YAML::Node& entry, ServerAchievement& read) {
  for (const YAML::Node& target : Sequence(entry, "targets")) {
    if (!target.IsSequence() || target.size() != kTargetFieldCount) {
      throw YAML::RepresentationException(target.Mark(),
                                          "un objectif n'est pas une paire [numéro, compte]");
    }
    const auto index = static_cast<std::size_t>(
        Whole(target[kTargetIndexField], 0, kAchievementObjectiveCount - 1));
    read.targets[index] =
        static_cast<std::uint32_t>(Whole(target[kTargetCountField], 0, kMostCount));
  }
}

[[nodiscard]] AchievementGauge Gauge(const YAML::Node& node) {
  const auto written = node.as<std::string>();
  if (written == kBulletsGauge) {
    return AchievementGauge::kBullets;
  }
  if (written == kCountsGauge) {
    return AchievementGauge::kCounts;
  }
  throw YAML::RepresentationException(node.Mark(), "une jauge ni « bullets » ni « counts »");
}

[[nodiscard]] std::uint8_t TabNumber(const YAML::Node& node) {
  return static_cast<std::uint8_t>(Whole(node, 0, std::numeric_limits<std::uint8_t>::max()));
}

[[nodiscard]] AchievementWindow ReadWindow(const YAML::Node& node) {
  if (!node.IsMap()) {
    throw YAML::RepresentationException(node.Mark(), "« window » n'est pas une table");
  }
  AchievementWindow read{.summary = Required(node, "summary").as<std::string>(),
                         .details = Required(node, "details").as<std::string>(),
                         .gauge = Gauge(Required(node, "gauge")),
                         .group = Required(node, "group").as<std::string>(),
                         .major = TabNumber(Required(node, "major")),
                         .minor = TabNumber(Required(node, "minor"))};
  const YAML::Node objectives = Sequence(node, "objectives");
  if (objectives.size() > kAchievementObjectiveCount) {
    throw YAML::RepresentationException(objectives.Mark(), "plus de dix objectifs");
  }
  for (const YAML::Node& objective : objectives) {
    if (!objective.IsMap()) {
      throw YAML::RepresentationException(objective.Mark(), "un objectif n'est pas une table");
    }
    AchievementLine line{.text = Required(objective, "text").as<std::string>()};
    if (const YAML::Node shortcut = objective["shortcut"]) {
      line.shortcut = static_cast<std::uint32_t>(Number(shortcut));
    }
    read.lines.push_back(std::move(line));
  }
  if (const YAML::Node buff = node["buff"]) {
    read.buff = Number(buff);
  }
  return read;
}

[[nodiscard]] ServerAchievement ReadEntry(std::uint32_t achievement_id, const YAML::Node& entry) {
  if (!entry.IsMap()) {
    throw YAML::RepresentationException(entry.Mark(), "un succès n'est pas une table");
  }
  ServerAchievement read{.achievement_id = achievement_id,
                         .name = Required(entry, "name").as<std::string>(),
                         .score = Number(Required(entry, "score"))};
  ReadTargets(entry, read);
  const YAML::Node reward = Required(entry, "reward");
  if (!reward.IsMap()) {
    throw YAML::RepresentationException(reward.Mark(), "« reward » n'est pas une table");
  }
  read.reward_item = Number(Required(reward, "item"));
  read.reward_title = Number(Required(reward, "title"));
  read.reward_script = Required(reward, "script").as<bool>();
  for (const YAML::Node& needed : Sequence(entry, "dependents")) {
    read.dependents.push_back(static_cast<std::uint32_t>(Number(needed)));
  }
  if (const YAML::Node window = entry["window"]) {
    read.window = ReadWindow(window);
  }
  return read;
}

}  // namespace

core::Result<AchievementDatabase> AchievementDatabase::Load(Vfs& vfs) {
  auto bytes = vfs.Read(std::string(kAchievementDatabasePath));
  if (!bytes) {
    return std::unexpected(bytes.error());
  }
  const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
  return Parse(text);
}

core::Result<AchievementDatabase> AchievementDatabase::Parse(std::string_view yaml_text) {
  AchievementDatabase loaded;
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
    const YAML::Node achievements = whole["achievements"];
    if (!achievements || !achievements.IsMap()) {
      return std::unexpected(Refused("« achievements » n'est pas une table"));
    }
    for (const auto& pair : achievements) {
      const auto achievement_id =
          static_cast<std::uint32_t>(Whole(pair.first, 0, kMostAchievementNumber));
      loaded.entries_.push_back(ReadEntry(achievement_id, pair.second));
    }
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
  }
  std::ranges::sort(loaded.entries_, {}, &ServerAchievement::achievement_id);
  return loaded;
}

const ServerAchievement* AchievementDatabase::Find(std::uint32_t achievement_id) const {
  const auto found =
      std::ranges::lower_bound(entries_, achievement_id, {}, &ServerAchievement::achievement_id);
  if (found == entries_.end() || found->achievement_id != achievement_id) {
    return nullptr;
  }
  return &*found;
}

}  // namespace ro::vfs
