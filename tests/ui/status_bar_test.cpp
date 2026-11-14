// L'emprise de la barre d'états, et le coin d'où elle grandit.
//
// 🔴 CE QUI SE PROUVE ICI DÉCIDE DE CE QUI NE BOUGE PAS. Une barre ancrée en bas
// à droite doit garder ses icônes en place quand un état tombe ; si son emprise
// et son placement ne parlent pas de la même grille, la dernière colonne sort du
// cadre — ou le cadre traîne une colonne vide. Rien de tout cela n'échoue : cela
// se voit à l'œil, ou pas du tout.

#include "ro/ui/status_bar.h"

#include <algorithm>
#include <cstddef>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/status_cell.h"

using ro::ui::GridOfStatusBar;
using ro::ui::PlaceStatusRow;
using ro::ui::RowOfStatusBar;
using ro::ui::StatusAnchor;
using ro::ui::StatusAnchorOf;
using ro::ui::StatusBarGrid;
using ro::ui::StatusBarLayout;
using ro::ui::StatusBarOrigin;
using ro::ui::StatusBarSize;
using ro::ui::StatusSort;
using ro::ui::StatusSortOf;
using ro::ui::StatusSweep;
using ro::ui::StatusSweepOf;

namespace {

// Une barre nette : des cases de dix, deux pixels d'écart.
//
// ⚠ SANS COMPTE À REBOURS, pour que la géométrie se lise en nombres ronds : il
// RÉSERVE de la place sous chaque case, et l'essai qui suit s'en occupe à part.
[[nodiscard]] StatusBarLayout Bar(int most, int rows, StatusAnchor anchor) {
  StatusBarLayout look;
  look.side_px = 10.0F;
  look.gap_px = 2.0F;
  look.most = most;
  look.rows = rows;
  look.anchor = static_cast<int>(anchor);
  look.show_countdown = false;
  return look;
}

// La case de rang zéro, telle qu'elle tombe dans une fenêtre posée en `at`.
[[nodiscard]] ImVec2 FirstCell(const StatusBarLayout& look, ImVec2 at) {
  const ImVec2 size = StatusBarSize(look, 1.0F);
  const ImVec2 origin = StatusBarOrigin(at, size, StatusAnchorOf(look.anchor));
  return PlaceStatusRow(1, origin, RowOfStatusBar(look, 1.0F)).front();
}

}  // namespace

TEST_CASE("Barre d'états : l'emprise est celle de la grille entière", "[ui][etats]") {
  // Huit cases sur trois lignes font trois colonnes, et l'emprise les porte
  // toutes — pas seulement les états du moment.
  const StatusBarGrid grid = GridOfStatusBar(Bar(8, 3, StatusAnchor::kTopLeft));
  CHECK(grid.columns == 3);
  CHECK(grid.rows == 3);

  const ImVec2 size = StatusBarSize(Bar(8, 3, StatusAnchor::kTopLeft), 1.0F);
  CHECK(size.x == 34.0F);
  CHECK(size.y == 34.0F);

  // Et l'échelle d'interface la multiplie en entier, gouttières comprises.
  const ImVec2 doubled = StatusBarSize(Bar(8, 3, StatusAnchor::kTopLeft), 2.0F);
  CHECK(doubled.x == 68.0F);
  CHECK(doubled.y == 68.0F);
}

TEST_CASE("Barre d'états : l'emprise et le placement parlent de la même grille", "[ui][etats]") {
  // 🔴 LA VÉRIFICATION QUI COMPTE. Deux formules, deux fichiers : celle de
  // l'emprise et celle du retour à la ligne. Si elles divergeaient d'une colonne,
  // la dernière icône tomberait hors du cadre qu'on a réservé — et le cadre ne se
  // voit qu'une fois la barre déverrouillée.
  const StatusBarLayout look = Bar(10, 3, StatusAnchor::kTopLeft);
  const StatusBarGrid grid = GridOfStatusBar(look);
  const ImVec2 size = StatusBarSize(look, 1.0F);
  const std::vector<ImVec2> places = PlaceStatusRow(static_cast<std::size_t>(look.most),
                                                    ImVec2(0.0F, 0.0F), RowOfStatusBar(look, 1.0F));
  REQUIRE(places.size() == static_cast<std::size_t>(look.most));

  const float below = ro::ui::StatusCountdownRoom(look, 1.0F);
  float widest = 0.0F;
  float lowest = 0.0F;
  for (const ImVec2& place : places) {
    widest = std::max(widest, place.x + look.side_px);
    lowest = std::max(lowest, place.y + look.side_px + below);
  }
  CHECK(widest == size.x);
  CHECK(lowest == size.y);
  // Et la dernière ligne n'est pas vide : la grille ne réserve pas plus qu'il
  // n'en faut.
  CHECK(lowest == static_cast<float>(grid.rows) * 12.0F - 2.0F);
}

