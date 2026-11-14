// LES CARTES FERMÉES AU CYCLE JOUR/NUIT : la table qui les nomme.

#include "world/closed_maps.h"

#include <catch2/catch_test_macros.hpp>

using moonclient::ClosedMaps;

TEST_CASE("Cartes fermées : la table nomme les cartes où le ciel ne se voit pas", "[app][sky]") {
  const auto table = ClosedMaps::Parse("version: 1\nmaps:\n  - anthell01\n  - prt_sewb1\n");
  REQUIRE(table.has_value());
  CHECK(table->size() == 2);
  CHECK(table->Closes("anthell01"));
  CHECK(table->Closes("prt_sewb1"));
  // Une carte absente garde le cycle, et le nom se compare tel quel.
  CHECK_FALSE(table->Closes("prontera"));
  CHECK_FALSE(table->Closes("anthell"));
}

TEST_CASE("Cartes fermées : une table malformée est refusée", "[app][sky]") {
  CHECK_FALSE(ClosedMaps::Parse("pas: [du YAML").has_value());
  CHECK_FALSE(ClosedMaps::Parse("version: 2\nmaps: []\n").has_value());
  CHECK_FALSE(ClosedMaps::Parse("version: 1\nmaps:\n  anthell01: vrai\n").has_value());
  CHECK_FALSE(ClosedMaps::Parse("version: 1\nmaps:\n  - [anthell01]\n").has_value());
}

TEST_CASE("Cartes fermées : une table vide ne ferme rien", "[app][sky]") {
  const auto table = ClosedMaps::Parse("version: 1\nmaps: []\n");
  REQUIRE(table.has_value());
  CHECK(table->size() == 0);
  CHECK_FALSE(table->Closes("anthell01"));
}
