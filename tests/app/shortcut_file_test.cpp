// Le fichier des barres de raccourcis : ce qu'il écrit, ce qu'il relit, et ce
// qu'il fait d'un fichier écrit par la version d'avant les huit barres.
//
// ⚠ AUCUNE ARCHIVE, AUCUN ASSET. L'arbre des compétences se monte ici même,
// de trois fiches — assez pour que trois noms internes se résolvent, ce qui est
// tout ce que ce fichier leur demande.

#include "character/shortcut_file.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/shortcut_bar.h"
#include "ro/sim/skill_tree.h"

#include "support/temp_directory.h"

namespace {

using moonclient::LoadJobShortcutBar;
using moonclient::LoadShortcutBar;
using moonclient::SaveShortcutBar;
using moonclient::ShortcutFilePath;
using ro::sim::kShortcutBarCount;
using ro::sim::ShortcutBars;
using ro::sim::ShortcutIndex;
using ro::sim::ShortcutSlot;

// Les identifiants sont ceux du jeu.
constexpr std::uint16_t kBash = 5;       // SM_BASH
constexpr std::uint16_t kMagnum = 7;     // SM_MAGNUM
constexpr std::uint16_t kColdBolt = 14;  // MG_COLDBOLT
constexpr std::uint32_t kRedPotion = 501;
constexpr std::uint32_t kOrangePotion = 502;
constexpr std::uint16_t kSwordsmanJob = 1;
constexpr std::uint16_t kMageJob = 2;

[[nodiscard]] ro::sim::SkillTree BuildTree() {
  return ro::sim::SkillTree::FromTables(
      {ro::sim::SkillNode{.id = kBash, .idname = "SM_BASH", .display_name = "Bash"},
       ro::sim::SkillNode{.id = kMagnum, .idname = "SM_MAGNUM", .display_name = "Magnum Break"},
       ro::sim::SkillNode{.id = kColdBolt, .idname = "MG_COLDBOLT", .display_name = "Cold Bolt"}},
      {});
}

[[nodiscard]] std::string Contents(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream all;
  all << file.rdbuf();
  return all.str();
}

}  // namespace

TEST_CASE("Fichier de barres : ce qui s'écrit se relit", "[app][barre]") {
  const ro::test_support::TempDirectory folder;
  const ro::sim::SkillTree tree = BuildTree();

  ShortcutBars written;
  written.Assign(ShortcutIndex(0, 0), ShortcutSlot{.skill_id = kBash, .level = 3});
  written.Assign(ShortcutIndex(0, 4), ShortcutSlot{.item_id = kRedPotion});
  // ⚠ LA DERNIÈRE BARRE, ET C'EST ELLE QUI COMPTE : c'est celle qu'un format
  // borné à la première perdrait sans un mot.
  written.Assign(ShortcutIndex(kShortcutBarCount - 1, 11),
                 ShortcutSlot{.skill_id = kColdBolt, .level = 10});
  REQUIRE(SaveShortcutBar(folder.path(), "Sting", &tree, written));

  ShortcutBars read;
  LoadShortcutBar(folder.path(), "Sting", &tree, read);
  CHECK(read.At(ShortcutIndex(0, 0)).skill_id == kBash);
  CHECK(read.At(ShortcutIndex(0, 0)).level == 3);
  CHECK(read.At(ShortcutIndex(0, 4)).item_id == kRedPotion);
  CHECK(read.At(ShortcutIndex(kShortcutBarCount - 1, 11)).skill_id == kColdBolt);
  CHECK(read.At(ShortcutIndex(kShortcutBarCount - 1, 11)).level == 10);
  // Ce qui n'a pas été posé ne l'est pas.
  CHECK_FALSE(read.At(ShortcutIndex(3, 0)).filled());
}

