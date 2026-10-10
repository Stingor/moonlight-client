#pragma once

#include <cstddef>

#include "ro/sim/achievement_book.h"
#include "ro/ui/achievement_window.h"
#include "ro/vfs/achievement_catalog.h"

#include "game/bug_report_context.h"

namespace moonclient {

// Ce que la fenêtre des succès montrait quand le joueur a signalé un bug.
struct AchievementBugScene {
  const ro::vfs::AchievementCatalog* catalog = nullptr;
  const ro::sim::AchievementBook* book = nullptr;
  const ro::ui::AchievementWindowState* window = nullptr;
  // La liste et le détail plutôt que le résumé.
  bool list_mode = false;
  // Les rangées de la liste montrée.
  std::size_t rows = 0;
};

// LE RAPPORT D'UN BUG DE SUCCÈS, selon ce que le joueur regardait :
//   - un succès choisi : sa fiche du catalogue et sa progression telle que le
//     serveur l'a dite — compteurs, achèvement, récompense, suivi ;
//   - sinon la liste : l'onglet, le filtre, l'ordre, la recherche ;
//   - sinon le résumé : le score, le rang et les comptes.
// Le classement — score, rang — et la vue de la fenêtre l'accompagnent toujours.
[[nodiscard]] BugReportContext AchievementBugReport(const AchievementBugScene& scene);

}  // namespace moonclient
