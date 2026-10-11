#pragma once

#include "ro/ui/party_booking_window.h"

#include "game/bug_report_context.h"

namespace moonclient {

// LE RAPPORT D'UN BUG DE LA RECHERCHE DE GROUPE, composé sur la vue que la
// fenêtre a dessinée — celle de l'image où le joueur a cliqué :
//   - la recherche : les critères tels qu'ils partiraient (niveau, filtre, code
//     de lieu ou de métier), l'état de la liste et ses premières annonces —
//     niveaux, lieu, métiers, sans le nom de leur auteur ;
//   - l'annonce du joueur et les métiers qu'il y modifie, sinon celle qu'il
//     rédige.
[[nodiscard]] BugReportContext PartyBookingBugReport(const ro::ui::PartyBookingWindowState& window,
                                                     const ro::ui::PartyBookingView& view);

}  // namespace moonclient
