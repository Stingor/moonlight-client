// Le titre de la fenêtre. Voir l'en-tête.

#include "options/window_title_options.h"

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

// Les clés du fichier. ⚠ GRAVÉES : en changer une rend à tous les joueurs le
// réglage d'origine, sans que rien ne le dise.
constexpr const char* kNamesShownKey = "window_title_names";
constexpr const char* kRevisionShownKey = "window_title_revision";

}  // namespace

void WindowTitleOptions::Declare(SettingsFile& into) {
  into.Declare(kNamesShownKey, &names_shown_);
  into.Declare(kRevisionShownKey, &revision_shown_);
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
  rows.push_back(ro::ui::OptionRowView{
      .id = kWindowTitleRowBase + static_cast<int>(WindowTitleRow::kRevisionShown),
      .label = ro::core::Tr("Révision du client dans le titre"),
      .help = ro::core::Tr("Le titre de la fenêtre finit par le hash du commit sur lequel le "
                           "client a été compilé."),
      .kind = ro::ui::OptionKind::kToggle,
      .on = revision_shown_});
}

bool WindowTitleOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed == kWindowTitleRowBase + static_cast<int>(WindowTitleRow::kNamesShown)) {
    names_shown_ = asked.on;
    return true;
  }
  if (asked.changed == kWindowTitleRowBase + static_cast<int>(WindowTitleRow::kRevisionShown)) {
    revision_shown_ = asked.on;
    return true;
  }
  return false;
}

}  // namespace moonclient
