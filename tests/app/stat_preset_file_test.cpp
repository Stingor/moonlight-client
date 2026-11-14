// LES PRÉRÉGLAGES DE STATS SUR LE DISQUE : le texte du fichier, dans les deux
// sens, sans rien lire ni écrire.

#include "character/stat_preset_file.h"

#include <string>

#include <catch2/catch_test_macros.hpp>

using moonclient::ReadStatPresetShelf;
using moonclient::StatPresetShelf;
using moonclient::WriteStatPresetShelf;
using ro::sim::StatPreset;

TEST_CASE("les préréglages de chaque compte se relisent tels qu'écrits", "[stat_preset_file]") {
  StatPresetShelf shelf;
  constexpr std::uint32_t kFirstAccount = 2000001;
  constexpr std::uint32_t kSecondAccount = 2000002;
  shelf[kFirstAccount] = {StatPreset{.name = "AGI-crit 99", .stats = {1, 99, 1, 1, 40, 1}},
                          StatPreset{.name = "Mage — INT", .stats = {1, 1, 30, 99, 60, 1}}};
  shelf[kSecondAccount] = {StatPreset{.name = "Tank", .stats = {50, 1, 999, 1, 1, 1}}};

  const ro::core::Result<StatPresetShelf> read = ReadStatPresetShelf(WriteStatPresetShelf(shelf));
  REQUIRE(read.has_value());
  CHECK(*read == shelf);
}

TEST_CASE("une entrée incomplète se saute, le reste se lit", "[stat_preset_file]") {
  const std::string text =
      "stat_presets:\n"
      "  - aid: 7\n"
      "    presets:\n"
      "      - {name: complet, stats: [1, 2, 3, 4, 5, 6]}\n"
      "      - {name: court, stats: [1, 2, 3]}\n"
      "      - {stats: [1, 2, 3, 4, 5, 6]}\n"
      "  - aid: 0\n"
      "    presets:\n"
      "      - {name: sans compte, stats: [1, 2, 3, 4, 5, 6]}\n";
  const ro::core::Result<StatPresetShelf> read = ReadStatPresetShelf(text);
  REQUIRE(read.has_value());
  REQUIRE(read->size() == 1);
  const std::vector<StatPreset>& mine = read->at(7);
  REQUIRE(mine.size() == 1);
  CHECK(mine[0].name == "complet");
  CHECK(mine[0].stats == ro::sim::BaseStats{1, 2, 3, 4, 5, 6});
}

TEST_CASE("un texte vide ne range rien, un texte abîmé est refusé", "[stat_preset_file]") {
  const ro::core::Result<StatPresetShelf> empty = ReadStatPresetShelf("");
  REQUIRE(empty.has_value());
  CHECK(empty->empty());
  CHECK_FALSE(ReadStatPresetShelf("stat_presets: [ {aid: 1, presets: [").has_value());
}
