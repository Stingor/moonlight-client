#include "world/item_obtain_rules.h"

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/item_obtain_toast.h"

namespace {

using moonclient::CapObtainedLines;
using moonclient::ExpireObtainedLines;
using moonclient::MergeOrPushObtained;
using moonclient::ObtainedLifeMs;
using moonclient::ObtainedLine;
using moonclient::ObtainedLineCap;
using moonclient::SameObtainedLine;

// Un apport, tel que le relève du sac le fabriquerait.
[[nodiscard]] ObtainedLine Picked(std::uint32_t item_id, std::int32_t amount,
                                  std::uint32_t born_ms) {
  ObtainedLine line;
  line.item_id = item_id;
  line.amount = amount;
  line.born_ms = born_ms;
  line.name = "Red Potion";
  return line;
}

}  // namespace

TEST_CASE("Bandeau : deux ramassages du même objet n'ouvrent qu'une ligne") {
  std::vector<ObtainedLine> stack;
  static_cast<void>(MergeOrPushObtained(stack, Picked(501, 3, 1000), /*merge_same=*/true));
  const std::size_t at = MergeOrPushObtained(stack, Picked(501, 2, 4000), /*merge_same=*/true);

  REQUIRE(stack.size() == 1);
  CHECK(at == 0);
  CHECK(stack.front().amount == 5);
  // Le regroupement fait repartir le minuteur : une ligne qu'on alimente
  // encore n'est pas une ligne qu'on a fini de lire.
  CHECK(stack.front().born_ms == 4000);
}

TEST_CASE("Bandeau : un affinage différent ouvre sa propre ligne") {
  std::vector<ObtainedLine> stack;
  static_cast<void>(MergeOrPushObtained(stack, Picked(1201, 1, 0), /*merge_same=*/true));
  ObtainedLine refined = Picked(1201, 1, 0);
  refined.refine_level = 7;
  const std::size_t at = MergeOrPushObtained(stack, refined, /*merge_same=*/true);

  REQUIRE(stack.size() == 2);
  CHECK(at == 1);
  CHECK(stack.back().refine_level == 7);
}

TEST_CASE("Bandeau : un équipement cassé ne se fond pas dans les intacts") {
  ObtainedLine intact = Picked(1201, 1, 0);
  ObtainedLine broken = Picked(1201, 1, 0);
  broken.damaged = true;
  CHECK_FALSE(SameObtainedLine(intact, broken));

  ObtainedLine unknown = Picked(1201, 1, 0);
  unknown.identified = false;
  CHECK_FALSE(SameObtainedLine(intact, unknown));
  CHECK(SameObtainedLine(intact, Picked(1201, 4, 9000)));
}

TEST_CASE("Bandeau : sans regroupement, chaque ramassage a sa ligne") {
  std::vector<ObtainedLine> stack;
  static_cast<void>(MergeOrPushObtained(stack, Picked(501, 1, 0), /*merge_same=*/false));
  static_cast<void>(MergeOrPushObtained(stack, Picked(501, 1, 0), /*merge_same=*/false));
  CHECK(stack.size() == 2);
}

TEST_CASE("Bandeau : une ligne meurt à sa durée exactement") {
  std::vector<ObtainedLine> stack;
  static_cast<void>(MergeOrPushObtained(stack, Picked(501, 1, 1000), /*merge_same=*/true));

  ExpireObtainedLines(stack, /*now_ms=*/5999, /*life_ms=*/5000);
  CHECK(stack.size() == 1);
  ExpireObtainedLines(stack, /*now_ms=*/6000, /*life_ms=*/5000);
  CHECK(stack.empty());
}

TEST_CASE("Bandeau : le débordement de l'horloge ne fige pas une ligne à l'écran") {
  // 🔴 LE DÉFAUT DU CLIENT, QU'ON NE REPREND PAS : sa comparaison est
  // `now > born + life`, et à cheval sur le débordement de son horloge la somme
  // déborde elle aussi et retombe SOUS `now` — le bandeau s'efface alors à la
  // milliseconde suivante, sans avoir été lu. La soustraction non signée, elle,
  // reste juste.
  std::vector<ObtainedLine> stack;
  static_cast<void>(MergeOrPushObtained(stack, Picked(501, 1, 0xFFFFFFF0U), /*merge_same=*/true));

  ExpireObtainedLines(stack, /*now_ms=*/5U, /*life_ms=*/5000);
  CHECK(stack.size() == 1);
  // 0xFFFFFFF0 + 5000 déborde : 5000 - 16 = 4984 après le passage à zéro.
  ExpireObtainedLines(stack, /*now_ms=*/4984U, /*life_ms=*/5000);
  CHECK(stack.empty());
}

TEST_CASE("Bandeau : l'écrêtage jette les PLUS ANCIENNES") {
  std::vector<ObtainedLine> stack;
  for (std::uint32_t rank = 1; rank <= 5; ++rank) {
    static_cast<void>(
        MergeOrPushObtained(stack, Picked(500 + rank, 1, rank), /*merge_same=*/false));
  }
  CapObtainedLines(stack, 2);

  REQUIRE(stack.size() == 2);
  // La pile est tenue dans l'ordre d'arrivée : les deux qui restent sont les
  // deux dernières entrées.
  CHECK(stack.front().item_id == 504);
  CHECK(stack.back().item_id == 505);
}

TEST_CASE("Bandeau : le plafond dur borne un réglage aberrant") {
  CHECK(ObtainedLineCap(8) == 8);
  CHECK(ObtainedLineCap(0) == ro::ui::kLeastToastLines);
  CHECK(ObtainedLineCap(-3) == ro::ui::kLeastToastLines);
  // Un nombre relu d'un fichier trafiqué ne fait pas grossir la file sans fin.
  CHECK(ObtainedLineCap(9999) == ro::ui::kMostToastLines);
}

TEST_CASE("Bandeau : une durée trop courte ne fait pas clignoter") {
  CHECK(ObtainedLifeMs(5000) == 5000U);
  CHECK(ObtainedLifeMs(0) == static_cast<std::uint32_t>(ro::ui::kLeastToastDurationMs));
  CHECK(ObtainedLifeMs(-1) == static_cast<std::uint32_t>(ro::ui::kLeastToastDurationMs));
}
