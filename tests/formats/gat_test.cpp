#include "ro/formats/gat.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"
#include "ro/formats/rsw.h"

#include "support/map_builder.h"

using ro::core::ErrorCode;
using ro::formats::DeriveServerCells;
using ro::formats::GatMap;
using ro::formats::WaterField;
using ro::formats::WaterLevelRule;
using ro::test_support::BuildGat;
using ro::test_support::GatCellSpec;

namespace {

// Une carte 3x2 dont chaque cellule porte une hauteur et un type distincts.
std::vector<GatCellSpec> SampleCells() {
  return {
      {{0.0F, 0.0F, 0.0F, 0.0F}, 0},   // marchable, au niveau 0
      {{1.0F, 1.0F, 1.0F, 1.0F}, 1},   // bloquée
      {{2.0F, 2.0F, 2.0F, 2.0F}, 0},   // marchable, plate
      {{3.0F, 3.0F, 3.0F, 3.0F}, 5},   // bloquée mais tirable
      {{-1.0F, 0.0F, 1.0F, 2.0F}, 6},  // pente, marchable
      {{9.0F, 9.0F, 9.0F, 9.0F}, 3},   // eau marchable, déjà classée
  };
}

}  // namespace

TEST_CASE("GAT : en-tête et cellules", "[formats][gat]") {
  auto gat = GatMap::Parse(BuildGat(3, 2, SampleCells()));
  REQUIRE(gat.has_value());

  CHECK(gat->width() == 3);
  CHECK(gat->height() == 2);
  CHECK(gat->major_version() == 1);
  CHECK(gat->minor_version() == 2);
  CHECK(gat->cells().size() == 6);
}

TEST_CASE("GAT : un assemblage rend, champ à champ, ce que Parse lit du fichier",
          "[formats][gat]") {
  auto parsed = GatMap::Parse(BuildGat(3, 2, SampleCells()));
  REQUIRE(parsed.has_value());
  auto assembled = GatMap::Assemble(parsed->width(), parsed->height(), parsed->cells(),
                                    parsed->major_version(), parsed->minor_version());
  REQUIRE(assembled.has_value());
  CHECK(assembled->width() == parsed->width());
  CHECK(assembled->height() == parsed->height());
  CHECK(assembled->major_version() == parsed->major_version());
  CHECK(assembled->minor_version() == parsed->minor_version());
  REQUIRE(assembled->cells().size() == parsed->cells().size());
  for (std::size_t rank = 0; rank < parsed->cells().size(); ++rank) {
    CHECK(assembled->cells()[rank].corner_height == parsed->cells()[rank].corner_height);
    CHECK(assembled->cells()[rank].type == parsed->cells()[rank].type);
  }
}

TEST_CASE("GAT : un assemblage incohérent est refusé", "[formats][gat]") {
  auto parsed = GatMap::Parse(BuildGat(3, 2, SampleCells()));
  REQUIRE(parsed.has_value());
  std::vector<ro::formats::GatCell> short_cells = parsed->cells();
  short_cells.pop_back();
  CHECK_FALSE(GatMap::Assemble(3, 2, short_cells).has_value());
  CHECK_FALSE(GatMap::Assemble(0, 2, {}).has_value());
  constexpr std::int32_t kWiderThanAnyMap = 8193;
  CHECK_FALSE(GatMap::Assemble(kWiderThanAnyMap, 1, {}).has_value());
}

TEST_CASE("GAT : indexation ligne par ligne", "[formats][gat]") {
  auto gat = GatMap::Parse(BuildGat(3, 2, SampleCells()));
  REQUIRE(gat.has_value());

  // at(x, y) == cells()[y * width + x]
  CHECK(gat->at(0, 0).type == 0);
  CHECK(gat->at(1, 0).type == 1);
  CHECK(gat->at(2, 0).type == 0);
  CHECK(gat->at(0, 1).type == 5);
  CHECK(gat->at(2, 1).type == 3);

  CHECK(gat->InBounds(2, 1));
  CHECK_FALSE(gat->InBounds(3, 1));
  CHECK_FALSE(gat->InBounds(0, 2));
  CHECK_FALSE(gat->InBounds(-1, 0));
}

