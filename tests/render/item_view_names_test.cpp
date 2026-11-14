// CE QUE DÉSIGNE L'APPARENCE D'UN OBJET : la même valeur se lit dans une table
// différente selon la pièce qu'il habille.

#include "ro/render/item_view_names.h"

#include <optional>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "support/sprite_tables.h"

using ro::render::DescribeItemView;
using ro::render::SpriteCatalog;
using ro::render::ViewedPiece;
using ro::render::ViewMeaning;

namespace {

// Les tables, réduites à quelques entrées : pour chaque numéro, son nom
// symbolique et son nom de sprite.
constexpr std::string_view kWeapons =
    "version: 1\n"
    "weapons:\n"
    "  0: {symbol: \"WEAPONTYPE_NONE\", sprite: \"\"}\n"
    "  1: {symbol: \"WEAPONTYPE_SHORTSWORD\", sprite: \"_dagger\"}\n"
    "  2: {symbol: \"WEAPONTYPE_SWORD\", sprite: \"_sword\"}\n";
constexpr std::string_view kHeadgears =
    "version: 1\n"
    "headgears:\n"
    "  1: {symbol: \"ACCESSORY_GOGGLE\", sprite: \"_goggle\"}\n"
    "  17: {symbol: \"ACCESSORY_RIBBON\", sprite: \"_ribbon\"}\n"
    "  40: {sprite: \"_unnamed\"}\n";
constexpr std::string_view kGarments =
    "version: 1\ngarments:\n  1: {symbol: \"ROBE_WINGS\", sprite: \"wings\"}\n";

SpriteCatalog Catalog() {
  using ro::render::SpriteTableFile;
  return SpriteCatalog{
      ro::test_support::SpriteTablesFrom({{SpriteTableFile::kWeapons, kWeapons},
                                          {SpriteTableFile::kHeadgears, kHeadgears},
                                          {SpriteTableFile::kGarments, kGarments}})};
}

}  // namespace

TEST_CASE("ViewID : une arme se lit comme une classe d'arme", "[render][objets]") {
  const SpriteCatalog catalog = Catalog();
  CHECK(DescribeItemView(catalog, ViewedPiece::kWeapon, 1) ==
        ViewMeaning{.symbol = "WEAPONTYPE_SHORTSWORD", .sprite = "dagger"});
  CHECK(DescribeItemView(catalog, ViewedPiece::kWeapon, 2) ==
        ViewMeaning{.symbol = "WEAPONTYPE_SWORD", .sprite = "sword"});
  // ⚠ LE MÊME NOMBRE DÉSIGNE AUTRE CHOSE SUR UNE AUTRE PIÈCE : c'est la pièce
  // qui choisit la table.
  CHECK(DescribeItemView(catalog, ViewedPiece::kHeadgear, 1) ==
        ViewMeaning{.symbol = "ACCESSORY_GOGGLE", .sprite = "goggle"});
  CHECK(DescribeItemView(catalog, ViewedPiece::kGarment, 1) ==
        ViewMeaning{.symbol = "ROBE_WINGS", .sprite = "wings"});
}

TEST_CASE("ViewID : un bouclier se lit comme une forme, sans nom symbolique", "[render][objets]") {
  const SpriteCatalog catalog = Catalog();
  const std::optional<ViewMeaning> guard = DescribeItemView(catalog, ViewedPiece::kShield, 1);
  REQUIRE(guard.has_value());
  CHECK(guard->symbol.empty());
  CHECK_FALSE(guard->sprite.empty());
  CHECK(guard->sprite.front() != '_');
  // La forme zéro n'en est pas une, et le client n'en connaît que quatre.
  CHECK_FALSE(DescribeItemView(catalog, ViewedPiece::kShield, 0).has_value());
  CHECK_FALSE(DescribeItemView(catalog, ViewedPiece::kShield, 5).has_value());
}

TEST_CASE("ViewID : un numéro que les tables ignorent ne désigne rien, sans erreur",
          "[render][objets]") {
  const SpriteCatalog catalog = Catalog();
  CHECK_FALSE(DescribeItemView(catalog, ViewedPiece::kWeapon, 7).has_value());
  CHECK_FALSE(DescribeItemView(catalog, ViewedPiece::kGarment, 2).has_value());
  // Une pièce qui ne se dessine pas n'a rien à lire.
  CHECK_FALSE(DescribeItemView(catalog, ViewedPiece::kNothing, 1).has_value());
  // Un sprite sans nom symbolique se nomme quand même.
  CHECK(DescribeItemView(catalog, ViewedPiece::kHeadgear, 40) ==
        ViewMeaning{.symbol = "", .sprite = "unnamed"});
  // Sans aucune table, rien ne se nomme.
  CHECK_FALSE(DescribeItemView(SpriteCatalog{}, ViewedPiece::kWeapon, 1).has_value());
}
