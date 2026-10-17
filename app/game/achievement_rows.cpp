// Les règles de la fenêtre des succès. Voir l'en-tête.

#include "game/achievement_rows.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <string_view>

#include "ro/core/ascii.h"
#include "ro/core/decimal_text.h"
#include "ro/core/local_time.h"

namespace moonclient {
namespace {

using ro::sim::AchievementBook;
using ro::sim::AchievementProgress;
using ro::vfs::AchievementCatalog;
using ro::vfs::AchievementGauge;
using ro::vfs::AchievementInfo;

// La ligne d'un objectif dans le panneau de suivi ne dépasse pas ces octets.
constexpr std::size_t kTrackerLineBytes = 40;
constexpr std::string_view kTrackerBullet = "- ";
constexpr std::string_view kTrackerEllipsis = "...";

// `tm_year` compte depuis 1900, `tm_mon` depuis zéro.
constexpr int kFirstYear = 1900;
constexpr int kFirstMonth = 1;
// « 2026.10. 7 » : trois champs de `std::tm`, que le tampon tient même à leur
// pire valeur.
constexpr std::size_t kDateBytes = 3 * ro::core::kDecimalTextBytes<int>;
constexpr std::size_t kCountBytes = 32;

// L'octet qui CONTINUE un caractère UTF-8 : 10xxxxxx.
constexpr unsigned char kContinuationMask = 0xC0;
constexpr unsigned char kContinuationMark = 0x80;

[[nodiscard]] bool Completed(const AchievementProgress* progress) {
  return progress != nullptr && progress->completed;
}

// Une récompense attend qu'on la réclame.
[[nodiscard]] bool Unclaimed(const AchievementInfo& info, const AchievementProgress* progress) {
  return info.rewards() && Completed(progress) && !progress->rewarded;
}

// Le texte tronqué à `bytes` octets au plus, sur une frontière de caractère.
[[nodiscard]] std::string_view CutAt(std::string_view text, std::size_t bytes) {
  if (text.size() <= bytes) {
    return text;
  }
  std::size_t end = bytes;
  while (end > 0 &&
         (static_cast<unsigned char>(text[end]) & kContinuationMask) == kContinuationMark) {
    --end;
  }
  return text.substr(0, end);
}

// Une lettre latine en minuscule ; un octet non ASCII passe tel quel, et c'est
// assez pour chercher : les textes du catalogue sont en anglais.
[[nodiscard]] char Folded(char letter) {
  return static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
}

[[nodiscard]] bool FoldedLess(std::string_view left, std::string_view right) {
  return std::ranges::lexicographical_compare(
      left, right, [](char first, char second) { return Folded(first) < Folded(second); });
}

}  // namespace

bool InAchievementTab(const AchievementInfo& info, int tab, int subtab) {
  if (tab == ro::ui::kAchievementSummaryTab || tab == ro::ui::kAchievementAllTab) {
    return true;
  }
  if (ro::ui::AchievementTabOfMajor(info.major) != tab) {
    return false;
  }
  return subtab == ro::ui::kAchievementWholeTab || info.minor == subtab;
}

ro::ui::AchievementTally TallyOfTab(const AchievementCatalog& catalog, const AchievementBook& book,
                                    int tab, int subtab) {
  ro::ui::AchievementTally tally;
  for (const AchievementInfo& info : catalog.entries()) {
    if (!InAchievementTab(info, tab, subtab)) {
      continue;
    }
    ++tally.total;
    if (Completed(book.Find(info.achievement_id))) {
      ++tally.done;
    }
  }
  return tally;
}

bool MatchesAchievementSearch(const AchievementInfo& info, std::string_view search) {
  if (search.empty()) {
    return true;
  }
  if (ro::core::AsciiContainsFolded(info.name, search) ||
      ro::core::AsciiContainsFolded(info.summary, search)) {
    return true;
  }
  return std::ranges::any_of(info.objectives, [search](const ro::vfs::AchievementObjective& line) {
    return ro::core::AsciiContainsFolded(line.text, search);
  });
}

double AchievementProgressShare(const AchievementInfo& info, const AchievementProgress* progress) {
  if (Completed(progress)) {
    return 1.0;
  }
  if (info.objectives.empty()) {
    return 0.0;
  }
  double sum = 0.0;
  for (std::size_t index = 0; index < info.objectives.size(); ++index) {
    const std::uint32_t count = ObjectiveCount(progress, index);
    const std::uint32_t target = info.objectives[index].target;
    if (info.gauge == AchievementGauge::kCounts && target > 0) {
      sum += std::min(1.0, static_cast<double>(count) / static_cast<double>(target));
    } else if (count > 0) {
      sum += 1.0;
    }
  }
  return sum / static_cast<double>(info.objectives.size());
}

std::vector<const AchievementInfo*> RowsOfTab(const AchievementCatalog& catalog,
                                              const AchievementBook& book,
                                              const AchievementQuery& query) {
  std::vector<const AchievementInfo*> done;
  std::vector<const AchievementInfo*> rest;
  const bool searching = !query.search.empty();
  for (const AchievementInfo& info : catalog.entries()) {
    // ⚠ UNE RECHERCHE TRAVERSE LES ONGLETS : on cherche un succès dont on ne
    // sait pas où il se range.
    if (searching ? !MatchesAchievementSearch(info, query.search)
                  : !InAchievementTab(info, query.tab, query.subtab)) {
      continue;
    }
    const AchievementProgress* progress = book.Find(info.achievement_id);
    switch (query.filter) {
      case ro::ui::AchievementFilter::kAll:
        break;
      case ro::ui::AchievementFilter::kUnclaimed:
        if (Completed(progress) && !Unclaimed(info, progress)) {
          continue;
        }
        break;
      case ro::ui::AchievementFilter::kInProgress:
        if (Completed(progress) || AchievementProgressShare(info, progress) <= 0.0) {
          continue;
        }
        break;
      case ro::ui::AchievementFilter::kTracked:
        if (!book.IsTracked(info.achievement_id)) {
          continue;
        }
        break;
    }
    (Completed(progress) ? done : rest).push_back(&info);
  }
  // Le tri est STABLE : deux succès obtenus à la même seconde gardent l'ordre
  // des numéros.
  std::ranges::stable_sort(done, {}, [&book](const AchievementInfo* info) {
    return book.Find(info->achievement_id)->completed_at;
  });
  done.insert(done.end(), rest.begin(), rest.end());

  // LES TRIS À NOUS reprennent l'ordre du jeu pour départager deux égaux.
  switch (query.sort) {
    case ro::ui::AchievementSort::kGame:
      break;
    case ro::ui::AchievementSort::kProgress:
      std::ranges::stable_sort(done, std::ranges::greater{}, [&book](const AchievementInfo* info) {
        return AchievementProgressShare(*info, book.Find(info->achievement_id));
      });
      break;
    case ro::ui::AchievementSort::kScore:
      std::ranges::stable_sort(done, std::ranges::greater{}, &AchievementInfo::score);
      break;
    case ro::ui::AchievementSort::kName:
      std::ranges::stable_sort(done, [](const AchievementInfo* left, const AchievementInfo* right) {
        return FoldedLess(left->name, right->name);
      });
      break;
  }
  return done;
}

std::vector<const AchievementInfo*> RowsOfTab(const AchievementCatalog& catalog,
                                              const AchievementBook& book, int tab, int subtab,
                                              ro::ui::AchievementFilter filter) {
  return RowsOfTab(catalog, book, AchievementQuery{.tab = tab, .subtab = subtab, .filter = filter});
}

std::vector<std::uint32_t> ClaimableAchievements(const AchievementCatalog& catalog,
                                                 const AchievementBook& book) {
  std::vector<std::uint32_t> claimable;
  for (const ro::sim::AchievementRecord& record : book.records()) {
    const AchievementInfo* info = catalog.Find(record.achievement_id);
    if (info != nullptr && Unclaimed(*info, &record.progress)) {
      claimable.push_back(record.achievement_id);
    }
  }
  return claimable;
}

std::vector<const AchievementInfo*> MostRecentlyCompleted(const AchievementCatalog& catalog,
                                                          const AchievementBook& book) {
  std::vector<const AchievementInfo*> done;
  for (const ro::sim::AchievementRecord& record : book.records()) {
    if (!record.progress.completed) {
      continue;
    }
    if (const AchievementInfo* info = catalog.Find(record.achievement_id); info != nullptr) {
      done.push_back(info);
    }
  }
  std::ranges::stable_sort(done, std::ranges::greater{}, [&book](const AchievementInfo* info) {
    return book.Find(info->achievement_id)->completed_at;
  });
  if (done.size() > ro::ui::kAchievementRecentCount) {
    done.resize(ro::ui::kAchievementRecentCount);
  }
  return done;
}

std::string AchievementDateText(std::uint32_t unix_s) {
  if (unix_s == 0) {
    return {};
  }
  const auto seconds = static_cast<std::time_t>(unix_s);
  std::tm local{};
  if (!ro::core::ToLocalTime(seconds, local)) {
    return {};
  }
  std::array<char, kDateBytes> text{};
  std::snprintf(text.data(), text.size(), "%4d.%2d.%2d", local.tm_year + kFirstYear,
                local.tm_mon + kFirstMonth, local.tm_mday);
  return text.data();
}

std::uint32_t ObjectiveCount(const AchievementProgress* progress, std::size_t index) {
  if (progress == nullptr || index >= progress->counts.size()) {
    return 0;
  }
  return progress->counts[index];
}

bool ObjectiveReached(const AchievementInfo& info, std::size_t index,
                      const AchievementProgress* progress) {
  const std::uint32_t count = ObjectiveCount(progress, index);
  if (info.gauge == AchievementGauge::kCounts) {
    return index < info.objectives.size() && count == info.objectives[index].target;
  }
  return count > 0;
}

ro::ui::AchievementObjectiveState ObjectiveStateOf(const AchievementInfo& info, std::size_t index,
                                                   const AchievementProgress* progress) {
  if (ObjectiveReached(info, index, progress)) {
    return ro::ui::AchievementObjectiveState::kReached;
  }
  if (ObjectiveCount(progress, index) == 0) {
    return ro::ui::AchievementObjectiveState::kUntouched;
  }
  return ro::ui::AchievementObjectiveState::kStarted;
}

std::string ObjectiveText(const AchievementInfo& info, std::size_t index,
                          const AchievementProgress* progress) {
  if (index >= info.objectives.size()) {
    return {};
  }
  const ro::vfs::AchievementObjective& objective = info.objectives[index];
  if (info.gauge != AchievementGauge::kCounts) {
    return objective.text;
  }
  std::array<char, kCountBytes> counts{};
  std::snprintf(counts.data(), counts.size(), " ( %u / %u )",
                static_cast<unsigned>(ObjectiveCount(progress, index)),
                static_cast<unsigned>(objective.target));
  return objective.text + counts.data();
}

std::string TrackerLineText(const AchievementInfo& info, std::size_t index,
                            const AchievementProgress* progress) {
  if (index >= info.objectives.size()) {
    return {};
  }
  const std::string_view label = info.objectives[index].text;
  std::string tail;
  if (info.gauge == AchievementGauge::kCounts) {
    std::array<char, kCountBytes> counts{};
    std::snprintf(counts.data(), counts.size(), "(%u/%u)",
                  static_cast<unsigned>(ObjectiveCount(progress, index)),
                  static_cast<unsigned>(info.objectives[index].target));
    tail = counts.data();
  }
  std::string line(kTrackerBullet);
  if (label.size() + tail.size() <= kTrackerLineBytes) {
    line += label;
    if (!tail.empty()) {
      line += ' ';
      line += tail;
    }
    return line;
  }
  const std::size_t room = kTrackerLineBytes > tail.size() ? kTrackerLineBytes - tail.size() : 0;
  line += CutAt(label, room);
  line += kTrackerEllipsis;
  line += tail;
  return line;
}

}  // namespace moonclient
