// UN MENU DE POLICES NE MONTRE QUE LES FAMILLES PRÉSENTES, et le réglage garde
// le rang de la table entière : la place dans le menu se traduit dans ce rang,
// et l'inverse.

#include <array>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/system_fonts.h"

using ro::ui::FontFamilyAtMenuRank;
using ro::ui::MenuRankOfFontFamily;

namespace {

// Une machine où manquent la deuxième et la quatrième famille de la table.
constexpr std::array<int, 3> kInstalled = {0, 2, 4};

}  // namespace

TEST_CASE("Menu des polices : la place et le rang se traduisent", "[app][police]") {
  // 🔴 LA TROISIÈME PLACE EST LE CINQUIÈME RANG : c'est tout le défaut qu'avait
  // le menu quand il montrait aussi les familles absentes et que l'atlas, lui,
  // ne les avait pas.
  CHECK(FontFamilyAtMenuRank(2, kInstalled) == 4);
  CHECK(MenuRankOfFontFamily(4, kInstalled) == 2);
  CHECK(FontFamilyAtMenuRank(0, kInstalled) == 0);
  CHECK(MenuRankOfFontFamily(2, kInstalled) == 1);
}

TEST_CASE("Menu des polices : ce que le menu ne montre pas retombe sur la première",
          "[app][police]") {
  // Une famille absente, choisie jadis sur une autre machine : le menu montre
  // la première, comme l'atlas la dessine.
  CHECK(MenuRankOfFontFamily(3, kInstalled) == 0);
  // Une place hors du menu — un fichier trafiqué — rend la première présente.
  CHECK(FontFamilyAtMenuRank(7, kInstalled) == 0);
  CHECK(FontFamilyAtMenuRank(-1, kInstalled) == 0);
  // Et sans aucune famille, le rang du défaut : l'atlas prend alors la police
  // intégrée.
  CHECK(FontFamilyAtMenuRank(1, {}) == 0);
}
