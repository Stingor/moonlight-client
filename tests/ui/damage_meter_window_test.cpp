#include "ro/ui/damage_meter_window.h"

#include <catch2/catch_test_macros.hpp>

using ro::ui::FormatDamageAmount;

TEST_CASE("Compteur de dégâts : un montant se lit tant qu'il est court", "[ui][dpsmeter]") {
  CHECK(FormatDamageAmount(0) == "0");
  CHECK(FormatDamageAmount(999) == "999");
  CHECK(FormatDamageAmount(12345) == "12 345");
  CHECK(FormatDamageAmount(99999) == "99 999");
  // Un montant négatif n'a pas de sens ici : il se lit zéro.
  CHECK(FormatDamageAmount(-5) == "0");
}

TEST_CASE("Compteur de dégâts : le montant exact se groupe d'une espace", "[ui][dpsmeter]") {
  using ro::ui::GroupDamageDigits;
  CHECK(GroupDamageDigits(0) == "0");
  CHECK(GroupDamageDigits(1000) == "1 000");
  CHECK(GroupDamageDigits(12345678) == "12 345 678");
  CHECK(GroupDamageDigits(-1234) == "-1 234");
}

TEST_CASE("Compteur de dégâts : au-delà, l'ordre de grandeur", "[ui][dpsmeter]") {
  // Au dixième de millier, puis au centième de million — ARRONDIS, et non
  // tronqués : 123 450 se lit 123,5 k.
  CHECK(FormatDamageAmount(100000) == "100,0 k");
  CHECK(FormatDamageAmount(123450) == "123,5 k");
  CHECK(FormatDamageAmount(999949) == "999,9 k");
  // ⚠ L'ARRONDI CHANGE D'UNITÉ : pas de « 1000,0 k ».
  CHECK(FormatDamageAmount(999950) == "1,00 M");
  CHECK(FormatDamageAmount(1000000) == "1,00 M");
  CHECK(FormatDamageAmount(12345678) == "12,35 M");
  // ⚠ SUR SOIXANTE-QUATRE BITS : un total de combat franchit deux milliards.
  CHECK(FormatDamageAmount(3000000000) == "3000,00 M");
}
