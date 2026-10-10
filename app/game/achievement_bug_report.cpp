#include "game/achievement_bug_report.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <format>
#include <string>

#include "ro/core/i18n.h"

#include "game/achievement_rows.h"

namespace moonclient {
namespace {

using ro::ui::AchievementFilter;
using ro::ui::AchievementSort;
using ro::vfs::AchievementInfo;

// Le libellé d'un rapport — « Succès : Premier sacrifice (#900001) » —, jamais
// plus long.
constexpr std::size_t kLabelBytes = 192;

[[nodiscard]] const char* FilterWord(AchievementFilter filter) {
  switch (filter) {
    case AchievementFilter::kAll:
      return "all";
    case AchievementFilter::kUnclaimed:
      return "unclaimed";
    case AchievementFilter::kInProgress:
      return "in_progress";
    case AchievementFilter::kTracked:
      return "tracked";
  }
  return "?";
}

[[nodiscard]] const char* SortWord(AchievementSort sort) {
  switch (sort) {
    case AchievementSort::kGame:
      return "game";
    case AchievementSort::kProgress:
      return "progress";
    case AchievementSort::kScore:
      return "score";
    case AchievementSort::kName:
      return "name";
  }
  return "?";
}

[[nodiscard]] const char* Flag(bool value) { return value ? "true" : "false"; }

// Une récompense, ou `null` quand le catalogue n'en donne pas.
[[nodiscard]] std::string Reward(const std::optional<std::int32_t>& reward) {
  return reward.has_value() ? std::to_string(*reward) : std::string("null");
}

// La fiche du succès choisi : le catalogue, puis ce que le serveur en a dit.
[[nodiscard]] std::string SelectedFields(const AchievementInfo& info,
                                         const ro::sim::AchievementBook& book) {
  std::string fields = std::format(
      R"("achievement":{{"achievement_id":{},"name":"{}","group":"{}","major":{},"minor":{},)"
      R"("score":{},"counted":{},"reward":{{"item":{},"title":{},"buff":{}}},"objectives":[)",
      info.achievement_id, JsonEscaped(info.name), JsonEscaped(info.group), info.major, info.minor,
      info.score, Flag(info.gauge == ro::vfs::AchievementGauge::kCounts), Reward(info.reward_item),
      Reward(info.reward_title), Reward(info.reward_buff));
  const ro::sim::AchievementProgress* progress = book.Find(info.achievement_id);
  const std::size_t shown = std::min(info.objectives.size(), ro::sim::kAchievementCounterCount);
  for (std::size_t index = 0; index < shown; ++index) {
    const std::uint32_t count = progress != nullptr ? progress->counts[index] : 0;
    fields +=
        std::format(R"({}{{"text":"{}","count":{},"target":{}}})", index == 0 ? "" : ",",
                    JsonEscaped(info.objectives[index].text), count, info.objectives[index].target);
  }
  fields += ']';
  // ⚠ « known » DIT SI LE SERVEUR A PARLÉ DE CE SUCCÈS : sans lui, des
  // compteurs à zéro ne se distinguent pas d'un succès jamais commencé.
  if (progress == nullptr) {
    fields += R"(,"progress":{"known":false})";
  } else {
    fields += std::format(
        R"(,"progress":{{"known":true,"completed":{},"completed_at":{},"rewarded":{}}})",
        Flag(progress->completed), progress->completed_at, Flag(progress->rewarded));
  }
  fields += std::format(R"(,"tracked":{},"claim_pending":{}}})",
                        Flag(book.IsTracked(info.achievement_id)),
                        Flag(book.ClaimPending(info.achievement_id)));
  return fields;
}

}  // namespace

BugReportContext AchievementBugReport(const AchievementBugScene& scene) {
  const ro::ui::AchievementWindowState& window = *scene.window;
  const ro::sim::AchievementBook& book = *scene.book;
  const ro::sim::AchievementStanding& standing = book.standing();
  const std::string_view search = window.search.data();

  std::string fields = std::format(
      R"("achievements":{{"view":"{}","tab":{},"subtab":{},"filter":"{}","sort":"{}",)"
      R"("search":"{}","rows":{},"standing":{{"total_points":{},"rank":{},"rank_points":{},)"
      R"("rank_span":{}}},"tracked":{})",
      scene.list_mode ? "list" : "summary", window.tab, window.subtab, FilterWord(window.filter),
      SortWord(window.sort), JsonEscaped(search), scene.rows, standing.total_points, standing.rank,
      standing.rank_points, standing.rank_span, book.tracked().size());

  const AchievementInfo* selected = nullptr;
  if (scene.list_mode && scene.catalog != nullptr && window.selected.has_value()) {
    selected = scene.catalog->Find(*window.selected);
  }
  if (selected != nullptr) {
    fields += ',';
    fields += SelectedFields(*selected, book);
  } else if (!scene.list_mode && scene.catalog != nullptr) {
    const ro::ui::AchievementTally overall = TallyOfTab(
        *scene.catalog, book, ro::ui::kAchievementSummaryTab, ro::ui::kAchievementWholeTab);
    fields += std::format(R"(,"overall":{{"done":{},"total":{}}})", overall.done, overall.total);
  }
  fields += '}';

  // Le libellé : le succès choisi, sinon la recherche ou l'onglet, sinon le
  // résumé.
  std::array<char, kLabelBytes> label{};
  if (selected != nullptr) {
    std::snprintf(label.data(), label.size(), ro::core::Tr("Succès : %s (#%u)"),
                  selected->name.empty() ? "?" : selected->name.c_str(),
                  static_cast<unsigned>(selected->achievement_id));
  } else if (scene.list_mode && !search.empty()) {
    std::snprintf(label.data(), label.size(), ro::core::Tr("Succès : recherche « %s »"),
                  std::string(search).c_str());
  } else if (scene.list_mode) {
    std::snprintf(label.data(), label.size(), ro::core::Tr("Succès : liste de l'onglet %d"),
                  window.tab);
  } else {
    std::snprintf(label.data(), label.size(), "%s", ro::core::Tr("Succès : résumé"));
  }
  return FieldsBugReport(label.data(), fields);
}

}  // namespace moonclient
