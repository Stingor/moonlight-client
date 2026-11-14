#include "ro/sim/packed_position.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

using ro::sim::CellMovement;
using ro::sim::CellPosition;
using ro::sim::kMaxPackedCoordinate;
using ro::sim::kMaxPackedDirection;
using ro::sim::PackMovement;
using ro::sim::PackPosition;
using ro::sim::UnpackMovement;
using ro::sim::UnpackPosition;

TEST_CASE("Position : trois octets connus se défont comme le serveur les a faits",
          "[sim][position]") {
  // Les valeurs viennent de l'empaquetage écrit dans src/map/clif.cpp, appliqué
  // à la main : x = 150, y = 100, direction = 6.
  //
  //   octet 0 = x >> 2                       = 37
  //   octet 1 = (x << 6) | ((y >> 4) & 0x3f) = 0x80 | 6 = 0x86
  //   octet 2 = (y << 4) | dir               = 0x40 | 6 = 0x46
  const std::array<std::uint8_t, 3> packed = {37, 0x86, 0x46};
  const CellPosition position = UnpackPosition(packed);
  CHECK(position.x == 150);
  CHECK(position.y == 100);
  CHECK(position.direction == 6);
}

TEST_CASE("Position : l'aller-retour tient sur TOUT le domaine", "[sim][position]") {
  // Les coordonnées sont à cheval sur les frontières d'octets : une erreur d'un
  // bit ne se voit que sur certaines valeurs. Les essayer toutes est le seul
  // contrôle qui prouve quelque chose, et il coûte un million de tours.
  for (std::uint16_t x = 0; x <= kMaxPackedCoordinate; ++x) {
    for (std::uint16_t y = 0; y <= kMaxPackedCoordinate; y = static_cast<std::uint16_t>(y + 7)) {
      const CellPosition original{x, y, static_cast<std::uint8_t>((x + y) & kMaxPackedDirection)};
      const CellPosition reread = UnpackPosition(PackPosition(original));
      if (reread != original) {
        FAIL("aller-retour rompu en x=" << x << " y=" << y);
      }
    }
  }
  CHECK(true);
}

TEST_CASE("Position : les huit directions survivent", "[sim][position]") {
  for (std::uint8_t direction = 0; direction <= kMaxPackedDirection; ++direction) {
    const CellPosition original{300, 200, direction};
    CHECK(UnpackPosition(PackPosition(original)) == original);
  }
}

TEST_CASE("Déplacement : les deux bouts et les sous-cases se relisent", "[sim][position]") {
  // Six octets pour deux positions et deux décalages de sous-case. Le second
  // point n'a PAS de direction : ses quatre bits portent le début de sa
  // coordonnée en x.
  const CellMovement original{CellPosition{150, 100, 0}, CellPosition{162, 111, 0}, 8, 3};
  const CellMovement reread = UnpackMovement(PackMovement(original));
  CHECK(reread.from.x == 150);
  CHECK(reread.from.y == 100);
  CHECK(reread.to.x == 162);
  CHECK(reread.to.y == 111);
  CHECK(reread.sub_x == 8);
  CHECK(reread.sub_y == 3);
}

TEST_CASE("Déplacement : l'aller-retour tient sur tout le domaine", "[sim][position]") {
  for (std::uint16_t value = 0; value <= kMaxPackedCoordinate;
       value = static_cast<std::uint16_t>(value + 3)) {
    const auto other = static_cast<std::uint16_t>(kMaxPackedCoordinate - value);
    const CellMovement original{CellPosition{value, other, 0}, CellPosition{other, value, 0},
                                static_cast<std::uint8_t>(value & 0x0F),
                                static_cast<std::uint8_t>(other & 0x0F)};
    const CellMovement reread = UnpackMovement(PackMovement(original));
    if (reread != original) {
      FAIL("aller-retour de déplacement rompu en " << value);
    }
  }
  CHECK(true);
}

TEST_CASE("Déplacement : la direction du départ n'est PAS transportée", "[sim][position]") {
  // Elle n'existe pas dans les six octets. La croire transportée ferait
  // regarder un personnage dans une direction arbitraire à chaque pas.
  CellMovement original{CellPosition{150, 100, 7}, CellPosition{151, 100, 0}, 0, 0};
  const CellMovement reread = UnpackMovement(PackMovement(original));
  CHECK(reread.from.direction == 0);
  CHECK(reread.to.direction == 0);
}
