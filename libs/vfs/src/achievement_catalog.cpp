// Le catalogue des succès. Voir l'en-tête : la base du serveur, jointe à la
// table du client lue comme la lit sa fonction `main`, entrée par entrée.

#include "ro/vfs/achievement_catalog.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ro/vfs/encoding.h"

namespace ro::vfs {
namespace {

using formats::LuaTable;
using formats::LuaValue;

// Les champs d'une entrée de la table, tels que `main` les lit.
constexpr std::string_view kContentField = "content";
constexpr std::string_view kSummaryField = "summary";
constexpr std::string_view kDetailsField = "details";
constexpr std::string_view kGaugeField = "UI_Type";
constexpr std::string_view kGroupField = "group";
constexpr std::string_view kMajorField = "major";
constexpr std::string_view kMinorField = "minor";
constexpr std::string_view kObjectivesField = "resource";
constexpr std::string_view kObjectiveTextField = "text";
constexpr std::string_view kObjectiveShortcutField = "shortcut";
constexpr std::string_view kRewardField = "reward";
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

[[nodiscard]] std::optional<std::int32_t> Positive(std::int32_t number) {
  if (number <= 0) {
    return std::nullopt;
  }
  return number;
}

// LES OBJECTIFS, comme `main` les parcourt : par `ipairs`, donc de 1 en 1
// jusqu'au premier trou. Un objectif sans texte arrête la lecture du succès.
// Le compte et le succès désigné sont ceux du serveur, au même rang.
[[nodiscard]] bool ReadObjectives(const LuaTable& entry, const ServerAchievement& server,
                                  AchievementInfo& info) {
  const LuaTable* list = TableOf(entry.Find(kObjectivesField));
  if (list == nullptr) {
    return true;
  }
  for (double index = 1.0;; index += 1.0) {
    const LuaTable* objective = TableOf(list->Find(index));
    if (objective == nullptr) {
      return true;
    }
    const std::size_t rank = info.objectives.size();
    if (rank == kAchievementObjectiveCount) {
      return true;
    }
    std::optional<std::string> text = TextOf(*objective, kObjectiveTextField);
    if (!text.has_value()) {
      return false;
    }
    AchievementObjective read{.text = std::move(*text)};
    if (info.gauge == AchievementGauge::kCounts) {
      read.target = server.targets[rank].value_or(0);
    }
    if (objective->Find(kObjectiveShortcutField) != nullptr && rank < server.dependents.size()) {
      read.shortcut = server.dependents[rank];
    }
    info.objectives.push_back(std::move(read));
  }
}

// Ce que la table décrit d'un succès que le serveur connaît, ou rien si
// l'entrée est mal formée.
[[nodiscard]] std::optional<AchievementInfo> ReadEntry(const ServerAchievement& server,
                                                       const LuaTable& entry) {
  AchievementInfo info{
      .achievement_id = server.achievement_id, .name = server.name, .score = server.score};
  const LuaTable* content = TableOf(entry.Find(kContentField));
  if (content == nullptr) {
    return std::nullopt;
  }
  std::optional<std::string> summary = TextOf(*content, kSummaryField);
  std::optional<std::string> details = TextOf(*content, kDetailsField);
  if (!summary.has_value() || !details.has_value()) {
    return std::nullopt;
  }
  info.summary = std::move(*summary);
  info.details = std::move(*details);

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

  if (!ReadObjectives(entry, server, info)) {
    return std::nullopt;
  }
  info.reward_item = Positive(server.reward_item);
  info.reward_title = Positive(server.reward_title);
  info.reward_script = server.reward_script;
  if (const LuaTable* reward = TableOf(entry.Find(kRewardField)); reward != nullptr) {
    if (const std::optional<std::uint32_t> buff =
            WholeOf(*reward, kBuffField, kMostAchievementNumber);
        buff.has_value()) {
      info.reward_buff = static_cast<std::int32_t>(*buff);
    }
  }
  return info;
}

}  // namespace

core::Result<AchievementCatalog> AchievementCatalog::Load(Vfs& vfs,
                                                          const AchievementDatabase& database) {
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
  return Parse(database, *read);
}

core::Result<AchievementCatalog> AchievementCatalog::Parse(const AchievementDatabase& database,
                                                           const formats::LuaTable& globals) {
  const LuaTable* table = TableOf(globals.Find(kAchievementTableGlobal));
  if (table == nullptr) {
    return core::Fail(core::ErrorCode::kCorruptData, std::string(kAchievementTableGlobal) +
                                                         " n'est pas une table dans " +
                                                         std::string(kAchievementTablePath));
  }
  AchievementCatalog loaded;
  std::size_t described = 0;
  for (const auto& [index, value] : table->by_index) {
    const std::optional<std::uint32_t> achievement_id =
        formats::LuaWholeNumber(index, kMostAchievementNumber);
    const LuaTable* entry = TableOf(&value);
    if (!achievement_id.has_value() || entry == nullptr) {
      ++loaded.skipped_;
      continue;
    }
    const ServerAchievement* server = database.Find(*achievement_id);
    if (server == nullptr) {
      ++loaded.unknown_to_server_;
      continue;
    }
    ++described;
    std::optional<AchievementInfo> info = ReadEntry(*server, *entry);
    if (!info.has_value()) {
      ++loaded.skipped_;
      continue;
    }
    loaded.entries_.push_back(std::move(*info));
  }
  loaded.without_texts_ = database.entries().size() - described;
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
