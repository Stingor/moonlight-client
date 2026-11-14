#include "ro/core/ascii.h"

#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

using ro::core::AsciiLess;
using ro::core::AsciiLower;

TEST_CASE("Ascii : l'ordre des noms ignore la casse", "[core][ascii]") {
  // C'est l'ordre dans lequel le sac trie ses favoris, et il doit se comporter
  // comme le `stricmp` du client : « potion » et « Potion » ne se distinguent
  // pas, et ce sont les LETTRES qui décident, pas leur code.
  CHECK(AsciiLess("apple", "Banane"));
  CHECK_FALSE(AsciiLess("Banane", "apple"));
  // ⚠ SANS L'INSENSIBILITÉ, CELUI-CI PASSERAIT À L'ENVERS : en ASCII brut,
  // toute majuscule précède toute minuscule, et « Banane » viendrait d'abord.
  CHECK_FALSE(AsciiLess("Potion", "potion"));
  CHECK_FALSE(AsciiLess("potion", "Potion"));
  // À égalité de lettres, le plus court passe devant : sinon deux noms dont
  // l'un préfixe l'autre seraient équivalents, et leur ordre dépendrait du tri.
  CHECK(AsciiLess("Potion", "Potion blanche"));
  CHECK_FALSE(AsciiLess("Potion blanche", "Potion"));
  CHECK_FALSE(AsciiLess("", ""));
  CHECK(AsciiLess("", "a"));
}

TEST_CASE("AsciiLower abaisse A-Z", "[core][ascii]") {
  CHECK(AsciiLower(static_cast<std::uint8_t>('A')) == static_cast<std::uint8_t>('a'));
  CHECK(AsciiLower(static_cast<std::uint8_t>('Z')) == static_cast<std::uint8_t>('z'));
  CHECK(AsciiLower(static_cast<std::uint8_t>('M')) == static_cast<std::uint8_t>('m'));
}

TEST_CASE("AsciiLower laisse tout le reste intact", "[core][ascii]") {
  // Les bornes, juste en dehors de la plage.
  CHECK(AsciiLower(0x40) == 0x40);  // '@'
  CHECK(AsciiLower(0x5B) == 0x5B);  // '['
  CHECK(AsciiLower(static_cast<std::uint8_t>('a')) == static_cast<std::uint8_t>('a'));
  CHECK(AsciiLower(static_cast<std::uint8_t>('0')) == static_cast<std::uint8_t>('0'));

  // Le point qui compte : AUCUN octet >= 0x80 ne bouge. Un tolower() dépendant
  // de la locale en abaisserait certains et casserait le nom coréen.
  for (int value = 0x80; value <= 0xFF; ++value) {
    const auto byte = static_cast<std::uint8_t>(value);
    CHECK(AsciiLower(byte) == byte);
  }
}

TEST_CASE("AsciiLower préserve les octets de tête CP949", "[core][ascii]") {
  // « 인간족 » en CP949 : que des octets >= 0x80, aucun ne doit bouger.
  const std::string korean_folder = "\xC0\xCE\xB0\xA3\xC1\xB7";
  CHECK(AsciiLower(korean_folder) == korean_folder);
}

TEST_CASE("AsciiLower abaisse un octet de queue CP949 tombant dans A-Z", "[core][ascii]") {
  // Ce comportement est VOULU, pas un défaut : le client fait pareil (_strlwr
  // en locale C). Comme la clé de recherche subit la même transformation que la
  // clé stockée, la correspondance tient. Ce test verrouille cet accord.
  const std::string pair = "\x8A\x41";
  CHECK(AsciiLower(pair) == std::string("\x8A\x61"));
}

TEST_CASE("AsciiLower sur une chaîne mixte", "[core][ascii]") {
  CHECK(AsciiLower("DATA\\Sprite\\Foo.SPR") == "data\\sprite\\foo.spr");
  CHECK(AsciiLower("") == "");
}

