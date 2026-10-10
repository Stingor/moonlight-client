#include "options/bug_report_options.h"

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

// La clé du fichier. GRAVÉE : la changer rendrait le bouton à qui l'avait éteint.
constexpr const char* kButtonsKey = "report_of_bug_buttons";

}  // namespace

void BugReportOptions::Declare(SettingsFile& into) { into.Declare(kButtonsKey, &buttons_); }

void BugReportOptions::AppendRows(std::vector<ro::ui::OptionRowView>& into) const {
  into.push_back(ro::ui::OptionRowView{
      .id = kBugReportRowBase + static_cast<int>(BugReportRow::kButtons),
      .label = ro::core::Tr("Bouton « Signaler un bug »"),
      .help = ro::core::Tr("Un bouton dans la barre de titre des descriptions, du dialogue de "
                           "PNJ et de l'éditeur de style, et le raccourci qui ouvre un rapport "
                           "général. Le contexte est joint automatiquement."),
      .kind = ro::ui::OptionKind::kToggle,
      .on = buttons_,
      .heading = ro::core::Tr("Rapport de bug")});
}

bool BugReportOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed < kBugReportRowBase ||
      asked.changed >= kBugReportRowBase + static_cast<int>(BugReportRow::kCount)) {
    return false;
  }
  switch (static_cast<BugReportRow>(asked.changed - kBugReportRowBase)) {
    case BugReportRow::kButtons:
      buttons_ = asked.on;
      break;
    case BugReportRow::kCount:
      break;
  }
  return true;
}

}  // namespace moonclient
