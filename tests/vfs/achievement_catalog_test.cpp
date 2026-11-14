// LE CATALOGUE DES SUCCÈS : les succès de la table qui ont leur fenêtre, tels
// que la fenêtre les montre.
//
// ⚠ LES MINIATURES SONT ÉCRITES EN CLAIR, dans la forme de la table : aucun
// asset du jeu n'est versé au dépôt.

#include "ro/vfs/achievement_catalog.h"

#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/achievement_database.h"
#include "ro/vfs/achievement_words.h"
#include "ro/vfs/item_words.h"

using ro::vfs::AchievementCatalog;
using ro::vfs::AchievementDatabase;
using ro::vfs::AchievementGauge;
using ro::vfs::AchievementInfo;

namespace {

constexpr std::string_view kTable = R"(
version: 1
achievements:
  110001:
    name: "A fan of this polarity"
    score: 10
    targets: [[0, 1]]
    reward: {item: 0, title: 0, script: false}
    dependents: []
    window:
      summary: "A glimpse"
      details: "Let's meet"
      gauge: bullets
      group: "SEE"
      major: 4
      minor: 0
      objectives:
        - {text: "Fine Anak"}
        - {text: "A beautiful wife"}
  120160:
    name: "Explorer of the server"
    score: 25
    targets: [[0, 4]]
    reward: {item: 501, title: 0, script: true}
    dependents: [120005]
    window:
      summary: "s"
      details: "d"
      gauge: counts
      group: "ADVENTURE"
      major: 2
      minor: 0
      objectives:
        - {text: "First done", shortcut: 120005}
        - {text: "Second done"}
      buff: 10
  120170:
    name: "Linked"
    score: 5
    targets: []
    reward: {item: 0, title: 0, script: false}
    dependents: []
    window:
      summary: "s"
      details: "d"
      gauge: bullets
      group: ""
      major: 2
      minor: 0
      objectives: []
      buff: 12
  128039:
    name: "Without texts"
    score: 10
    targets: []
    reward: {item: 0, title: 1001, script: false}
    dependents: []
)";

[[nodiscard]] AchievementCatalog ReadCatalog() {
  auto database = AchievementDatabase::Parse(kTable);
  REQUIRE(database.has_value());
  return AchievementCatalog::Of(*database);
}

// La table miniature, dont la fenêtre de 120170 est remplacée par `window`.
[[nodiscard]] std::string WithWindow(std::string_view window) {
  return std::string(R"(
version: 1
achievements:
  120170:
    name: "Linked"
    score: 5
    targets: []
    reward: {item: 0, title: 0, script: false}
    dependents: []
    window: )") +
         std::string(window) + "\n";
}

}  // namespace

TEST_CASE("Catalogue des succès : le serveur donne le nom, les points, les buts et les récompenses",
          "[vfs][succes]") {
  const AchievementCatalog catalog = ReadCatalog();
  const AchievementInfo* info = catalog.Find(120160);
  REQUIRE(info != nullptr);
  CHECK(info->name == "Explorer of the server");
  CHECK(info->score == 25);
  REQUIRE(info->objectives.size() == 2);
  CHECK(info->objectives[0].target == 4);
  // Le serveur ne suit pas le second objectif : aucun but.
  CHECK(info->objectives[1].target == 0);
  CHECK(info->reward_item == 501);
  CHECK_FALSE(info->reward_title.has_value());
  CHECK(info->reward_script);
  CHECK(info->rewards());
}

TEST_CASE("Catalogue des succès : la fenêtre donne les textes, la jauge, l'icône et l'onglet",
          "[vfs][succes]") {
  const AchievementCatalog catalog = ReadCatalog();
  const AchievementInfo* info = catalog.Find(120160);
  REQUIRE(info != nullptr);
  CHECK(info->summary == "s");
  CHECK(info->details == "d");
  CHECK(info->gauge == AchievementGauge::kCounts);
  CHECK(info->group == "ADVENTURE");
  CHECK(info->major == 2);
  CHECK(info->minor == 0);
  CHECK(info->objectives[0].text == "First done");
  CHECK(info->objectives[1].text == "Second done");
  // L'état montré par l'aide, que le serveur ne nomme pas.
  CHECK(info->reward_buff == 10);
}

TEST_CASE("Catalogue des succès : une ligne désigne le succès que la table nomme",
          "[vfs][succes]") {
  const AchievementCatalog catalog = ReadCatalog();
  const AchievementInfo* info = catalog.Find(120160);
  REQUIRE(info != nullptr);
  CHECK(info->objectives[0].shortcut == 120005U);
  CHECK_FALSE(info->objectives[1].shortcut.has_value());
}

