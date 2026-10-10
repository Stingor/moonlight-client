#pragma once

// LES QUATRE FENÊTRES DE L'ÉCHOPPE D'ACHAT, telles que `TradeWindows` les
// porte : pour chacune, ce qu'elle garde d'une image à l'autre, la vue que la
// boucle compose — nulle, rien ne se dessine —, et ce que le joueur a demandé.

#include "ro/ui/buying_store_log_window.h"
#include "ro/ui/buying_store_own_window.h"
#include "ro/ui/buying_store_sell_window.h"
#include "ro/ui/buying_store_setup_window.h"

namespace moonclient {

struct BuyingStoreWindows {
  // LA PRÉPARATION : ouverte par le serveur, jusqu'à sa réponse.
  struct Setup {
    ro::ui::BuyingStoreSetupState state;
    const ro::ui::BuyingStoreSetupView* view = nullptr;
    ro::ui::BuyingStoreSetupAction answer;
  };
  // MON ÉCHOPPE, ouverte.
  struct Own {
    ro::ui::BuyingStoreOwnState state;
    const ro::ui::BuyingStoreOwnView* view = nullptr;
    ro::ui::BuyingStoreOwnAction answer;
  };
  // SES ACHATS, relus quand elle se ferme.
  struct Log {
    ro::ui::BuyingStoreLogState state;
    const ro::ui::BuyingStoreLogView* view = nullptr;
    ro::ui::BuyingStoreLogAction answer;
  };
  // L'ÉCHOPPE D'UN AUTRE, à qui je vends.
  struct Sell {
    ro::ui::BuyingStoreSellState state;
    const ro::ui::BuyingStoreSellView* view = nullptr;
    ro::ui::BuyingStoreSellAction answer;
  };
  Setup setup;
  Own own;
  Log log;
  Sell sell;
};

}  // namespace moonclient
