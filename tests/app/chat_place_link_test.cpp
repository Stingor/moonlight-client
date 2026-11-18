// LE LIEN D'UN LIEU : `<NAVIL>` + nom interne de carte + x et y en base 62.
//
// 🔴 LES QUATRE DERNIERS CARACTÈRES SONT LES COORDONNÉES, poids faible d'abord,
// et tout ce qui précède est le nom de la carte — lequel contient lui-même des
// caractères base 62. La découpe se fait par la fin.

#include "chat/chat_place_link.h"

#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"
#include "ro/vfs/map_display_names.h"

#include "chat/chat_item_link.h"

using moonclient::BuildPlaceLink;
using moonclient::ParsePlaceLink;
using moonclient::PlaceLink;

namespace {

constexpr int kProntera150 = 150;
constexpr int kProntera180 = 180;

[[nodiscard]] ro::vfs::MapDisplayNames Names() {
  auto names =
      ro::vfs::MapDisplayNames::Parse("version: 1\nmaps:\n  prontera:\n    name: Prontera\n");
  REQUIRE(names.has_value());
  return std::move(*names);
}

}  // namespace

TEST_CASE("Lien de lieu : la balise fait l'aller-retour", "[app][chat][navi]") {
  const PlaceLink link{.map = "prontera", .x = kProntera150, .y = kProntera180};
  const std::string wire = BuildPlaceLink(link);
  // 150 = 26 + 2 × 62 : « q » puis « 2 » ; 180 = 56 + 2 × 62 : « U » puis « 2 ».
  CHECK(wire ==
        "<NAVIL>prontera"
        "q2U2"
        "</NAVIL>");
  PlaceLink read;
  std::size_t after = 0;
  REQUIRE(ParsePlaceLink(wire + " suite", &read, &after));
  CHECK(read.map == "prontera");
  CHECK(read.x == kProntera150);
  CHECK(read.y == kProntera180);
  CHECK(after == wire.size());
}

TEST_CASE("Lien de lieu : la base 62 va du poids faible au poids fort", "[app][chat][navi]") {
  // 61 = « Z » puis « 0 », 62 = « 0 » puis « 1 » : le second caractère porte les soixante-deux.
  CHECK(BuildPlaceLink({.map = "a", .x = 61, .y = 62}) == "<NAVIL>aZ001</NAVIL>");
  CHECK(BuildPlaceLink({.map = "a", .x = 1, .y = 0}) == "<NAVIL>a1000</NAVIL>");
}

TEST_CASE("Lien de lieu : la carte entière est (0, 0)", "[app][chat][navi]") {
  const std::string wire = BuildPlaceLink({.map = "prontera"});
  CHECK(wire == "<NAVIL>prontera0000</NAVIL>");
  PlaceLink read;
  REQUIRE(ParsePlaceLink(wire, &read, nullptr));
  CHECK(read.x == 0);
  CHECK(read.y == 0);
  CHECK(moonclient::PlaceLinkLabel(read, "Prontera") == "[Prontera]");
}

TEST_CASE("Lien de lieu : un nom de carte qui finit en base 62 se découpe par la fin",
          "[app][chat][navi]") {
  // « 1@tower » et « gef_fild10 » finissent par des chiffres : seule la fin est sûre.
  for (const char* map : {"gef_fild10", "1@tower", "abcd"}) {
    PlaceLink read;
    REQUIRE(ParsePlaceLink(BuildPlaceLink({.map = map, .x = 3, .y = 4}), &read, nullptr));
    CHECK(read.map == map);
    CHECK(read.x == 3);
    CHECK(read.y == 4);
  }
}

TEST_CASE("Lien de lieu : une balise tronquée ou illisible est refusée", "[app][chat][navi]") {
  PlaceLink read;
  std::size_t after = 0;
  // Fermante absente : refusée, et `after` dit la fin du texte.
  const std::string cut =
      "<NAVIL>prontera"
      "q2U2";
  CHECK_FALSE(ParsePlaceLink(cut, &read, &after));
  CHECK(after == cut.size());
  // Une queue qui n'est pas du base 62 n'est pas une balise.
  CHECK_FALSE(ParsePlaceLink("<NAVIL>prontera q2!2</NAVIL>", &read, nullptr));
  // Sans nom de carte, ou trop court.
  CHECK_FALSE(ParsePlaceLink("<NAVIL>q2U2</NAVIL>", &read, nullptr));
  CHECK_FALSE(ParsePlaceLink("<NAVIL></NAVIL>", &read, nullptr));
  // Ce n'est pas une balise de lieu.
  CHECK_FALSE(ParsePlaceLink("<MOBL>1039:0:x</MOBL>", &read, nullptr));
  // Rien à bâtir sans carte, ni avec un chevron dans son nom.
  CHECK(BuildPlaceLink({}).empty());
  CHECK(BuildPlaceLink({.map = "a</NAVIL>b"}).empty());
}

