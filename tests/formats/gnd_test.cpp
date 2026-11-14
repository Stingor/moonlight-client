#include "ro/formats/gnd.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"

#include "support/map_builder.h"

using ro::core::ErrorCode;
using ro::formats::GndMap;
using ro::test_support::BuildGnd;
using ro::test_support::GndBuildOptions;
using ro::test_support::GndCubeSpec;

TEST_CASE("GND : en-tête et cubes", "[formats][gnd]") {
  GndBuildOptions options;
  options.width = 3;
  options.height = 2;
  options.texture_paths = {"herbe.bmp", "pierre.bmp"};
  options.cubes = {
      {{1.0F, 2.0F, 3.0F, 4.0F}, 0, -1, -1},  {{5.0F, 6.0F, 7.0F, 8.0F}, 1, 2, 3},
      {{0.0F, 0.0F, 0.0F, 0.0F}, -1, -1, -1}, {{9.0F, 9.0F, 9.0F, 9.0F}, 4, -1, -1},
      {{0.5F, 0.5F, 0.5F, 0.5F}, 5, -1, -1},  {{1.5F, 1.5F, 1.5F, 1.5F}, 6, -1, -1},
  };

  auto gnd = GndMap::Parse(BuildGnd(options));
  REQUIRE(gnd.has_value());

  CHECK(gnd->width() == 3);
  CHECK(gnd->height() == 2);
  CHECK(gnd->version() == 0x0109);
  CHECK(gnd->proportion_ratio() == 10.0F);

  REQUIRE(gnd->texture_paths().size() == 2);
  CHECK(gnd->texture_paths()[0] == "herbe.bmp");
  CHECK(gnd->texture_paths()[1] == "pierre.bmp");

  REQUIRE(gnd->cubes().size() == 6);
  CHECK(gnd->at(0, 0).corner_height[0] == 1.0F);
  CHECK(gnd->at(0, 0).corner_height[3] == 4.0F);
  CHECK(gnd->at(1, 0).tile_up == 1);
  CHECK(gnd->at(1, 0).tile_front == 2);
  CHECK(gnd->at(1, 0).tile_side == 3);
  CHECK(gnd->at(2, 0).tile_up == -1);
  CHECK(gnd->at(0, 1).tile_up == 4);
}

TEST_CASE("GND : le champ « cellules par lightmap » est ignoré", "[formats][gnd]") {
  // GRFEditor lit ce champ puis le force à 1, et calcule la taille des données
  // avec 1. Un parseur qui s'y fierait décalerait TOUT ce qui suit — tuiles,
  // cubes, eau. Ce test met une valeur mensongère et vérifie que la suite du
  // fichier reste correctement placée.
  GndBuildOptions options;
  options.lightmap_count = 3;
  options.lightmap_width = 8;
  options.lightmap_height = 8;
  options.lightmap_cells = 4;  // mensonger : les données restent dimensionnées à 1
  options.tile_count = 2;
  options.water_levels = {42.0F};

  auto gnd = GndMap::Parse(BuildGnd(options));
  REQUIRE(gnd.has_value());

  CHECK(gnd->lightmap_count() == 3);
  CHECK(gnd->lightmap_data().size() == 3 * 8 * 8 * 4);
  CHECK(gnd->tiles().size() == 2);
  REQUIRE(gnd->water_zones().size() == 1);
  CHECK(gnd->water_zones()[0].level == 42.0F);
}

TEST_CASE("GND : les tuiles rangent quatre U puis quatre V", "[formats][gnd]") {
  GndBuildOptions options;
  options.tile_count = 1;

  auto gnd = GndMap::Parse(BuildGnd(options));
  REQUIRE(gnd.has_value());
  REQUIRE(gnd->tiles().size() == 1);

  const auto& tile = gnd->tiles().front();
  CHECK(tile.u == std::array<float, 4>{0.0F, 1.0F, 2.0F, 3.0F});
  CHECK(tile.v == std::array<float, 4>{0.0F, 2.0F, 4.0F, 6.0F});
  // Les quatre octets de couleur sont rendus DANS L'ORDRE DU FICHIER : bleu,
  // vert, rouge, alpha. Les nommer est l'affaire de qui les emploie, pas du
  // parseur, dont le travail est de ne rien réarranger.
  CHECK(tile.color == std::array<std::uint8_t, 4>{0x11, 0x22, 0x33, 0xFF});
}

