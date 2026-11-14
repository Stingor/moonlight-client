// LES SCORES ET LES TROPHÉES DES MINI-JEUX, sur le disque : aller-retour, clés
// inconnues, fichier abîmé, qui ne s'écrase pas.

#include "game/minigame_file.h"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "game/save_folder.h"
#include "support/temp_directory.h"

using moonclient::LoadMinigameRecord;
using moonclient::MinigameRecordHeld;
using moonclient::ReadMinigameRecord;
using moonclient::SaveMinigameRecord;
using moonclient::WriteMinigameRecord;
using ro::sim::MinigameRecord;
using ro::sim::MinigameTrophy;

TEST_CASE("l'enregistrement des mini-jeux fait l'aller-retour") {
  MinigameRecord record;
  record.rojeweled_best = 12345;
  record.roggle_best = 6789;
  record.roggle_daily_day = 20396;
  record.roggle_daily_best = 4321;
  record.unlocked[static_cast<std::size_t>(MinigameTrophy::kRojeweledCombo4)] = true;
  record.unlocked[static_cast<std::size_t>(MinigameTrophy::kRoggleFreeBall)] = true;
  record.unlocked[static_cast<std::size_t>(MinigameTrophy::kRoggleWon)] = true;
  const auto read = ReadMinigameRecord(WriteMinigameRecord(record));
  REQUIRE(read.has_value());
  CHECK(*read == record);
}

TEST_CASE("les évolués des Survivants font l'aller-retour, triés, sans doublon ni Id hors bornes") {
  MinigameRecord record;
  record.survivors_evolved = {80, 89, 400};
  const auto read = ReadMinigameRecord(WriteMinigameRecord(record));
  REQUIRE(read.has_value());
  CHECK(*read == record);
  const auto messy = ReadMinigameRecord("survivors_evolved: [400, 89, 89, 0, 70000, x, 80]\n");
  REQUIRE(messy.has_value());
  CHECK(messy->survivors_evolved == std::vector<std::uint16_t>{80, 89, 400});
}

TEST_CASE("le carnet de Roggle fait l'aller-retour, trié, sans doublon ni numéro nul") {
  MinigameRecord record;
  record.roggle_revealed = {501, 1101, 2301};
  const auto read = ReadMinigameRecord(WriteMinigameRecord(record));
  REQUIRE(read.has_value());
  CHECK(*read == record);
  const auto messy = ReadMinigameRecord("roggle_revealed: [2301, 501, 501, 0, x, 1101]\n");
  REQUIRE(messy.has_value());
  CHECK(messy->roggle_revealed == std::vector<std::uint32_t>{501, 1101, 2301});
}

TEST_CASE("l'option du carnet de Roggle fait l'aller-retour, éteinte par défaut") {
  MinigameRecord record;
  CHECK_FALSE(ReadMinigameRecord(WriteMinigameRecord(record))->roggle_skip_revealed);
  record.roggle_skip_revealed = true;
  const auto read = ReadMinigameRecord(WriteMinigameRecord(record));
  REQUIRE(read.has_value());
  CHECK(*read == record);
  CHECK(read->roggle_skip_revealed);
}

TEST_CASE("une clé de trophée inconnue se lit sans rien gagner") {
  const auto read = ReadMinigameRecord("roggle_best: 10\ntrophies: [roggle_won, demain]\n");
  REQUIRE(read.has_value());
  CHECK(read->roggle_best == 10);
  CHECK(read->Has(MinigameTrophy::kRoggleWon));
  CHECK_FALSE(read->Has(MinigameTrophy::kRojeweledFinished));
}

TEST_CASE("une clé de trophée inconnue se réécrit telle quelle") {
  const auto read = ReadMinigameRecord("trophies: [roggle_won, demain]\n");
  REQUIRE(read.has_value());
  CHECK(read->foreign_trophy_keys == std::vector<std::string>{"demain"});
  const auto reread = ReadMinigameRecord(WriteMinigameRecord(*read));
  REQUIRE(reread.has_value());
  CHECK(*reread == *read);
  CHECK(reread->Has(MinigameTrophy::kRoggleWon));
}

