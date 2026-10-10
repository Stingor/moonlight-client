#pragma once

// LES FENÊTRES DES COMMERCES, telles que `UiStage` les porte : pour chacune, ce
// qu'elle garde d'une image à l'autre, la vue que la boucle compose, et ce que
// le joueur a demandé.
//
// ⚠ GROUPÉES À PART de `UiStage::Windows` : quatre commerces, et bientôt
// davantage de fenêtres, y alourdiraient une liste déjà longue. C'est aussi ce
// qui laisse chaque commerce s'ajouter ici sans toucher au reste de l'étage.

#include <vector>

#include "ro/ui/barter_window.h"
#include "ro/ui/npc_shop_window.h"
#include "ro/ui/signboard_tip.h"

#include "game/buying_store_windows.h"
#include "game/deal_stage.h"
#include "game/store_search_stage.h"
#include "game/vending_windows.h"

namespace moonclient {

struct TradeWindows {
  // LA BOUTIQUE D'UN PNJ. Vue nulle, rien ne se dessine : c'est le serveur qui
  // ouvre.
  struct NpcShop {
    ro::ui::NpcShopWindowState state;
    const ro::ui::NpcShopView* view = nullptr;
    ro::ui::NpcShopWindowAction answer;
  };
  NpcShop npc_shop;

  // LE TROC : une fenêtre à deux volets, liste et panier. Même contrat.
  struct Barter {
    ro::ui::BarterWindowState state;
    const ro::ui::BarterView* view = nullptr;
    ro::ui::BarterWindowAction answer;
  };
  Barter barter;

  // L'ÉCHOPPE D'ACHAT : la préparation, la mienne ouverte, son historique, et
  // celle d'un autre à qui je vends.
  BuyingStoreWindows buying_store;

  // L'ÉCHOPPE DE VENTE : la préparation, la mienne ouverte, son journal, et
  // celle d'un autre où j'achète.
  VendingWindows vending;

  // L'ÉCHANGE ENTRE JOUEURS : la fenêtre des deux offres, et la boîte de la
  // proposition, reçue ou faite.
  DealWindows deal;

  // LA RECHERCHE D'ÉCHOPPES, qu'ouvre un objet de catalogue, et la boîte de ses
  // refus.
  StoreSearchWindows store_search;

  // LES ENSEIGNES PEINTES À L'IMAGE PRÉCÉDENTE — échoppes et salons —, pour
  // la bulle qui montre un titre coupé en entier.
  std::vector<ro::ui::SignboardTipView> signboard_tips;
};

}  // namespace moonclient
