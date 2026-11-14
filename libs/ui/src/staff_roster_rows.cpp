// La liste des joueurs en ligne sans son dessin : durées, signaux, filtres,
// tri et regroupements. Voir l'en-tête.

#include <algorithm>
#include <map>
#include <tuple>
#include <utility>

#include "ro/core/ascii.h"
#include "ro/core/i18n.h"
#include "ro/core/printed.h"
#include "ro/ui/staff_roster_window.h"

namespace ro::ui {
namespace {

constexpr std::uint32_t kSecondsPerMinute = 60;
constexpr std::uint32_t kMinutesPerHour = 60;
constexpr std::uint32_t kHoursPerDay = 24;
constexpr std::uint32_t kSecondsPerHour = kSecondsPerMinute * kMinutesPerHour;
constexpr std::uint32_t kSecondsPerDay = kSecondsPerHour * kHoursPerDay;

template <typename Value>
[[nodiscard]] int Compared(const Value& left, const Value& right) {
  return left < right ? -1 : (right < left ? 1 : 0);
}

// Un texte vide se range après tous les autres.
[[nodiscard]] int ComparedBlankLast(std::string_view left, std::string_view right) {
  if (left.empty() != right.empty()) {
    return left.empty() ? 1 : -1;
  }
  return core::AsciiCompareFolded(left, right);
}

[[nodiscard]] int ComparedOnColumn(const StaffRosterRow& left, const StaffRosterRow& right,
                                   StaffRosterColumn column) {
  switch (column) {
    case StaffRosterColumn::kPlayer:
      break;
    case StaffRosterColumn::kJob:
      return ComparedBlankLast(left.job_name, right.job_name);
    case StaffRosterColumn::kLevel:
      return Compared(std::tie(left.base_level, left.job_level),
                      std::tie(right.base_level, right.job_level));
    case StaffRosterColumn::kMap: {
      const int by_map = core::AsciiCompareFolded(left.map, right.map);
      return by_map != 0 ? by_map : Compared(left.instance_id, right.instance_id);
    }
    case StaffRosterColumn::kGuild:
      return ComparedBlankLast(left.guild, right.guild);
    case StaffRosterColumn::kOnline:
      return Compared(left.online_s, right.online_s);
    case StaffRosterColumn::kIdle:
      return Compared(left.idle_s, right.idle_s);
    case StaffRosterColumn::kAction: {
      // Sans action, après toutes les autres.
      const auto rank_of = [](const StaffRosterRow& row) {
        const std::optional<StaffRosterActivity> activity = FreshestActivity(row);
        return activity.has_value() ? static_cast<int>(*activity)
                                    : static_cast<int>(kStaffRosterActivityCount);
      };
      const int by_kind = Compared(rank_of(left), rank_of(right));
      return by_kind != 0 ? by_kind : Compared(left.idle_s, right.idle_s);
    }
    case StaffRosterColumn::kClient: {
      const int by_client = Compared(std::tie(left.client, left.patch_index),
                                     std::tie(right.client, right.patch_index));
      return by_client != 0 ? by_client : left.revision.compare(right.revision);
    }
  }
  return 0;
}

// Les noms des autres lignes qui portent le même numéro de groupe non nul.
[[nodiscard]] std::vector<std::string_view> MatesOf(std::span<const StaffRosterRow> rows,
                                                    std::size_t rank, int StaffRosterRow::* group) {
  std::vector<std::string_view> mates;
  if (rank >= rows.size() || rows[rank].*group == 0) {
    return mates;
  }
  for (std::size_t other = 0; other < rows.size(); ++other) {
    if (other != rank && rows[other].*group == rows[rank].*group) {
      mates.emplace_back(rows[other].name);
    }
  }
  return mates;
}

}  // namespace

std::string StaffRosterDurationText(std::uint32_t seconds) {
  if (seconds < kSecondsPerMinute) {
    return core::Printed(core::Tr("%u s"), seconds);
  }
  if (seconds < kSecondsPerHour) {
    return core::Printed(core::Tr("%u min"), seconds / kSecondsPerMinute);
  }
  if (seconds < kSecondsPerDay) {
    return core::Printed(core::Tr("%u h %02u"), seconds / kSecondsPerHour,
                         seconds % kSecondsPerHour / kSecondsPerMinute);
  }
  return core::Printed(core::Tr("%u j %02u h"), seconds / kSecondsPerDay,
                       seconds % kSecondsPerDay / kSecondsPerHour);
}

std::optional<StaffRosterActivity> FreshestActivity(const StaffRosterRow& row) {
  std::optional<StaffRosterActivity> freshest;
  std::uint32_t least_age_s = 0;
  for (std::size_t rank = 0; rank < row.action_age_s.size(); ++rank) {
    const std::optional<std::uint32_t>& age_s = row.action_age_s[rank];
    if (age_s.has_value() && (!freshest.has_value() || *age_s < least_age_s)) {
      freshest = static_cast<StaffRosterActivity>(rank);
      least_age_s = *age_s;
    }
  }
  return freshest;
}

bool RowIsIdle(const StaffRosterRow& row) {
  return !row.offline_shop && row.idle_s >= kStaffRosterIdleSeconds;
}

bool RowIsShared(const StaffRosterRow& row) {
  return row.machine_group != 0 || row.account_group != 0;
}

bool RowIsFlagged(const StaffRosterRow& row) {
  return row.cheat_clock > 0 || row.cheat_cadence > 0 || row.cheat_client > 0 ||
         row.mute_minutes > 0 || row.jailed || row.tree_modified ||
         row.client == StaffRosterClientKind::kNone;
}

bool RowMatches(const StaffRosterRow& row, std::string_view needle,
                const StaffRosterFilters& filters) {
  if ((filters.staff_only && !row.staff) || (filters.hide_offline_shops && row.offline_shop) ||
      (filters.idle_only && !RowIsIdle(row)) || (filters.shared_only && !RowIsShared(row)) ||
      (filters.flagged_only && !RowIsFlagged(row))) {
    return false;
  }
  return core::AsciiContainsFolded(row.name, needle) ||
         core::AsciiContainsFolded(row.guild, needle) ||
         core::AsciiContainsFolded(row.party, needle) ||
         core::AsciiContainsFolded(row.map, needle) ||
         core::AsciiContainsFolded(row.job_name, needle);
}

std::vector<std::size_t> OrderedRows(std::span<const StaffRosterRow> rows, std::string_view needle,
                                     const StaffRosterFilters& filters, StaffRosterColumn column,
                                     bool descending) {
  std::vector<std::size_t> order;
  order.reserve(rows.size());
  for (std::size_t rank = 0; rank < rows.size(); ++rank) {
    if (RowMatches(rows[rank], needle, filters)) {
      order.push_back(rank);
    }
  }
  std::ranges::sort(order, [&](std::size_t left_rank, std::size_t right_rank) {
    const StaffRosterRow& left = rows[left_rank];
    const StaffRosterRow& right = rows[right_rank];
    int compared = ComparedOnColumn(left, right, column);
    if (compared == 0) {
      compared = core::AsciiCompareFolded(left.name, right.name);
    }
    if (compared == 0) {
      // Deux lignes de même nom gardent un ordre fixe d'une image à l'autre.
      return left.account_id < right.account_id;
    }
    return descending ? compared > 0 : compared < 0;
  });
  return order;
}

std::vector<std::string_view> MachineMates(std::span<const StaffRosterRow> rows, std::size_t rank) {
  return MatesOf(rows, rank, &StaffRosterRow::machine_group);
}

std::vector<std::string_view> AccountMates(std::span<const StaffRosterRow> rows, std::size_t rank) {
  return MatesOf(rows, rank, &StaffRosterRow::account_group);
}

StaffRosterSummary SummaryOf(std::span<const StaffRosterRow> rows) {
  StaffRosterSummary summary;
  std::map<std::pair<std::string_view, int>, int> maps;
  for (const StaffRosterRow& row : rows) {
    if (row.offline_shop) {
      ++summary.offline_shops;
    } else {
      ++summary.players;
    }
    ++maps[{row.map, row.instance_id}];
    if (row.client_outdated) {
      ++summary.outdated_clients;
    }
    if (RowIsFlagged(row)) {
      ++summary.flagged;
    }
  }
  summary.maps = static_cast<int>(maps.size());
  return summary;
}

std::vector<StaffRosterMapGroup> MapGroupsOf(std::span<const StaffRosterRow> rows,
                                             std::span<const std::size_t> order) {
  std::vector<StaffRosterMapGroup> groups;
  std::map<std::pair<std::string_view, int>, std::size_t> group_of;
  for (const std::size_t rank : order) {
    const StaffRosterRow& row = rows[rank];
    const auto [slot, fresh] = group_of.try_emplace({row.map, row.instance_id}, groups.size());
    if (fresh) {
      groups.push_back(StaffRosterMapGroup{.map = row.map, .instance_id = row.instance_id});
    }
    StaffRosterMapGroup& group = groups[slot->second];
    if (row.offline_shop) {
      ++group.offline_shops;
    } else {
      ++group.players;
    }
    group.rows.push_back(rank);
  }
  std::ranges::sort(groups, [](const StaffRosterMapGroup& left, const StaffRosterMapGroup& right) {
    const int left_size = left.players + left.offline_shops;
    const int right_size = right.players + right.offline_shops;
    if (left_size != right_size) {
      return left_size > right_size;
    }
    const int by_map = core::AsciiCompareFolded(left.map, right.map);
    return by_map != 0 ? by_map < 0 : left.instance_id < right.instance_id;
  });
  return groups;
}

}  // namespace ro::ui
