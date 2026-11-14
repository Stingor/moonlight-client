// LE PLACEMENT LIBRE DU SAC : L'ORDRE DES CASES, ET SES TROUS.
//
// 🔴 TROIS PANNES MUETTES À TENIR, et aucune ne se verrait en jeu autrement que
// par un objet qu'on ne retrouve plus : une case prise deux fois qui en avale
// un, un objet libre qui ne trouve plus de trou, et un échange qui écrase au
// lieu d'échanger.

#include "items/item_layout.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/inventory_window.h"

#include "items/item_layout_file.h"
#include "support/temp_directory.h"

using moonclient::ArrangeCells;
using moonclient::ItemLayout;
using ro::ui::InventoryCellView;

namespace {

// Une case occupée : la place serveur commence à deux, et c'est elle qui
// distingue une case pleine d'un trou.
[[nodiscard]] InventoryCellView CellOf(std::uint16_t slot, std::uint32_t item_id) {
  InventoryCellView cell;
  cell.client_slot = slot;
  cell.item_id = item_id;
  cell.amount = 1;
  return cell;
}

[[nodiscard]] std::vector<InventoryCellView> ThreeItems() {
  return {CellOf(2, 501), CellOf(3, 502), CellOf(4, 503)};
}

// Ce que chaque case porte, en identifiants — zéro pour un trou.
[[nodiscard]] std::vector<std::uint32_t> IdsOf(const std::vector<InventoryCellView>& cells) {
  std::vector<std::uint32_t> ids;
  ids.reserve(cells.size());
  for (const InventoryCellView& cell : cells) {
    ids.push_back(cell.client_slot == 0 ? 0U : cell.item_id);
  }
  return ids;
}

}  // namespace

TEST_CASE("Placement : sans table, l'ordre du serveur ne bouge pas", "[app][sac]") {
  // ⚠ C'EST LE CAS DE TOUT LE MONDE : un joueur qui n'a jamais posé un objet
  // doit voir exactement ce qu'il voyait avant que ce code existe.
  std::vector<InventoryCellView> cells = ThreeItems();
  ArrangeCells(cells, ItemLayout{});
  CHECK(IdsOf(cells) == std::vector<std::uint32_t>{501, 502, 503});
}

TEST_CASE("Placement : un objet posé loin laisse les trous devant lui", "[app][sac]") {
  // 🔴 C'EST TOUT L'OBJET DU RÉGLAGE. Sans les trous, un objet ramassé viendrait
  // se coller devant et pousserait les autres d'une case — ce que le placement
  // libre existe précisément pour empêcher.
  ItemLayout layout;
  layout.Put(503, 4);
  std::vector<InventoryCellView> cells = ThreeItems();
  ArrangeCells(cells, layout);
  REQUIRE(cells.size() == 5);
  CHECK(IdsOf(cells) == std::vector<std::uint32_t>{501, 502, 0, 0, 503});
}

TEST_CASE("Placement : la grille ne s'étend pas pour un objet absent", "[app][sac]") {
  // Un objet placé loin puis vendu ne doit pas laisser derrière lui un champ de
  // cases vides que rien ne remplirait jamais.
  ItemLayout layout;
  layout.Put(999, 40);  // vendu depuis : il n'est plus dans le sac
  std::vector<InventoryCellView> cells = ThreeItems();
  ArrangeCells(cells, layout);
  CHECK(cells.size() == 3);
  CHECK(IdsOf(cells) == std::vector<std::uint32_t>{501, 502, 503});
}

TEST_CASE("Placement : deux objets sur la même case, aucun ne se perd", "[app][sac]") {
  // ⚠ CELA NE PEUT VENIR QUE D'UN FICHIER CORRIGÉ À LA MAIN, et c'est
  // exactement le genre de chose qui ne doit pas escamoter un objet : le second
  // retombe dans les trous.
  ItemLayout layout;
  layout.Put(501, 1);
  layout.Put(502, 1);
  std::vector<InventoryCellView> cells = ThreeItems();
  ArrangeCells(cells, layout);
  REQUIRE(cells.size() == 3);
  const std::vector<std::uint32_t> ids = IdsOf(cells);
  CHECK(ids[1] == 501);
  // Les deux autres sont là, quelque part dans les trous.
  CHECK(std::ranges::count(ids, 502U) == 1);
  CHECK(std::ranges::count(ids, 503U) == 1);
  CHECK(std::ranges::count(ids, 0U) == 0);
}

TEST_CASE("Placement : poser sur une case prise ÉCHANGE les deux", "[app][sac]") {
  // 🔴 ÉCHANGER, ET NON ÉCRASER. L'occupant prend la case que l'autre quitte :
  // sans cela il partirait au premier trou venu, et le joueur qui permute deux
  // objets en verrait un troisième bouger.
  ItemLayout layout;
  layout.Put(501, 0);
  layout.Put(502, 1);
  layout.Place(501, 1, 502);
  CHECK(layout.CellOf(501) == 1);
  CHECK(layout.CellOf(502) == 0);
}

TEST_CASE("Placement : un objet SANS case libère celui qu'il déloge", "[app][sac]") {
  // ⚠ IL N'Y A RIEN À LUI DONNER : l'objet tiré remplissait un trou. Garder
  // l'occupant sur une case qu'un autre occupe désormais ferait DEUX objets sur
  // la même, et le second disparaîtrait de la grille.
  ItemLayout layout;
  layout.Put(502, 3);
  layout.Place(501, 3, 502);
  CHECK(layout.CellOf(501) == 3);
  CHECK(layout.CellOf(502) == ItemLayout::kNowhere);
}

TEST_CASE("Placement : oublier les places rend la grille au serveur", "[app][sac]") {
  ItemLayout layout;
  layout.Put(501, 7);
  REQUIRE_FALSE(layout.empty());
  layout.Clear();
  CHECK(layout.empty());
  CHECK(layout.CellOf(501) == ItemLayout::kNowhere);
}

TEST_CASE("Placement : une case négative ne se retient pas", "[app][sac]") {
  // Elle vient d'un fichier abîmé ou d'une entrée illisible, et « nulle part »
  // est déjà ce que dit l'absence : la retenir ferait un index qui se
  // promènerait dans les tableaux.
  ItemLayout layout;
  layout.Put(501, ItemLayout::kNowhere);
  CHECK(layout.empty());
  layout.Place(502, -3, 0);
  CHECK(layout.empty());
}

TEST_CASE("Placement : une case démesurée ne se retient pas", "[app][sac]") {
  // 🔴 LA GRILLE S'ÉTEND JUSQU'À LA CASE LA PLUS LOINTAINE : un fichier corrigé
  // à la main qui en nomme une à deux milliards faisait planter le sac.
  ItemLayout layout;
  layout.Put(501, 2'000'000'000);
  layout.Put(502, ItemLayout::kFarthestCell);
  layout.Place(503, ItemLayout::kFarthestCell, 0);
  CHECK(layout.empty());
  layout.Put(504, ItemLayout::kFarthestCell - 1);
  CHECK(layout.CellOf(504) == ItemLayout::kFarthestCell - 1);
}
