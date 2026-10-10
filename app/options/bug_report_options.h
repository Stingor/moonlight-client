#pragma once

#include <span>
#include <vector>

#include "ro/ui/options_window.h"

#include "game/settings_file.h"
#include "options/description_options.h"

namespace moonclient {

// ── LE RENONCEMENT AU RAPPORT DE BUG ────────────────────────────────────────
//
// Une case, dans « Interface » : elle éteint les boutons de barre de titre ET
// le raccourci. Un joueur qui ne signalera jamais rien n'a pas à voir un bouton
// de plus sur chaque fenêtre. Voir l'ADR 0052.
//
// ⚠ SES LIGNES S'AJOUTENT À CELLES DE L'INTERFACE, sous un intertitre, et
// portent un identifiant composé, hors de `GameOption`.
inline constexpr int kBugReportRowBase = 6100;

enum class BugReportRow : int {
  kButtons = 0,
  kCount,
};

static_assert(kDescriptionRowBase + static_cast<int>(DescriptionRow::kCount) <= kBugReportRowBase,
              "la plage du rapport de bug chevauche celle des descriptions");

class BugReportOptions {
 public:
  // Les boutons et le raccourci. ALLUMÉS par défaut.
  [[nodiscard]] bool buttons() const noexcept { return buttons_; }

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);

  // Ajoute ses lignes à celles d'une section — celle de l'interface.
  void AppendRows(std::vector<ro::ui::OptionRowView>& into) const;

  // Rend vrai quand la ligne était l'une des siennes.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

 private:
  bool buttons_ = true;
};

}  // namespace moonclient
