// `/navi` ET `/navigation` : ce que la ligne tapée demande.

#include "chat/navigation_command.h"

#include <catch2/catch_test_macros.hpp>

using moonclient::NavigationCommandKind;
using moonclient::ReadNavigationCommand;

TEST_CASE("Commande /navi : sans argument, la fenêtre s'ouvre", "[app][chat][navi]") {
  CHECK(ReadNavigationCommand("/navi").kind == NavigationCommandKind::kOpen);
  CHECK(ReadNavigationCommand("/navigation").kind == NavigationCommandKind::kOpen);
  CHECK(ReadNavigationCommand("  /Navi  ").kind == NavigationCommandKind::kOpen);
}

TEST_CASE("Commande /navi : carte et coordonnées lancent le guidage", "[app][chat][navi]") {
  for (const char* line : {"/navi prontera 150/180", "/navi Prontera 150 180",
                           "/navi prontera 150,180", "/navi prontera   150   180"}) {
    const auto read = ReadNavigationCommand(line);
    REQUIRE(read.kind == NavigationCommandKind::kGuide);
    CHECK(read.place.map == "prontera");
    CHECK(read.place.x == 150);
    CHECK(read.place.y == 180);
  }
  // Un blanc de trop autour de la barre n'est pas une forme reconnue : c'est une
  // recherche.
  CHECK(ReadNavigationCommand("/navi prontera 150 / 180").kind == NavigationCommandKind::kSearch);
}

TEST_CASE("Commande /navi : sans coordonnées, une recherche", "[app][chat][navi]") {
  const auto read = ReadNavigationCommand("/navi Poring Marin");
  REQUIRE(read.kind == NavigationCommandKind::kSearch);
  CHECK(read.term == "Poring Marin");
  // Coordonnées illisibles : la ligne entière est le terme.
  CHECK(ReadNavigationCommand("/navi prontera x/y").kind == NavigationCommandKind::kSearch);
  CHECK(ReadNavigationCommand("/navi prontera -3/4").kind == NavigationCommandKind::kSearch);
}

TEST_CASE("Commande /navigation : un terme ouvre une recherche, jamais un guidage",
          "[app][chat][navi]") {
  const auto read = ReadNavigationCommand("/navigation prontera 150/180");
  REQUIRE(read.kind == NavigationCommandKind::kSearch);
  CHECK(read.term == "prontera 150/180");
}

TEST_CASE("Commande /navi : les autres lignes ne sont pas la commande", "[app][chat][navi]") {
  CHECK(ReadNavigationCommand("/navi2").kind == NavigationCommandKind::kNone);
  CHECK(ReadNavigationCommand("/atlas").kind == NavigationCommandKind::kNone);
  CHECK(ReadNavigationCommand("navi").kind == NavigationCommandKind::kNone);
  CHECK(ReadNavigationCommand("").kind == NavigationCommandKind::kNone);
}

TEST_CASE("Téléport du staff : la case quand on la connaît, la map sinon", "[app][chat][navi]") {
  using moonclient::PlaceLink;
  using moonclient::StaffWarpCommand;
  CHECK(StaffWarpCommand(PlaceLink{.map = "prontera", .x = 150, .y = 180}) ==
        "@warp prontera,150,180");
  CHECK(StaffWarpCommand(PlaceLink{.map = "pay_fild10"}) == "@warp pay_fild10");
  // Une moitié de case ne désigne rien : le serveur lirait l'autre à zéro.
  CHECK(StaffWarpCommand(PlaceLink{.map = "izlude", .x = 12}) == "@warp izlude");
}
