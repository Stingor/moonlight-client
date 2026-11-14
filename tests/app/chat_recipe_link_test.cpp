// LE LIEN D'UNE RECETTE : `<CRAF>id:nom</CRAF>`.
//
// 🔴 CE N'EST PAS UN LIEN D'ITEM, et le fil doit le dire : son libellé porte
// « Recette », ses crochets font partie du morceau cliquable, et son genre est
// celui qui fait ouvrir l'Atlas au lieu de la description. Le nom est le
// dernier champ, et garde donc ses « : ».

#include "chat/chat_recipe_link.h"

#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "chat/chat_item_link.h"

using moonclient::BuildRecipeLink;
using moonclient::ParseRecipeLink;
using moonclient::RecipeLink;

namespace {

constexpr std::uint32_t kAcidBottle = 7136;
constexpr std::uint32_t kPoring = 1002;

}  // namespace

TEST_CASE("Lien de recette : la balise fait l'aller-retour", "[app][chat][atlas]") {
  const RecipeLink link{.item_id = kAcidBottle, .name = "Acid Bottle: la vraie"};
  const std::string wire = BuildRecipeLink(link);
  CHECK(wire == "<CRAF>7136:Acid Bottle: la vraie</CRAF>");
  RecipeLink read;
  std::size_t after = 0;
  REQUIRE(ParseRecipeLink(wire + " suite", &read, &after));
  CHECK(read.item_id == kAcidBottle);
  CHECK(read.name == "Acid Bottle: la vraie");
  CHECK(after == wire.size());
  CHECK(moonclient::RecipeLinkLabel(read.name) == "[Recette : Acid Bottle: la vraie]");
}

TEST_CASE("Lien de recette : ce qui ne se relirait pas ne se compose pas", "[app][chat][atlas]") {
  CHECK(BuildRecipeLink(RecipeLink{.item_id = 0, .name = "Poring"}).empty());
  CHECK(BuildRecipeLink(RecipeLink{.item_id = kPoring}).empty());
  // La fermante dans le nom couperait la balise en deux à la relecture.
  CHECK(BuildRecipeLink(RecipeLink{.item_id = kPoring, .name = "a</CRAF>b"}).empty());
}

TEST_CASE("Lien de recette : une balise coupée ou mal formée ne se lit pas", "[app][chat][atlas]") {
  RecipeLink read;
  std::size_t after = 0;
  CHECK_FALSE(ParseRecipeLink("<CRAF>7136:Aci", &read, &after));
  CHECK(after == std::string("<CRAF>7136:Aci").size());
  CHECK_FALSE(ParseRecipeLink("<CRAF>7136</CRAF>", &read, nullptr));
  CHECK_FALSE(ParseRecipeLink("<CRAF>0:Acid Bottle</CRAF>", &read, nullptr));
  CHECK_FALSE(ParseRecipeLink("<CRAF>71x6:Acid Bottle</CRAF>", &read, nullptr));
  CHECK_FALSE(ParseRecipeLink("<CRAF>7136:</CRAF>", &read, nullptr));
}

TEST_CASE("Lien de recette : le fil le montre, crochets compris, à côté d'un monstre",
          "[app][chat][atlas]") {
  const ro::vfs::ItemTable items;
  const ro::vfs::ItemWords words;
  moonclient::DecoratedChatLine line;
  REQUIRE(moonclient::DecorateChatLine(
      "<MOBL>1002:0:Poring</MOBL> lâche de quoi faire <CRAF>7136:Acid Bottle</CRAF> !", items,
      words, &line));
  CHECK(line.text == "<[Mob] Poring> lâche de quoi faire [Recette : Acid Bottle] !");
  REQUIRE(line.links.size() == 2);
  CHECK(line.links[1].kind == ro::ui::ChatLinkKind::kRecipe);
  CHECK(line.links[1].item_id == kAcidBottle);
  CHECK(line.text.substr(line.links[1].offset, line.links[1].length) == "[Recette : Acid Bottle]");
  REQUIRE(line.carried.size() == 2);
  CHECK(line.carried[1].kind == ro::ui::ChatLinkKind::kRecipe);
  CHECK(line.carried[1].recipe.name == "Acid Bottle");
}

TEST_CASE("Lien de recette : une balise illisible reste à l'écran telle quelle",
          "[app][chat][atlas]") {
  const ro::vfs::ItemTable items;
  const ro::vfs::ItemWords words;
  moonclient::DecoratedChatLine line;
  REQUIRE(moonclient::DecorateChatLine("tiens <CRAF>0:rien</CRAF>", items, words, &line));
  CHECK(line.text == "tiens <CRAF>0:rien</CRAF>");
  CHECK(line.links.empty());
}
