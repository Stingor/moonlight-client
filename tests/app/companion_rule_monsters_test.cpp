// LA LISTE « MONSTRE DE LA CIBLE » D'UNE RÈGLE : ceux des tactiques et ceux en
// vue d'abord, puis tous ceux que la table du serveur connaît, par nom, chacun
// avec ce qui aide à le choisir.

#include "game/companion_rule_monsters.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/companion_rules.h"
#include "ro/vfs/monster_list.h"

#include "game/companion_ai_fields.h"

using moonclient::RuleNames;
using ro::vfs::MonsterListEntry;

namespace {

constexpr std::uint16_t kPoring = 1002;
constexpr std::uint16_t kBaphomet = 1039;
constexpr std::uint16_t kAngeling = 1096;
constexpr std::uint16_t kAnotherPoring = 20100;
constexpr std::uint8_t kDemonRace = 6;
constexpr std::uint8_t kDarkElement = 7;
constexpr std::uint8_t kLargeSize = 2;
constexpr std::uint16_t kBaphometLevel = 81;

[[nodiscard]] ro::vfs::MonsterList Table() {
  return ro::vfs::MonsterList::Known({
      MonsterListEntry{.monster_id = kPoring, .aegis_name = "PORING", .name = "Poring", .level = 1},
      MonsterListEntry{.monster_id = kBaphomet,
                       .aegis_name = "BAPHOMET",
                       .name = "Baphomet",
                       .level = kBaphometLevel,
                       .race = kDemonRace,
                       .element = kDarkElement,
                       .element_level = 3,
                       .size = kLargeSize,
                       .rank = ro::vfs::kMonsterMvpRank},
      MonsterListEntry{.monster_id = kAngeling,
                       .aegis_name = "ANGELING",
                       .name = "angeling",
                       .level = 20,
                       .element_level = 1,
                       .rank = ro::vfs::kMonsterBossRank},
      MonsterListEntry{
          .monster_id = kAnotherPoring, .aegis_name = "E_PORING", .name = "Poring", .level = 1},
  });
}

// Les noms que la fenêtre donne : celui de la table, et les mots de la fiche.
[[nodiscard]] RuleNames Names(const ro::vfs::MonsterList& table) {
  RuleNames names;
  names.monster = [&table](std::uint16_t monster) {
    const MonsterListEntry* entry = table.Find(monster);
    return entry != nullptr ? entry->name : std::to_string(monster);
  };
  names.race = [](int race) { return race == kDemonRace ? std::string("Demon") : std::string(); };
  names.element = [](int element) {
    return element == kDarkElement ? std::string("Dark") : std::string("Neutral");
  };
  names.size = [](int size) { return size == kLargeSize ? std::string("Large") : std::string(); };
  return names;
}

// La liste « Monstre de la cible » du vocabulaire ; vide sans elle.
[[nodiscard]] std::vector<ro::ui::RuleChoice> MonsterChoices(
    const ro::ui::RuleVocabulary& vocabulary) {
  for (const ro::ui::RuleConditionKind& kind : vocabulary.conditions) {
    if (kind.predicate == static_cast<int>(ro::sim::RulePredicate::kMonsterJob)) {
      return {kind.choices.begin(), kind.choices.end()};
    }
  }
  return {};
}

}  // namespace

TEST_CASE("Monstres d'une règle : la table se range par nom, à la casse près",
          "[app][companion_rule_monsters]") {
  // « angeling » avant « Baphomet » malgré sa minuscule ; deux « Poring » par
  // identifiant.
  CHECK(moonclient::MonstersByName(Table()) ==
        std::vector<std::uint16_t>{kAngeling, kBaphomet, kPoring, kAnotherPoring});
  CHECK(moonclient::MonstersByName(ro::vfs::MonsterList{}).empty());
}

TEST_CASE("Monstres d'une règle : ce qui aide à choisir", "[app][companion_rule_monsters]") {
  const ro::vfs::MonsterList table = Table();
  const RuleNames names = Names(table);
  CHECK(moonclient::MonsterListDetail(*table.Find(kBaphomet), names) ==
        "niv. 81 · MVP · Demon · Dark 3 · Large");
  // Un mot que l'installation n'a pas est tu ; sans mots du tout, seul le niveau
  // et le rang restent.
  CHECK(moonclient::MonsterListDetail(*table.Find(kAngeling), names) ==
        "niv. 20 · Mini-boss · Neutral 1");
  CHECK(moonclient::MonsterListDetail(*table.Find(kPoring), RuleNames{}) == "niv. 1");
}

TEST_CASE("Monstres d'une règle : la liste propose un monstre connu de la table seule",
          "[app][companion_rule_monsters]") {
  const ro::vfs::MonsterList table = Table();
  RuleNames names = Names(table);
  names.monster_detail = [&table, &names](std::uint16_t monster) {
    const MonsterListEntry* entry = table.Find(monster);
    return entry != nullptr ? moonclient::MonsterListDetail(*entry, names) : std::string();
  };
  // Le Poring vient d'une tactique ou de la vue ; la table suit, par nom.
  std::vector<std::uint16_t> offered{kPoring};
  const std::vector<std::uint16_t> by_name = moonclient::MonstersByName(table);
  offered.insert(offered.end(), by_name.begin(), by_name.end());

  moonclient::RuleVocabularyStore store;
  store.Build({}, {}, offered, {}, names);
  const std::vector<ro::ui::RuleChoice> choices = MonsterChoices(store.view());
  REQUIRE(choices.size() == 4);
  // D'abord celui qu'on a vu, une seule fois, puis la table par nom.
  CHECK(choices[0].value == kPoring);
  CHECK(choices[1].value == kAngeling);
  CHECK(choices[2].value == kBaphomet);
  CHECK(choices[3].value == kAnotherPoring);
  CHECK(std::ranges::count(choices, static_cast<int>(kPoring), &ro::ui::RuleChoice::value) == 1);
  // Jamais vu, il se nomme et se décrit par la table.
  CHECK(choices[2].label == "Baphomet");
  CHECK(choices[2].detail == "niv. 81 · MVP · Demon · Dark 3 · Large");
}
