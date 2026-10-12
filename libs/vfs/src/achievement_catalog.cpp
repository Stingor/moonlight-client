// Le catalogue des succès. Voir l'en-tête.

#include "ro/vfs/achievement_catalog.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace ro::vfs {
namespace {

[[nodiscard]] std::optional<std::int32_t> Positive(std::int32_t number) {
  if (number <= 0) {
    return std::nullopt;
  }
  return number;
}

// Un succès tel que la fenêtre le montre. En mode comptes, chaque ligne prend
// le but du serveur au même rang.
[[nodiscard]] AchievementInfo Shown(const ServerAchievement& server,
                                    const AchievementWindow& window) {
  AchievementInfo info{.achievement_id = server.achievement_id,
                       .name = server.name,
                       .summary = window.summary,
                       .details = window.details,
                       .score = server.score,
                       .gauge = window.gauge,
                       .group = window.group,
                       .major = window.major,
                       .minor = window.minor,
                       .reward_item = Positive(server.reward_item),
                       .reward_title = Positive(server.reward_title),
                       .reward_script = server.reward_script,
                       .reward_buff = window.buff};
  for (const AchievementLine& line : window.lines) {
    const std::size_t rank = info.objectives.size();
    if (rank == kAchievementObjectiveCount) {
      break;
    }
    AchievementObjective objective{.text = line.text, .shortcut = line.shortcut};
    if (window.gauge == AchievementGauge::kCounts) {
      objective.target = server.targets[rank].value_or(0);
    }
    info.objectives.push_back(std::move(objective));
  }
  return info;
}

}  // namespace

AchievementCatalog AchievementCatalog::Of(const AchievementDatabase& database) {
  AchievementCatalog built;
  // La base est rangée par numéro : le catalogue en garde l'ordre.
  for (const ServerAchievement& server : database.entries()) {
    if (!server.window.has_value()) {
      ++built.without_texts_;
      continue;
    }
    built.entries_.push_back(Shown(server, *server.window));
  }
  return built;
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
