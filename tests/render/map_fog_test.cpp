#include "ro/render/map_fog.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using ro::render::FoggedClearColor;
using ro::render::FogTableKeyOf;
using ro::render::MapFog;
using ro::render::MapFogTable;
using ro::render::SceneFog;
using ro::render::SceneFogOf;

namespace {

// Ce que la conversion tire d'un extrait de la table du client, tel qu'il
// s'écrit — commentaires, lignes vides, fins de ligne Windows :
//
//   //commentaire sans diese
//   prontera.rsw#  0.23#  0.9#  0xffffffff#  0.3#     (un mot par ligne)
//   1@exds.rsw#    0.23#  0.9#  0x4499ccff#  0.3#
//   note sans diese
//   carte.rsw#1#2#0x10#3#
//   seule.rsw#1#2#0x0#                             sa quatrième valeur manque
//
// La couleur « 0x10 » saute ses deux premiers caractères : il ne reste que
// « 10 ».
constexpr std::string_view kConverted =
    "version: 1\n"
    "maps:\n"
    "  1@exds:\n"
    "    near: 0.23\n"
    "    far: 0.9\n"
    "    color: [68, 153, 204, 255]\n"
    "    density: 0.3\n"
    "  carte:\n"
    "    near: 1.0\n"
    "    far: 2.0\n"
    "    color: [0, 0, 0, 16]\n"
    "    density: 3.0\n"
    "  prontera:\n"
    "    near: 0.23\n"
    "    far: 0.9\n"
    "    color: [255, 255, 255, 255]\n"
    "    density: 0.3\n";

[[nodiscard]] MapFogTable Parsed(std::string_view yaml_text) {
  auto table = MapFogTable::Parse(yaml_text);
  REQUIRE(table.has_value());
  return std::move(*table);
}

}  // namespace

TEST_CASE("Brouillard : la table convertie garde les entrées complètes", "[render][fog]") {
  const MapFogTable table = Parsed(kConverted);
  CHECK(table.size() == 3);
  CHECK_FALSE(table.FogOf("seule").has_value());
  const std::optional<MapFog> carte = table.FogOf("carte");
  REQUIRE(carte.has_value());
  CHECK(carte->color_argb == 0x10U);
  CHECK(carte->density == 3.0F);
}

TEST_CASE("Brouillard : une fraction se lit comme le client la lisait", "[render][fog]") {
  // Le client lisait un double, puis le rangeait en float.
  const std::optional<MapFog> fog = Parsed(kConverted).FogOf("prontera");
  REQUIRE(fog.has_value());
  CHECK(fog->near_fraction == static_cast<float>(0.23));
  CHECK(fog->far_fraction == static_cast<float>(0.9));
}

TEST_CASE("Brouillard : une table mal formée est refusée", "[render][fog]") {
  CHECK_FALSE(MapFogTable::Parse("version: 1\nmaps: []\n").has_value());
  CHECK_FALSE(MapFogTable::Parse("version: 2\nmaps: {}\n").has_value());
  CHECK_FALSE(MapFogTable::Parse(
                  "version: 1\nmaps:\n  a: {near: 0, far: 1, color: [1, 2, 3], density: 0}\n")
                  .has_value());
  CHECK_FALSE(MapFogTable::Parse("version: 1\nmaps:\n  a: {near: 0, far: 1, color: [1, 2, 3, 256], "
                                 "density: 0}\n")
                  .has_value());
  CHECK_FALSE(
      MapFogTable::Parse("version: 1\nmaps:\n  a: {near: 0, color: [1, 2, 3, 4]}\n").has_value());
}

TEST_CASE("Brouillard : une carte d'instance perd son numéro", "[render][fog]") {
  CHECK(FogTableKeyOf("prontera") == "prontera.rsw");
  CHECK(FogTableKeyOf("0011@exds") == "1@exds.rsw");
  // Un `@` trop près du début ne retranche rien.
  CHECK(FogTableKeyOf("1@exds") == "1@exds.rsw");
}

TEST_CASE("Brouillard : une carte se cherche à la lettre", "[render][fog]") {
  const MapFogTable table = Parsed(kConverted);
  const std::optional<MapFog> fog = table.FogOf("prontera");
  REQUIRE(fog.has_value());
  CHECK(fog->near_fraction == Approx(0.23F));
  CHECK(fog->far_fraction == Approx(0.9F));
  CHECK(fog->color_argb == 0xFFFFFFFFU);
  CHECK(fog->density == Approx(0.3F));
  CHECK(table.FogOf("1@exds")->color_argb == 0x4499CCFFU);
  CHECK(table.FogOf("0021@exds").has_value());
  CHECK_FALSE(table.FogOf("Prontera").has_value());
  CHECK_FALSE(table.FogOf("geffen").has_value());
}

TEST_CASE("Brouillard : les fractions se placent sur la vue du client", "[render][fog]") {
  // Dix unités devant l'œil, mille cinq cents au plus loin.
  const SceneFog fog = SceneFogOf(MapFog{
      .near_fraction = 0.23F, .far_fraction = 0.9F, .color_argb = 0xFF102030U, .density = 0.3F});
  CHECK(fog.on);
  CHECK(fog.start_depth == Approx(10.0F + (0.23F * 1490.0F)));
  CHECK(fog.end_depth == Approx(10.0F + (0.9F * 1490.0F)));
  CHECK(fog.color[0] == Approx(16.0F / 255.0F));
  CHECK(fog.color[1] == Approx(32.0F / 255.0F));
  CHECK(fog.color[2] == Approx(48.0F / 255.0F));
}

TEST_CASE("Brouillard : deux fractions égales couvrent toute la vue", "[render][fog]") {
  // Le client ne soustrait alors pas le début : la plage entière porte le
  // brouillard, quelle que soit la fraction.
  const SceneFog fog = SceneFogOf(MapFog{.near_fraction = 0.5F, .far_fraction = 0.5F});
  CHECK(fog.start_depth == Approx(10.0F));
  CHECK(fog.end_depth == Approx(1500.0F));
}

TEST_CASE("Brouillard : le fond s'y mêle par son alpha, tronqué à l'octet", "[render][fog]") {
  // Alpha 0x44 : 68 / 255 du brouillard. Le fond (7, 7, 24) de moc_akhet.
  const MapFog fog{.color_argb = 0x4499CCFFU};
  const std::array<float, 4> clear = {7.0F / 255.0F, 7.0F / 255.0F, 24.0F / 255.0F, 1.0F};
  const std::array<float, 4> mixed = FoggedClearColor(clear, fog);
  constexpr float kWeight = 68.0F / 255.0F;
  CHECK(mixed[0] == Approx(static_cast<float>(
                               static_cast<int>((7.0F * (1.0F - kWeight)) + (153.0F * kWeight))) /
                           255.0F));
  CHECK(mixed[1] == Approx(static_cast<float>(
                               static_cast<int>((7.0F * (1.0F - kWeight)) + (204.0F * kWeight))) /
                           255.0F));
  CHECK(mixed[2] == Approx(static_cast<float>(
                               static_cast<int>((24.0F * (1.0F - kWeight)) + (255.0F * kWeight))) /
                           255.0F));
  CHECK(mixed[3] == 1.0F);
}
