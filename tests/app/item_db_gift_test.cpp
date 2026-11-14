// LE TYPE QUALIFIÉ ET LES EFFETS DE DON, sans fenêtre : la catégorie que la
// base porte et les drapeaux qui qualifient le type, puis les objets qu'un
// script donne et les monstres qu'il invoque — tout ou rien, hors bloc et en
// littéraux. Les entrées sont celles de `dist/data/yaml_files/item_db.yml`.

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/server_db.h"

#include "game/item_db_block.h"
#include "game/item_script_facts.h"

using moonclient::ItemDbBlockOf;
using moonclient::ReadItemScript;
using ro::vfs::ServerItem;

namespace {

constexpr std::string_view kItemDb = R"yaml(Header:
  Type: ITEM_DB
  Version: 3

Body:
  - Id: 512
    AegisName: Apple
    Name: Apple
    Category: Cooked Food
  - Id: 603
    AegisName: Old_Blue_Box
    Name: Old Blue Box
    Type: Usable
    Flags: {BuyingStore: true, Container: true}
)yaml";
constexpr std::string_view kMobDb = R"yaml(Header:
  Type: MOB_DB
  Version: 5

Body:
  - Id: 1904
    AegisName: BOMBPORING
    Name: Bomb Poring
)yaml";
constexpr std::string_view kContext = R"yaml(version: 1
battle:
  base_exp_rate: 100
)yaml";

[[nodiscard]] ro::vfs::ServerDb Base() {
  auto db = ro::vfs::ParseServerDb(kItemDb, kMobDb, kContext);
  REQUIRE(db.has_value());
  return std::move(*db);
}

[[nodiscard]] bool Holds(const std::vector<std::string>& lines, std::string_view line) {
  return std::ranges::find(lines, line) != lines.end();
}

[[nodiscard]] std::size_t EffectCount(const std::vector<std::string>& lines) {
  return static_cast<std::size_t>(std::ranges::count_if(
      lines, [](const std::string& line) { return line.starts_with("^0000CCEffet :"); }));
}

}  // namespace

TEST_CASE("Type et don : 1301, un type d'arme sans qualificatif", "[app][item_db_gift]") {
  // « Axe » : la table disait « One-Handed Axe », le bloc dit le même type.
  const ServerItem axe{.item_id = 1301,
                       .type = "Weapon",
                       .sub_type = "1hAxe",
                       .locations = {"Right_Hand"},
                       .weapon_level = 1,
                       .refineable = true};
  const std::vector<std::string> lines = ItemDbBlockOf(axe);
  CHECK(Holds(lines, "^0000CCType :^000000 Hache à une main"));
  CHECK(EffectCount(lines) == 0);
}

TEST_CASE("Type et don : 603, un conteneur qui tire dans un groupe", "[app][item_db_gift]") {
  const ro::vfs::ServerDb db = Base();
  const ServerItem* const stored = db.FindItem(603);
  REQUIRE(stored != nullptr);
  // La base lit les drapeaux levés, dans l'ordre écrit.
  CHECK(stored->flags == std::vector<std::string>{"BuyingStore", "Container"});
  ServerItem box = *stored;
  box.script = "getrandgroupitem(IG_BlueBox,1);";
  const moonclient::ItemDbNames names{.db = &db};
  const std::vector<std::string> lines = ItemDbBlockOf(box, names);
  // Seul le conteneur qualifie le type : la boutique d'achat n'en dit rien.
  CHECK(Holds(lines, "^0000CCType :^000000 Consommable (Conteneur)"));
  CHECK(Holds(lines, "^0000CCEffet :^000000 donne 1 item(s) au hasard (groupe Blue Box)"));
  // `getgroupitem` : le groupe fixe le nombre.
  box.script = "getgroupitem(IG_Gift_Box_China);";
  CHECK(Holds(ItemDbBlockOf(box, names),
              "^0000CCEffet :^000000 donne des items du groupe Gift Box China"));
}

