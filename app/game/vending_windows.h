#pragma once

// LES FENÊTRES DE L'ÉCHOPPE DE VENTE, telles que `TradeWindows` les porte :
// pour chacune, ce qu'elle garde, la vue que la boucle compose — nulle, rien ne
// se dessine —, et ce que le joueur a demandé.

#include "ro/ui/trade_widgets.h"
#include "ro/ui/vending_setup_window.h"
#include "ro/ui/vending_shop_window.h"
#include "ro/ui/vending_visit_window.h"

namespace moonclient {

struct VendingWindows {
  // LA PRÉPARATION, qu'ouvre la compétence.
  ro::ui::VendingSetupWindowState setup;
  const ro::ui::VendingSetupView* setup_view = nullptr;
  ro::ui::VendingSetupAction setup_answer;

  // MON ÉCHOPPE OUVERTE.
  ro::ui::VendingShopWindowState shop;
  const ro::ui::VendingShopView* shop_view = nullptr;
  ro::ui::VendingShopAction shop_answer;

  // LE JOURNAL DES VENTES, une fois l'échoppe fermée.
  ro::ui::VendingLogWindowState log;
  const ro::ui::VendingLogView* log_view = nullptr;
  ro::ui::VendingLogAction log_answer;

  // L'ÉCHOPPE D'UN AUTRE.
  ro::ui::VendingVisitWindowState visit;
  const ro::ui::VendingVisitView* visit_view = nullptr;
  ro::ui::VendingVisitAction visit_answer;

  // L'AVIS QUI SE DIT DANS UNE BOÎTE : l'ouverture qui a échoué.
  ro::ui::TradeNoticeState notice;
};

}  // namespace moonclient