TEST_CASE("GND : index de tuile étroits avant la version 1.6", "[formats][gnd]") {
  // Jusqu'en 1.5 les trois index tiennent sur 16 bits, suivis de deux octets de
  // bourrage. Se tromper de largeur décale tous les cubes suivants.
  GndBuildOptions options;
  options.minor = 5;
  options.width = 2;
  options.height = 1;
  options.cubes = {{{1.0F, 2.0F, 3.0F, 4.0F}, 7, 8, 9}, {{5.0F, 6.0F, 7.0F, 8.0F}, 10, 11, 12}};

  auto gnd = GndMap::Parse(BuildGnd(options));
  REQUIRE(gnd.has_value());
  REQUIRE(gnd->cubes().size() == 2);

  CHECK(gnd->at(0, 0).tile_up == 7);
  CHECK(gnd->at(0, 0).tile_side == 9);
  // Le second cube n'est juste que si le premier a consommé la bonne longueur.
  CHECK(gnd->at(1, 0).corner_height[0] == 5.0F);
  CHECK(gnd->at(1, 0).tile_up == 10);
}

TEST_CASE("GND : l'eau 1.8 ne porte que le niveau par zone", "[formats][gnd]") {
  // En 1.8 chaque zone n'écrit que son niveau et hérite du reste du jeu de
  // valeurs par défaut qui la précède ; en 1.9 elle est complète.
  GndBuildOptions options;
  options.minor = 8;
  options.water_split_width = 2;
  options.water_split_height = 1;
  options.water_levels = {11.0F, 22.0F};

  auto gnd = GndMap::Parse(BuildGnd(options));
  REQUIRE(gnd.has_value());
  REQUIRE(gnd->water_zones().size() == 2);

  CHECK(gnd->water_zones()[0].level == 11.0F);
  CHECK(gnd->water_zones()[1].level == 22.0F);
  // Hérité du jeu par défaut écrit par le constructeur.
  CHECK(gnd->water_zones()[0].type == 1);
  CHECK(gnd->water_zones()[0].texture_cycling == 7);
}

TEST_CASE("GND : l'eau 1.9 porte un enregistrement complet par zone", "[formats][gnd]") {
  GndBuildOptions options;
  options.minor = 9;
  options.water_split_width = 2;
  options.water_split_height = 2;
  options.water_levels = {1.0F, 2.0F, 3.0F, 4.0F};

  auto gnd = GndMap::Parse(BuildGnd(options));
  REQUIRE(gnd.has_value());
  REQUIRE(gnd->water_zones().size() == 4);

  CHECK(gnd->water_zones()[3].level == 4.0F);
  // Valeurs propres à la zone, pas celles du jeu par défaut.
  CHECK(gnd->water_zones()[0].type == 2);
  CHECK(gnd->water_zones()[0].texture_cycling == 5);
}

TEST_CASE("GND : pas d'eau avant la version 1.8", "[formats][gnd]") {
  GndBuildOptions options;
  options.minor = 7;

  auto gnd = GndMap::Parse(BuildGnd(options));
  REQUIRE(gnd.has_value());
  CHECK(gnd->water_zones().empty());

  auto field = gnd->MakeWaterField();
  REQUIRE(field.has_value());
  CHECK_FALSE(field->has_water());
}

TEST_CASE("GND : MakeWaterField reprend les niveaux par zone", "[formats][gnd]") {
  GndBuildOptions options;
  options.width = 4;
  options.height = 4;
  options.water_split_width = 2;
  options.water_split_height = 2;
  options.water_levels = {10.0F, 20.0F, 30.0F, 40.0F};

  auto gnd = GndMap::Parse(BuildGnd(options));
  REQUIRE(gnd.has_value());

  auto field = gnd->MakeWaterField();
  REQUIRE(field.has_value());
  CHECK(field->zone_count() == 4);
  // Un cube couvre 2 × 2 cellules GAT : la grille de 4 × 4 cubes en couvre 8 × 8.
  CHECK(field->LevelAt(0, 0) == 10.0F);
  CHECK(field->LevelAt(7, 7) == 40.0F);
}

