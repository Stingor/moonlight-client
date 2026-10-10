// Le catalogue des succès. Voir l'en-tête : la lecture refait la fonction
// `main` du fichier, entrée par entrée.

#include "ro/vfs/achievement_catalog.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ro/vfs/achievement_titles.h"
#include "ro/vfs/encoding.h"

namespace ro::vfs {
namespace {

using formats::LuaTable;
using formats::LuaValue;

// Les champs d'une entrée, tels que `main` les lit.
constexpr std::string_view kNameField = "title";
constexpr std::string_view kContentField = "content";
constexpr std::string_view kSummaryField = "summary";
constexpr std::string_view kDetailsField = "details";
constexpr std::string_view kScoreField = "score";
constexpr std::string_view kGaugeField = "UI_Type";
constexpr std::string_view kGroupField = "group";
constexpr std::string_view kMajorField = "major";
constexpr std::string_view kMinorField = "minor";
constexpr std::string_view kObjectivesField = "resource";
constexpr std::string_view kObjectiveTextField = "text";
constexpr std::string_view kObjectiveCountField = "count";
constexpr std::string_view kObjectiveShortcutField = "shortcut";
constexpr std::string_view kItemField = "item";
constexpr std::string_view kBuffField = "buff";

// Un onglet et un sous-onglet tiennent sur un octet : la fenêtre en range six
// et, sous chacun, moins de dix.
constexpr std::uint32_t kMostTabNumber = std::numeric_limits<std::uint8_t>::max();

[[nodiscard]] const LuaTable* TableOf(const LuaValue* value) {
  if (value == nullptr || value->kind != LuaValue::Kind::kTable) {
    return nullptr;
  }
  return value->table.get();
}

[[nodiscard]] std::optional<std::string> TextOf(const LuaTable& table, std::string_view field) {
  const LuaValue* value = table.Find(field);
  if (value == nullptr || value->kind != LuaValue::Kind::kString) {
    return std::nullopt;
  }
  return Cp949ToUtf8Lossy(value->text);
}

[[nodiscard]] std::optional<std::uint32_t> WholeOf(const LuaTable& table, std::string_view field,
                                                   std::uint32_t most) {
  const LuaValue* value = table.Find(field);
  if (value == nullptr || value->kind != LuaValue::Kind::kNumber) {
    return std::nullopt;
  }
  return formats::LuaWholeNumber(value->number, most);
}

[[nodiscard]] std::optional<std::int32_t> NumberOf(const LuaTable& table, std::string_view field) {
  const std::optional<std::uint32_t> whole = WholeOf(table, field, kMostAchievementNumber);
  if (!whole.has_value()) {
    return std::nullopt;
  }
  return static_cast<std::int32_t>(*whole);
}

// LES OBJECTIFS, comme `main` les parcourt : par `ipairs`, donc de 1 en 1
// jusqu'au premier trou. Un objectif sans texte arrête la lecture du succès.
[[nodiscard]] bool ReadObjectives(const LuaTable& entry, AchievementInfo& info) {
  const LuaTable* list = TableOf(entry.Find(kObjectivesField));
  if (list == nullptr) {
    return true;
  }
  for (double index = 1.0;; index += 1.0) {
    const LuaTable* objective = TableOf(list->Find(index));
    if (objective == nullptr) {
      return true;
    }
    if (info.objectives.size() == kAchievementObjectiveCount) {
      return true;
    }
    std::optional<std::string> text = TextOf(*objective, kObjectiveTextField);
    if (!text.has_value()) {
      return false;
    }
    AchievementObjective read{.text = std::move(*text)};
    if (info.gauge == AchievementGauge::kCounts) {
      read.target =
          WholeOf(*objective, kObjectiveCountField, std::numeric_limits<std::uint32_t>::max())
              .value_or(0);
    }
    read.shortcut = WholeOf(*objective, kObjectiveShortcutField, kMostAchievementNumber);
    info.objectives.push_back(std::move(read));
  }
}

[[nodiscard]] std::optional<AchievementInfo> ReadEntry(std::uint32_t achievement_id,
                                                       const LuaTable& entry) {
  AchievementInfo info{.achievement_id = achievement_id};
  const LuaTable* content = TableOf(entry.Find(kContentField));
  std::optional<std::string> name = TextOf(entry, kNameField);
  const std::optional<std::int32_t> score = NumberOf(entry, kScoreField);
  if (content == nullptr || !name.has_value() || !score.has_value()) {
    return std::nullopt;
  }
  std::optional<std::string> summary = TextOf(*content, kSummaryField);
  std::optional<std::string> details = TextOf(*content, kDetailsField);
  if (!summary.has_value() || !details.has_value()) {
    return std::nullopt;
  }
  info.name = std::move(*name);
  info.summary = std::move(*summary);
  info.details = std::move(*details);
  info.score = *score;

  const std::optional<std::uint32_t> gauge = WholeOf(entry, kGaugeField, kMostTabNumber);
  if (!gauge.has_value() || *gauge > static_cast<std::uint32_t>(AchievementGauge::kCounts)) {
    return std::nullopt;
  }
  info.gauge = static_cast<AchievementGauge>(*gauge);
  info.group = TextOf(entry, kGroupField).value_or(std::string());

  const std::optional<std::uint32_t> major = WholeOf(entry, kMajorField, kMostTabNumber);
  const std::optional<std::uint32_t> minor = WholeOf(entry, kMinorField, kMostTabNumber);
  if (!major.has_value() || !minor.has_value()) {
    return std::nullopt;
  }
  info.major = static_cast<std::uint8_t>(*major);
  info.minor = static_cast<std::uint8_t>(*minor);

  if (!ReadObjectives(entry, info)) {
    return std::nullopt;
  }
  if (const LuaTable* reward = TableOf(entry.Find(kAchievementRewardField)); reward != nullptr) {
    info.reward_item = NumberOf(*reward, kItemField);
    info.reward_title = NumberOf(*reward, kAchievementTitleField);
    info.reward_buff = NumberOf(*reward, kBuffField);
  }
  return info;
}

}  // namespace

core::Result<AchievementCatalog> AchievementCatalog::Load(Vfs& vfs) {
  auto bytes = vfs.Read(kAchievementTablePath);
  if (!bytes) {
    return std::unexpected(bytes.error());
  }
  // ⚠ LE FICHIER SE TERMINE PAR `main` : le lecteur la saute, la donnée est
  // dans la table.
  auto read = formats::ReadLuaFile(*bytes, formats::LuaTable{});
  if (!read) {
    return std::unexpected(read.error());
  }
  return Parse(*read);
}

core::Result<AchievementCatalog> AchievementCatalog::Parse(const formats::LuaTable& globals) {
  const LuaTable* table = TableOf(globals.Find(kAchievementTableGlobal));
  if (table == nullptr) {
    return core::Fail(core::ErrorCode::kCorruptData, std::string(kAchievementTableGlobal) +
                                                         " n'est pas une table dans " +
                                                         std::string(kAchievementTablePath));
  }
  AchievementCatalog loaded;
  for (const auto& [index, value] : table->by_index) {
    const std::optional<std::uint32_t> achievement_id =
        formats::LuaWholeNumber(index, kMostAchievementNumber);
    const LuaTable* entry = TableOf(&value);
    std::optional<AchievementInfo> info;
    if (achievement_id.has_value() && entry != nullptr) {
      info = ReadEntry(*achievement_id, *entry);
    }
    if (!info.has_value()) {
      ++loaded.skipped_;
      continue;
    }
    loaded.entries_.push_back(std::move(*info));
  }
  std::ranges::sort(loaded.entries_, {}, &AchievementInfo::achievement_id);
  return loaded;
}

const AchievementInfo* AchievementCatalog::Find(std::uint32_t achievement_id) const {
  const auto found =
      std::ranges::lower_bound(entries_, achievement_id, {}, &AchievementInfo::achievement_id);
  if (found == entries_.end() || found->achievement_id != achievement_id) {
    return nullptr;
  }
  return &*found;
}

}  // namespace ro::vfs
