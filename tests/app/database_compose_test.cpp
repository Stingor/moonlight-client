// LA BASE DE DONNÉES VUE DE L'ÉCRAN, sans fenêtre : la fiche d'un objet en
// lignes, ses monstres par onglet, la recherche que les filtres demandent, la
// fiche d'un monstre tirée de la base — taux réels et cartes d'apparition —,
// `/db`, et les vendeurs que le Marché rend.

#include "game/database_compose.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/database_window.h"
#include "ro/vfs/server_db.h"

using moonclient::ItemFactLine;
using ro::ui::DatabaseFilters;

namespace {

constexpr std::uint32_t kJellopy = 909;
constexpr std::uint32_t kSword = 1101;
constexpr std::uint32_t kPoring = 1002;

constexpr std::string_view kItemDb = R"yaml(Header:
  Type: ITEM_DB
  Version: 3

Body:
  - Id: 909
    AegisName: Jellopy
    Name: Jellopy
    Buy: 6
    Sell: 3
    Weight: 10
  - Id: 1101
    AegisName: Sword
    Name: Sword
    Type: Weapon
    SubType: 1hSword
    Buy: 1550
    Sell: 775
    Weight: 15
    Attack: 25
    Slots: 3
    Jobs: {All: true}
    Locations: {Right_Hand: true}
    WeaponLevel: 1
    EquipLevelMin: 2
    Refineable: true
)yaml";

constexpr std::string_view kMobDb = R"yaml(Header:
  Type: MOB_DB
  Version: 5

Body:
  - Id: 1002
    AegisName: PORING
    Name: Poring
    Level: 1
    Hp: 50
    BaseExp: 2
    JobExp: 3
    Attack: 7
    Attack2: 10
    Vit: 4
    Int: 14
    Drops:
      - {Item: Jellopy, Rate: 7000}
      - {Item: Sword, Rate: 0}
  - Id: 1003
    AegisName: PORING_TWIN
    Name: Poring
    Level: 2
)yaml";

constexpr std::string_view kContext = R"yaml(version: 1
battle:
  item_rate_common: 50
  base_exp_rate: 100000
  job_exp_rate: 250
spawns:
  1002: {prt_fild08: 70, pay_fild01: 30}
monsters:
  1002: [3, 1, 1, 1, 0, 131]
)yaml";

[[nodiscard]] ro::vfs::ServerDb Parsed() {
  auto db = ro::vfs::ParseServerDb(kItemDb, kMobDb, kContext);
  REQUIRE(db.has_value());
  return std::move(*db);
}

[[nodiscard]] std::string ValueOf(const std::vector<ItemFactLine>& lines, std::string_view label) {
  for (const ItemFactLine& line : lines) {
    if (line.label == label) {
      return line.value;
    }
  }
  return "(absent)";
}

[[nodiscard]] std::string NameOf(std::uint32_t item_id) { return "#" + std::to_string(item_id); }

}  // namespace

TEST_CASE("Base de données : la fiche d'un objet en lignes", "[app][database]") {
  const ro::vfs::ServerDb db = Parsed();
  const std::vector<ItemFactLine> sword = moonclient::ItemFactsOf(*db.FindItem(kSword));
  CHECK(ValueOf(sword, "Type") == "Armes (1hSword)");
  CHECK(ValueOf(sword, "Poids") == "1,5");
  CHECK(ValueOf(sword, "Attaque") == "25");
  CHECK(ValueOf(sword, "Emplacements de carte") == "3");
  CHECK(ValueOf(sword, "Se porte") == "Arme");
  CHECK(ValueOf(sword, "Niveau requis") == "2");
  CHECK(ValueOf(sword, "Jobs") == "Tous les jobs");
  CHECK(ValueOf(sword, "Raffinable") == "Oui");
  // Un objet divers n'a ni attaque, ni emplacement, ni raffinement.
  const std::vector<ItemFactLine> jellopy = moonclient::ItemFactsOf(*db.FindItem(kJellopy));
  CHECK(ValueOf(jellopy, "Type") == "Divers");
  CHECK(ValueOf(jellopy, "Poids") == "1");
  CHECK(ValueOf(jellopy, "Attaque") == "(absent)");
  CHECK(ValueOf(jellopy, "Raffinable") == "(absent)");
  // Ouvert à tous par défaut, un objet divers ne dit rien des jobs.
  CHECK(ValueOf(jellopy, "Jobs") == "(absent)");
}