TEST_CASE("GND : signature invalide", "[formats][gnd]") {
  std::vector<std::uint8_t> gnd = BuildGnd(GndBuildOptions{});
  gnd[1] = 'X';
  auto parsed = GndMap::Parse(gnd);
  REQUIRE_FALSE(parsed.has_value());
  CHECK(parsed.error().code == ErrorCode::kBadSignature);
}

TEST_CASE("GND : dimensions refusées", "[formats][gnd]") {
  GndBuildOptions options;
  options.width = 0;
  auto parsed = GndMap::Parse(BuildGnd(options));
  REQUIRE_FALSE(parsed.has_value());
  CHECK(parsed.error().code == ErrorCode::kCorruptData);
}

TEST_CASE("GND : aucune troncature ne fait planter le parseur", "[formats][gnd]") {
  GndBuildOptions options;
  options.texture_paths = {"a.bmp"};
  options.lightmap_count = 1;
  options.tile_count = 2;
  options.water_levels = {5.0F};

  const std::vector<std::uint8_t> complete = BuildGnd(options);
  for (std::size_t length = 0; length <= complete.size(); ++length) {
    const std::vector<std::uint8_t> truncated(
        complete.begin(), complete.begin() + static_cast<std::ptrdiff_t>(length));
    auto parsed = GndMap::Parse(truncated);
    if (parsed.has_value()) {
      CHECK(parsed->cubes().size() ==
            static_cast<std::size_t>(parsed->width()) * static_cast<std::size_t>(parsed->height()));
    }
  }
  SUCCEED("aucune troncature n'a provoqué de lecture hors bornes");
}

namespace {

// Les pièces d'un sol déjà lu : ce qu'un assemblage doit rendre à l'identique.
ro::formats::GndParts PartsOf(const GndMap& gnd) {
  return ro::formats::GndParts{.major_version = gnd.major_version(),
                               .minor_version = gnd.minor_version(),
                               .width = gnd.width(),
                               .height = gnd.height(),
                               .proportion_ratio = gnd.proportion_ratio(),
                               .texture_paths = gnd.texture_paths(),
                               .lightmap_width = gnd.lightmap_width(),
                               .lightmap_height = gnd.lightmap_height(),
                               .lightmap_data = gnd.lightmap_data(),
                               .tiles = gnd.tiles(),
                               .cubes = gnd.cubes(),
                               .water_zones = gnd.water_zones(),
                               .water_split_width = gnd.water_split_width(),
                               .water_split_height = gnd.water_split_height()};
}

// Le fichier écrit par la fabrique : textures, lightmaps, tuiles teintées,
// cubes distincts, deux zones d'eau.
GndBuildOptions WrittenGround() {
  GndBuildOptions options;
  options.width = 3;
  options.height = 2;
  options.texture_paths = {"herbe.bmp", "terre.bmp"};
  options.lightmap_count = 2;
  options.lightmap_shadow = 0x40;
  options.lightmap_rgb = {0x10, 0x20, 0x30};
  options.tile_count = 3;
  options.tile_texture_indices = {0, 1, 1};
  options.tile_colors = {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}};
  options.cubes = {
      {{1.0F, 2.0F, 3.0F, 4.0F}, 0, -1, -1}, {{5.0F, 6.0F, 7.0F, 8.0F}, 1, 2, -1},
      {{0.0F, 0.0F, 0.0F, 0.0F}, 2, -1, 0},  {{9.0F, 9.0F, 9.0F, 9.0F}, -1, -1, -1},
      {{0.5F, 0.5F, 0.5F, 0.5F}, 1, -1, -1}, {{1.5F, 1.5F, 1.5F, 1.5F}, 0, -1, -1},
  };
  options.water_split_width = 2;
  options.water_levels = {3.0F, 4.0F};
  return options;
}

}  // namespace

