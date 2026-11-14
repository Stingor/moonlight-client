// UN TEXTE À LA MANIÈRE DE `printf`, mesuré avant d'être écrit.

#include "ro/core/printed.h"

#include <cstddef>
#include <limits>
#include <string>

#include <catch2/catch_test_macros.hpp>

using ro::core::Printed;

TEST_CASE("Printed : les arguments se placent comme sous printf", "[core][printed]") {
  CHECK(Printed("%d objets", 3) == "3 objets");
  CHECK(Printed("%s est porté : %lld", "Épée", 42LL) == "Épée est porté : 42");
  CHECK(Printed("@storage%d", std::numeric_limits<int>::min()) == "@storage-2147483648");
}

TEST_CASE("Printed : un texte long n'est jamais tronqué", "[core][printed]") {
  // Bien au-delà de ce qu'un tampon fixe de quelques centaines d'octets tient.
  constexpr std::size_t kLongNameBytes = 1000;
  const std::string long_name(kLongNameBytes, 'x');
  const std::string printed = Printed("Pas assez de %s.", long_name.c_str());
  CHECK(printed.size() == long_name.size() + std::string("Pas assez de .").size());
  CHECK(printed == "Pas assez de " + long_name + ".");
}

TEST_CASE("Printed : un format vide rend un texte vide", "[core][printed]") {
  CHECK(Printed("%s", "").empty());
}