TEST_CASE("Base de données : le type d'un objet du sac, par la base puis par sa famille",
          "[app][database]") {
  // Les types que le fil donne : 3 divers, 5 arme, 6 carte ; 0x7F, aucune
  // famille connue.
  constexpr std::uint8_t kWireEtc = 3;
  constexpr std::uint8_t kWireWeapon = 5;
  constexpr std::uint8_t kWireCard = 6;
  constexpr std::uint8_t kWireUnknown = 0x7F;
  constexpr std::uint32_t kPoringCard = 4001;
  const auto held = [](std::uint32_t item_id, std::uint8_t type) {
    ro::sim::InventoryItem item;
    item.item_id = item_id;
    item.type = type;
    item.amount = 1;
    return item;
  };
  const ro::vfs::ServerDb db = Parsed();
  // La base l'emporte sur la famille du fil.
  CHECK(moonclient::ItemTypeLabelOf(held(kJellopy, kWireCard), &db) == "Divers");
  CHECK(moonclient::ItemTypeLabelOf(held(kSword, kWireEtc), &db) == "Armes");
  // Un objet qu'elle ignore, une base absente ou pas encore lue : la famille.
  CHECK(moonclient::ItemTypeLabelOf(held(kPoringCard, kWireCard), &db) == "Cartes");
  CHECK(moonclient::ItemTypeLabelOf(held(kSword, kWireWeapon), nullptr) == "Armes");
  const ro::vfs::ServerDb unread;
  CHECK(moonclient::ItemTypeLabelOf(held(kJellopy, kWireEtc), &unread) == "Divers");
  CHECK(moonclient::ItemTypeLabelOf(held(kPoringCard, kWireUnknown), nullptr) == "Autre");
  CHECK(moonclient::ItemDbTypeOfCategory(ro::sim::ItemCategory::kOther).empty());
}

TEST_CASE("Base de données : les monstres qui lâchent un objet, par onglet", "[app][database]") {
  const ro::vfs::ServerDb db = Parsed();
  const moonclient::DropperRows rows = moonclient::DropperRowsOf(db, kJellopy);
  // Le Poring est ordinaire : il va du côté des monstres, au taux réel, nommé
  // par la base.
  REQUIRE(rows.ordinary.size() == 1);
  CHECK(rows.boss.empty());
  CHECK(rows.ordinary[0].monster_id == kPoring);
  CHECK(rows.ordinary[0].name == "Poring");
  CHECK(rows.ordinary[0].boss_rank == ro::vfs::kServerMonsterOrdinary);
  CHECK_FALSE(rows.ordinary[0].mvp_reward);
  CHECK(rows.ordinary[0].rate_hundredths == db.DroppersOf(kJellopy)[0].rate_hundredths);
  // Un objet que personne ne lâche n'a de ligne d'aucun côté.
  constexpr std::uint32_t kNobodyDrops = 4001;
  CHECK(moonclient::DropperRowsOf(db, kNobodyDrops).ordinary.empty());
}

TEST_CASE("Base de données : les filtres de la fenêtre font la recherche", "[app][database]") {
  DatabaseFilters filters;
  ro::vfs::ItemSearch all = moonclient::ItemSearchOf(filters, "épée");
  CHECK(all.text == "épée");
  CHECK(all.type.empty());
  CHECK(all.locations.empty());
  // Le rang 1 de chaque liste est la première entrée de sa table.
  filters.item_type = 1;
  filters.item_place = 5;
  filters.item_level_min = 10;
  const ro::vfs::ItemSearch chosen = moonclient::ItemSearchOf(filters, "");
  CHECK(chosen.type == ro::ui::kDatabaseItemTypes[0].server_type);
  REQUIRE(chosen.locations.size() == ro::ui::kDatabaseWeaponPlaces.size());
  CHECK(chosen.locations[0] == "Right_Hand");
  CHECK(chosen.level_min == 10);
  // Un rang hors des tables ne filtre rien.
  filters.item_type = 99;
  CHECK(moonclient::ItemSearchOf(filters, "").type.empty());

  DatabaseFilters monsters;
  monsters.race = 4;
  monsters.rank = 3;
  const ro::vfs::MonsterSearch search = moonclient::MonsterSearchOf(monsters, "poring");
  CHECK(search.race == 3);
  CHECK(search.element == ro::vfs::kAnyMonsterTrait);
  CHECK(search.rank == ro::vfs::kServerMonsterMvp);
}