TEST_CASE("Fichier de barres : une compétence à deux rangs se relit deux fois", "[app][barre]") {
  // La relecture pose chaque case par `Assign` : la seconde ne doit pas chasser
  // la première quand leurs rangs diffèrent.
  const ro::test_support::TempDirectory folder;
  const ro::sim::SkillTree tree = BuildTree();

  ShortcutBars written;
  written.Assign(ShortcutIndex(0, 0), ShortcutSlot{.skill_id = kBash, .level = 1});
  written.Assign(ShortcutIndex(0, 1), ShortcutSlot{.skill_id = kBash, .level = 10});
  REQUIRE(SaveShortcutBar(folder.path(), "Sting", &tree, written));

  ShortcutBars read;
  LoadShortcutBar(folder.path(), "Sting", &tree, read);
  CHECK(read.At(ShortcutIndex(0, 0)).skill_id == kBash);
  CHECK(read.At(ShortcutIndex(0, 0)).level == 1);
  CHECK(read.At(ShortcutIndex(0, 1)).skill_id == kBash);
  CHECK(read.At(ShortcutIndex(0, 1)).level == 10);
}

TEST_CASE("Fichier de barres : une barre vide ne s'écrit pas", "[app][barre]") {
  // ⚠ CE QUE CET ESSAI PROTÈGE : un fichier fait pour se corriger à la main.
  // Huit listes de douze pour une seule case posée le rendraient illisible.
  const ro::test_support::TempDirectory folder;
  const ro::sim::SkillTree tree = BuildTree();

  ShortcutBars bars;
  bars.Assign(ShortcutIndex(2, 1), ShortcutSlot{.item_id = kOrangePotion});
  REQUIRE(SaveShortcutBar(folder.path(), "Sting", &tree, bars));

  // ⚠ LA CLÉ, DEUX-POINTS COMPRIS, et non le mot : l'en-tête du fichier nomme
  // déjà « bar1 a bar8 » en toutes lettres pour expliquer sa forme.
  const std::string text = Contents(ShortcutFilePath(folder.path()));
  CHECK(text.find("bar3:") != std::string::npos);
  CHECK(text.find("bar1:") == std::string::npos);
  CHECK(text.find("bar8:") == std::string::npos);
}

TEST_CASE("Fichier de barres : une liste de barre trop longue ne déborde pas", "[app][barre]") {
  // ⚠ CORRIGÉ À LA MAIN, ET TROP LONG. Sans borne, les cases en trop
  // apparaîtraient sur la barre SUIVANTE — et le joueur ne comprendrait pas d'où
  // elles sortent.
  const ro::test_support::TempDirectory folder;
  const ro::sim::SkillTree tree = BuildTree();
  std::string text = "Sting:\n  bar1:\n";
  for (int rank = 0; rank < 14; ++rank) {
    text += "    - item: 501\n";
  }
  folder.WriteText(std::filesystem::path("savedata") / "shortcut_bar.yml", text);

  ShortcutBars read;
  LoadShortcutBar(folder.path(), "Sting", &tree, read);
  CHECK(read.At(ShortcutIndex(0, 11)).item_id == kRedPotion);
  CHECK_FALSE(read.At(ShortcutIndex(1, 0)).filled());
  CHECK_FALSE(read.At(ShortcutIndex(1, 1)).filled());
}

TEST_CASE("Fichier de barres : écrire la sienne garde celle des autres", "[app][barre]") {
  const ro::test_support::TempDirectory folder;
  const ro::sim::SkillTree tree = BuildTree();

  ShortcutBars first;
  first.Assign(ShortcutIndex(0, 0), ShortcutSlot{.skill_id = kMagnum, .level = 5});
  REQUIRE(SaveShortcutBar(folder.path(), "Sting", &tree, first));

  ShortcutBars second;
  second.Assign(ShortcutIndex(1, 2), ShortcutSlot{.item_id = kRedPotion});
  REQUIRE(SaveShortcutBar(folder.path(), "Autre", &tree, second));

  ShortcutBars read;
  LoadShortcutBar(folder.path(), "Sting", &tree, read);
  CHECK(read.At(ShortcutIndex(0, 0)).skill_id == kMagnum);
}

TEST_CASE("Fichier de barres : vider sa dernière case ne la fait pas revenir", "[app][barre]") {
  // 🔴 CE QUE L'ABSENCE D'ENTRÉE AURAIT COÛTÉ. Ne rien écrire pour un jeu de
  // barres vide laisserait l'ANCIENNE entrée du fichier en place, et la barre
  // reviendrait au prochain démarrage — une case qu'on ne peut pas retirer.
  const ro::test_support::TempDirectory folder;
  const ro::sim::SkillTree tree = BuildTree();

  ShortcutBars bars;
  bars.Assign(ShortcutIndex(0, 0), ShortcutSlot{.skill_id = kBash, .level = 1});
  REQUIRE(SaveShortcutBar(folder.path(), "Sting", &tree, bars));

  bars.Clear(ShortcutIndex(0, 0));
  REQUIRE(SaveShortcutBar(folder.path(), "Sting", &tree, bars));

  ShortcutBars read;
  LoadShortcutBar(folder.path(), "Sting", &tree, read);
  CHECK_FALSE(read.At(ShortcutIndex(0, 0)).filled());
}

