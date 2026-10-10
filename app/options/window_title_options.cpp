// Le titre de la fenêtre. Voir l'en-tête.

#include "options/window_title_options.h"

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

// La clé du fichier. ⚠ GRAVÉE : la changer rend à tous les joueurs le réglage
// d'origine, sans que rien ne le dise.
constexpr const char* kNamesShownKey = "window_title_names";

}  // namespace

void WindowTitleOptions::Declare(SettingsFile& into) {
  into.Declare(kNamesShownKey, &names_shown_);
}

void WindowTitleOptions::ComposeInto(std::vector<ro::ui::OptionRowView>& rows) const {
  rows.push_back(ro::ui::OptionRowView{
      .id = kWindowTitleRowBase + static_cast<int>(WindowTitleRow::kNamesShown),
      .label = ro::core::Tr("Pseudo et carte dans le titre"),
      .help = ro::core::Tr("Le titre de la fenêtre commence par le pseudo et la carte, et la "
                           "barre des tâches distingue ainsi deux clients ouverts."),
      .kind = ro::ui::OptionKind::kToggle,
      .on = names_shown_});
  rows.back().heading = ro::core::Tr("Titre de la fenêtre");
}

bool WindowTitleOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed != kWindowTitleRowBase + static_cast<int>(WindowTitleRow::kNamesShown)) {
    return false;
  }
  names_shown_ = asked.on;
  return true;
}

}  // namespace moonclient
