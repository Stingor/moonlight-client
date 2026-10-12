// La minimap dans le panneau d'options. Voir l'en-tête.

#include "options/minimap_panel_options.h"

#include "ro/core/i18n.h"

namespace moonclient {

void MinimapPanelOptions::Compose() {
  rows_.clear();
  if (!draw_) {
    rows_.push_back(ro::ui::OptionRowView{
        .id = kMinimapPanelRowBase + static_cast<int>(MinimapPanelRow::kAbsent),
        .label = ro::core::Tr("Les réglages de la minimap s'ouvrent en jeu."),
        .kind = ro::ui::OptionKind::kNote});
    return;
  }
  rows_.push_back(
      ro::ui::OptionRowView{.id = kMinimapPanelRowBase + static_cast<int>(MinimapPanelRow::kPanel),
                            .kind = ro::ui::OptionKind::kPanel});
  rows_.back().panel = draw_;
}

bool MinimapPanelOptions::Change(const ro::ui::OptionsWindowAction& asked) const noexcept {
  return asked.changed == kMinimapPanelRowBase + static_cast<int>(MinimapPanelRow::kPanel);
}

}  // namespace moonclient
