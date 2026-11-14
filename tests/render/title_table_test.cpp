// LES LIBELLÉS DE TITRE.
//
// 🔴 LE FIL N'EN PORTE QU'UN NUMÉRO. `ZC_ACK_REQNAMEALL` envoie `title_id` et
// rien d'autre ; le texte est à cent pour cent du côté du client, dans
// `titles.yml`. Une étiquette sans cette table montre un pseudo sans son titre.
//
// La miniature est synthétique : aucune table du jeu n'est versée aux essais.

#include "ro/render/title_table.h"

#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

using ro::render::TitleTable;

namespace {

constexpr std::string_view kMiniature = R"(
version: 1
titles:
  1004: Grand Master
  1000: Junction of life
  1003: "Master: the second"
)";

[[nodiscard]] TitleTable ReadTable() {
  auto table = TitleTable::Parse(kMiniature);
  REQUIRE(table.has_value());
  return std::move(*table);
}

}  // namespace

TEST_CASE("Titres : le numéro du serveur rend le libellé du client", "[render][title]") {
  const TitleTable table = ReadTable();
  REQUIRE(table.entries().size() == 3);
  CHECK(table.Find(1000) == "Junction of life");
  CHECK(table.Find(1003) == "Master: the second");
  CHECK(table.Find(1004) == "Grand Master");
  // Triée par numéro, quel que soit l'ordre du fichier.
  CHECK(table.entries().front().title_id == 1000);
  CHECK(table.entries().back().title_id == 1004);
}

TEST_CASE("Titres : un numéro sans libellé n'est pas une erreur", "[render][title]") {
  // Le serveur en connaît jusqu'à `TITLE_MAX` ; la table n'en traduit que ce
  // que son lot de données porte. Rien ne s'écrit alors, et une étiquette sans
  // titre reste une étiquette.
  const TitleTable table = ReadTable();
  CHECK(table.Find(1001).empty());
  CHECK(table.Find(9999).empty());
}

TEST_CASE("Titres : ZÉRO veut dire AUCUN titre", "[render][title]") {
  // ⚠ ET NON « LE TITRE NUMÉRO ZÉRO » : la table commence à mille
  // (`TITLE_BASE`), et `char.title_id` vaut zéro tant que rien n'est équipé.
  const TitleTable table = ReadTable();
  CHECK(table.Find(0).empty());
  CHECK(ro::render::kFirstTitleId == 1000);
}

TEST_CASE("Titres : une table mal formée est refusée", "[render][title]") {
  CHECK_FALSE(TitleTable::Parse("titles: {1000: A}").has_value());
  CHECK_FALSE(TitleTable::Parse("version: 2\ntitles: {1000: A}").has_value());
  CHECK_FALSE(TitleTable::Parse("version: 1").has_value());
  CHECK_FALSE(TitleTable::Parse("version: 1\ntitles: [A, B]").has_value());
  CHECK_FALSE(TitleTable::Parse("version: 1\ntitles: {first: A}").has_value());
  CHECK_FALSE(TitleTable::Parse("version: 1\ntitles: {1000: [A]}").has_value());
  CHECK_FALSE(TitleTable::Parse("version: 1\ntitles: {1000: A").has_value());
}
