// Les règles du journal des quêtes. Voir l'en-tête.

#include "quest/quest_rules.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <limits>

#include "ro/core/decimal_text.h"
#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::sim::QuestHunt;
using ro::sim::QuestRecord;

constexpr std::int64_t kSecondsPerMinute = 60;
constexpr std::int64_t kSecondsPerHour = 3600;
constexpr std::int64_t kSecondsPerDay = 86400;

// L'année et le mois de `std::tm` partent de 1900 et de zéro.
constexpr int kFirstYear = 1900;
constexpr int kFirstMonth = 1;
// « 2026/10/07 18:00 » : cinq champs de `std::tm`, que le tampon tient même
// à leur pire valeur.
constexpr std::size_t kDateBytes = 5 * ro::core::kDecimalTextBytes<int>;

// Le balisage du jeu : `^RRGGBB` change la couleur, `<TAG>texte<INFO>charge
// </INFO></TAG>` fait un lien dont seul le texte se lit.
constexpr char kColorMark = '^';
constexpr std::size_t kColorDigits = 6;
constexpr std::string_view kInfoOpen = "<INFO>";
constexpr std::string_view kInfoClose = "</INFO>";

[[nodiscard]] bool IsHexDigit(char letter) {
  return std::isxdigit(static_cast<unsigned char>(letter)) != 0;
}

[[nodiscard]] char Lower(char letter) {
  return static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
}

[[nodiscard]] bool LessByName(const QuestRecord* left, const QuestRecord* right,
                              const QuestCatalog* catalog) {
  const QuestInfo* left_info = catalog != nullptr ? catalog->Find(left->quest_id) : nullptr;
  const QuestInfo* right_info = catalog != nullptr ? catalog->Find(right->quest_id) : nullptr;
  const std::string_view left_title = left_info != nullptr ? left_info->title : std::string_view();
  const std::string_view right_title =
      right_info != nullptr ? right_info->title : std::string_view();
  const auto order = std::lexicographical_compare_three_way(
      left_title.begin(), left_title.end(), right_title.begin(), right_title.end(),
      [](char a, char b) { return Lower(a) <=> Lower(b); });
  if (order != 0) {
    return order < 0;
  }
  return left->quest_id < right->quest_id;
}

}  // namespace

bool IsWaitingQuest(const QuestInfo* info) { return info != nullptr && info->cooldown; }

bool QuestInTab(const QuestRecord& quest, const QuestInfo* info, ro::ui::QuestTab tab) {
  switch (tab) {
    case ro::ui::QuestTab::kActive:
      return quest.active && !IsWaitingQuest(info);
    case ro::ui::QuestTab::kWaiting:
      return quest.active && IsWaitingQuest(info);
    case ro::ui::QuestTab::kSuspended:
      return !quest.active;
    case ro::ui::QuestTab::kHistory:
    case ro::ui::QuestTab::kRecommended:
      return false;
  }
  return false;
}

// LA RECHERCHE IGNORE LA CASSE ASCII : un accent reste un accent, mais
// « poring » trouve « Poring ».
bool QuestTextContains(std::string_view haystack, std::string_view needle) {
  if (needle.empty()) {
    return true;
  }
  const auto found = std::ranges::search(
      haystack, needle, [](char left, char right) { return Lower(left) == Lower(right); });
  return !found.empty();
}

std::string QuestFill(std::string_view pattern, std::initializer_list<std::string_view> values) {
  std::string out;
  out.reserve(pattern.size());
  auto next = values.begin();
  for (std::size_t at = 0; at < pattern.size(); ++at) {
    if (pattern[at] == '{' && at + 1 < pattern.size() && pattern[at + 1] == '}' &&
        next != values.end()) {
      out.append(*next++);
      ++at;
      continue;
    }
    out.push_back(pattern[at]);
  }
  return out;
}

std::string HuntLabel(const QuestHunt& hunt) {
  std::string label = hunt.mob_name;
  const bool has_min = hunt.level_min > 0;
  const bool has_max = hunt.level_max > 0;
  if (has_min && has_max && hunt.level_min != hunt.level_max) {
    label +=
        QuestFill(" Lv.{}~Lv.{}", {std::to_string(hunt.level_min), std::to_string(hunt.level_max)});
  } else if (has_min || has_max) {
    label += QuestFill(" Lv.{}", {std::to_string(has_min ? hunt.level_min : hunt.level_max)});
  }
  return label;
}

