// La recherche dans la base de données du serveur. Voir l'en-tête.

#include "ro/vfs/server_db_search.h"

#include <algorithm>
#include <cctype>
#include <charconv>

#include "ro/core/search_key.h"

namespace ro::vfs {
namespace {

[[nodiscard]] bool SameText(std::string_view left, std::string_view right) {
  return left.size() == right.size() &&
         std::ranges::equal(left, right, [](unsigned char a, unsigned char b) {
           return std::tolower(a) == std::tolower(b);
         });
}

// La saisie, nettoyée de ses blancs autour.
[[nodiscard]] std::string_view Trimmed(std::string_view text) {
  const auto blank = [](unsigned char letter) { return std::isspace(letter) != 0; };
  while (!text.empty() && blank(static_cast<unsigned char>(text.front()))) {
    text.remove_prefix(1);
  }
  while (!text.empty() && blank(static_cast<unsigned char>(text.back()))) {
    text.remove_suffix(1);
  }
  return text;
}

[[nodiscard]] bool WithinLevels(int level, int least, int most) {
  return (least == kNoLevelBound || level >= least) && (most == kNoLevelBound || level <= most);
}

[[nodiscard]] bool KeyMatches(std::span<const std::string> keys, std::size_t rank,
                              std::string_view needle_key) {
  if (needle_key.empty()) {
    return true;
  }
  return rank < keys.size() && core::SearchKeyContains(keys[rank], needle_key);
}

}  // namespace

std::optional<std::uint32_t> SearchedId(std::string_view text) {
  text = Trimmed(text);
  if (text.empty() ||
      !std::ranges::all_of(text, [](unsigned char letter) { return std::isdigit(letter) != 0; })) {
    return std::nullopt;
  }
  std::uint32_t id = 0;
  const auto [end, failure] = std::from_chars(text.data(), text.data() + text.size(), id);
  if (failure != std::errc() || end != text.data() + text.size()) {
    return std::nullopt;
  }
  return id;
}

std::vector<std::size_t> SearchItems(const ServerDb& db, std::span<const std::string> name_keys,
                                     const ItemSearch& search) {
  std::vector<std::size_t> found;
  const std::span<const ServerItem> items = db.items();
  const std::optional<std::uint32_t> id = SearchedId(search.text);
  const std::string needle = id.has_value() ? std::string() : core::SearchKey(Trimmed(search.text));
  for (std::size_t rank = 0; rank < items.size(); ++rank) {
    const ServerItem& item = items[rank];
    if (id.has_value() ? item.item_id != *id : !KeyMatches(name_keys, rank, needle)) {
      continue;
    }
    if (!search.type.empty() && !SameText(item.type, search.type)) {
      continue;
    }
    if (!search.locations.empty() &&
        std::ranges::none_of(item.locations, [&](const std::string& raised) {
          return std::ranges::any_of(
              search.locations, [&](std::string_view wanted) { return SameText(raised, wanted); });
        })) {
      continue;
    }
    if (!WithinLevels(item.equip_level_min, search.level_min, search.level_max)) {
      continue;
    }
    found.push_back(rank);
  }
  return found;
}

std::vector<std::size_t> SearchMonsters(const ServerDb& db, std::span<const std::string> name_keys,
                                        const MonsterSearch& search) {
  std::vector<std::size_t> found;
  const std::span<const ServerMonster> monsters = db.monsters();
  const std::optional<std::uint32_t> id = SearchedId(search.text);
  const std::string needle = id.has_value() ? std::string() : core::SearchKey(Trimmed(search.text));
  const auto wanted = [](int asked, int value) {
    return asked == kAnyMonsterTrait || asked == value;
  };
  for (std::size_t rank = 0; rank < monsters.size(); ++rank) {
    const ServerMonster& monster = monsters[rank];
    if (id.has_value() ? monster.monster_id != *id : !KeyMatches(name_keys, rank, needle)) {
      continue;
    }
    if (!wanted(search.race, monster.race) || !wanted(search.element, monster.element) ||
        !wanted(search.size, monster.size) || !wanted(search.rank, monster.rank) ||
        !WithinLevels(monster.level, search.level_min, search.level_max)) {
      continue;
    }
    found.push_back(rank);
  }
  return found;
}

}  // namespace ro::vfs
