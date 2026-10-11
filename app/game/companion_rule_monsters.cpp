#include "game/companion_rule_monsters.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string_view>

#include "ro/core/case_fold.h"
#include "ro/core/i18n.h"

namespace moonclient {
namespace {

constexpr std::size_t kLevelBytes = 32;
constexpr std::string_view kSeparator = " · ";

void Append(std::string& detail, std::string_view part) {
  if (part.empty()) {
    return;
  }
  if (!detail.empty()) {
    detail += kSeparator;
  }
  detail += part;
}

}  // namespace

std::vector<std::uint16_t> MonstersByName(const ro::vfs::MonsterList& list) {
  struct Keyed {
    std::string folded_name;
    std::uint16_t monster_id = 0;
  };
  std::vector<Keyed> keyed;
  keyed.reserve(list.entries().size());
  for (const ro::vfs::MonsterListEntry& entry : list.entries()) {
    keyed.push_back(
        Keyed{.folded_name = ro::core::CaseFolded(entry.name), .monster_id = entry.monster_id});
  }
  std::ranges::sort(keyed, [](const Keyed& left, const Keyed& right) {
    return left.folded_name != right.folded_name ? left.folded_name < right.folded_name
                                                 : left.monster_id < right.monster_id;
  });
  std::vector<std::uint16_t> ordered;
  ordered.reserve(keyed.size());
  for (const Keyed& one : keyed) {
    ordered.push_back(one.monster_id);
  }
  return ordered;
}

std::string MonsterListDetail(const ro::vfs::MonsterListEntry& entry, const RuleNames& names) {
  std::string detail;
  std::array<char, kLevelBytes> level{};
  std::snprintf(level.data(), level.size(), ro::core::Tr("niv. %u"),
                static_cast<unsigned>(entry.level));
  Append(detail, level.data());
  if (entry.rank == ro::vfs::kMonsterMvpRank) {
    Append(detail, ro::core::Tr("MVP"));
  } else if (entry.rank == ro::vfs::kMonsterBossRank) {
    Append(detail, ro::core::Tr("Mini-boss"));
  }
  if (names.race) {
    Append(detail, names.race(entry.race));
  }
  if (names.element) {
    const std::string element = names.element(entry.element);
    Append(detail, element.empty() ? element : element + ' ' + std::to_string(entry.element_level));
  }
  if (names.size) {
    Append(detail, names.size(entry.size));
  }
  return detail;
}

}  // namespace moonclient