TEST_CASE("Fichier de barres : une compétence inconnue laisse la case vide", "[app][barre]") {
  // ⚠ VIDE PLUTÔT QUE FAUSSE, et le reste du fichier survit : une case sans
  // icône ni nom ne se lirait ni ne se retirerait.
  const ro::test_support::TempDirectory folder;
  const ro::sim::SkillTree tree = BuildTree();
  folder.WriteText(std::filesystem::path("savedata") / "shortcut_bar.yml",
                   "Sting:\n"
                   "  bar1:\n"
                   "    - skill: XX_INCONNUE\n"
                   "      level: 1\n"
                   "    - skill: SM_BASH\n"
                   "      level: 4\n");

  ShortcutBars read;
  LoadShortcutBar(folder.path(), "Sting", &tree, read);
  CHECK_FALSE(read.At(ShortcutIndex(0, 0)).filled());
  CHECK(read.At(ShortcutIndex(0, 1)).skill_id == kBash);
  CHECK(read.At(ShortcutIndex(0, 1)).level == 4);
}

TEST_CASE("Fichier de barres : une compétence inconnue survit à la réécriture", "[app][barre]") {
  // 🔴 LUE VIDE, ÉCRITE VIDE, PERDUE POUR DE BON — sauf à la garder. Elle ne cède
  // qu'à ce que le joueur pose dans sa case.
  const ro::test_support::TempDirectory folder;
  const ro::sim::SkillTree tree = BuildTree();
  folder.WriteText(std::filesystem::path("savedata") / "shortcut_bar.yml",
                   "Sting:\n"
                   "  bar1:\n"
                   "    - skill: XX_INCONNUE\n"
                   "      level: 1\n"
                   "  bar2:\n"
                   "    - ~\n"
                   "    - ~\n"
                   "    - skill: XX_AUTRE\n"
                   "      level: 2\n");

  ShortcutBars bars;
  LoadShortcutBar(folder.path(), "Sting", &tree, bars);
  bars.Assign(ShortcutIndex(0, 1), ShortcutSlot{.item_id = kRedPotion});
  REQUIRE(SaveShortcutBar(folder.path(), "Sting", &tree, bars));
  std::string text = Contents(ShortcutFilePath(folder.path()));
  CHECK(text.find("XX_INCONNUE") != std::string::npos);
  CHECK(text.find("XX_AUTRE") != std::string::npos);

  bars.Assign(ShortcutIndex(0, 0), ShortcutSlot{.skill_id = kBash, .level = 2});
  REQUIRE(SaveShortcutBar(folder.path(), "Sting", &tree, bars));
  text = Contents(ShortcutFilePath(folder.path()));
  CHECK(text.find("XX_INCONNUE") == std::string::npos);
  CHECK(text.find("XX_AUTRE") != std::string::npos);

  ShortcutBars read;
  LoadShortcutBar(folder.path(), "Sting", &tree, read);
  CHECK(read.At(ShortcutIndex(0, 0)).skill_id == kBash);
  CHECK(read.At(ShortcutIndex(0, 1)).item_id == kRedPotion);
  CHECK_FALSE(read.At(ShortcutIndex(1, 2)).filled());
}

TEST_CASE("Fichier de barres : un fichier illisible ne se réécrit pas", "[app][barre]") {
  // 🔴 REPARTIR D'UNE TABLE VIDE emporterait les barres de tous les autres
  // personnages : le fichier reste tel quel, à corriger à la main.
  const ro::test_support::TempDirectory folder;
  const ro::sim::SkillTree tree = BuildTree();
  const std::string broken = "Sting:\n  bar1: [ - item: 501\n";
  folder.WriteText(std::filesystem::path("savedata") / "shortcut_bar.yml", broken);

  ShortcutBars bars;
  bars.Assign(ShortcutIndex(0, 0), ShortcutSlot{.item_id = kOrangePotion});
  CHECK_FALSE(SaveShortcutBar(folder.path(), "Autre", &tree, bars));
  CHECK(Contents(ShortcutFilePath(folder.path())) == broken);
}

