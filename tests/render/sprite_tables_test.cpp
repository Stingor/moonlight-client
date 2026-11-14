// LES TABLES DE SPRITES : la forme stricte que le script écrit, ce qu'elle
// range, et tout ce qu'elle refuse.

#include "ro/render/sprite_tables.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

#include "support/temp_directory.h"

using ro::core::ErrorCode;
using ro::render::GarmentLayerFrames;
using ro::render::ParseSpriteTableText;
using ro::render::ReadSpriteTables;
using ro::render::SpriteTableFile;
using ro::render::SpriteTables;
using ro::test_support::TempDirectory;
using ro::vfs::Vfs;

namespace {

// La forme exacte que `sprite_tables.py` écrit : en-tête commenté, version,
// une section, une entrée par ligne.
constexpr std::string_view kClasses =
    "# en-tête\n"
    "#\n"
    "version: 1\n"
    "classes:\n"
    "  13: {shadow: 1.5}\n"
    "  45: {sprite: \"1_ETC_01\", shadow: 0}\n"
    "  2075: {sprite: \"E_VADON_X\", radius: 2}\n"
    "  6017: {sprite: \"여\\\\활용병\"}\n";

[[nodiscard]] std::string Refusal(SpriteTableFile file, std::string_view text) {
  SpriteTables tables;
  const auto parsed = ParseSpriteTableText(file, text, tables);
  REQUIRE_FALSE(parsed.has_value());
  CHECK(parsed.error().code == ErrorCode::kCorruptData);
  return parsed.error().detail;
}

}  // namespace

TEST_CASE("Tables de sprites : les classes, leur sprite, leur rayon et leur ombre",
          "[render][sprite-tables]") {
  SpriteTables tables;
  REQUIRE(ParseSpriteTableText(SpriteTableFile::kClasses, kClasses, tables).has_value());
  CHECK(tables.class_sprites.size() == 3);
  CHECK(tables.class_sprites.at(45) == "1_ETC_01");
  // L'échappement de JSON se résout : un seul antislash, entre deux dossiers.
  CHECK(tables.class_sprites.at(6017) == "여\\활용병");
  CHECK(tables.class_radii == std::unordered_map<std::uint32_t, double>{{2075, 2.0}});
  CHECK(tables.class_shadows == std::unordered_map<std::uint32_t, double>{{13, 1.5}, {45, 0.0}});
}

TEST_CASE("Tables de sprites : armes, coiffes et robes", "[render][sprite-tables]") {
  SpriteTables tables;
  REQUIRE(ParseSpriteTableText(SpriteTableFile::kWeapons,
                               "version: 1\nweapons:\n"
                               "  0: {symbol: \"WEAPONTYPE_NONE\", sprite: \"\", hit_sound: "
                               "\"_hit_mace.wav\"}\n"
                               "  17: {sprite: \"_권총\", hit_sound: \"_hit_권총.wav\"}\n"
                               "  31: {symbol: \"WEAPONTYPE_Main_Gauche\", sprite: \"_1207\", "
                               "family: 1}\n",
                               tables)
              .has_value());
  CHECK(tables.weapon_sprites.at(0).empty());
  CHECK(tables.weapon_symbols.at(31) == "WEAPONTYPE_Main_Gauche");
  CHECK(tables.weapon_families == std::unordered_map<std::uint32_t, std::uint32_t>{{31, 1}});
  // LE BRUIT RESTE EN CP949, comme le client le donne : « _hit_권총.wav ».
  CHECK(tables.weapon_hit_sounds.at(17) == "_hit_\xB1\xC7\xC3\xD1.wav");
  CHECK(tables.weapon_hit_sounds.at(0) == "_hit_mace.wav");

  REQUIRE(ParseSpriteTableText(SpriteTableFile::kHeadgears,
                               "version: 1\nheadgears:\n  1: {symbol: \"ACCESSORY_GOGGLES\", "
                               "sprite: \"_고글\"}\n  40: {sprite: \"_\\u00e9t\\u00e9\"}\n",
                               tables)
              .has_value());
  CHECK(tables.headgear_sprites.at(1) == "_고글");
  CHECK(tables.headgear_sprites.at(40) == "_été");
  CHECK_FALSE(tables.headgear_symbols.contains(40));

  REQUIRE(ParseSpriteTableText(SpriteTableFile::kGarments,
                               "version: 1\r\ngarments:\r\n  1: {symbol: \"ROBE_WINGS\", "
                               "sprite: \"천사날개\"}\r\n  2: {sprite: \"sac\", small_layer: "
                               "true}\r\n  3: {sprite: \"x\", small_layer: false}\r\n",
                               tables)
              .has_value());
  CHECK(tables.garment_sprites.at(1) == "천사날개");
  CHECK(tables.small_layer_garments == std::unordered_set<std::uint32_t>{2});
}

TEST_CASE("Tables de sprites : les calques des capes", "[render][sprite-tables]") {
  SpriteTables tables;
  REQUIRE(ParseSpriteTableText(SpriteTableFile::kLayers,
                               "version: 1\n"
                               "inherited_jobs:\n  27: 26\n  4001: 0\n"
                               "big_female:\n  0: {0: [0, 1, 2], 8: [3, 3]}\n  1: {}\n"
                               "big_male:\n  7: {5: []}\n"
                               "small_female:\n"
                               "small_male:\n  4: {16: [0]}\n",
                               tables)
              .has_value());
  CHECK(tables.layer_inherited_jobs ==
        std::unordered_map<std::uint32_t, std::uint32_t>{{27, 26}, {4001, 0}});
  // L'ordre et les doublons d'une liste restent ceux du fichier.
  CHECK(tables.big_female_layers.at(0) == GarmentLayerFrames{{0, {0, 1, 2}}, {8, {3, 3}}});
  CHECK(tables.big_female_layers.at(1).empty());
  CHECK(tables.big_male_layers.at(7) == GarmentLayerFrames{{5, {}}});
  CHECK(tables.small_female_layers.empty());
  CHECK(tables.small_male_layers.at(4) == GarmentLayerFrames{{16, {0}}});
}