TEST_CASE("GND : un assemblage rend, champ à champ, ce que Parse lit du fichier",
          "[formats][gnd]") {
  auto parsed = GndMap::Parse(BuildGnd(WrittenGround()));
  REQUIRE(parsed.has_value());
  auto assembled = GndMap::Assemble(PartsOf(*parsed));
  REQUIRE(assembled.has_value());

  CHECK(assembled->version() == parsed->version());
  CHECK(assembled->width() == parsed->width());
  CHECK(assembled->height() == parsed->height());
  CHECK(assembled->proportion_ratio() == parsed->proportion_ratio());
  CHECK(assembled->texture_paths() == parsed->texture_paths());
  CHECK(assembled->lightmap_count() == 2);
  CHECK(assembled->lightmap_count() == parsed->lightmap_count());
  CHECK(assembled->lightmap_width() == parsed->lightmap_width());
  CHECK(assembled->lightmap_height() == parsed->lightmap_height());
  CHECK(assembled->lightmap_data() == parsed->lightmap_data());
  REQUIRE(assembled->tiles().size() == parsed->tiles().size());
  for (std::size_t rank = 0; rank < parsed->tiles().size(); ++rank) {
    const ro::formats::GndTile& left = assembled->tiles()[rank];
    const ro::formats::GndTile& right = parsed->tiles()[rank];
    CHECK(left.u == right.u);
    CHECK(left.v == right.v);
    CHECK(left.texture_index == right.texture_index);
    CHECK(left.lightmap_index == right.lightmap_index);
    CHECK(left.color == right.color);
  }
  REQUIRE(assembled->cubes().size() == parsed->cubes().size());
  for (std::size_t rank = 0; rank < parsed->cubes().size(); ++rank) {
    const ro::formats::GndCube& left = assembled->cubes()[rank];
    const ro::formats::GndCube& right = parsed->cubes()[rank];
    CHECK(left.corner_height == right.corner_height);
    CHECK(left.tile_up == right.tile_up);
    CHECK(left.tile_front == right.tile_front);
    CHECK(left.tile_side == right.tile_side);
  }
  REQUIRE(assembled->water_zones().size() == 2);
  CHECK(assembled->water_zones()[1].level == parsed->water_zones()[1].level);
  CHECK(assembled->water_split_width() == parsed->water_split_width());
  CHECK(assembled->water_split_height() == parsed->water_split_height());
}

TEST_CASE("GND : un assemblage incohérent est refusé comme le fichier le serait",
          "[formats][gnd]") {
  auto parsed = GndMap::Parse(BuildGnd(WrittenGround()));
  REQUIRE(parsed.has_value());
  const auto refused = [](ro::formats::GndParts parts) {
    auto assembled = GndMap::Assemble(std::move(parts));
    REQUIRE_FALSE(assembled.has_value());
    CHECK(assembled.error().code == ErrorCode::kCorruptData);
  };

  ro::formats::GndParts no_width = PartsOf(*parsed);
  no_width.width = 0;
  refused(no_width);

  ro::formats::GndParts missing_cube = PartsOf(*parsed);
  missing_cube.cubes.pop_back();
  refused(missing_cube);

  ro::formats::GndParts torn_lightmap = PartsOf(*parsed);
  torn_lightmap.lightmap_data.pop_back();
  refused(torn_lightmap);

  ro::formats::GndParts missing_zone = PartsOf(*parsed);
  missing_zone.water_zones.pop_back();
  refused(missing_zone);

  ro::formats::GndParts long_path = PartsOf(*parsed);
  constexpr std::size_t kLongestReadPath = 1024;
  long_path.texture_paths.emplace_back(kLongestReadPath, 'a');
  refused(long_path);

  // Avant 1.8, le fichier n'a pas de place pour l'eau.
  ro::formats::GndParts dry_version = PartsOf(*parsed);
  dry_version.minor_version = 7;
  refused(dry_version);
  dry_version.water_zones.clear();
  CHECK(GndMap::Assemble(dry_version).has_value());
}
