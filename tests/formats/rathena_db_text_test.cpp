// LE CURSEUR DES BASES RATHENA SOUS FORME STRICTE : chaque forme de valeur que
// `server_db.py` écrit — texte nu, texte à la JSON, table d'une ligne, table et
// suite vides, bloc aux trois chomps, suite de tables —, et le refus de ce qui
// sort de la forme.

#include "ro/formats/rathena_db_text.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::formats::RathenaEntry;
using ro::formats::RathenaField;
using ro::formats::RathenaShape;

namespace {

constexpr std::string_view kTable = R"yaml(# Les objets du serveur
# (un commentaire de tête)

Header:
  Type: ITEM_DB
  Version: 3

Body:
  - Id: 501
    Name: Red Potion
    Quoted: "Catalog: \"Silver\" #1 é😀"
    Jobs: {All: true, Thief: false}
    Spaced: {Name: "a, b", Plain: c d}
    Classes: {}
    Empty: []
    Script: |
      itemheal rand(45,65),0;
        bonus bStr,1;

      end;
    Stripped: |-
      sans fin
    Kept: |+
      gardé

  - Id: 1002
    Drops:
      - {Item: Jellopy, Rate: 7000}
      - {Item: Poring_Card, Rate: 1, StealProtected: true}
    Last: x
)yaml";

[[nodiscard]] std::vector<RathenaEntry> Read(std::string_view text, std::string_view type,
                                             bool& ok) {
  std::vector<RathenaEntry> entries;
  const auto read = ro::formats::ReadRathenaDb(
      text, type, "essai", [&](const RathenaEntry& entry) { entries.push_back(entry); });
  ok = read.has_value();
  return entries;
}

[[nodiscard]] const RathenaField& FieldOf(const RathenaEntry& entry, std::string_view key) {
  const auto found = std::ranges::find(entry, key, &RathenaField::key);
  REQUIRE(found != entry.end());
  return *found;
}

}  // namespace

TEST_CASE("Base rAthena stricte : chaque forme de valeur se lit", "[formats][rathena_db]") {
  bool ok = false;
  const std::vector<RathenaEntry> entries = Read(kTable, "ITEM_DB", ok);
  REQUIRE(ok);
  REQUIRE(entries.size() == 2);
  const RathenaEntry& potion = entries[0];
  CHECK(FieldOf(potion, "Id").text == "501");
  CHECK(FieldOf(potion, "Name").text == "Red Potion");
  CHECK(FieldOf(potion, "Quoted").text == "Catalog: \"Silver\" #1 \xC3\xA9\xF0\x9F\x98\x80");
  const RathenaField& jobs = FieldOf(potion, "Jobs");
  CHECK(jobs.shape == RathenaShape::kMap);
  REQUIRE(jobs.map.size() == 2);
  CHECK(jobs.map[0].key == "All");
  CHECK(jobs.map[0].value == "true");
  CHECK(jobs.map[1].value == "false");
  const RathenaField& spaced = FieldOf(potion, "Spaced");
  REQUIRE(spaced.map.size() == 2);
  CHECK(spaced.map[0].value == "a, b");
  CHECK(spaced.map[1].value == "c d");
  CHECK(FieldOf(potion, "Classes").shape == RathenaShape::kMap);
  CHECK(FieldOf(potion, "Classes").map.empty());
  CHECK(FieldOf(potion, "Empty").shape == RathenaShape::kList);
  // Le bloc garde ses retraits intérieurs et sa ligne vide ; `|` un seul saut
  // final, `|-` aucun, `|+` tous.
  CHECK(FieldOf(potion, "Script").text == "itemheal rand(45,65),0;\n  bonus bStr,1;\n\nend;\n");
  CHECK(FieldOf(potion, "Stripped").text == "sans fin");
  CHECK(FieldOf(potion, "Kept").text == "gard\xC3\xA9\n\n");
  const RathenaEntry& poring = entries[1];
  const RathenaField& drops = FieldOf(poring, "Drops");
  CHECK(drops.shape == RathenaShape::kList);
  REQUIRE(drops.list.size() == 2);
  CHECK(drops.list[1][2].key == "StealProtected");
  CHECK(FieldOf(poring, "Last").text == "x");
}

TEST_CASE("Base rAthena stricte : une autre base ou une forme libre est refusée",
          "[formats][rathena_db]") {
  bool ok = true;
  static_cast<void>(Read(kTable, "MOB_DB", ok));
  CHECK_FALSE(ok);
  // Une table imbriquée sur plusieurs lignes n'est pas de la forme.
  constexpr std::string_view kNested =
      "Header:\n  Type: ITEM_DB\n\nBody:\n  - Id: 1\n    Jobs:\n"
      "      All: true\n";
  static_cast<void>(Read(kNested, "ITEM_DB", ok));
  CHECK_FALSE(ok);
  constexpr std::string_view kUnclosed =
      "Header:\n  Type: ITEM_DB\n\nBody:\n  - Id: 1\n    Name: \"a\n";
  static_cast<void>(Read(kUnclosed, "ITEM_DB", ok));
  CHECK_FALSE(ok);
  // Un corps vide se lit, sans entrée.
  const auto empty = Read("Header:\n  Type: ITEM_DB\n\nBody: []\n", "ITEM_DB", ok);
  CHECK(ok);
  CHECK(empty.empty());
}
