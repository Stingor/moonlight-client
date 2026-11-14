// LE CODE D'UNE COULEUR : la forme qu'on écrit, celles qu'on accepte de lire,
// et le refus d'un code à moitié tapé — c'est lui qui empêche le nuancier de
// repeindre à chaque touche.

#include "ro/ui/color_code.h"

#include <array>
#include <cstdint>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

using ro::ui::FormatColorCode;
using ro::ui::ParseColorCode;

namespace {

using Rgb = std::array<std::uint8_t, 3>;

}  // namespace

TEST_CASE("Code couleur : il s'écrit 0xRRGGBB, chiffres en majuscules", "[ui][palette]") {
  CHECK(std::string_view(FormatColorCode(Rgb{0, 0, 0}).data()) == "0x000000");
  CHECK(std::string_view(FormatColorCode(Rgb{255, 255, 255}).data()) == "0xFFFFFF");
  CHECK(std::string_view(FormatColorCode(Rgb{255, 136, 10}).data()) == "0xFF880A");
}

TEST_CASE("Code couleur : il se lit sous les formes qu'un joueur colle", "[ui][palette]") {
  const Rgb orange{255, 136, 10};
  CHECK(ParseColorCode("0xFF880A") == orange);
  CHECK(ParseColorCode("0Xff880a") == orange);
  CHECK(ParseColorCode("#ff880a") == orange);
  CHECK(ParseColorCode("^FF880A") == orange);
  CHECK(ParseColorCode("FF880A") == orange);
  CHECK(ParseColorCode("  0xFF880A \r\n") == orange);
  CHECK(ParseColorCode("0x000000") == Rgb{0, 0, 0});
  // Ce qu'on écrit se relit.
  CHECK(ParseColorCode(FormatColorCode(orange).data()) == orange);
}

TEST_CASE("Code couleur : un code à moitié tapé, ou trop long, ne se lit pas", "[ui][palette]") {
  for (const std::string_view refused :
       {"", "0x", "#", "0xFF", "0xFF880", "#F80", "FF880A0", "0xFF880A00", "0xFF880G", "0x FF880A",
        "x0FF880A", "0x#FF880A"}) {
    CAPTURE(refused);
    CHECK_FALSE(ParseColorCode(refused).has_value());
  }
}