TEST_CASE("Lien de lieu : le libellé montre le nom affiché, puis les coordonnées",
          "[app][chat][navi]") {
  const PlaceLink link{.map = "prontera", .x = kProntera150, .y = kProntera180};
  CHECK(moonclient::PlaceLinkLabel(link, "Prontera") == "[Prontera 150,180]");
  // Sans nom affiché, le nom interne le remplace.
  CHECK(moonclient::PlaceLinkLabel(link, "") == "[prontera 150,180]");
  const ro::vfs::MapDisplayNames names = Names();
  CHECK(moonclient::PlaceLinkLabel(link, &names) == "[Prontera 150,180]");
  CHECK(moonclient::PlaceLinkLabel({.map = "geffen", .x = 1, .y = 2}, &names) == "[geffen 1,2]");
}

TEST_CASE("Lien de lieu : l'infobulle porte le nom lisible et la carte interne",
          "[app][chat][navi]") {
  const ro::vfs::MapDisplayNames names = Names();
  // Une carte d'apparition de la fiche de monstre : la carte entière.
  const ro::ui::NavigationPlaceTip whole = moonclient::PlaceTipText({.map = "prontera"}, &names);
  CHECK(whole.label == "Prontera");
  CHECK(whole.map_name == "prontera");
  CHECK(whole.x == 0);
  CHECK(whole.y == 0);
  CHECK_FALSE(whole.map_image.usable());
  // Une case précise la garde.
  const ro::ui::NavigationPlaceTip cell =
      moonclient::PlaceTipText({.map = "prontera", .x = kProntera150, .y = kProntera180}, &names);
  CHECK(cell.x == kProntera150);
  CHECK(cell.y == kProntera180);
  // Inconnue de la table, ou sans table : pas de nom lisible, l'infobulle
  // montrera le nom interne.
  CHECK(moonclient::PlaceTipText({.map = "geffen"}, &names).label.empty());
  CHECK(moonclient::PlaceTipText({.map = "prontera"}, nullptr).label.empty());
}

TEST_CASE("Lien de lieu : le fil le rend cliquable", "[app][chat][navi]") {
  const ro::vfs::ItemTable items;
  const ro::vfs::ItemWords words;
  const ro::vfs::MapDisplayNames names = Names();
  const std::string raw =
      "viens " + BuildPlaceLink({.map = "prontera", .x = kProntera150, .y = kProntera180}) + " !";
  moonclient::DecoratedChatLine out;
  REQUIRE(moonclient::DecorateChatLine(raw, items, words, &out, nullptr, &names));
  CHECK(out.text == "viens [Prontera 150,180] !");
  REQUIRE(out.links.size() == 1);
  CHECK(out.links[0].kind == ro::ui::ChatLinkKind::kPlace);
  // Les crochets FONT le libellé : ils sont dans le morceau cliquable.
  CHECK(out.text.substr(out.links[0].offset, out.links[0].length) == "[Prontera 150,180]");
  REQUIRE(out.carried.size() == 1);
  CHECK(out.carried[0].place.map == "prontera");
  CHECK(out.carried[0].place.x == kProntera150);

  // Une balise tronquée reste à l'écran telle quelle.
  moonclient::DecoratedChatLine cut;
  REQUIRE(
      moonclient::DecorateChatLine("<NAVIL>prontera"
                                   "q2U2",
                                   items, words, &cut));
  CHECK(cut.text ==
        "<NAVIL>prontera"
        "q2U2");
  CHECK(cut.links.empty());
}

TEST_CASE("Lien de lieu : la ligne de destination se clique et relance le guidage",
          "[app][chat][navi]") {
  const ro::vfs::ItemTable items;
  const ro::vfs::ItemWords words;
  const ro::vfs::MapDisplayNames names = Names();
  const std::string line =
      moonclient::PlaceDestinationLine({.map = "prontera", .x = kProntera150, .y = kProntera180});
  moonclient::DecoratedChatLine out;
  REQUIRE(moonclient::DecorateChatLine(line, items, words, &out, nullptr, &names));
  REQUIRE(out.links.size() == 1);
  CHECK(out.text.substr(out.links[0].offset, out.links[0].length) == "[Prontera 150,180]");
  REQUIRE(out.carried.size() == 1);
  CHECK(out.carried[0].place.map == "prontera");
  CHECK(out.carried[0].place.y == kProntera180);
  // Une carte que la balise ne peut pas porter reste écrite, inerte.
  CHECK(moonclient::PlaceDestinationLine({.map = "a<b"}).ends_with("[a<b]"));
}
