// LA TABLE DES EFFETS D'UNE CARTE, lue dans son YAML.
//
// La lecture du client d'origine — choix du lecteur selon la version, champs
// facultatifs, entrées mal formées, bornes des couleurs — s'applique à la
// conversion, et son essai est l'auto-test de `scripts/yaml_tables/map_effects.py`.
// Ici : ce que le YAML pose, ce qu'il laisse aux défauts, et ce qu'il refuse.

#include "ro/render/map_effect_table.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/vfs.h"

#include "support/grf_builder.h"
#include "support/temp_directory.h"

using ro::render::MapEffectTable;
using ro::render::ParseMapEffectTable;

namespace {

[[nodiscard]] MapEffectTable Read(std::string_view yaml_text) {
  auto table = ParseMapEffectTable(yaml_text);
  REQUIRE(table.has_value());
  return std::move(*table);
}

constexpr std::string_view kTable = R"(
version: 1
table_version: 2
emitters:
  1: {max_count: 7, texture: 'un.bmp'}
  0: {max_count: 7, first_direction: [1, 2, 3], second_direction: [4, 5, 6], gravity: [0, -1, 0],
      radius: [7, 8, 9], color: [255, -5, 10, 40], rate: [60, 60], size: [2, 3], life: [4, 5],
      texture: 'zero.bmp', speed: 1.5, blend_source: 5, blend_destination: 2, depth_tested: false,
      flat: true, rotation_degrees: [90, 0, 45], eternal: true, scale: [2, 3]}
winds:
  0: {position: [327, -37.1, -611], rings: 3, alpha_peak: 200, rgb: [140, 141, 240], radius: 70,
      thickness: 50, height: 5, speed: 0.4, full_angle: 360, tilt_x_degrees: -45,
      tilt_z_degrees: 30, blend_source: 7, blend_destination: 2, texture: 'effect\mus_spell.bmp'}
other_tables: {magicfloorInfo: 3}
malformed_emitters: 1
)";

}  // namespace

TEST_CASE("chaque réglage d'émetteur est rangé sous sa clé") {
  const MapEffectTable table = Read(kTable);
  REQUIRE(table.present);
  CHECK(table.version == 2);
  REQUIRE(table.emitters.size() == 2);
  const ro::render::MapEmitterSetting& zero = table.emitters.at(0);
  CHECK(zero.texture == "zero.bmp");
  CHECK(zero.max_count == 7);
  CHECK(zero.first_direction == glm::vec3{1.0F, 2.0F, 3.0F});
  CHECK(zero.second_direction == glm::vec3{4.0F, 5.0F, 6.0F});
  CHECK(zero.gravity == glm::vec3{0.0F, -1.0F, 0.0F});
  CHECK(zero.radius == glm::vec3{7.0F, 8.0F, 9.0F});
  CHECK(zero.color == std::array<std::int32_t, 4>{255, -5, 10, 40});
  CHECK(zero.rate[0] == 60.0F);
  CHECK(zero.size[1] == 3.0F);
  CHECK(zero.life[0] == 4.0F);
  CHECK(zero.speed == 1.5F);
  CHECK(zero.blend_source == 5);
  CHECK(zero.blend_destination == 2);
  CHECK_FALSE(zero.depth_tested);
  CHECK(zero.flat);
  CHECK(zero.eternal);
  CHECK(zero.rotation_degrees == glm::vec3{90.0F, 0.0F, 45.0F});
  CHECK(zero.scale[1] == 3.0F);
  CHECK(table.other_tables.at("magicfloorInfo") == 3);
  CHECK(table.malformed_emitters == 1);
  CHECK(table.malformed_winds == 0);
}

TEST_CASE("un champ que la table ne nomme pas garde le défaut du client") {
  // C'est la forme d'une entrée mal formée, dont la conversion n'a gardé que
  // le début, et celle de tout champ que l'ancien lecteur ne transmettait pas.
  const MapEffectTable table = Read(kTable);
  const ro::render::MapEmitterSetting& one = table.emitters.at(1);
  const ro::render::MapEmitterSetting defaults;
  CHECK(one.texture == "un.bmp");
  CHECK(one.max_count == 7);
  CHECK(one.first_direction == defaults.first_direction);
  CHECK(one.rate == defaults.rate);
  CHECK(one.depth_tested == defaults.depth_tested);
  CHECK(one.eternal == defaults.eternal);
}