TEST_CASE("GAT : les quatre hauteurs de coin", "[formats][gat]") {
  auto gat = GatMap::Parse(BuildGat(3, 2, SampleCells()));
  REQUIRE(gat.has_value());

  // L'indice 0 est le coin BAS-GAUCHE, et c'est celui que le serveur lit.
  const auto& slope = gat->at(1, 1);
  CHECK(slope.corner_height[0] == -1.0F);
  CHECK(slope.corner_height[1] == 0.0F);
  CHECK(slope.corner_height[2] == 1.0F);
  CHECK(slope.corner_height[3] == 2.0F);
}

TEST_CASE("GAT : signature invalide", "[formats][gat]") {
  std::vector<std::uint8_t> gat = BuildGat(3, 2, SampleCells());
  gat[1] = 'X';
  auto parsed = GatMap::Parse(gat);
  REQUIRE_FALSE(parsed.has_value());
  CHECK(parsed.error().code == ErrorCode::kBadSignature);
}

TEST_CASE("GAT : dimensions refusées", "[formats][gat]") {
  SECTION("largeur nulle") {
    auto parsed = GatMap::Parse(BuildGat(0, 2, {}));
    REQUIRE_FALSE(parsed.has_value());
    CHECK(parsed.error().code == ErrorCode::kCorruptData);
  }

  SECTION("dimension négative") {
    auto parsed = GatMap::Parse(BuildGat(-4, 2, {}));
    REQUIRE_FALSE(parsed.has_value());
    CHECK(parsed.error().code == ErrorCode::kCorruptData);
  }

  SECTION("dimension absurde") {
    // Le plafond existe pour refuser une allocation démesurée AVANT de la
    // tenter : 100000 x 100000 cellules à 20 octets font 200 Gio.
    auto parsed = GatMap::Parse(BuildGat(100000, 100000, {}));
    REQUIRE_FALSE(parsed.has_value());
    CHECK(parsed.error().code == ErrorCode::kCorruptData);
  }
}

TEST_CASE("GAT : dimensions annoncées plus grandes que le fichier", "[formats][gat]") {
  // L'en-tête promet 100 cellules, le corps n'en porte que 6.
  auto parsed = GatMap::Parse(BuildGat(10, 10, SampleCells()));
  REQUIRE_FALSE(parsed.has_value());
  CHECK(parsed.error().code == ErrorCode::kUnexpectedEnd);
}

TEST_CASE("GAT : aucune troncature ne fait planter le parseur", "[formats][gat]") {
  const std::vector<std::uint8_t> complete = BuildGat(3, 2, SampleCells());
  for (std::size_t length = 0; length <= complete.size(); ++length) {
    const std::vector<std::uint8_t> truncated(
        complete.begin(), complete.begin() + static_cast<std::ptrdiff_t>(length));
    auto parsed = GatMap::Parse(truncated);
    if (parsed.has_value()) {
      // Ouvert malgré la troncature : toutes les cellules doivent être là.
      CHECK(parsed->cells().size() ==
            static_cast<std::size_t>(parsed->width()) * static_cast<std::size_t>(parsed->height()));
    }
  }
  SUCCEED("aucune troncature n'a provoqué de lecture hors bornes");
}

TEST_CASE("DeriveServerCells : sans eau, le type passe tel quel", "[formats][gat][parité]") {
  auto gat = GatMap::Parse(BuildGat(3, 2, SampleCells()));
  REQUIRE(gat.has_value());

  const std::vector<std::uint8_t> cells = DeriveServerCells(*gat, WaterField::None());
  REQUIRE(cells.size() == 6);
  CHECK(cells[0] == 0);
  CHECK(cells[1] == 1);
  CHECK(cells[2] == 0);
  CHECK(cells[3] == 5);
  CHECK(cells[4] == 6);
  CHECK(cells[5] == 3);
}