QuestTimeLeft TimeLeftOf(std::uint32_t expires_unix_s, std::int64_t now_unix_s) {
  QuestTimeLeft left;
  if (expires_unix_s == 0) {
    return left;
  }
  const std::int64_t seconds = static_cast<std::int64_t>(expires_unix_s) - now_unix_s;
  if (seconds <= 0) {
    left.expired = true;
    left.text = ro::core::Tr("expirée");
    return left;
  }
  left.urgent = seconds < kQuestUrgentSeconds;
  const std::int64_t days = seconds / kSecondsPerDay;
  const std::int64_t hours = seconds % kSecondsPerDay / kSecondsPerHour;
  const std::int64_t minutes = seconds % kSecondsPerHour / kSecondsPerMinute;
  if (days > 0) {
    left.text = QuestFill(ro::core::Tr("{} j {} h"), {std::to_string(days), std::to_string(hours)});
  } else if (hours > 0) {
    left.text =
        QuestFill(ro::core::Tr("{} h {} min"), {std::to_string(hours), std::to_string(minutes)});
  } else if (minutes > 0) {
    left.text = QuestFill(ro::core::Tr("{} min"), {std::to_string(minutes)});
  } else {
    left.text = ro::core::Tr("moins d'une minute");
  }
  return left;
}

QuestTimeLeft WaitLeftOf(std::uint32_t expires_unix_s, std::int64_t now_unix_s) {
  QuestTimeLeft left = TimeLeftOf(expires_unix_s, now_unix_s);
  left.urgent = false;
  if (left.expired) {
    left.text = ro::core::Tr("prête");
  }
  return left;
}

std::string QuestDateText(std::int64_t unix_s) {
  if (unix_s <= 0) {
    return {};
  }
  const auto seconds = static_cast<std::time_t>(unix_s);
  std::tm local{};
#ifdef _WIN32
  if (localtime_s(&local, &seconds) != 0) {
    return {};
  }
#else
  if (localtime_r(&seconds, &local) == nullptr) {
    return {};
  }
#endif
  std::array<char, kDateBytes> text{};
  std::snprintf(text.data(), text.size(), "%04d/%02d/%02d %02d:%02d", local.tm_year + kFirstYear,
                local.tm_mon + kFirstMonth, local.tm_mday, local.tm_hour, local.tm_min);
  return text.data();
}

float QuestProgress(const QuestRecord& quest) {
  std::int64_t killed = 0;
  std::int64_t total = 0;
  for (const QuestHunt& hunt : quest.hunts) {
    if (hunt.total <= 0) {
      continue;
    }
    total += hunt.total;
    killed += std::clamp<std::int64_t>(hunt.killed, 0, hunt.total);
  }
  if (total == 0) {
    return -1.0F;
  }
  return static_cast<float>(killed) / static_cast<float>(total);
}

bool ObjectivesMet(const QuestRecord& quest) {
  return !quest.hunts.empty() &&
         std::ranges::all_of(quest.hunts, [](const QuestHunt& hunt) { return hunt.done(); });
}

std::string PlainQuestText(std::string_view marked) {
  std::string plain;
  plain.reserve(marked.size());
  for (std::size_t at = 0; at < marked.size();) {
    if (marked[at] == kColorMark && at + kColorDigits < marked.size() &&
        std::all_of(marked.begin() + static_cast<std::ptrdiff_t>(at + 1),
                    marked.begin() + static_cast<std::ptrdiff_t>(at + 1 + kColorDigits),
                    IsHexDigit)) {
      at += kColorDigits + 1;
      continue;
    }
    if (marked.substr(at).starts_with(kInfoOpen)) {
      // La charge d'un lien ne se lit pas : on saute jusqu'à sa fermeture.
      const std::size_t closing = marked.find(kInfoClose, at);
      at = closing == std::string_view::npos ? marked.size() : closing + kInfoClose.size();
      continue;
    }
    if (marked[at] == '<') {
      const std::size_t closing = marked.find('>', at);
      if (closing != std::string_view::npos) {
        at = closing + 1;
        continue;
      }
    }
    plain.push_back(marked[at]);
    ++at;
  }
  return plain;
}

