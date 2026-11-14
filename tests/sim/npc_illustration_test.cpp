// L'ILLUSTRATION D'UN PNJ : le type reçu, et ce qui la remplace ou l'efface.

#include "ro/sim/npc_illustration.h"

#include <optional>

#include <catch2/catch_test_macros.hpp>

using ro::sim::IllustrationPlace;
using ro::sim::IllustrationPlaceOf;
using ro::sim::NpcIllustration;
using ro::sim::NpcIllustrationSlot;

TEST_CASE("Illustration : les cinq places du protocole, et 255 qui efface", "[sim][illustration]") {
  CHECK(IllustrationPlaceOf(0) == IllustrationPlace::kBottomLeft);
  CHECK(IllustrationPlaceOf(1) == IllustrationPlace::kBottomMiddle);
  CHECK(IllustrationPlaceOf(2) == IllustrationPlace::kBottomRight);
  CHECK(IllustrationPlaceOf(3) == IllustrationPlace::kCenteredWindow);
  CHECK(IllustrationPlaceOf(4) == IllustrationPlace::kCenteredFloating);
  CHECK_FALSE(IllustrationPlaceOf(255).has_value());
  // Un type hors du protocole se montre en bas au milieu plutôt que nulle part.
  CHECK(IllustrationPlaceOf(7) == IllustrationPlace::kBottomMiddle);
}

TEST_CASE("Illustration : une seule à la fois, la suivante remplace", "[sim][illustration]") {
  NpcIllustrationSlot slot;
  slot.Show(NpcIllustration{.image = "kafra_01.bmp", .place = IllustrationPlace::kBottomRight});
  slot.Show(NpcIllustration{.image = "kafra_02.bmp", .place = IllustrationPlace::kBottomLeft});
  REQUIRE(slot.shown().has_value());
  CHECK(slot.shown()->image == "kafra_02.bmp");
  CHECK(slot.shown()->place == IllustrationPlace::kBottomLeft);
  slot.Clear();
  CHECK_FALSE(slot.shown().has_value());
}
