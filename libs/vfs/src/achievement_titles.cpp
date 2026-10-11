// Le titre de chaque succès. Voir l'en-tête : le serveur ne l'envoie jamais.

#include "ro/vfs/achievement_titles.h"

#include <algorithm>

namespace ro::vfs {

AchievementTitles AchievementTitles::Of(const AchievementDatabase& database) {
  AchievementTitles found;
  for (const ServerAchievement& achievement : database.entries()) {
    // Un titre n'existe que si son numéro est POSITIF.
    if (achievement.reward_title > 0) {
      found.entries_.push_back(Entry{achievement.achievement_id, achievement.reward_title});
    }
  }
  return found;
}

std::optional<std::int32_t> AchievementTitles::TitleOf(std::uint32_t achievement_id) const {
  const auto found = std::ranges::lower_bound(entries_, achievement_id, {}, &Entry::achievement_id);
  if (found == entries_.end() || found->achievement_id != achievement_id) {
    return std::nullopt;
  }
  return found->title_id;
}

}  // namespace ro::vfs
