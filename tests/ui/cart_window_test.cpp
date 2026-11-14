#include "ro/ui/cart_window.h"

#include <algorithm>

#include <catch2/catch_test_macros.hpp>

using ro::ui::CartTabs;
using ro::ui::CartTally;
using ro::ui::ItemTab;

TEST_CASE("Charrette : six onglets, et pas celui des favoris", "[ui][charrette]") {
  // 🔴 L'ONGLET DES FAVORIS EST UN RANGEMENT DU SAC, que le serveur retient par
  // objet. Une charrette n'en a pas ; lui en dessiner un montrerait une case que
  // rien ne peut jamais remplir.
  const auto tabs = CartTabs();
  CHECK(tabs.size() == 6);
  CHECK(std::ranges::find(tabs, ItemTab::kFavorite) == tabs.end());
  // ⚠ NI AUCUN DES QUATRE DU COFFRE : il sépare les armes des armures et sort le
  // cash des consommables ; la charrette garde le découpage du client.
  for (const ItemTab absent :
       {ItemTab::kWeapon, ItemTab::kArmor, ItemTab::kCostume, ItemTab::kCash}) {
    CHECK(std::ranges::find(tabs, absent) == tabs.end());
  }

  // Les six autres y sont, chacune une fois.
  for (const ItemTab wanted : {ItemTab::kAll, ItemTab::kConsumable, ItemTab::kEquipment,
                               ItemTab::kAmmo, ItemTab::kEtc, ItemTab::kCard}) {
    CHECK(std::ranges::count(tabs, wanted) == 1);
  }
}

TEST_CASE("Charrette : « pas de charrette » n'est pas « charrette vide »", "[ui][charrette]") {
  // 🔴 LES DEUX ONT ZÉRO OBJET, et une seule a une capacité. C'est le serveur qui
  // annonce celle-ci, et il la reprend quand la charrette part.
  //
  // ⚠ CETTE DISTINCTION DÉCIDE SI LA FENÊTRE S'OUVRE. Sans charrette elle ne
  // s'ouvre pas du tout — la touche avale la demande et le refus se dit dans la
  // conversation —, et si la charrette part pendant qu'elle est ouverte, elle se
  // referme. Sans ce prédicat, il resterait une grille vide invitant à y ranger
  // quelque chose.
  CartTally none;
  CHECK_FALSE(none.exists());

  CartTally empty;
  empty.capacity = 100;
  CHECK(empty.exists());
  CHECK(empty.item_count == 0);
}
