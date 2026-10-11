#pragma once

#include <span>
#include <string>

#include "ro/ui/hotkey_window.h"

#include "game/bug_report_context.h"

namespace moonclient {

// Ce que la configuration des raccourcis montrait quand le joueur a signalé un
// bug.
struct HotkeyBugScene {
  const ro::ui::HotkeyWindowState* window = nullptr;
  ro::ui::HotkeyWindowView view;
  // LA CLÉ STABLE DE CHAQUE ACTION, au rang de sa ligne — celle du fichier de
  // raccourcis, qui ne se traduit pas. Plus courte qu'elle : la ligne se nomme
  // par son libellé.
  std::span<const std::string> action_keys;
};

// LE RAPPORT D'UN BUG DE RACCOURCIS : l'onglet et la recherche, l'action dont
// la fenêtre attend une touche, la phrase qui dit à qui la dernière touche a été
// reprise, puis les lignes MONTRÉES — `[action, touche]`, selon l'onglet et la
// recherche, comme la fenêtre les filtre.
//
// ⚠ LA LISTE EST BORNÉE en octets, pour que le contexte entier tienne dans la
// colonne du serveur ; `shown_total` dit combien de lignes la fenêtre montrait,
// `shown_cut` qu'il en manque.
[[nodiscard]] BugReportContext HotkeyBugReport(const HotkeyBugScene& scene);

}  // namespace moonclient