TEST_CASE("la couleur du vent se lit alpha d'abord, et sa place compte") {
  const MapEffectTable table = Read(kTable);
  REQUIRE(table.winds.size() == 1);
  const ro::render::MapWindSetting& wind = table.winds.at(0);
  REQUIRE(wind.position.has_value());
  CHECK(*wind.position == glm::vec3{327.0F, -37.1F, -611.0F});
  CHECK(wind.rings == 3);
  CHECK(wind.alpha_peak == 200);
  CHECK(wind.rgb == std::array<std::uint8_t, 3>{140, 141, 240});
  CHECK(wind.radius == 70.0F);
  CHECK(wind.thickness == 50.0F);
  CHECK(wind.height == 5.0F);
  CHECK(wind.speed == 0.4F);
  CHECK(wind.full_angle == 360);
  CHECK(wind.tilt_x_degrees == -45.0F);
  CHECK(wind.tilt_z_degrees == 30.0F);
  CHECK(wind.blend_source == 7);
  CHECK(wind.blend_destination == 2);
  CHECK(wind.texture == "effect\\mus_spell.bmp");
}

TEST_CASE("une texture coréenne se cherche en CP949") {
  const MapEffectTable table =
      Read("version: 1\nemitters:\n  0: {texture: 'effect\\번개4.bmp'}\nwinds: {}\n");
  // Le YAML est en UTF-8 ; les noms du RSW et du VFS sont en CP949.
  CHECK(table.emitters.at(0).texture ==
        "effect\\\xB9\xF8\xB0\xB3"
        "4.bmp");
}

TEST_CASE("une table mal formée est refusée") {
  CHECK_FALSE(ParseMapEffectTable("version: 2\nemitters: {}\n").has_value());
  CHECK_FALSE(ParseMapEffectTable("version: 1\nemitters: {0: {pos: [1, 2, 3]}}\n").has_value());
  CHECK_FALSE(ParseMapEffectTable("version: 1\nemitters: {0: {gravity: [1, 2]}}\n").has_value());
  CHECK_FALSE(ParseMapEffectTable("version: 1\nwinds: {0: {alpha_peak: 300}}\n").has_value());
  CHECK_FALSE(ParseMapEffectTable("version: 1\nemitters: {0: 3}\n").has_value());
  CHECK_FALSE(ParseMapEffectTable("version: 1\nemitters: [1]\n").has_value());
  CHECK_FALSE(ParseMapEffectTable("[").has_value());
  // Une table sans réglage est présente, et vide.
  const MapEffectTable empty = Read("version: 1\n");
  CHECK(empty.present);
  CHECK(empty.emitters.empty());
}

TEST_CASE("la table d'une carte se lit dans son fichier, le @ compris") {
  CHECK(ro::render::MapEffectTablePath("1@adv") == "data\\yaml_files\\map_effects\\1@adv.yml");
  const ro::test_support::TempDirectory client;
  client.WriteFile(
      "effets.grf",
      ro::test_support::BuildGrf(
          {{ro::render::MapEffectTablePath("1@adv"),
            ro::test_support::Bytes("version: 1\nemitters: {0: {texture: 'i.bmp'}}\n")},
           {ro::render::MapEffectTablePath("brisee"),
            ro::test_support::Bytes("version: 1\nemitters: {0: {inconnu: 1}}\n")}}));
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"effets.grf"}});
  REQUIRE(vfs.has_value());

  const MapEffectTable found = ro::render::LoadMapEffectTable(*vfs, "1@adv");
  CHECK(found.present);
  CHECK(found.emitters.at(0).texture == "i.bmp");
  // Absente ou illisible : rien ne se règle.
  CHECK_FALSE(ro::render::LoadMapEffectTable(*vfs, "prontera").present);
  CHECK_FALSE(ro::render::LoadMapEffectTable(*vfs, "brisee").present);
}
