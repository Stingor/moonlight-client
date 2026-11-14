// Le fichier des presets : des clés courtes et figées, tous les personnages
// dans le même fichier. Ce qu'il perd ne se voit qu'à la partie
// suivante.

#include "character/equip_preset_file.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <SDL3/SDL_keycode.h>

#include <catch2/catch_test_macros.hpp>

#include "support/temp_directory.h"

namespace {

using moonclient::Binding;
using moonclient::EquipPreset;
using moonclient::EquipPresetFilePath;
using moonclient::KeyOfVirtualKey;
using moonclient::LoadEquipPresets;
using moonclient::PresetFamily;
using moonclient::PresetPiece;
using moonclient::SaveEquipPresets;
using moonclient::VirtualKeyOf;

constexpr std::uint32_t kMine = 150000;
constexpr std::uint32_t kOther = 150001;
// F5, en code de touche virtuelle de Windows.
constexpr int kVirtualF5 = 0x74;

[[nodiscard]] std::string Contents(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

[[nodiscard]] std::filesystem::path OursRelative() {
  return std::filesystem::path("savedata") / "equip_presets.yml";
}

[[nodiscard]] EquipPreset Sample() {
  EquipPreset preset{.name = "PvM", .with_costumes = true};
  preset.hotkey = Binding{.key = SDLK_F5, .ctrl = true, .alt = true};
  preset.pieces.push_back(PresetPiece{.item_id = 1201,
                                      .refine_level = 7,
                                      .grade = 2,
                                      .cards = {4001, 4002, 0, 4003},
                                      .left_hand = true});
  preset.pieces.push_back(PresetPiece{.item_id = 19500, .family = PresetFamily::kCostume});
  preset.pieces.push_back(PresetPiece{.item_id = 1750, .family = PresetFamily::kAmmo});
  return preset;
}

}  // namespace

TEST_CASE("Fichier des presets : ce qui s'écrit se relit", "[app][presets]") {
  const ro::test_support::TempDirectory folder;
  const std::vector<EquipPreset> written = {Sample(), EquipPreset{.name = "Vide"}};
  REQUIRE(SaveEquipPresets(folder.path(), kMine, written));

  const std::vector<EquipPreset> read = LoadEquipPresets(folder.path(), kMine);
  REQUIRE(read.size() == 2);
  CHECK(read[0].name == "PvM");
  CHECK(read[0].with_costumes);
  CHECK(read[0].hotkey == written[0].hotkey);
  CHECK(read[0].pieces == written[0].pieces);
  CHECK(read[1].name == "Vide");
  CHECK(read[1].pieces.empty());
  CHECK_FALSE(read[1].hotkey.bound());
  // Un autre personnage n'en a aucun.
  CHECK(LoadEquipPresets(folder.path(), kOther).empty());
}

TEST_CASE("Fichier des presets : les clés écrites sont figées", "[app][presets]") {
  // Clé pour clé : un fichier déjà écrit chez le joueur reste lisible.
  const ro::test_support::TempDirectory folder;
  REQUIRE(SaveEquipPresets(folder.path(), kMine, std::vector<EquipPreset>{Sample()}));
  const std::string text = Contents(EquipPresetFilePath(folder.path()));
  for (const char* expected :
       {"equip_presets:", "cid: 150000", "name: PvM", "hkvk: 116", "hkc: true", "hka: true",
        "hks: false", "costumes: true", "items:", "id: 1201", "refine: 7", "grade: 2", "left: true",
        "kind: 0", "cards: [4001, 4002, 0, 4003]", "kind: 1", "kind: 2"}) {
    INFO(expected);
    CHECK(text.find(expected) != std::string::npos);
  }
}

TEST_CASE("Fichier des presets : enregistrer un personnage garde les autres, à leur place",
          "[app][presets]") {
  const ro::test_support::TempDirectory folder;
  REQUIRE(SaveEquipPresets(folder.path(), kMine, std::vector<EquipPreset>{Sample()}));
  REQUIRE(SaveEquipPresets(folder.path(), kOther,
                           std::vector<EquipPreset>{EquipPreset{.name = "Autre"}}));
  REQUIRE(SaveEquipPresets(folder.path(), kMine,
                           std::vector<EquipPreset>{EquipPreset{.name = "Neuf"}}));

  CHECK(LoadEquipPresets(folder.path(), kOther).size() == 1);
  const std::vector<EquipPreset> mine = LoadEquipPresets(folder.path(), kMine);
  REQUIRE(mine.size() == 1);
  CHECK(mine[0].name == "Neuf");
  // À sa place : un personnage ne saute pas d'un bout du fichier à l'autre.
  const std::string text = Contents(EquipPresetFilePath(folder.path()));
  CHECK(text.find("Neuf") < text.find("Autre"));

  SECTION("et vider ses presets les retire") {
    REQUIRE(SaveEquipPresets(folder.path(), kMine, std::vector<EquipPreset>{}));
    CHECK(LoadEquipPresets(folder.path(), kMine).empty());
    CHECK(LoadEquipPresets(folder.path(), kOther).size() == 1);
  }
}

TEST_CASE("Fichier des presets : un fichier illisible ne se réécrit pas", "[app][presets]") {
  // Repartir d'une liste vide effacerait les presets des autres personnages.
  const ro::test_support::TempDirectory folder;
  const std::string broken = "equip_presets: [ {cid: 150000, name: \"PvM\n";
  folder.WriteText(OursRelative(), broken);
  CHECK(LoadEquipPresets(folder.path(), kMine).empty());
  CHECK_FALSE(SaveEquipPresets(folder.path(), kMine, std::vector<EquipPreset>{Sample()}));
  CHECK(Contents(EquipPresetFilePath(folder.path())) == broken);
}

TEST_CASE("Fichier des presets : ce qui manque ou déborde se lit sans casser", "[app][presets]") {
  const ro::test_support::TempDirectory folder;
  folder.WriteText(OursRelative(),
                   "equip_presets:\n"
                   "  - cid: 150000\n"
                   "    items: []\n"
                   "  - cid: 150000\n"
                   "    name: Ancien\n"
                   "    hkvk: 9\n"
                   "    hkc: true\n"
                   "    items:\n"
                   "      - id: 1201\n"
                   "        kind: 7\n"
                   "      - id: 1202\n"
                   "        cards: [1, 2, 3, 4, 5]\n");
  const std::vector<EquipPreset> read = LoadEquipPresets(folder.path(), kMine);
  // Sans nom, une entrée se saute : on ne saurait ni la charger ni la supprimer.
  REQUIRE(read.size() == 1);
  CHECK(read[0].name == "Ancien");
  // Tab n'est pas une touche de preset : ni elle, ni ses modificateurs.
  CHECK_FALSE(read[0].hotkey.bound());
  CHECK_FALSE(read[0].hotkey.ctrl);
  CHECK_FALSE(read[0].with_costumes);
  REQUIRE(read[0].pieces.size() == 2);
  // Une famille inconnue est de l'équipement, comme une famille absente.
  CHECK(read[0].pieces[0].family == PresetFamily::kEquipment);
  CHECK(read[0].pieces[0].refine_level == 0);
  CHECK(read[0].pieces[1].cards == std::array<std::uint32_t, 4>{1, 2, 3, 4});
}

TEST_CASE("Fichier des presets : les quatre plages de touches, dans les deux sens",
          "[app][presets]") {
  CHECK(VirtualKeyOf(SDLK_A) == 0x41);
  CHECK(VirtualKeyOf(SDLK_Z) == 0x5A);
  CHECK(VirtualKeyOf(SDLK_0) == 0x30);
  CHECK(VirtualKeyOf(SDLK_9) == 0x39);
  CHECK(VirtualKeyOf(SDLK_F1) == 0x70);
  CHECK(VirtualKeyOf(SDLK_F5) == kVirtualF5);
  CHECK(VirtualKeyOf(SDLK_F12) == 0x7B);
  CHECK(VirtualKeyOf(SDLK_SPACE) == 0x20);
  CHECK(VirtualKeyOf(SDLK_TAB) == 0);
  CHECK(VirtualKeyOf(SDLK_KP_1) == 0);
  for (const std::uint32_t key :
       {SDLK_A, SDLK_M, SDLK_Z, SDLK_0, SDLK_7, SDLK_F1, SDLK_F12, SDLK_SPACE}) {
    CHECK(KeyOfVirtualKey(VirtualKeyOf(key)) == key);
  }
  CHECK(KeyOfVirtualKey(0) == 0);
  CHECK(KeyOfVirtualKey(0x09) == 0);
}