TEST_CASE("DeriveServerCells : une cellule marchable sous l'eau devient 3",
          "[formats][gat][parité]") {
  // Règle littérale de read_map() : type == 0 ET hauteur du coin bas-gauche
  // STRICTEMENT supérieure au niveau d'eau. Les autres types ne bougent pas.
  const std::vector<GatCellSpec> cells = {
      {{5.0F, 0.0F, 0.0F, 0.0F}, 0},  // marchable, sous l'eau -> 3
      {{5.0F, 0.0F, 0.0F, 0.0F}, 1},  // bloquée, sous l'eau -> reste 1
      {{1.0F, 0.0F, 0.0F, 0.0F}, 0},  // marchable, au-dessus -> reste 0
      {{2.0F, 0.0F, 0.0F, 0.0F}, 0},  // pile au niveau -> reste 0 (comparaison stricte)
  };
  auto gat = GatMap::Parse(BuildGat(4, 1, cells));
  REQUIRE(gat.has_value());

  const std::vector<std::uint8_t> derived = DeriveServerCells(*gat, WaterField::Uniform(2.0F));
  CHECK(derived[0] == 3);
  CHECK(derived[1] == 1);
  CHECK(derived[2] == 0);
  CHECK(derived[3] == 0);
}

TEST_CASE("DeriveServerCells : les deux règles de niveau d'eau", "[formats][gat][parité]") {
  // Le cœur de docs/adr/0004. Avec un niveau d'eau NON entier, tronquer ou non
  // change le classement, et les deux sens sont observables :
  //   · niveau -0.5 : une cellule à -0.2 est sous l'eau au brut, pas au tronqué
  //     (qui vaut 0) — c'est le cas majoritaire, 110 694 cellules mesurées ;
  //   · niveau  2.7 : une cellule à 2.4 est sous l'eau au tronqué (2), pas au
  //     brut — c'est le cas minoritaire, 20 cellules mesurées.
  const std::vector<GatCellSpec> cells = {{{-0.2F, 0.0F, 0.0F, 0.0F}, 0}};
  auto gat = GatMap::Parse(BuildGat(1, 1, cells));
  REQUIRE(gat.has_value());

  CHECK(DeriveServerCells(*gat, WaterField::Uniform(-0.5F), WaterLevelRule::kRawLevel)[0] == 3);
  CHECK(DeriveServerCells(*gat, WaterField::Uniform(-0.5F), WaterLevelRule::kTruncatedLevel)[0] ==
        0);

  const std::vector<GatCellSpec> above = {{{2.4F, 0.0F, 0.0F, 0.0F}, 0}};
  auto high = GatMap::Parse(BuildGat(1, 1, above));
  REQUIRE(high.has_value());

  CHECK(DeriveServerCells(*high, WaterField::Uniform(2.7F), WaterLevelRule::kRawLevel)[0] == 0);
  CHECK(DeriveServerCells(*high, WaterField::Uniform(2.7F), WaterLevelRule::kTruncatedLevel)[0] ==
        3);
}

TEST_CASE("DeriveServerCells : la règle par défaut est le niveau brut", "[formats][gat][parité]") {
  // Défaut choisi parce que c'est ce que contient le map_cache.dat que le
  // serveur charge réellement. Le verrouiller évite qu'un changement de défaut
  // passe inaperçu.
  const std::vector<GatCellSpec> cells = {{{-0.2F, 0.0F, 0.0F, 0.0F}, 0}};
  auto gat = GatMap::Parse(BuildGat(1, 1, cells));
  REQUIRE(gat.has_value());

  CHECK(DeriveServerCells(*gat, WaterField::Uniform(-0.5F))[0] == 3);
}

