// LE CATALOGUE DES QUÊTES : langue des textes, entrées mal formées, catégories,
// et le fichier livré avec le client.

#include "quest/quest_catalog.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

using moonclient::QuestCatalog;

namespace {

constexpr const char* kSample = R"(version: 1
source: "synthetique"
language: en
icons:
  ep: { image: ico_ep.bmp, label: "EPISODE QUEST" }
  dq: { image: ico_dq.bmp, label: { fr: "QUETE QUOTIDIENNE", en: "DAILY QUEST" } }
categories:
  - { key: all, label: "All Quests" }
  - { key: etc, label: "Other Quests" }
  - { key: normal, label: "Normal", icons: [ep] }
  - { key: daily, label: "Daily", icons: [dq] }
quests:
  1001:
    title: "Job Change"
    icon: ep
    summary: "Talk"
    description: "ligne 1\nligne 2"
    npc: { sprite: SPR, map: prt_fild03, x: 365, y: 255 }
    rewards: { base_exp: 1300, job_exp: 100, items: [ { id: 12636, count: 20 } ] }
  1002:
    title: { fr: "Titre", en: "Title", es: "Titulo" }
    icon: dq
  1003:
    title: { fr: "Seulement fr" }
  1004:
    title: { de: "Premier venu" }
  1005:
    summary: "sans titre"
  1006: "pas une table"
  1007:
    title: "Sans icône"
    icon: inconnue
  1008:
    title: "Revenez demain"
    cooldown: true
)";

}  // namespace

TEST_CASE("un texte nu se lit tel quel et tout le reste se range", "[quest_catalog]") {
  auto catalog = QuestCatalog::Parse(kSample, "fr");
  REQUIRE(catalog.has_value());
  const auto* quest = catalog->Find(1001);
  REQUIRE(quest != nullptr);
  CHECK(quest->title == "Job Change");
  CHECK(quest->summary == "Talk");
  CHECK(quest->description == "ligne 1\nligne 2");
  CHECK(quest->icon == "ep");
  REQUIRE(quest->npc.has_value());
  CHECK(quest->npc->sprite == "SPR");
  CHECK(quest->npc->map_name == "prt_fild03");
  CHECK(quest->npc->cell_x == 365);
  CHECK(quest->npc->cell_y == 255);
  CHECK(quest->reward_base_exp == 1300);
  CHECK(quest->reward_job_exp == 100);
  REQUIRE(quest->reward_items.size() == 1);
  CHECK(quest->reward_items[0].item_id == 12636);
  CHECK(quest->reward_items[0].count == 20);
  CHECK(catalog->Find(9999) == nullptr);
}

TEST_CASE("une chaîne {fr,en,es} se résout selon la langue demandée", "[quest_catalog]") {
  CHECK(QuestCatalog::Parse(kSample, "es")->Find(1002)->title == "Titulo");
  CHECK(QuestCatalog::Parse(kSample, "fr")->Find(1002)->title == "Titre");
  CHECK(QuestCatalog::Parse(kSample, "en")->Find(1002)->title == "Title");
}

TEST_CASE("une langue absente se replie : fichier, fr, première", "[quest_catalog]") {
  // Ni es ni la langue du fichier (en) : on tombe sur fr.
  CHECK(QuestCatalog::Parse(kSample, "es")->Find(1003)->title == "Seulement fr");
  // Aucune des trois : la première entrée.
  CHECK(QuestCatalog::Parse(kSample, "es")->Find(1004)->title == "Premier venu");
  // Les libellés d'icône se résolvent de la même façon.
  CHECK(QuestCatalog::Parse(kSample, "fr")->FindIcon("dq")->label == "QUETE QUOTIDIENNE");
  CHECK(QuestCatalog::Parse(kSample, "es")->FindIcon("dq")->label == "DAILY QUEST");
  CHECK(QuestCatalog::Parse(kSample, "fr")->FindIcon("ep")->image == "ico_ep.bmp");
  CHECK(QuestCatalog::Parse(kSample, "fr")->FindIcon("zz") == nullptr);
}

TEST_CASE("une entrée mal formée est écartée seule et comptée", "[quest_catalog]") {
  auto catalog = QuestCatalog::Parse(kSample, "fr");
  REQUIRE(catalog.has_value());
  CHECK(catalog->size() == 6);
  CHECK(catalog->skipped() == 2);
  CHECK(catalog->Find(1005) == nullptr);
  CHECK(catalog->Find(1006) == nullptr);
  CHECK(catalog->Find(1007) != nullptr);
}

TEST_CASE("une version autre que 1, ou un texte qui n'est pas du YAML, est refusé",
          "[quest_catalog]") {
  CHECK_FALSE(QuestCatalog::Parse("version: 2\nquests: {}\n", "fr").has_value());
  CHECK_FALSE(QuestCatalog::Parse("quests: {}\n", "fr").has_value());
  CHECK_FALSE(QuestCatalog::Parse("a: [1, 2\n  b: }{", "fr").has_value());
  CHECK_FALSE(QuestCatalog::Parse("- une\n- liste\n", "fr").has_value());
}

TEST_CASE("une quête d'attente porte son drapeau, les autres non", "[quest_catalog]") {
  auto catalog = QuestCatalog::Parse(kSample, "fr");
  REQUIRE(catalog.has_value());
  CHECK(catalog->Find(1008)->cooldown);
  CHECK_FALSE(catalog->Find(1001)->cooldown);
}

TEST_CASE("la catégorie d'une quête suit son icône, et jamais « all »", "[quest_catalog]") {
  auto catalog = QuestCatalog::Parse(kSample, "fr");
  REQUIRE(catalog.has_value());
  CHECK(catalog->CategoryOf(*catalog->Find(1001)) == "normal");
  CHECK(catalog->CategoryOf(*catalog->Find(1002)) == "daily");
  CHECK(catalog->CategoryOf(*catalog->Find(1007)) == "etc");
  CHECK(catalog->categories().size() == 4);
}

TEST_CASE("le catalogue livré avec le client se relit en entier", "[quest_catalog]") {
  // Le dépôt se situe depuis ce fichier : le crible Linux ne pose pas les
  // définitions de cible.
  const std::filesystem::path path = std::filesystem::path(__FILE__).parent_path() / ".." / ".." /
                                     "dist" / "data" / "yaml_files" / "quests.yml";
  std::ifstream file(path, std::ios::binary);
  REQUIRE(file.good());
  std::ostringstream text;
  text << file.rdbuf();
  for (const char* language : {"fr", "en", "es"}) {
    auto catalog = QuestCatalog::Parse(text.str(), language);
    REQUIRE(catalog.has_value());
    CHECK(catalog->skipped() == 0);
    CHECK(catalog->size() > 1000);
    CHECK(catalog->categories().size() == 6);
  }
  auto catalog = QuestCatalog::Parse(text.str(), "en");
  const auto* quest = catalog->Find(1001);
  REQUIRE(quest != nullptr);
  CHECK(quest->title == "Job Change to Acolyte");
  REQUIRE(quest->npc.has_value());
  CHECK(quest->npc->map_name == "prt_fild03");
  CHECK_FALSE(quest->cooldown);
  // « Revenez demain chasser les coyotes » : une attente de 4 heures.
  const auto* waiting = catalog->Find(16079);
  REQUIRE(waiting != nullptr);
  CHECK(waiting->cooldown);
}