TEST_CASE("Tables de sprites : tout texte d'une autre forme se refuse, avec sa ligne",
          "[render][sprite-tables]") {
  // Un texte sans guillemets : du YAML valide, mais pas la forme.
  CHECK(Refusal(SpriteTableFile::kClasses, "version: 1\nclasses:\n  1: {sprite: Poring}\n")
            .find("ligne 3") != std::string::npos);
  CHECK(Refusal(SpriteTableFile::kClasses, "version: 2\nclasses:\n").find("version") !=
        std::string::npos);
  CHECK(Refusal(SpriteTableFile::kClasses, "classes:\n  1: {shadow: 1}\n").find("version") !=
        std::string::npos);
  CHECK(Refusal(SpriteTableFile::kClasses, "version: 1\n  1: {shadow: 1}\n")
            .find("hors de toute section") != std::string::npos);
  CHECK(Refusal(SpriteTableFile::kClasses, "version: 1\nmonstres:\n  1: {shadow: 1}\n")
            .find("monstres") != std::string::npos);
  CHECK(Refusal(SpriteTableFile::kClasses, "version: 1\nclasses:\n  1: {taille: 1}\n")
            .find("taille") != std::string::npos);
  CHECK(Refusal(SpriteTableFile::kClasses, "version: 1\nclasses:\n  1: {shadow: \"1\"}\n")
            .find("shadow") != std::string::npos);
  CHECK(Refusal(SpriteTableFile::kClasses,
                "version: 1\nclasses:\n  1: {shadow: 1}\n  1: {sprite: \"x\"}\n")
            .find("redite") != std::string::npos);
  CHECK(Refusal(SpriteTableFile::kClasses, "version: 1\nclasses:\n  1: {shadow: 1, shadow: 2}\n")
            .find("redit") != std::string::npos);
  CHECK(Refusal(SpriteTableFile::kClasses, "version: 1\nclasses:\n  1: {shadow: 1} x\n")
            .find("nom:") != std::string::npos);
  CHECK(Refusal(SpriteTableFile::kClasses, "version: 1\nclasses:\n  -1: {shadow: 1}\n")
            .find("ligne 3") != std::string::npos);
  CHECK(Refusal(SpriteTableFile::kClasses, "version: 1\nclasses:\n  1: {sprite: \"\\x41\"}\n")
            .find("ligne 3") != std::string::npos);
  CHECK(Refusal(SpriteTableFile::kWeapons, "version: 1\nweapons:\n  31: {family: 1.5}\n")
            .find("family") != std::string::npos);
  CHECK(Refusal(SpriteTableFile::kWeapons, "version: 1\nweapons:\n  31: {family: -1}\n")
            .find("family") != std::string::npos);
  CHECK(Refusal(SpriteTableFile::kLayers, "version: 1\nbig_male:\n  7: {5: [1, -2]}\n")
            .find("valeur illisible") != std::string::npos);
  CHECK(Refusal(SpriteTableFile::kLayers, "version: 1\nbig_male:\n  7: {5: [1], 05: [2]}\n")
            .find("redit") != std::string::npos);
  CHECK(Refusal(SpriteTableFile::kLayers, "version: 1\nbig_male:\n  7: {debout: [1]}\n")
            .find("debout") != std::string::npos);
  CHECK(Refusal(SpriteTableFile::kLayers, "version: 1\ninherited_jobs:\n  27: {x: 1}\n")
            .find("hérité") != std::string::npos);
  // Le fichier se nomme dans le refus.
  CHECK(Refusal(SpriteTableFile::kGarments, "version: 1\ngarments:\n  1: 2\n")
            .find("garment_sprites.yml") != std::string::npos);
}

TEST_CASE("Tables de sprites : un fichier absent laisse sa table vide, un illisible refuse tout",
          "[render][sprite-tables]") {
  TempDirectory client;
  client.WriteText("data/yaml_files/class_sprites.yml", kClasses);
  client.WriteText("data/yaml_files/pet_info.yml",
                   "version: 1\nspecies:\n  1002: \"Poring\"\naccessories:\n"
                   "  10013: \"Poring_고글.ACT\"\n");
  auto vfs = Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());
  auto tables = ReadSpriteTables(*vfs);
  REQUIRE(tables.has_value());
  CHECK(tables->class_sprites.at(45) == "1_ETC_01");
  CHECK(tables->headgear_sprites.empty());
  CHECK(tables->big_male_layers.empty());
  // LE .ACT D'UN ACCESSOIRE DE FAMILIER vient de la table des familiers.
  CHECK(tables->pet_accessory_acts ==
        std::unordered_map<std::uint32_t, std::string>{{10013, "Poring_고글.ACT"}});

  client.WriteText("data/yaml_files/weapon_sprites.yml", "version: 1\nweapons:\n  1: {x}\n");
  auto refused = ReadSpriteTables(*vfs);
  REQUIRE_FALSE(refused.has_value());
  CHECK(refused.error().detail.find("weapon_sprites.yml, ligne 3") != std::string::npos);

  client.WriteText("data/yaml_files/weapon_sprites.yml", "version: 1\nweapons:\n");
  client.WriteText("data/yaml_files/pet_info.yml", "version: 7\n");
  auto refused_pets = ReadSpriteTables(*vfs);
  REQUIRE_FALSE(refused_pets.has_value());
  CHECK(refused_pets.error().detail.find("pet_info.yml") != std::string::npos);
}
