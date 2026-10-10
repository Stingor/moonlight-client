// Le titre de chaque succès. Voir l'en-tête : le serveur ne le dit jamais.

#include "ro/vfs/achievement_titles.h"

#include <algorithm>
#include <string>
#include <utility>

namespace ro::vfs {
namespace {

// Le titre d'une entrée, ou rien : elle n'a pas de récompense, sa récompense
// ne porte pas de titre, ou ce titre n'est pas un numéro entier.
[[nodiscard]] std::optional<std::int32_t> RewardTitle(const formats::LuaValue& entry) {
  if (entry.kind != formats::LuaValue::Kind::kTable || entry.table == nullptr) {
    return std::nullopt;
  }
  const formats::LuaValue* reward = entry.table->Find(kAchievementRewardField);
  if (reward == nullptr || reward->kind != formats::LuaValue::Kind::kTable ||
      reward->table == nullptr) {
    return std::nullopt;
  }
  const formats::LuaValue* title = reward->table->Find(kAchievementTitleField);
  if (title == nullptr || title->kind != formats::LuaValue::Kind::kNumber) {
    return std::nullopt;
  }
  const std::optional<std::uint32_t> whole =
      formats::LuaWholeNumber(title->number, kMostAchievementNumber);
  if (!whole.has_value()) {
    return std::nullopt;
  }
  return static_cast<std::int32_t>(*whole);
}

}  // namespace

core::Result<AchievementTitles> AchievementTitles::Load(Vfs& vfs) {
  auto bytes = vfs.Read(kAchievementTablePath);
  if (!bytes) {
    return std::unexpected(bytes.error());
  }
  // ⚠ LE FICHIER SE TERMINE PAR UNE FONCTION — celle qui, chez le client,
  // parcourt la table et déclare chaque récompense. Le lecteur la saute : la
  // donnée est dans la table, et c'est elle seule qu'on lit.
  auto read = formats::ReadLuaFile(*bytes, formats::LuaTable{});
  if (!read) {
    return std::unexpected(read.error());
  }
  return Parse(*read);
}

core::Result<AchievementTitles> AchievementTitles::Parse(const formats::LuaTable& globals) {
  const formats::LuaValue* table = globals.Find(kAchievementTableGlobal);
  if (table == nullptr || table->kind != formats::LuaValue::Kind::kTable ||
      table->table == nullptr) {
    return core::Fail(core::ErrorCode::kCorruptData, std::string(kAchievementTableGlobal) +
                                                         " n'est pas une table dans " +
                                                         std::string(kAchievementTablePath));
  }

  AchievementTitles loaded;
  for (const auto& [index, value] : table->table->by_index) {
    const std::optional<std::uint32_t> achievement_id =
        formats::LuaWholeNumber(index, kMostAchievementNumber);
    const std::optional<std::int32_t> title_id = RewardTitle(value);
    if (!achievement_id.has_value() || !title_id.has_value()) {
      continue;
    }
    loaded.entries_.push_back(Entry{*achievement_id, *title_id});
  }
  std::ranges::sort(loaded.entries_, {}, &Entry::achievement_id);
  return loaded;
}

std::optional<std::int32_t> AchievementTitles::TitleOf(std::uint32_t achievement_id) const {
  const auto found = std::ranges::lower_bound(entries_, achievement_id, {}, &Entry::achievement_id);
  if (found == entries_.end() || found->achievement_id != achievement_id) {
    return std::nullopt;
  }
  return found->title_id;
}

}  // namespace ro::vfs
