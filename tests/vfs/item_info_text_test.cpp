// L'ÉCRIVAIN D'`item_info.yml` : la forme du script d'import, octet pour
// octet, et l'aller-retour lecture → écriture qui rend les mêmes octets.

#include "ro/vfs/item_info_text.h"

#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::vfs::ItemInfoHeader;
using ro::vfs::ItemInfoRecord;
using ro::vfs::ReadItemInfoText;
using ro::vfs::WriteItemInfoText;

namespace {

// UNE TABLE TELLE QUE LE SCRIPT D'IMPORT L'ÉCRIT, à la main : chaque champ, dans
// son ordre, et les textes tels que `json.dumps(…, ensure_ascii=False)` les
// rend — le coréen tel quel, `/` non échappé, un caractère de contrôle en
// `\u00xx` minuscule.
constexpr std::string_view kScriptTable =
    R"(# Une mention d'origine
#
#  Ce que le client montre d'un objet
version: 1
items: {
  2: {name: "Bouclier \"rond\" \\ / ok", resource: "빨간포션", slots: 3, view: 4, tint: "gold", description: ["a\u0000b\u001fc\nd\te\rf\bg\fh", "^0000CCPoids :^000000 40#", ""]},
  10: {},
  4000000000: {name: "Dernier", description: ["seul"]}
}
)";

[[nodiscard]] std::vector<ItemInfoRecord> Read(std::string_view text) {
  std::vector<ItemInfoRecord> records;
  const auto read = ReadItemInfoText(text, [&records](ItemInfoRecord&& record) {
    records.push_back(std::move(record));
    return ro::core::Result<void>{};
  });
  REQUIRE(read.has_value());
  return records;
}

}  // namespace

TEST_CASE("item_info.yml : une table relue puis réécrite rend ses propres octets",
          "[vfs][objets][ecriture]") {
  const std::vector<ItemInfoRecord> records = Read(kScriptTable);
  REQUIRE(records.size() == 3);
  CHECK(records[0].name == "Bouclier \"rond\" \\ / ok");
  CHECK(records[0].view == 4);
  CHECK(records[0].tint == "gold");
  CHECK(records[0].description[0] == std::string("a\0b\x1F"
                                                 "c\nd\te\rf\bg\fh",
                                                 15));
  CHECK(WriteItemInfoText(ItemInfoHeader(kScriptTable), records) == kScriptTable);
}

TEST_CASE("item_info.yml : l'en-tête est fait des commentaires de tête",
          "[vfs][objets][ecriture]") {
  CHECK(ItemInfoHeader("# a\n\n# b\nversion: 1\n# c\n") == "# a\n\n# b\n");
  CHECK(ItemInfoHeader("version: 1\n").empty());
}

TEST_CASE("item_info.yml : l'écrivain range par identifiant et omet les champs nuls",
          "[vfs][objets][ecriture]") {
  const std::vector<ItemInfoRecord> records = {
      ItemInfoRecord{.item_id = 502, .name = "Orange", .slots = 0},
      ItemInfoRecord{.item_id = 501, .resource = "r", .view = 7},
  };
  CHECK(WriteItemInfoText("# t\n", records) ==
        "# t\n"
        "version: 1\n"
        "items: {\n"
        "  501: {resource: \"r\", view: 7},\n"
        "  502: {name: \"Orange\"}\n"
        "}\n");
  CHECK(WriteItemInfoText("", {}) == "version: 1\nitems: {\n}\n");
}

TEST_CASE("item_info.yml : un caractère de contrôle s'écrit en \\u00xx et se relit",
          "[vfs][objets][ecriture]") {
  const std::vector<ItemInfoRecord> records = {
      ItemInfoRecord{.item_id = 1, .description = {std::string("x\0y\x7F", 4), "\x01\x1E"}}};
  const std::string text = WriteItemInfoText("", records);
  // ⚠ LE 0x7F N'EST PAS UN CONTRÔLE POUR JSON : il passe tel quel.
  CHECK(text.find(R"(["x\u0000y)"
                  "\x7F"
                  R"(", "\u0001\u001e"])") != std::string::npos);
  CHECK(Read(text) == records);
}

TEST_CASE("item_info.yml : la table ne porte plus ni effet au sol ni drapeau d'apparat",
          "[vfs][objets][ecriture]") {
  for (const std::string_view retired : {
           "version: 1\nitems: {1: {costume: true}}\n",
           "version: 1\nitems: {1: {effect: 1186}}\n",
           // Les champs des faits du serveur ne sont pas ceux de la table.
           "version: 1\nitems: {1: {type: \"WEAPON\"}}\n",
           "version: 1\nitems: {1: {locations: [\"HEAD_TOP\"]}}\n",
       }) {
    INFO(retired);
    const auto read =
        ReadItemInfoText(retired, [](ItemInfoRecord&&) { return ro::core::Result<void>{}; });
    CHECK_FALSE(read.has_value());
  }
}

TEST_CASE("Faits du serveur : la forme de la table, plus le type et les emplacements",
          "[vfs][objets][ecriture]") {
  constexpr std::string_view kFacts =
      "# Les faits du serveur\n"
      "version: 1\n"
      "items: {\n"
      "  1201: {name: \"Knife\", slots: 3, type: \"WEAPON\", locations: [\"RIGHT_HAND\"]},\n"
      "  2220: {name: \"Hat\", type: \"ARMOR\", locations: [\"COSTUME_HEAD_TOP\", \"HEAD_TOP\"]},\n"
      "  501: {name: \"Red Potion\", type: \"HEALING\"}\n"
      "}\n";
  std::vector<ro::vfs::ItemServerFactsRecord> facts;
  const auto read =
      ro::vfs::ReadItemServerFactsText(kFacts, [&facts](ro::vfs::ItemServerFactsRecord&& record) {
        facts.push_back(std::move(record));
        return ro::core::Result<void>{};
      });
  REQUIRE(read.has_value());
  REQUIRE(facts.size() == 3);
  CHECK(facts[0].sheet == ItemInfoRecord{.item_id = 1201, .name = "Knife", .slots = 3});
  CHECK(facts[0].type == "WEAPON");
  CHECK(facts[0].locations == std::vector<std::string>{"RIGHT_HAND"});
  CHECK(facts[1].locations == std::vector<std::string>{"COSTUME_HEAD_TOP", "HEAD_TOP"});
  CHECK(facts[2].type == "HEALING");
  CHECK(facts[2].locations.empty());
  // ⚠ LE LECTEUR DES FAITS NE PREND PAS N'IMPORTE QUOI : un champ retiré de la
  // table ne s'y glisse pas davantage.
  const auto refused = ro::vfs::ReadItemServerFactsText(
      "version: 1\nitems: {1: {costume: true}}\n",
      [](ro::vfs::ItemServerFactsRecord&&) { return ro::core::Result<void>{}; });
  CHECK_FALSE(refused.has_value());
}
