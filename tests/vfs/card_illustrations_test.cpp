#include "ro/vfs/card_illustrations.h"

#include <string>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/vfs.h"

#include "support/temp_directory.h"

using ro::test_support::TempDirectory;
using ro::vfs::CardIllustrations;
using ro::vfs::ParseCardIllustrations;
using ro::vfs::ReadCardIllustrations;
using ro::vfs::Vfs;

namespace {

// Ce que la conversion tire du fichier que lisait l'ancien lecteur :
//
//   // 2011-11-18 nouvelles cartes
//
//   4001#poring_card#
//   ligne sans diese
//   // suite
//   4002#fabre_card#
//   4003#포링카드#
constexpr std::string_view kConverted =
    "version: 1\n"
    "cards:\n"
    "  4001: poring_card\n"
    "  4002: fabre_card\n"
    "  4003: 포링카드\n";

[[nodiscard]] CardIllustrations Parsed(std::string_view yaml_text) {
  auto cards = ParseCardIllustrations(yaml_text);
  REQUIRE(cards.has_value());
  return std::move(*cards);
}

}  // namespace

TEST_CASE("Illustrations de cartes : une entrée donne un nom de fichier", "[vfs][cartes]") {
  const CardIllustrations cards = Parsed(kConverted);
  CHECK(cards.size() == 3);
  CHECK(cards.Find(4001) == "poring_card");
  CHECK(cards.Find(4002) == "fabre_card");
}

TEST_CASE("Illustrations de cartes : ce que la table ignore n'est pas une carte", "[vfs][cartes]") {
  // 🔴 C'EST LE TEST D'APPARTENANCE LUI-MÊME. Une carte ne se reconnaît pas à sa
  // ressource — elles la partagent toutes —, mais à sa présence ici. Un objet
  // absent doit rendre VIDE, pour que l'appelant se rabatte sur l'illustration
  // de collection au lieu d'aller chercher un fichier qui n'existe pas.
  const CardIllustrations cards = Parsed(kConverted);
  CHECK(cards.Find(617).empty());
  CHECK(cards.Find(0).empty());
}

TEST_CASE("Illustrations de cartes : le nom redevient ses octets d'archive", "[vfs][cartes]") {
  // 🔴 C'EST UN NOM DE FICHIER D'ARCHIVE, pas un texte à montrer. Les archives
  // s'indexent en CP949 : garder l'UTF-8 de la table ferait chercher un fichier
  // qui n'existe nulle part.
  const CardIllustrations cards = Parsed(kConverted);
  // « 포링카드 » en CP949, tel que le fichier du client le porte.
  CHECK(cards.Find(4003) == "\xC6\xF7\xB8\xB5\xC4\xAB\xB5\xE5");
}

TEST_CASE("Illustrations de cartes : une table mal formée est refusée", "[vfs][cartes]") {
  CHECK_FALSE(ParseCardIllustrations("version: 1\ncards: [4001]\n").has_value());
  CHECK_FALSE(ParseCardIllustrations("version: 1\ncards: {x: poring}\n").has_value());
  CHECK_FALSE(ParseCardIllustrations("version: 3\ncards: {}\n").has_value());
}

TEST_CASE("Illustrations de cartes : une table absente n'est pas une erreur", "[vfs][cartes]") {
  // Les cartes montreront alors le dos de carte générique, comme ce client le
  // faisait avant cette table. C'est moins bien, ce n'est pas une panne.
  TempDirectory client;
  client.WriteText("data/marqueur.txt", "");
  auto vfs = Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());
  auto cards = ReadCardIllustrations(*vfs);
  REQUIRE(cards.has_value());
  CHECK(cards->empty());
}
