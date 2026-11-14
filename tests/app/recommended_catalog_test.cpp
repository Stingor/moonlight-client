// LES QUÊTES RECOMMANDÉES : l'ordre du fichier, la langue des textes et des
// blocs, les affiches mal formées, et le fichier livré avec le client.

#include "quest/recommended_catalog.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

using moonclient::RecommendedCatalog;

namespace {

constexpr const char* kSample = R"(version: 1
language: en
recommended:
  99:
    title: { fr: "Épisode 15", en: "Episode 15" }
    summary: "Science"
    icon: ep
    image: reco_99.bmp
    npc: { sprite: SPR, map: moc_para01, x: 38, y: 175 }
    blocks:
    - { fr: "ligne 1\nligne 2", en: "line 1\nline 2" }
    - "^3275f0Conditions^000000"
  12:
    title: "Plus ancienne, mais après"
  13: "pas une table"
  14:
    summary: "sans titre"
)";

}  // namespace

TEST_CASE("une affiche se lit dans la langue du joueur, dans l'ordre du fichier",
          "[recommended_catalog]") {
  auto catalog = RecommendedCatalog::Parse(kSample, "fr");
  REQUIRE(catalog.has_value());
  REQUIRE(catalog->entries().size() == 2);
  // L'ORDRE DU FICHIER, pas celui des clés : c'est celui de l'onglet.
  CHECK(catalog->entries()[0].entry_id == 99);
  CHECK(catalog->entries()[1].entry_id == 12);
  const auto* entry = catalog->Find(99);
  REQUIRE(entry != nullptr);
  CHECK(entry->title == "Épisode 15");
  CHECK(entry->summary == "Science");
  CHECK(entry->icon == "ep");
  CHECK(entry->image == "reco_99.bmp");
  REQUIRE(entry->npc.has_value());
  CHECK(entry->npc->map_name == "moc_para01");
  CHECK(entry->npc->cell_y == 175);
  REQUIRE(entry->blocks.size() == 2);
  CHECK(entry->blocks[0] == "ligne 1\nligne 2");
  CHECK(entry->blocks[1] == "^3275f0Conditions^000000");
  CHECK(RecommendedCatalog::Parse(kSample, "en")->Find(99)->blocks[0] == "line 1\nline 2");
  CHECK(catalog->Find(7) == nullptr);
}

TEST_CASE("une affiche mal formée est écartée seule, un fichier faux est refusé",
          "[recommended_catalog]") {
  auto catalog = RecommendedCatalog::Parse(kSample, "fr");
  REQUIRE(catalog.has_value());
  CHECK(catalog->skipped() == 2);
  CHECK_FALSE(RecommendedCatalog::Parse("version: 2\nrecommended: {}\n", "fr").has_value());
  CHECK_FALSE(RecommendedCatalog::Parse("a: [1, 2\n  b: }{", "fr").has_value());
  CHECK(RecommendedCatalog::Parse("version: 1\n", "fr")->entries().empty());
}

TEST_CASE("les recommandations livrées avec le client se relisent en entier",
          "[recommended_catalog]") {
  const std::filesystem::path path = std::filesystem::path(__FILE__).parent_path() / ".." / ".." /
                                     "dist" / "data" / "yaml_files" / "recommended_quests.yml";
  std::ifstream file(path, std::ios::binary);
  REQUIRE(file.good());
  std::ostringstream text;
  text << file.rdbuf();
  for (const char* language : {"fr", "en", "es"}) {
    auto catalog = RecommendedCatalog::Parse(text.str(), language);
    REQUIRE(catalog.has_value());
    CHECK(catalog->skipped() == 0);
    REQUIRE_FALSE(catalog->entries().empty());
    for (const auto& entry : catalog->entries()) {
      CHECK_FALSE(entry.title.empty());
      CHECK(entry.npc.has_value());
      CHECK_FALSE(entry.blocks.empty());
    }
  }
}