TEST_CASE("DeriveServerCells : seul le coin bas-gauche compte", "[formats][gat][parité]") {
  // Les trois autres coins sont bien au-dessus de l'eau ; le serveur les ignore.
  const std::vector<GatCellSpec> cells = {{{0.0F, 99.0F, 99.0F, 99.0F}, 0}};
  auto gat = GatMap::Parse(BuildGat(1, 1, cells));
  REQUIRE(gat.has_value());

  CHECK(DeriveServerCells(*gat, WaterField::Uniform(50.0F))[0] == 0);
}

TEST_CASE("DeriveServerCells : le bit de poids fort marque l'eau", "[formats][gat][parité]") {
  // Relevé sur 300 cartes réelles confrontées au cache du serveur : 0x80000000
  // vaut 3, tandis que 0x80000001, 0x80000002 et 0x80000005 gardent leur type
  // de base. Le drapeau convertit une cellule MARCHABLE en eau marchable, et
  // n'a aucun effet ailleurs. Sans cette règle, 51 cartes divergeaient.
  const std::vector<GatCellSpec> flagged = {
      {{-99.0F, 0.0F, 0.0F, 0.0F}, static_cast<std::int32_t>(0x80000000U)},
      {{-99.0F, 0.0F, 0.0F, 0.0F}, static_cast<std::int32_t>(0x80000001U)},
      {{-99.0F, 0.0F, 0.0F, 0.0F}, static_cast<std::int32_t>(0x80000002U)},
      {{-99.0F, 0.0F, 0.0F, 0.0F}, static_cast<std::int32_t>(0x80000005U)},
  };
  auto gat = GatMap::Parse(BuildGat(4, 1, flagged));
  REQUIRE(gat.has_value());

  // Aucune eau déclarée, hauteur très basse : seul le drapeau peut agir.
  const std::vector<std::uint8_t> derived = DeriveServerCells(*gat, WaterField::None());
  CHECK(derived[0] == 3);
  CHECK(derived[1] == 1);
  CHECK(derived[2] == 2);
  CHECK(derived[3] == 5);
}

TEST_CASE("DeriveServerCells : le drapeau d'eau l'emporte sur le niveau",
          "[formats][gat][parité]") {
  // Une cellule marquée est de l'eau même si sa hauteur la placerait bien
  // au-dessus du niveau : l'éditeur de carte a tranché, on ne recalcule pas.
  const std::vector<GatCellSpec> cells = {
      {{-500.0F, 0.0F, 0.0F, 0.0F}, static_cast<std::int32_t>(0x80000000U)}};
  auto gat = GatMap::Parse(BuildGat(1, 1, cells));
  REQUIRE(gat.has_value());

  CHECK(DeriveServerCells(*gat, WaterField::Uniform(0.0F))[0] == 3);
}

TEST_CASE("DeriveServerCells : les types marqués gardent leur valeur de base",
          "[formats][gat][parité]") {
  // Le drapeau retiré, le type restant est tronqué à un octet comme le fait le
  // `(unsigned char)type` du serveur.
  const std::vector<GatCellSpec> cells = {
      {{0.0F, 0.0F, 0.0F, 0.0F}, static_cast<std::int32_t>(0x80000003U)},
      {{0.0F, 0.0F, 0.0F, 0.0F}, static_cast<std::int32_t>(0xFFFFFF01U)},
  };
  auto gat = GatMap::Parse(BuildGat(2, 1, cells));
  REQUIRE(gat.has_value());

  // La valeur BRUTE est conservée par le parseur…
  CHECK(gat->at(0, 0).type == static_cast<std::int32_t>(0x80000003U));
  // …et seule la dérivation serveur tronque.
  const std::vector<std::uint8_t> derived = DeriveServerCells(*gat, WaterField::None());
  CHECK(derived[0] == 3);
  CHECK(derived[1] == 1);
}
