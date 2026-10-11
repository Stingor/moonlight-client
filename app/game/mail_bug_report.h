#pragma once

#include "ro/ui/mail_window.h"

#include "game/bug_report_context.h"

namespace moonclient {

// LES RAPPORTS D'UN BUG DU COURRIER, composés sur les vues que les fenêtres ont
// dessinées — celles de l'image où le joueur a cliqué.
//
// 🔴 UN COURRIER EST PRIVÉ : le rapport en emporte l'en-tête — numéro, boîte,
// expéditeur, sujet, ce qu'il porte — et la TAILLE du corps, jamais le corps.

// La boîte : l'onglet, le filtre, les comptes, la question en cours, et le
// courrier sélectionné avec son contenu — zeny, pièces jointes par item et
// quantité.
[[nodiscard]] BugReportContext MailboxBugReport(const ro::ui::MailboxWindowState& window,
                                                const ro::ui::MailboxView& view);

// L'écriture : le destinataire et ce que le serveur en a dit, le sujet, la
// taille du message, les pièces jointes, le zeny, les frais, le dernier refus
// et l'envoi en vol.
[[nodiscard]] BugReportContext MailComposeBugReport(const ro::ui::MailComposeWindowState& window,
                                                    const ro::ui::MailComposeView& view);

}  // namespace moonclient