TEST_CASE("Type et don : 604, une invocation", "[app][item_db_gift]") {
  const ro::vfs::ServerDb db = Base();
  const moonclient::ItemDbNames names{.db = &db};
  ServerItem branch{
      .item_id = 604,
      .type = "Usable",
      .flags = {"DeadBranch"},
      .script =
          "monster "
          "\"this\",-1,-1,\"--ja--\",-1-MOBG_Branch_Of_Dead_Tree,1,\"huntmission::OnMobDbDead\";"};
  const std::vector<std::string> lines = ItemDbBlockOf(branch, names);
  CHECK(Holds(lines, "^0000CCType :^000000 Consommable (Invocation)"));
  CHECK(Holds(lines,
              "^0000CCEffet :^000000 invoque 1 monstre(s) au hasard (groupe Branch Of Dead Tree)"));
  // Un identifiant littéral nomme le monstre.
  branch.script = "monster \"this\",-1,-1,\"--ja--\",1904,1,\"\";";
  CHECK(Holds(ItemDbBlockOf(branch, names), "^0000CCEffet :^000000 invoque 1 × Bomb Poring"));
  // Un monstre tiré au sort entre deux bornes ne se dit pas.
  branch.script = "monster \"this\",-1,-1,\"--ja--\",rand(1324,1363),1,\"\";";
  CHECK(EffectCount(ItemDbBlockOf(branch, names)) == 0);
}

TEST_CASE("Type et don : getitem, un lien vers l'objet donné", "[app][item_db_gift]") {
  const ro::vfs::ServerDb db = Base();
  const moonclient::ItemDbNames names{.db = &db};
  const std::vector<std::string> lines = ItemDbBlockOf(
      ServerItem{.type = "Usable", .script = "getitem 512,2; getitem \"apple\",1;"}, names);
  CHECK(Holds(lines, "^0000CCEffet :^000000 donne 2 × <ITEM>Apple<INFO>512</INFO></ITEM>"));
  // Par son nom Aegis, sans égard à la casse.
  CHECK(Holds(lines, "^0000CCEffet :^000000 donne 1 × <ITEM>Apple<INFO>512</INFO></ITEM>"));
  // Un nom que la base ignore reste du texte.
  CHECK(
      Holds(ItemDbBlockOf(ServerItem{.type = "Usable", .script = "getitem \"Nothing\",3;"}, names),
            "^0000CCEffet :^000000 donne 3 × Nothing"));
}

TEST_CASE("Type et don : la catégorie que la base porte", "[app][item_db_gift]") {
  const ro::vfs::ServerDb db = Base();
  const ServerItem* const apple = db.FindItem(512);
  REQUIRE(apple != nullptr);
  CHECK(apple->category == "Cooked Food");
  CHECK(Holds(ItemDbBlockOf(*apple), "^0000CCType :^000000 Divers — Cooked Food"));
  // Catégorie et drapeau : une seule mention, celle du drapeau.
  const ServerItem box{.type = "Usable", .category = "container", .flags = {"Container"}};
  CHECK(Holds(ItemDbBlockOf(box), "^0000CCType :^000000 Consommable (Conteneur)"));
  const ServerItem both{.type = "Usable", .category = "Quest", .flags = {"NoConsume"}};
  CHECK(Holds(ItemDbBlockOf(both), "^0000CCType :^000000 Consommable — Quest (Non consommé)"));
}

TEST_CASE("Type et don : un don conditionnel ne dit rien", "[app][item_db_gift]") {
  // Dans un bloc, après une condition sans accolades, ou calculé : toute la
  // famille se tait, même les dons lisibles à côté.
  CHECK(ReadItemScript("getitem 512,1; if (rand(2)) { getitem 513,1; }").gifts.empty());
  CHECK(ReadItemScript("if (rand(2)) getitem 512,1;").gifts.empty());
  CHECK(ReadItemScript("getitem 512,rand(1,3);").gifts.empty());
  CHECK(ReadItemScript("getrandgroupitem(IG_BlueBox,0);").gifts.empty());
  CHECK(ReadItemScript("getgroupitem(.@group);").gifts.empty());
  CHECK(ReadItemScript("getitem callfunc(\"F_Rand\",512,513),1;").gifts.empty());
  CHECK(ReadItemScript("switch (rand(2)) { case 0: monster \"this\",-1,-1,\"--ja--\",1904,1; }")
            .summons.empty());
  // Les dons et les invocations sont deux familles : l'une ne tait pas l'autre.
  const moonclient::ItemScriptFacts facts =
      ReadItemScript("getitem 512,1; if (x) { monster \"this\",-1,-1,\"a\",1904,1; }");
  REQUIRE(facts.gifts.size() == 1);
  CHECK(facts.gifts[0].item_id == 512);
  CHECK(facts.summons.empty());
}
