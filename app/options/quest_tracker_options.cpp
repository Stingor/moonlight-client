// Le journal des quêtes et son suivi dans le panneau d'options. Voir l'en-tête.

#include "options/quest_tracker_options.h"

#include "ro/core/i18n.h"

namespace moonclient {

void QuestTrackerOptions::Compose() {
  rows_.clear();
  if (!draw_) {
    rows_.push_back(ro::ui::OptionRowView{
        .id = kQuestTrackerRowBase + static_cast<int>(QuestTrackerRow::kAbsent),
        .label = ro::core::Tr("Les réglages du suivi des quêtes s'ouvrent en jeu."),
        .kind = ro::ui::OptionKind::kNote});
    return;
  }
  rows_.push_back(
      ro::ui::OptionRowView{.id = kQuestTrackerRowBase + static_cast<int>(QuestTrackerRow::kPanel),
                            .kind = ro::ui::OptionKind::kPanel});
  rows_.back().panel = draw_;
}

bool QuestTrackerOptions::Change(const ro::ui::OptionsWindowAction& asked) const noexcept {
  return asked.changed == kQuestTrackerRowBase + static_cast<int>(QuestTrackerRow::kPanel);
}

}  // namespace moonclient
