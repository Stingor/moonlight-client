// Le fichier des terrains du champ de bataille : schéma, langues, récompenses,
// et ce que la lecture écarte sans condamner le reste.
#include "game/battleground_file.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

using moonclient::BattlegroundField;
using moonclient::BattlegroundFile;
using moonclient::BattlegroundText;

namespace {

constexpr std::uint32_t kBraveryBadge = 7828;
constexpr int kWinCount = 3;
constexpr int kLevelMin = 80;
constexpr int kTeamMin = 3;
constexpr int kTeamMax = 15;

// Vrai si l'une des remarques contient `piece`.
[[nodiscard]] bool Told(const BattlegroundFile& file, std::string_view piece) {
  return std::ranges::any_of(file.problems(), [piece](const std::string& problem) {
    return problem.find(piece) != std::string::npos;
  });
}

}  // namespace

TEST_CASE("Fichier des terrains : un terrain complet se lit champ par champ",
          "[app][battleground]") {
  const auto file = BattlegroundFile::Parse(R"(version: 1
fields:
  - server_name: "Tierra Gorge"
    name: { fr: "Gorge de Tierra", en: "Tierra Gorge" }
    objective: { fr: "Une Réserve est détruite.", en: "A Reserve is destroyed." }
    help: "Deux camps, une Réserve chacun."
    minimap: "bat_a01.bmp"
    team: { min: 3, max: 15 }
    apply: [solo, guild]
    level: { min: 80 }
    rewards:
      win: [{ item: 7828, count: 3 }]
      draw: [{ item: 7828 }]
      lose: [{ text: { fr: "Rien", en: "Nothing" } }]
)");
  REQUIRE(file.has_value());
  // L'aide, en chaîne seule, n'est qu'en français quand le reste parle aussi
  // anglais : c'est la seule remarque.
  REQUIRE(file->problems().size() == 1);
  CHECK(Told(*file, "help"));
  REQUIRE(file->fields().size() == 1);
  const BattlegroundField& field = file->fields()[0];
  CHECK(field.server_name == "Tierra Gorge");
  CHECK(field.name.In("en") == "Tierra Gorge");
  CHECK(field.objective.In("fr") == "Une Réserve est détruite.");
  // Une chaîne seule vaut pour le français.
  CHECK(field.help.In("fr") == "Deux camps, une Réserve chacun.");
  CHECK(field.minimap_file == "bat_a01.bmp");
  CHECK(field.team_min == kTeamMin);
  CHECK(field.team_max == kTeamMax);
  CHECK(field.solo_apply);
  CHECK_FALSE(field.party_apply);
  CHECK(field.guild_apply);
  CHECK(field.level_min == kLevelMin);
  CHECK(field.level_max == 0);
  REQUIRE(field.reward_win.size() == 1);
  CHECK(field.reward_win[0].item_id == kBraveryBadge);
  CHECK(field.reward_win[0].count == kWinCount);
  REQUIRE(field.reward_draw.size() == 1);
  CHECK(field.reward_draw[0].count == 1);
  REQUIRE(field.reward_lose.size() == 1);
  CHECK(field.reward_lose[0].item_id == 0);
  CHECK(field.reward_lose[0].text.In("en") == "Nothing");
  CHECK(file->FindByServerName("Tierra Gorge") == &field);
  CHECK(file->FindByServerName("Gorge de Tierra") == nullptr);
}

TEST_CASE("Fichier des terrains : une langue absente retombe sur le français, puis sur la première",
          "[app][battleground]") {
  BattlegroundText text;
  CHECK(text.In("fr").empty());
  text.by_language = {{"en", "Flavius"}, {"es", "Flavio"}};
  CHECK(text.In("es") == "Flavio");
  CHECK(text.In("de") == "Flavius");
  text.by_language.emplace_back("fr", "Flavie");
  CHECK(text.In("de") == "Flavie");
}

TEST_CASE("Fichier des terrains : une entrée fautive est écartée et dite, les autres restent",
          "[app][battleground]") {
  const auto file = BattlegroundFile::Parse(R"(version: 1
fields:
  - name: { fr: "Sans nom de serveur" }
  - server_name: "Flavius"
    name: { fr: "Flavius", en: "Flavius" }
    objective: { fr: "Un Cristal est détruit." }
    apply: [solo, raid]
    rewards:
      win: [{ count: 2 }]
  - server_name: "Flavius"
  - "pas une table"
)");
  REQUIRE(file.has_value());
  REQUIRE(file->fields().size() == 1);
  CHECK(file->fields()[0].server_name == "Flavius");
  CHECK(file->fields()[0].reward_win.empty());
  CHECK(Told(*file, "server_name"));
  CHECK(Told(*file, "raid"));
  CHECK(Told(*file, "sans objet ni texte"));
  CHECK(Told(*file, "déjà décrit"));
  CHECK(Told(*file, "une table est attendue"));
  // L'objectif n'a pas d'anglais alors que le nom en a : la traduction est
  // incomplète.
  CHECK(Told(*file, "objective"));
}

TEST_CASE("Fichier des terrains : celui que le dépôt livre se lit sans une remarque",
          "[app][battleground]") {
  // Le dépôt, à deux dossiers au-dessus de cet essai : CMake compile chaque
  // source sous son chemin absolu.
  const std::filesystem::path repo = std::filesystem::path(__FILE__).parent_path() / ".." / "..";
  std::ifstream shipped(repo / "dist" / "data" / "yaml_files" / "battleground.yml",
                        std::ios::binary);
  REQUIRE(shipped.is_open());
  const std::string text((std::istreambuf_iterator<char>(shipped)),
                         std::istreambuf_iterator<char>());
  const auto file = BattlegroundFile::Parse(text);
  REQUIRE(file.has_value());
  CHECK(file->problems().empty());
  CHECK(file->fields().size() == 3);
  for (const BattlegroundField& field : file->fields()) {
    CHECK_FALSE(field.name.empty());
    CHECK_FALSE(field.objective.empty());
    CHECK(field.team_min > 0);
  }
}

TEST_CASE("Fichier des terrains : illisible, d'une autre version ou sans liste, il est refusé",
          "[app][battleground]") {
  CHECK_FALSE(BattlegroundFile::Parse("version: 1\nfields: [").has_value());
  CHECK_FALSE(BattlegroundFile::Parse("version: 2\nfields: []\n").has_value());
  CHECK_FALSE(BattlegroundFile::Parse("fields: []\n").has_value());
  CHECK_FALSE(BattlegroundFile::Parse("version: 1\n").has_value());
  CHECK_FALSE(BattlegroundFile::Parse("- version\n").has_value());
  const auto empty = BattlegroundFile::Parse("version: 1\nfields: []\n");
  REQUIRE(empty.has_value());
  CHECK(empty->fields().empty());
}