TEST_CASE("Ascii : égalité et préfixe à la casse près", "[core][ascii]") {
  CHECK(ro::core::AsciiEqualsFolded("/BattleChat", "/battlechat"));
  CHECK_FALSE(ro::core::AsciiEqualsFolded("/battlechat ", "/battlechat"));
  CHECK_FALSE(ro::core::AsciiEqualsFolded("/bd", "/bc"));
  // Hors ASCII, un octet ne vaut que lui-même : « É » (0xC9) n'est pas « é ».
  CHECK_FALSE(ro::core::AsciiEqualsFolded("\xC9", "\xE9"));
  CHECK(ro::core::AsciiStartsWithFolded("/BC texte", "/bc "));
  CHECK(ro::core::AsciiStartsWithFolded("/W Bob salut", "/w "));
  CHECK_FALSE(ro::core::AsciiStartsWithFolded("/b", "/bc "));
  CHECK_FALSE(ro::core::AsciiStartsWithFolded("/bcast", "/bc "));
  CHECK(ro::core::AsciiStartsWithFolded("", ""));
}

TEST_CASE("Ascii : une fin de texte à la casse près", "[core][ascii]") {
  CHECK(ro::core::AsciiEndsWithFolded("data\\wav\\Hit.WAV", ".wav"));
  CHECK(ro::core::AsciiEndsWithFolded("prontera.gat", ".GAT"));
  CHECK_FALSE(ro::core::AsciiEndsWithFolded("prontera.gat", ".rsw"));
  // Un suffixe plus long que le texte n'en est pas la fin.
  CHECK_FALSE(ro::core::AsciiEndsWithFolded("spr", ".spr"));
  CHECK(ro::core::AsciiEndsWithFolded(".spr", ".SPR"));
  // Un suffixe vide termine tout, même un texte vide.
  CHECK(ro::core::AsciiEndsWithFolded("prontera.gat", ""));
  CHECK(ro::core::AsciiEndsWithFolded("", ""));
  // Hors ASCII, un octet ne vaut que lui-même.
  CHECK_FALSE(ro::core::AsciiEndsWithFolded("a\xC9", "\xE9"));
}

TEST_CASE("Ascii : l'ordre en trois issues, et le plus court devant", "[core][ascii]") {
  CHECK(ro::core::AsciiCompareFolded("potion", "POTION") == 0);
  CHECK(ro::core::AsciiCompareFolded("Apple", "banana") == -1);
  CHECK(ro::core::AsciiCompareFolded("banana", "Apple") == 1);
  CHECK(ro::core::AsciiCompareFolded("Potion", "Potion blanche") == -1);
  CHECK(ro::core::AsciiCompareFolded("Potion blanche", "Potion") == 1);
  // Un octet hors ASCII se compare non signé : « É » (0xC9) passe après « z ».
  CHECK(ro::core::AsciiCompareFolded("\xC9", "z") == 1);
  CHECK(ro::core::AsciiCompareFolded("", "") == 0);
}

TEST_CASE("Ascii : un filtre se cherche à la casse près", "[core][ascii]") {
  CHECK(ro::core::AsciiContainsFolded("Red Potion", "POT"));
  CHECK(ro::core::AsciiContainsFolded("Red Potion", "red potion"));
  CHECK_FALSE(ro::core::AsciiContainsFolded("Red Potion", "Potions"));
  CHECK_FALSE(ro::core::AsciiContainsFolded("Pot", "Potion"));
  // Un filtre vide ne cache rien, pas même un texte vide.
  CHECK(ro::core::AsciiContainsFolded("Red Potion", ""));
  CHECK(ro::core::AsciiContainsFolded("", ""));
  CHECK_FALSE(ro::core::AsciiContainsFolded("", "a"));
  // Hors ASCII, un octet ne vaut que lui-même.
  CHECK_FALSE(ro::core::AsciiContainsFolded("\xC9", "\xE9"));
}

TEST_CASE("Ascii : un mot perd ses espaces et tabulations des deux bouts", "[core][ascii]") {
  CHECK(ro::core::AsciiTrimmed(" \tprontera 150 \t") == "prontera 150");
  CHECK(ro::core::AsciiTrimmed("prontera") == "prontera");
  // Que des blancs : il ne reste rien.
  CHECK(ro::core::AsciiTrimmed(" \t ").empty());
  CHECK(ro::core::AsciiTrimmed("").empty());
  // Une fin de ligne n'est pas un blanc ici.
  CHECK(ro::core::AsciiTrimmed(" mot\r\n") == "mot\r\n");
}
