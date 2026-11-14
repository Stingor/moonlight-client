// CE QUE LE CLIENT RETIENT DES SUCCÈS SUR LE DISQUE : un aller-retour fidèle,
// et un fichier abîmé qui retombe sur les défauts sans rien casser.

#include "game/achievement_memory.h"

#include <catch2/catch_test_macros.hpp>

#include "support/temp_directory.h"

using moonclient::AchievementMemory;
using moonclient::RememberedAchievements;
using ro::test_support::TempDirectory;

TEST_CASE("Mémoire des succès : ce qu'on écrit se relit tel quel", "[app][succes]") {
  const TempDirectory folder;
  AchievementMemory written;
  written.banner = ro::ui::AchievementBannerStyle::kDiscreet;
  written.Remember(RememberedAchievements{.character = "Stingor",
                                          .tracked = {120001, 128038},
                                          .tab = 2,
                                          .subtab = 5,
                                          .filter = ro::ui::AchievementFilter::kTracked,
                                          .sort = ro::ui::AchievementSort::kName,
                                          .selected = 128038});
  REQUIRE(moonclient::SaveAchievementMemory(folder.path(), written));

  AchievementMemory read;
  moonclient::LoadAchievementMemory(folder.path(), read);
  CHECK(read.banner == ro::ui::AchievementBannerStyle::kDiscreet);
  const RememberedAchievements* kept = read.Of("Stingor");
  REQUIRE(kept != nullptr);
  CHECK(kept->tracked == std::vector<std::uint32_t>{120001, 128038});
  CHECK(kept->tab == 2);
  CHECK(kept->subtab == 5);
  CHECK(kept->filter == ro::ui::AchievementFilter::kTracked);
  CHECK(kept->sort == ro::ui::AchievementSort::kName);
  CHECK(kept->selected == 128038U);
  CHECK(read.Of("Autre") == nullptr);
}

TEST_CASE("Mémoire des succès : un personnage retenu remplace ce qu'il avait laissé",
          "[app][succes]") {
  AchievementMemory memory;
  memory.Remember(RememberedAchievements{.character = "A", .tab = 1});
  memory.Remember(RememberedAchievements{.character = "A", .tab = 3});
  REQUIRE(memory.characters.size() == 1);
  CHECK(memory.Of("A")->tab == 3);
}

TEST_CASE("Mémoire des succès : les valeurs hors bornes retombent à leur défaut", "[app][succes]") {
  const TempDirectory folder;
  folder.WriteText("savedata/succes.yml",
                   "banner: 9\n"
                   "characters:\n"
                   "  - character: X\n"
                   "    tracked: [1, 2, 3, 4, 0, 2]\n"
                   "    tab: 12\n"
                   "    filter: 7\n"
                   "    sort: -1\n");
  AchievementMemory read;
  moonclient::LoadAchievementMemory(folder.path(), read);
  CHECK(read.banner == ro::ui::AchievementBannerStyle::kNative);
  const RememberedAchievements* kept = read.Of("X");
  REQUIRE(kept != nullptr);
  // Trois au plus, sans zéro ni doublon.
  CHECK(kept->tracked == std::vector<std::uint32_t>{1, 2, 3});
  CHECK(kept->tab == ro::ui::kAchievementSummaryTab);
  CHECK(kept->filter == ro::ui::AchievementFilter::kAll);
  CHECK(kept->sort == ro::ui::AchievementSort::kGame);
}

TEST_CASE("Mémoire des succès : un fichier illisible laisse les défauts", "[app][succes]") {
  const TempDirectory folder;
  folder.WriteText("savedata/succes.yml", "{ ceci n'est : pas [ du yaml");
  AchievementMemory read;
  moonclient::LoadAchievementMemory(folder.path(), read);
  CHECK(read.characters.empty());
  CHECK(read.banner == ro::ui::AchievementBannerStyle::kNative);
}
