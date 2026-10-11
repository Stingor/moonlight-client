#pragma once

#include "ro/ui/card_album_window.h"

#include "game/bug_report_context.h"

namespace moonclient {

// LE RAPPORT D'UN BUG DE L'ALBUM DE CARTES, selon ce que le classeur montrait :
//   - les comptes de la collection — pochettes, ouvertes, réserve — et ceux de
//     l'intercalaire choisi ;
//   - la vue : recherche, ordre, filtre d'affichage, intercalaire, natures, et
//     combien de pochettes elle retient ;
//   - la double page : son rang dans la vue et ses premières pochettes, chacune
//     avec son état — c'est ce que le joueur avait sous les yeux ;
//   - le dialogue en cours — sacrifice à confirmer, quantité — et le dernier
//     compte rendu, quand il y en a.
// ⚠ LES PAGES SE LISENT DANS L'ÉTAT DE LA FENÊTRE TEL QUE L'IMAGE L'A DESSINÉ :
// à composer avant que la vue ne soit refaite.
[[nodiscard]] BugReportContext CardAlbumBugReport(const ro::ui::CardAlbumWindowState& window,
                                                  const ro::ui::CardAlbumView& view);

}  // namespace moonclient