bool QuestMatches(const QuestRecord& quest, const QuestInfo* info, std::string_view needle) {
  if (needle.empty()) {
    return true;
  }
  if (info != nullptr) {
    if (QuestTextContains(info->title, needle) || QuestTextContains(info->summary, needle) ||
        QuestTextContains(PlainQuestText(info->description), needle)) {
      return true;
    }
    if (info->npc.has_value() && QuestTextContains(info->npc->map_name, needle)) {
      return true;
    }
  }
  return std::ranges::any_of(quest.hunts, [needle](const QuestHunt& hunt) {
    return QuestTextContains(hunt.mob_name, needle);
  });
}

std::string_view QuestGroupKey(const QuestInfo* info, const QuestCatalog* catalog) {
  if (info == nullptr || catalog == nullptr) {
    return kQuestOtherCategory;
  }
  return catalog->CategoryOf(*info);
}

bool QuestInCategory(const QuestInfo* info, const QuestCatalog* catalog,
                     std::string_view category_key) {
  if (category_key.empty() || category_key == kQuestAllCategory) {
    return true;
  }
  return QuestGroupKey(info, catalog) == category_key;
}

std::vector<const QuestRecord*> QuestsOfTab(std::span<const QuestRecord> quests,
                                            const QuestCatalog* catalog, const QuestQuery& query,
                                            const QuestSortFacts& facts, std::int64_t now_unix_s) {
  std::vector<const QuestRecord*> rows;
  for (const QuestRecord& quest : quests) {
    const QuestInfo* info = catalog != nullptr ? catalog->Find(quest.quest_id) : nullptr;
    if (!QuestInTab(quest, info, query.tab)) {
      continue;
    }
    if (!QuestInCategory(info, catalog, query.category) ||
        !QuestMatches(quest, info, query.search)) {
      continue;
    }
    rows.push_back(&quest);
  }
  const auto received = [&facts](const QuestRecord* quest) {
    const auto found = facts.received_unix_s.find(quest->quest_id);
    return found == facts.received_unix_s.end() ? std::int64_t{0} : found->second;
  };
  switch (query.sort) {
    case ro::ui::QuestSort::kRecent:
      std::ranges::stable_sort(rows,
                               [&received](const QuestRecord* left, const QuestRecord* right) {
                                 return received(left) > received(right);
                               });
      break;
    case ro::ui::QuestSort::kTimeLeft: {
      // Une quête sans limite va au bout ; une échue garde sa place en tête.
      const auto deadline = [now_unix_s](const QuestRecord* quest) {
        return quest->expires_unix_s == 0
                   ? std::numeric_limits<std::int64_t>::max()
                   : static_cast<std::int64_t>(quest->expires_unix_s) - now_unix_s;
      };
      std::ranges::stable_sort(rows, std::ranges::less{}, deadline);
      break;
    }
    case ro::ui::QuestSort::kName:
      std::ranges::stable_sort(rows, [catalog](const QuestRecord* left, const QuestRecord* right) {
        return LessByName(left, right, catalog);
      });
      break;
    case ro::ui::QuestSort::kProgress:
      std::ranges::stable_sort(rows, std::ranges::greater{},
                               [](const QuestRecord* quest) { return QuestProgress(*quest); });
      break;
  }
  return rows;
}

std::vector<const QuestRecord*> TrackedQuests(std::span<const QuestRecord> quests,
                                              std::span<const std::uint32_t> tracked,
                                              const QuestCatalog* catalog,
                                              std::string_view current_map,
                                              bool current_map_first) {
  std::vector<const QuestRecord*> shown;
  for (const QuestRecord& quest : quests) {
    const QuestInfo* info = catalog != nullptr ? catalog->Find(quest.quest_id) : nullptr;
    if (quest.active && !IsWaitingQuest(info) &&
        std::ranges::find(tracked, quest.quest_id) != tracked.end()) {
      shown.push_back(&quest);
    }
  }
  if (current_map_first && !current_map.empty()) {
    const auto here = [catalog, current_map](const QuestRecord* quest) {
      const QuestInfo* info = catalog != nullptr ? catalog->Find(quest->quest_id) : nullptr;
      return info != nullptr && info->npc.has_value() && info->npc->map_name == current_map;
    };
    std::ranges::stable_partition(shown, here);
  }
  return shown;
}

}  // namespace moonclient