TEST_CASE("un score négatif ou illisible vaut zéro") {
  const auto read = ReadMinigameRecord("rojeweled_best: -5\nroggle_best: beaucoup\n");
  REQUIRE(read.has_value());
  CHECK(read->rojeweled_best == 0);
  CHECK(read->roggle_best == 0);
}

TEST_CASE("un texte que le YAML refuse rend une erreur") {
  CHECK_FALSE(ReadMinigameRecord("trophies: [roggle_won\n").has_value());
}

TEST_CASE("un texte vide est vierge, une racine qui n'est pas une table est une erreur") {
  const auto empty = ReadMinigameRecord("");
  REQUIRE(empty.has_value());
  CHECK(*empty == MinigameRecord{});
  CHECK_FALSE(ReadMinigameRecord("du texte abime\n").has_value());
}

namespace {

constexpr std::string_view kBrokenText = "trophies: [roggle_won, demain\n";

[[nodiscard]] std::filesystem::path RelativeMinigameFile() {
  return std::filesystem::path(moonclient::kSaveFolder) / moonclient::kMinigameFileName;
}

[[nodiscard]] std::string TextOf(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream text;
  text << file.rdbuf();
  return text.str();
}

}  // namespace

TEST_CASE("un fichier absent donne un enregistrement vierge, qui s'écrit") {
  const ro::test_support::TempDirectory root;
  MinigameRecordHeld held = LoadMinigameRecord(root.path());
  CHECK_FALSE(held.file_unread);
  CHECK(held.record == MinigameRecord{});
  held.record.roggle_best = 42;
  REQUIRE(SaveMinigameRecord(root.path(), held));
  CHECK(LoadMinigameRecord(root.path()).record.roggle_best == 42);
}

TEST_CASE("un fichier abîmé ne s'écrase pas tant qu'il ne se relit pas") {
  const ro::test_support::TempDirectory root;
  root.WriteText(RelativeMinigameFile(), kBrokenText);
  MinigameRecordHeld held = LoadMinigameRecord(root.path());
  CHECK(held.file_unread);
  CHECK(held.record == MinigameRecord{});
  held.record.unlocked[static_cast<std::size_t>(MinigameTrophy::kRojeweledFinished)] = true;
  CHECK_FALSE(SaveMinigameRecord(root.path(), held));
  CHECK(held.file_unread);
  CHECK(TextOf(root.path() / RelativeMinigameFile()) == kBrokenText);
}

TEST_CASE("un fichier redevenu lisible se fond dans la partie avant d'être écrit") {
  const ro::test_support::TempDirectory root;
  root.WriteText(RelativeMinigameFile(), kBrokenText);
  MinigameRecordHeld held = LoadMinigameRecord(root.path());
  REQUIRE(held.file_unread);
  held.record.rojeweled_best = 900;
  held.record.unlocked[static_cast<std::size_t>(MinigameTrophy::kRojeweledFinished)] = true;

  root.WriteText(RelativeMinigameFile(),
                 "rojeweled_best: 100\nroggle_best: 7000\ntrophies: [roggle_won, demain]\n");
  REQUIRE(SaveMinigameRecord(root.path(), held));
  CHECK_FALSE(held.file_unread);

  const MinigameRecordHeld reread = LoadMinigameRecord(root.path());
  CHECK(reread.record.rojeweled_best == 900);
  CHECK(reread.record.roggle_best == 7000);
  CHECK(reread.record.Has(MinigameTrophy::kRojeweledFinished));
  CHECK(reread.record.Has(MinigameTrophy::kRoggleWon));
  CHECK(reread.record.foreign_trophy_keys == std::vector<std::string>{"demain"});
}