TEST_CASE("Catalogue des succès : une aide sans script n'est pas une récompense", "[vfs][succes]") {
  const AchievementCatalog catalog = ReadCatalog();
  const AchievementInfo* linked = catalog.Find(120170);
  REQUIRE(linked != nullptr);
  CHECK(linked->reward_buff == 12);
  CHECK_FALSE(linked->reward_script);
  CHECK_FALSE(linked->rewards());
  // Un groupe vide : aucune icône.
  CHECK(linked->group.empty());
  CHECK(linked->objectives.empty());
}

TEST_CASE("Catalogue des succès : en mode puces, le but est toujours nul", "[vfs][succes]") {
  const AchievementCatalog catalog = ReadCatalog();
  const AchievementInfo* info = catalog.Find(110001);
  REQUIRE(info != nullptr);
  CHECK(info->gauge == AchievementGauge::kBullets);
  REQUIRE(info->objectives.size() == 2);
  CHECK(info->objectives[0].target == 0);
  CHECK_FALSE(info->reward_buff.has_value());
  CHECK_FALSE(info->rewards());
}

TEST_CASE("Catalogue des succès : seuls les succès qui ont leur fenêtre se montrent",
          "[vfs][succes]") {
  const AchievementCatalog catalog = ReadCatalog();
  CHECK(catalog.Find(128039) == nullptr);
  CHECK(catalog.without_texts() == 1);
  REQUIRE(catalog.entries().size() == 3);
  CHECK(catalog.entries().front().achievement_id == 110001);
  CHECK(catalog.entries().back().achievement_id == 120170);
}

TEST_CASE("Catalogue des succès : une fenêtre mal formée refuse la table entière",
          "[vfs][succes]") {
  constexpr std::string_view kWellFormed =
      "{summary: s, details: d, gauge: bullets, group: SEE, major: 1, minor: 0, objectives: []}";
  REQUIRE(AchievementDatabase::Parse(WithWindow(kWellFormed)).has_value());
  // Une jauge inconnue.
  CHECK_FALSE(AchievementDatabase::Parse(
                  WithWindow("{summary: s, details: d, gauge: dots, group: SEE, major: 1, "
                             "minor: 0, objectives: []}"))
                  .has_value());
  // Sans détail.
  CHECK_FALSE(AchievementDatabase::Parse(
                  WithWindow("{summary: s, gauge: bullets, group: SEE, major: 1, minor: 0, "
                             "objectives: []}"))
                  .has_value());
  // Un onglet hors d'un octet.
  CHECK_FALSE(AchievementDatabase::Parse(
                  WithWindow("{summary: s, details: d, gauge: bullets, group: SEE, major: 256, "
                             "minor: 0, objectives: []}"))
                  .has_value());
  // Un objectif sans texte.
  CHECK_FALSE(AchievementDatabase::Parse(
                  WithWindow("{summary: s, details: d, gauge: bullets, group: SEE, major: 1, "
                             "minor: 0, objectives: [{shortcut: 1}]}"))
                  .has_value());
  // Onze objectifs : la fenêtre n'en a que dix lignes.
  CHECK_FALSE(AchievementDatabase::Parse(
                  WithWindow("{summary: s, details: d, gauge: bullets, group: SEE, major: 1, "
                             "minor: 0, objectives: [{text: a}, {text: a}, {text: a}, {text: a}, "
                             "{text: a}, {text: a}, {text: a}, {text: a}, {text: a}, {text: a}, "
                             "{text: a}]}"))
                  .has_value());
}

TEST_CASE("Mots des succès : la bannière pose le nom dans le gabarit", "[vfs][succes]") {
  using ro::vfs::AchievementWord;
  ro::vfs::ItemWords words;
  const auto rank = static_cast<std::size_t>(AchievementWord::kCompleted);
  words.achievement.texts[rank] = "< %s > achieved. 100%%";
  CHECK(ro::vfs::AchievementCompletedText(words, "Kill 100%s") == "< Kill 100%s > achieved. 100%");
  // 🔴 UN GABARIT QUI NE PORTE PAS EXACTEMENT UN « %s » SE REFUSE.
  words.achievement.texts[rank] = "< %s > %d";
  CHECK(ro::vfs::AchievementCompletedText(words, "x").empty());
  words.achievement.texts[rank] = "";
  CHECK(ro::vfs::AchievementCompletedText(words, "x").empty());
}