TEST_CASE("Barre d'états : la première case touche le coin ancré", "[ui][etats]") {
  // 🔴 C'EST TOUT L'OBJET DE L'ANCRAGE. La rangée grandit DANS l'emprise, en
  // partant du coin choisi ; ancrée du mauvais côté, elle reculerait à chaque
  // état qui tombe et toutes les icônes glisseraient sous le curseur.
  const ImVec2 at(100.0F, 200.0F);
  // Quatre colonnes, deux lignes : une emprise de 46 sur 22.
  const ImVec2 size = StatusBarSize(Bar(8, 2, StatusAnchor::kTopLeft), 1.0F);
  REQUIRE(size.x == 46.0F);
  REQUIRE(size.y == 22.0F);

  const ImVec2 top_left = FirstCell(Bar(8, 2, StatusAnchor::kTopLeft), at);
  CHECK(top_left.x == 100.0F);
  CHECK(top_left.y == 200.0F);

  const ImVec2 top_right = FirstCell(Bar(8, 2, StatusAnchor::kTopRight), at);
  CHECK(top_right.x == 100.0F + 46.0F - 10.0F);
  CHECK(top_right.y == 200.0F);

  const ImVec2 bottom_left = FirstCell(Bar(8, 2, StatusAnchor::kBottomLeft), at);
  CHECK(bottom_left.x == 100.0F);
  CHECK(bottom_left.y == 200.0F + 22.0F - 10.0F);

  const ImVec2 bottom_right = FirstCell(Bar(8, 2, StatusAnchor::kBottomRight), at);
  CHECK(bottom_right.x == 100.0F + 46.0F - 10.0F);
  CHECK(bottom_right.y == 200.0F + 22.0F - 10.0F);
}

TEST_CASE("Barre d'états : un réglage venu du fichier reste dans ses valeurs", "[ui][etats]") {
  // ⚠ LE FICHIER DE RÉGLAGES EST UN TEXTE QUE LE JOUEUR PEUT ÉCRIRE, et un
  // entier hors bornes converti en énumération est un comportement indéfini —
  // pas un affichage bizarre.
  CHECK(StatusAnchorOf(3) == StatusAnchor::kBottomRight);
  CHECK(StatusAnchorOf(4) == StatusAnchor::kTopLeft);
  CHECK(StatusAnchorOf(-1) == StatusAnchor::kTopLeft);
  CHECK(StatusSortOf(2) == StatusSort::kLongest);
  CHECK(StatusSortOf(9) == StatusSort::kArrival);
  CHECK(StatusSweepOf(2) == StatusSweep::kVertical);
  CHECK(StatusSweepOf(99) == StatusSweep::kNone);
}

TEST_CASE("Barre d'états : zéro ligne ne divise pas par zéro", "[ui][etats]") {
  StatusBarLayout broken = Bar(8, 0, StatusAnchor::kTopLeft);
  const StatusBarGrid grid = GridOfStatusBar(broken);
  CHECK(grid.rows == 1);
  CHECK(grid.columns == 8);

  broken.most = 0;
  CHECK(GridOfStatusBar(broken).columns == 1);
}

TEST_CASE("Barre d'états : le compte à rebours a sa place réservée", "[ui][etats]") {
  // 🔴 IL SE POSE SOUS L'ICÔNE, DONC HORS DE LA CASE — ce que son libellé promet.
  // La fenêtre découpe à son bord : sans cette réserve, la dernière ligne de
  // chiffres serait simplement invisible, et l'avant-dernière tomberait sur les
  // icônes du dessous.
  StatusBarLayout look = Bar(8, 2, StatusAnchor::kTopLeft);
  look.show_countdown = true;
  look.countdown_px = 11.0F;

  CHECK(ro::ui::StatusCountdownRoom(look, 1.0F) == 11.0F);
  // Deux lignes de (10 + 11) plus une gouttière de 2.
  CHECK(StatusBarSize(look, 1.0F).y == 44.0F);

  // Et le pas vertical porte la réserve, là où l'horizontal ne la porte pas.
  const std::vector<ImVec2> places =
      PlaceStatusRow(8, ImVec2(0.0F, 0.0F), RowOfStatusBar(look, 1.0F));
  REQUIRE(places.size() == 8);
  CHECK(places[1].x == 12.0F);
  CHECK(places[4].y == 23.0F);

  // Éteint, il ne réserve plus rien.
  look.show_countdown = false;
  CHECK(ro::ui::StatusCountdownRoom(look, 1.0F) == 0.0F);
  CHECK(StatusBarSize(look, 1.0F).y == 22.0F);
}

TEST_CASE("Rangée d'états : le bord replie avant le quota de la ligne", "[ui][etats]") {
  // Le quota vaut trois par ligne, mais le bord n'en laisse tenir que deux : la
  // ligne se termine avant son quota, et la suivante se compte à part.
  ro::ui::StatusRowLayout row;
  row.side = 10.0F;
  row.gap = 2.0F;
  row.most = 6;
  row.rows = 2;
  row.limit = 25.0F;
  const std::vector<ImVec2> places = PlaceStatusRow(6, ImVec2(0.0F, 0.0F), row);
  REQUIRE(places.size() == 4);
  CHECK(places[1].x == 12.0F);
  CHECK(places[1].y == 0.0F);
  CHECK(places[2].x == 0.0F);
  CHECK(places[2].y == 12.0F);
  CHECK(places[3].x == 12.0F);

  // De droite à gauche, le bord est à gauche.
  row.right_to_left = true;
  row.limit = 75.0F;
  const std::vector<ImVec2> leftward = PlaceStatusRow(6, ImVec2(100.0F, 0.0F), row);
  REQUIRE(leftward.size() == 4);
  CHECK(leftward[0].x == 90.0F);
  CHECK(leftward[1].x == 78.0F);
  CHECK(leftward[2].x == 90.0F);
  CHECK(leftward[2].y == 12.0F);

  // Sans bord, rien ne change.
  row.right_to_left = false;
  row.limit = 0.0F;
  CHECK(PlaceStatusRow(6, ImVec2(0.0F, 0.0F), row).size() == 6);
}
