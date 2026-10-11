#pragma once

#include "ro/ui/cash_shop_window.h"

#include "game/bug_report_context.h"

namespace moonclient {

// LE RAPPORT D'UN BUG DE LA BOUTIQUE DE VOTE, selon ce que la fenêtre montrait :
//   - la vue : l'onglet — à l'index du serveur — et ses cartes, l'emplacement,
//     le tri, la recherche ;
//   - l'argent : les deux soldes, la case « Utiliser Event », ce que l'achat
//     peut dépenser ;
//   - le panier : ses premières lignes, son total, et s'il est payable — c'est
//     ce qui décide du bouton « Acheter » ou « Points insuffisants » ;
//   - le dernier achat : réussi, ou le refus tel que la fenêtre l'affichait.
[[nodiscard]] BugReportContext CashShopBugReport(const ro::ui::CashShopWindowState& window,
                                                 const ro::ui::CashShopView& view);

}  // namespace moonclient
