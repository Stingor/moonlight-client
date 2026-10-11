#pragma once

#include "ro/ui/mvp_tracker_window.h"

#include "game/bug_report_context.h"

namespace moonclient {

// LE RAPPORT D'UN BUG DU CARNET DE CHASSE MVP, composé sur la vue que la
// fenêtre a dessinée — celle de l'image où le joueur a cliqué :
//   - le carnet : interrupteur, liaison, catalogue, tri, filtre, rangées, et les
//     premières dans l'ordre montré ;
//   - le groupe : son numéro, ses membres, la propriété ; le dernier refus et
//     l'invitation en cours ;
//   - le créneau visé, s'il y en a un — celui sur lequel le carnet s'est ouvert,
//     sinon le dernier dont on a ouvert le menu — : monstre, carte, délais,
//     fenêtre de retour et observation.
// L'heure du serveur l'accompagne : le minuteur se relit contre elle.
[[nodiscard]] BugReportContext MvpTrackerBugReport(const ro::ui::MvpTrackerWindowState& window,
                                                   const ro::ui::MvpTrackerView& view);

}  // namespace moonclient