TEST_CASE("Fichier de barres : chaque métier du staff garde son jeu", "[app][barre]") {
  const ro::test_support::TempDirectory folder;
  const ro::sim::SkillTree tree = BuildTree();

  ShortcutBars swordsman;
  swordsman.Assign(ShortcutIndex(0, 0), ShortcutSlot{.skill_id = kBash, .level = 10});
  REQUIRE(SaveShortcutBar(folder.path(), "Sting", &tree, swordsman, kSwordsmanJob));
  ShortcutBars mage;
  mage.Assign(ShortcutIndex(0, 0), ShortcutSlot{.skill_id = kColdBolt, .level = 10});
  REQUIRE(SaveShortcutBar(folder.path(), "Sting", &tree, mage, kMageJob));

  ShortcutBars read;
  REQUIRE(LoadJobShortcutBar(folder.path(), "Sting", kSwordsmanJob, &tree, read));
  CHECK(read.At(ShortcutIndex(0, 0)).skill_id == kBash);
  ShortcutBars other;
  REQUIRE(LoadJobShortcutBar(folder.path(), "Sting", kMageJob, &tree, other));
  CHECK(other.At(ShortcutIndex(0, 0)).skill_id == kColdBolt);
  // Le jeu du personnage est le dernier écrit : c'est lui qui se relit tant
  // que le niveau de groupe n'est pas arrivé.
  ShortcutBars plain;
  LoadShortcutBar(folder.path(), "Sting", &tree, plain);
  CHECK(plain.At(ShortcutIndex(0, 0)).skill_id == kColdBolt);
}

TEST_CASE("Fichier de barres : un métier sans jeu n'en rend pas", "[app][barre]") {
  const ro::test_support::TempDirectory folder;
  const ro::sim::SkillTree tree = BuildTree();
  ShortcutBars bars;
  bars.Assign(ShortcutIndex(0, 0), ShortcutSlot{.item_id = kRedPotion});
  REQUIRE(SaveShortcutBar(folder.path(), "Sting", &tree, bars, kSwordsmanJob));

  ShortcutBars read;
  read.Assign(ShortcutIndex(0, 1), ShortcutSlot{.item_id = kOrangePotion});
  CHECK_FALSE(LoadJobShortcutBar(folder.path(), "Sting", kMageJob, &tree, read));
  CHECK_FALSE(LoadJobShortcutBar(folder.path(), "Autre", kSwordsmanJob, &tree, read));
  // ⚠ SANS TOUCHER LA BARRE : c'est elle que le staff garde à l'écran.
  CHECK_FALSE(read.At(ShortcutIndex(0, 0)).filled());
  CHECK(read.At(ShortcutIndex(0, 1)).item_id == kOrangePotion);
}

TEST_CASE("Fichier de barres : écrire sans métier garde les jeux des métiers", "[app][barre]") {
  // 🔴 L'ENTRÉE DU PERSONNAGE SE RÉÉCRIT ENTIÈRE à chaque enregistrement : sans
  // reprise, les jeux des autres métiers disparaîtraient au premier.
  const ro::test_support::TempDirectory folder;
  const ro::sim::SkillTree tree = BuildTree();
  ShortcutBars swordsman;
  swordsman.Assign(ShortcutIndex(0, 0), ShortcutSlot{.skill_id = kMagnum, .level = 3});
  REQUIRE(SaveShortcutBar(folder.path(), "Sting", &tree, swordsman, kSwordsmanJob));
  ShortcutBars mage;
  mage.Assign(ShortcutIndex(0, 0), ShortcutSlot{.skill_id = kColdBolt, .level = 1});
  REQUIRE(SaveShortcutBar(folder.path(), "Sting", &tree, mage, kMageJob));
  REQUIRE(SaveShortcutBar(folder.path(), "Sting", &tree, ShortcutBars()));

  ShortcutBars read;
  REQUIRE(LoadJobShortcutBar(folder.path(), "Sting", kSwordsmanJob, &tree, read));
  CHECK(read.At(ShortcutIndex(0, 0)).skill_id == kMagnum);
  CHECK(read.At(ShortcutIndex(0, 0)).level == 3);
}
