// LE TITRE QUE RÉCOMPENSE CHAQUE SUCCÈS, lu dans la base des succès du
// serveur.
//
// 🔴 LE SERVEUR NE L'ENVOIE JAMAIS : la liste des succès ne porte aucun titre,
// et c'est sa base seule qui les relie.

#include "ro/vfs/achievement_titles.h"

#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/achievement_database.h"

using ro::vfs::AchievementDatabase;
using ro::vfs::AchievementTitles;

namespace {

constexpr std::string_view kServer = R"(
version: 1
achievements: {
  110000: {name: a, score: 10, targets: [], reward: {item: 0, title: 0, script: false},
           dependents: []},
  128001: {name: b, score: 20, targets: [], reward: {item: 22876, title: 1001, script: false},
           dependents: []},
  900007: {name: c, score: 150, targets: [], reward: {item: 12246, title: 1103, script: true},
           dependents: []},
}
)";

[[nodiscard]] AchievementTitles ReadTitles() {
  auto database = AchievementDatabase::Parse(kServer);
  REQUIRE(database.has_value());
  return AchievementTitles::Of(*database);
}

}  // namespace

TEST_CASE("Succès : un succès récompensé d'un titre le rend", "[vfs][titres]") {
  const AchievementTitles table = ReadTitles();
  CHECK(table.TitleOf(128001) == 1001);
  CHECK(table.TitleOf(900007) == 1103);
}

TEST_CASE("Succès : sans titre dans la récompense, il n'y a rien", "[vfs][titres]") {
  // ⚠ LE CAS ORDINAIRE : la base écrit 0 quand le succès ne donne pas de titre.
  const AchievementTitles table = ReadTitles();
  CHECK_FALSE(table.TitleOf(110000).has_value());
  CHECK(table.entries().size() == 2);
}

TEST_CASE("Succès : un succès inconnu du serveur ne rend rien", "[vfs][titres]") {
  CHECK_FALSE(ReadTitles().TitleOf(424242).has_value());
}
