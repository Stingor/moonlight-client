// LA SECTION `codex` DU CARNET : ce que les parties ont vu se relit tel qu'il
// a été écrit, et une forme inconnue se garde comme dans tout le carnet.

#include "survivors/survivors_codex_save.h"

#include <cstddef>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "survivors/survivors_codex_keys.h"
#include "survivors/survivors_save.h"
#include "survivors/survivors_sightings.h"

using moonclient::CodexCategory;
using moonclient::ReadSurvivorsCodex;
using moonclient::ReadSurvivorsSave;
using moonclient::SurvivorsCodexSeen;
using moonclient::SurvivorsSave;
using moonclient::WriteSurvivorsCodex;
using moonclient::WriteSurvivorsSave;

namespace {

// Le carnet relu après une réécriture.
[[nodiscard]] SurvivorsSave Reread(const SurvivorsSave& save) {
  const auto read = ReadSurvivorsSave(WriteSurvivorsSave(save));
  REQUIRE(read.has_value());
  return *read;
}

[[nodiscard]] SurvivorsSave Read(std::string_view text) {
  const auto read = ReadSurvivorsSave(text);
  REQUIRE(read.has_value());
  return *read;
}

[[nodiscard]] bool Contains(const std::string& text, std::string_view part) {
  return text.find(part) != std::string::npos;
}

}  // namespace

TEST_CASE("Codex du carnet : un Codex plein fait l'aller-retour", "[app][survivors][codex]") {
  SurvivorsSave save;
  save.codex.Add(CodexCategory::kSkills, "fire_bolt");
  save.codex.Add(CodexCategory::kSkills, "cold_bolt");
  save.codex.Add(CodexCategory::kFoes, "1063");
  save.codex.Add(CodexCategory::kFoes, "1002");
  save.codex.Add(CodexCategory::kPlants, "1078");
  save.codex.Add(CodexCategory::kItems, "blue_gem");
  save.codex.Add(CodexCategory::kEvents, "swarm_1011");
  save.codex.Add(CodexCategory::kEvents, "night");
  const SurvivorsSave again = Reread(save);
  CHECK(again.codex == save.codex);
  CHECK(again == save);
  // L'ordre de la première rencontre se garde.
  REQUIRE(again.codex.lists[static_cast<std::size_t>(CodexCategory::kFoes)].size() == 2);
  CHECK(again.codex.lists[static_cast<std::size_t>(CodexCategory::kFoes)][0].text == "1063");
}

TEST_CASE("Codex du carnet : une section absente est un Codex vierge, et ne s'ajoute pas",
          "[app][survivors][codex]") {
  const SurvivorsSave save = Read("meta:\n  zeny: 12\n");
  CHECK(save.codex == SurvivorsCodexSeen{});
  const std::string written = WriteSurvivorsSave(save);
  CHECK_FALSE(Contains(written, "\ncodex:"));
  // Réécrit deux fois, le texte ne bouge plus.
  CHECK(WriteSurvivorsSave(Read(written)) == written);
}

TEST_CASE("Codex du carnet : une catégorie inconnue se recopie", "[app][survivors][codex]") {
  SurvivorsSave save = Read("codex:\n  foes: [\"1002\"]\n  relics: [a, b]\n");
  CHECK(save.codex.Has(CodexCategory::kFoes, "1002"));
  REQUIRE(save.codex.foreign_categories.size() == 1);
  CHECK(save.codex.foreign_categories.front().key == "relics");
  save.codex.Add(CodexCategory::kFoes, "1063");
  const SurvivorsSave again = Reread(save);
  CHECK(again.codex == save.codex);
}

TEST_CASE("Codex du carnet : une catégorie d'une autre forme se garde jusqu'à ce qu'on y écrive",
          "[app][survivors][codex]") {
  SurvivorsSave save = Read("codex:\n  foes: {poring: 3}\n");
  CHECK_FALSE(save.codex.other_shapes[static_cast<std::size_t>(CodexCategory::kFoes)].empty());
  CHECK(Contains(WriteSurvivorsSave(save), "poring"));
  save.codex.Add(CodexCategory::kFoes, "1002");
  const std::string written = WriteSurvivorsSave(save);
  CHECK_FALSE(Contains(written, "poring"));
  CHECK(Read(written).codex.Has(CodexCategory::kFoes, "1002"));
}

TEST_CASE("Codex du carnet : une entrée qui n'est pas une chaîne se garde à sa place",
          "[app][survivors][codex]") {
  const SurvivorsSave save = Read("codex:\n  foes: [\"1002\", {a: 1}, \"1063\"]\n");
  const auto& foes = save.codex.lists[static_cast<std::size_t>(CodexCategory::kFoes)];
  REQUIRE(foes.size() == 3);
  CHECK(foes[1].foreign);
  CHECK(save.codex.Has(CodexCategory::kFoes, "1063"));
  CHECK(Reread(save).codex == save.codex);
}

TEST_CASE("Codex du carnet : une clé inconnue du catalogue se garde, un doublon se lit une fois",
          "[app][survivors][codex]") {
  const SurvivorsSave save = Read("codex:\n  foes: [\"9999\", \"1002\", \"9999\", 42]\n");
  const auto& foes = save.codex.lists[static_cast<std::size_t>(CodexCategory::kFoes)];
  CHECK(foes.size() == 3);
  CHECK(save.codex.Has(CodexCategory::kFoes, "9999"));
  CHECK(save.codex.Has(CodexCategory::kFoes, "42"));
  CHECK(Reread(save).codex == save.codex);
}

TEST_CASE("Codex du carnet : un `codex` en liste est une section étrangère recopiée",
          "[app][survivors][codex]") {
  SurvivorsSave save = Read("codex: [a, b]\n");
  CHECK(save.codex == SurvivorsCodexSeen{});
  CHECK(Contains(WriteSurvivorsSave(save), "codex:"));
  // Ce client y écrit : la sienne remplace l'étrangère.
  save.codex.Add(CodexCategory::kEvents, "night");
  CHECK(Read(WriteSurvivorsSave(save)).codex.Has(CodexCategory::kEvents, "night"));
}

TEST_CASE("Codex du carnet : la section seule, dans les deux sens", "[app][survivors][codex]") {
  CHECK(ReadSurvivorsCodex("") == SurvivorsCodexSeen{});
  CHECK(ReadSurvivorsCodex("[a]") == SurvivorsCodexSeen{});
  CHECK(ReadSurvivorsCodex("{skills: [") == SurvivorsCodexSeen{});
  SurvivorsCodexSeen seen;
  seen.Add(CodexCategory::kItems, "meat");
  CHECK(ReadSurvivorsCodex(WriteSurvivorsCodex(seen)) == seen);
}