TEST_CASE("Base de données : la fiche d'un monstre tirée de la base", "[app][database]") {
  const ro::vfs::ServerDb db = Parsed();
  const ro::sim::MonsterSheet sheet =
      moonclient::MonsterSheetOf(*db.FindMonster(kPoring), db, NameOf);
  CHECK(sheet.name == "Poring");
  CHECK(sheet.sprite_class == kPoring);
  // L'EXP RÉELLE, taux du serveur appliqués, et l'écrite pour l'infobulle.
  CHECK(sheet.base_exp == 2000);
  CHECK(sheet.job_exp == 7);
  REQUIRE(sheet.written_exp.has_value());
  CHECK(sheet.written_exp->base_exp == 2);
  CHECK(sheet.written_exp->base_rate_percent == 100000);
  CHECK(sheet.written_exp->job_rate_percent == 250);
  CHECK(sheet.atk_min == 7);
  CHECK(sheet.atk_max == 10);
  // Les formules d'avant la refonte : INT + (INT/7)², INT + (INT/5)², VIT, INT + VIT/2.
  CHECK(sheet.matk_min == 18);
  CHECK(sheet.matk_max == 18);
  CHECK(sheet.def_bonus == 4);
  CHECK(sheet.mdef_bonus == 16);
  CHECK(sheet.race == 3);
  CHECK(sheet.boss_rank == 0);
  // Le taux RÉEL — divers ×0,5 — ; la ligne à taux nul est retirée.
  REQUIRE(sheet.drops.size() == 1);
  CHECK(sheet.drops[0].item_id == kJellopy);
  CHECK(sheet.drops[0].rate_hundredths == 3500);
  CHECK(sheet.drops[0].name == "#909");
  REQUIRE(sheet.spawns.size() == 2);
  CHECK(sheet.spawns[0].map_name == "prt_fild08");
  CHECK(sheet.spawns[0].count == 70);
  // L'homonymie se compte dans la base.
  CHECK(sheet.namesake_count == 2);
  CHECK(sheet.namesake_origin == kPoring);
  // Ni compétences, ni gain d'EXP : le serveur seul les sait.
  CHECK(sheet.skills.empty());
  CHECK_FALSE(sheet.exp_gain.has_value());
}

TEST_CASE("Base de données : /db, avec ou sans recherche", "[app][database]") {
  CHECK(moonclient::ReadDatabaseCommand("/db") == std::string());
  CHECK(moonclient::ReadDatabaseCommand("/DB  poring  ") == std::string("poring"));
  CHECK_FALSE(moonclient::ReadDatabaseCommand("/dbx").has_value());
  CHECK_FALSE(moonclient::ReadDatabaseCommand("/d").has_value());
  CHECK_FALSE(moonclient::ReadDatabaseCommand("bonjour /db").has_value());
}

TEST_CASE("Base de données : un vendeur du Marché devient une ligne", "[app][database]") {
  ro::sim::MarketOffer offer;
  offer.shop_name = "Tool Dealer#moc1";
  offer.map_name = "prontera";
  offer.cell_x = 150;
  offer.cell_y = 120;
  offer.price_zeny = 1500;
  const std::vector<moonclient::SellerLine> lines =
      moonclient::SellerLinesOf(std::vector<ro::sim::MarketOffer>{offer}, NameOf);
  REQUIRE(lines.size() == 1);
  CHECK(lines[0].npc == "Tool Dealer");
  CHECK(lines[0].place == "prontera 150,120");
  CHECK_FALSE(lines[0].price.empty());
}

TEST_CASE("Base de données : les map drops d'un objet et d'un monstre", "[app][database]") {
  constexpr std::string_view kMapDrops = R"yaml(Header:
  Type: MAP_DROP_DB
  Version: 2

Body:
  - Map: gef_d01_i
    Monster: PORING
    Drops:
      - {Item: Sword, Rate: 25, RandomOptionGroup: GROUP_A}
  - Map: nowhere
    Drops:
      - {Item: Sword, Rate: 5}
)yaml";
  auto db = ro::vfs::ParseServerDb(kItemDb, kMobDb, kContext, {}, kMapDrops);
  REQUIRE(db.has_value());

  const std::vector<ro::ui::DescriptionMapDropRow> rows = moonclient::MapDropRowsOf(
      *db, kSword, [](const std::string& map_name) { return "Carte " + map_name; });
  REQUIRE(rows.size() == 2);
  CHECK(rows[0].monster_id == kPoring);
  CHECK(rows[0].monster_name == "Poring");
  CHECK(rows[0].map_name == "gef_d01_i");
  CHECK(rows[0].map_label == "Carte gef_d01_i");
  CHECK(rows[0].rate_thousandths == 25);
  CHECK(rows[0].random_options);
  // Une ligne de toute la carte n'a pas de monstre à nommer.
  CHECK(rows[1].monster_id == ro::vfs::kServerMapDropAllMonsters);
  CHECK(rows[1].monster_name.empty());

  const std::vector<ro::sim::MonsterMapDrop> drops = moonclient::MonsterMapDropsOf(
      *db, kPoring, [](std::uint32_t item_id) { return "#" + std::to_string(item_id); });
  REQUIRE(drops.size() == 1);
  CHECK(drops[0].item_id == kSword);
  CHECK(drops[0].name == "#1101");
  CHECK(drops[0].map_name == "gef_d01_i");
  CHECK(drops[0].rate_thousandths == 25);
}
