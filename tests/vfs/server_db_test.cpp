// LA BASE DE DONNÉES DU SERVEUR : la lecture des trois tables que
// `scripts/yaml_tables/server_db.py` écrit, le taux réel d'un butin calculé
// comme le serveur, et la recherche par nom, par identifiant et par filtre.
//
// 🔴 LA FUSION N'EST PAS ÉPROUVÉE ICI : le script l'a faite, et son propre essai
// la juge. Ce banc juge ce que le client en LIT.

#include "ro/vfs/server_db.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/search_key.h"
#include "ro/vfs/server_db_search.h"

using ro::vfs::DropSettings;
using ro::vfs::EffectiveDropRate;
using ro::vfs::ParseServerDb;
using ro::vfs::ServerDb;

namespace {

constexpr std::uint32_t kRedPotion = 501;
constexpr std::uint32_t kJellopy = 909;
constexpr std::uint32_t kSword = 1101;
constexpr std::uint32_t kPoringCard = 4001;
constexpr std::uint32_t kPoring = 1002;
constexpr std::uint32_t kBaphomet = 1039;
constexpr std::uint32_t kChest = 1324;
constexpr std::uint32_t kGroomRing = 2634;
constexpr std::uint32_t kViolin = 1901;
constexpr std::uint32_t kRope = 1950;
constexpr std::uint32_t kBareWeapon = 1100;
constexpr std::uint32_t kBareCard = 4002;
constexpr std::uint32_t kBareMonster = 1001;

constexpr std::string_view kItemDb = R"yaml(# Les objets du serveur
Header:
  Type: ITEM_DB
  Version: 3

Body:
  - Id: 501
    AegisName: Red_Potion
    Name: Red Potion
    Type: Healing
    Buy: 10
    Sell: 5
    Weight: 70
    Script: |
      itemheal rand(45,65),0;
  - Id: 909
    AegisName: Jellopy
    Name: Jellopy
    Buy: 6
    Sell: 3
    Weight: 10
  - Id: 1101
    AegisName: Sword
    Name: "Sword [3]"
    Type: Weapon
    SubType: 1hSword
    Buy: 1550
    Sell: 775
    Weight: 500
    Attack: 25
    Slots: 3
    Jobs: {Knight: true, Swordman: true, Thief: false}
    Locations: {Right_Hand: true}
    WeaponLevel: 1
    EquipLevelMin: 2
    Refineable: true
    EquipScript: "bonus bStr,1;"
  - Id: 4001
    AegisName: Poring_Card
    Name: Poring Card
    Type: Card
    Buy: 20
    Sell: 10
)yaml";

constexpr std::string_view kMobDb = R"yaml(Header:
  Type: MOB_DB
  Version: 5

Body:
  - Id: 1002
    AegisName: PORING
    Name: Poring
    JapaneseName: Poporing
    Level: 1
    Hp: 50
    BaseExp: 2
    JobExp: 1
    Attack: 7
    Attack2: 10
    Dex: 6
    Luk: 30
    AttackRange: 1
    WalkSpeed: 400
    Drops:
      - {Item: Jellopy, Rate: 7000}
      - {Item: Unknown_Thing, Rate: 10}
      - {Item: Red_Potion, Rate: 50}
      - {Item: Sword, Rate: 100}
      - {Item: Poring_Card, Rate: 1, StealProtected: true}
  - Id: 1039
    AegisName: BAPHOMET
    Name: Baphomet
    Level: 81
    Hp: 668000
    MvpExp: 100000
    MvpDrops:
      - {Item: Sword, Rate: 2000}
    Drops:
      - {Item: Red_Potion, Rate: 50}
  - Id: 1324
    AegisName: TREASURE_BOX1
    Name: Treasure Chest
    Level: 98
    RaceGroups: {Treasure: true}
    Drops:
      - {Item: Jellopy, Rate: 100}
)yaml";

constexpr std::string_view kContext = R"yaml(version: 1
mode: Prerenewal
battle:
  item_rate_common: 1000
  item_drop_common_min: 1000
  item_drop_common_max: 10000
  item_rate_heal: 2500
  item_rate_heal_mvp: 500
  item_drop_heal_min: 500
  item_drop_heal_max: 10000
  item_rate_equip: 10000
  item_drop_equip_min: 500
  item_drop_equip_max: 500
  item_rate_card: 10000
  item_drop_card_min: 50
  item_rate_mvp: 1000
  item_rate_treasure: 1250
  drop_rate0item: 0
  drop_rateincrease: 0
  override_mob_names: 1
item_ratios: {}
spawns:
  1002: {prt_fild08: 70, pay_fild01: 30}
  1039: {prt_maze03: 1}
monsters:
  1002: [3, 1, 1, 1, 0, 131]
  1039: [6, 7, 3, 2, 2, 103298709]
  1324: [0, 0, 1, 0, 1, 0]
)yaml";

[[nodiscard]] ServerDb Parsed(std::string_view context = kContext) {
  auto db = ParseServerDb(kItemDb, kMobDb, context);
  INFO((db.has_value() ? std::string() : db.error().detail));
  REQUIRE(db.has_value());
  REQUIRE(db->known());
  return std::move(*db);
}

// La clé de chaque objet, au rang de la base : le nom puis l'AegisName.
[[nodiscard]] std::vector<std::string> ItemKeys(const ServerDb& db) {
  std::vector<std::string> keys;
  for (const ro::vfs::ServerItem& item : db.items()) {
    keys.push_back(ro::core::SearchKey(item.name) + "\n" + ro::core::SearchKey(item.aegis_name));
  }
  return keys;
}

[[nodiscard]] std::vector<std::string> MonsterKeys(const ServerDb& db) {
  std::vector<std::string> keys;
  for (const ro::vfs::ServerMonster& monster : db.monsters()) {
    keys.push_back(ro::core::SearchKey(monster.name));
  }
  return keys;
}

}  // namespace

TEST_CASE("Base de données : les objets se lisent avec tous leurs champs utiles",
          "[vfs][server_db]") {
  const ServerDb db = Parsed();
  REQUIRE(db.items().size() == 4);
  const ro::vfs::ServerItem* sword = db.FindItem(kSword);
  REQUIRE(sword != nullptr);
  CHECK(sword->aegis_name == "Sword");
  CHECK(sword->name == "Sword [3]");
  CHECK(sword->type == "Weapon");
  CHECK(sword->sub_type == "1hSword");
  CHECK(sword->buy_zeny == 1550);
  CHECK(sword->sell_zeny == 775);
  CHECK(sword->weight_tenths == 500);
  CHECK(sword->attack == 25);
  CHECK(sword->slots == 3);
  // Seuls les métiers LEVÉS comptent.
  CHECK(sword->jobs == std::vector<std::string>{"Knight", "Swordman"});
  CHECK(sword->locations == std::vector<std::string>{"Right_Hand"});
  CHECK(sword->weapon_level == 1);
  CHECK(sword->equip_level_min == 2);
  CHECK(sword->refineable);
  CHECK(sword->equip_script == "bonus bStr,1;");
  // Un script en bloc garde son saut de ligne final, comme le serveur le lit.
  CHECK(db.FindItem(kRedPotion)->script == "itemheal rand(45,65),0;\n");
  // Sans `Type`, l'objet est divers, comme au serveur.
  CHECK(db.FindItem(kJellopy)->type == "Etc");
  CHECK(db.FindItem(1) == nullptr);
}

TEST_CASE("Base de données : les monstres prennent les nombres du contexte", "[vfs][server_db]") {
  const ServerDb db = Parsed();
  const ro::vfs::ServerMonster* poring = db.FindMonster(kPoring);
  REQUIRE(poring != nullptr);
  CHECK(poring->name == "Poring");
  CHECK(poring->hp == 50);
  CHECK(poring->attack == 7);
  CHECK(poring->attack2 == 10);
  CHECK(poring->luk == 30);
  CHECK(poring->walk_ms_per_cell == 400);
  CHECK(poring->race == 3);
  CHECK(poring->element == 1);
  CHECK(poring->size == 1);
  CHECK(poring->rank == ro::vfs::kServerMonsterOrdinary);
  CHECK(poring->mode == 131);
  // L'objet inconnu de l'item_db est sauté, comme au serveur.
  REQUIRE(poring->drops.size() == 4);
  CHECK(poring->drops[0].item_id == kJellopy);
  CHECK(poring->drops[3].steal_protected);
  CHECK(db.FindMonster(kBaphomet)->rank == ro::vfs::kServerMonsterMvp);
  CHECK(db.FindMonster(kChest)->treasure);
  // Les cartes d'apparition, de la plus peuplée à la moins peuplée.
  const auto spawns = db.SpawnsOf(kPoring);
  REQUIRE(spawns.size() == 2);
  CHECK(spawns[0].map_name == "prt_fild08");
  CHECK(spawns[0].count == 70);
  CHECK(db.SpawnsOf(kChest).empty());
}

// Le script n'écrit pas un champ égal au défaut que le serveur pose sur une
// entrée neuve : chaque champ omis se relit à ce défaut.
constexpr std::string_view kBareItemDb = R"yaml(Header:
  Type: ITEM_DB
  Version: 3

Body:
  - Id: 2634
    AegisName: Wedding_Ring_M
    Name: Groom Ring
  - Id: 1901
    AegisName: Violin
    Name: Violin
    Type: Weapon
    SubType: Musical
    Gender: Female
  - Id: 1950
    AegisName: Rope
    Name: Rope
    Type: Weapon
    SubType: Whip
  - Id: 1100
    AegisName: Bare_Weapon
    Name: Bare Weapon
    Type: Weapon
  - Id: 4002
    AegisName: Bare_Card
    Name: Bare Card
    Type: Card
)yaml";

constexpr std::string_view kBareMobDb = R"yaml(Header:
  Type: MOB_DB
  Version: 5

Body:
  - Id: 1001
    AegisName: BARE
    Name: Bare
)yaml";

constexpr std::string_view kBareContext = R"yaml(version: 1
battle:
  override_mob_names: 2
)yaml";

TEST_CASE("Base de données : un champ omis se relit au défaut du serveur", "[vfs][server_db]") {
  auto read = ParseServerDb(kBareItemDb, kBareMobDb, kBareContext);
  REQUIRE(read.has_value());
  const ro::vfs::ServerItem* ring = read->FindItem(kGroomRing);
  REQUIRE(ring != nullptr);
  CHECK(ring->type == ro::vfs::kServerItemDefaultType);
  CHECK(ring->sub_type.empty());
  CHECK(ring->buy_zeny == 0);
  CHECK(ring->sell_zeny == 0);
  CHECK(ring->weight_tenths == 0);
  CHECK(ring->attack == 0);
  CHECK(ring->magic_attack == 0);
  CHECK(ring->defense == 0);
  CHECK(ring->range_cells == 0);
  CHECK(ring->slots == 0);
  // Sans `Jobs`, le serveur ouvre l'objet à tous les jobs.
  CHECK(ring->jobs == std::vector<std::string>{"All"});
  CHECK(ring->jobs_lowered.empty());
  CHECK(ring->classes == std::vector<std::string>{"All"});
  CHECK(ring->locations.empty());
  CHECK(ring->weapon_level == 0);
  CHECK(ring->armor_level == 0);
  CHECK(ring->equip_level_min == ro::vfs::kServerItemDefaultEquipLevelMin);
  CHECK(ring->equip_level_max == 0);
  CHECK_FALSE(ring->refineable);
  CHECK(ring->view == 0);
  CHECK(ring->script.empty());
  // L'anneau du marié est aux hommes, sans `Gender` écrit.
  CHECK(ring->gender == "Male");
  // L'instrument est aux hommes et le fouet aux femmes, écrit ou non.
  CHECK(read->FindItem(kViolin)->gender == "Male");
  CHECK(read->FindItem(kRope)->gender == "Female");
  // Le sous-type 0 : un poing pour une arme, une carte ordinaire ; sans
  // genre imposé, les deux.
  const ro::vfs::ServerItem* weapon = read->FindItem(kBareWeapon);
  CHECK(weapon->sub_type == ro::vfs::kServerWeaponDefaultSubType);
  CHECK(weapon->gender == ro::vfs::kServerItemDefaultGender);
  CHECK(read->FindItem(kBareCard)->sub_type == ro::vfs::kServerCardDefaultSubType);

  const ro::vfs::ServerMonster* bare = read->FindMonster(kBareMonster);
  REQUIRE(bare != nullptr);
  // Sans `JapaneseName`, le nom japonais est le nom.
  CHECK(bare->name == "Bare");
  CHECK(bare->level == ro::vfs::kServerMonsterDefaultLevel);
  CHECK(bare->hp == ro::vfs::kServerMonsterDefaultHp);
  CHECK(bare->sp == ro::vfs::kServerMonsterDefaultSp);
  CHECK(bare->base_exp == 0);
  CHECK(bare->job_exp == 0);
  CHECK(bare->mvp_exp == 0);
  CHECK(bare->attack == 0);
  CHECK(bare->attack2 == 0);
  CHECK(bare->defense == 0);
  CHECK(bare->magic_defense == 0);
  for (const std::uint16_t stat :
       {bare->str, bare->agi, bare->vit, bare->int_stat, bare->dex, bare->luk}) {
    CHECK(stat == ro::vfs::kServerMonsterDefaultStat);
  }
  CHECK(bare->attack_range_cells == 0);
  CHECK(bare->walk_ms_per_cell == ro::vfs::kServerMonsterDefaultWalkMs);
  CHECK(bare->element_level == ro::vfs::kServerMonsterDefaultElementLevel);
  CHECK_FALSE(bare->treasure);
  CHECK(bare->drops.empty());
  CHECK(bare->mvp_drops.empty());
}

TEST_CASE("Base de données : override_mob_names à 2 montre le nom japonais", "[vfs][server_db]") {
  std::string context(kContext);
  context.replace(context.find("override_mob_names: 1"),
                  std::string_view("override_mob_names: 1").size(), "override_mob_names: 2");
  CHECK(Parsed(context).FindMonster(kPoring)->name == "Poporing");
}

TEST_CASE("Base de données : des tables absentes font une base inconnue, une illisible une erreur",
          "[vfs][server_db]") {
  const auto absent = ParseServerDb({}, {}, {});
  REQUIRE(absent.has_value());
  CHECK_FALSE(absent->known());
  CHECK_FALSE(ParseServerDb(kMobDb, kMobDb, kContext).has_value());
  CHECK_FALSE(ParseServerDb(kItemDb, kMobDb, "version: 9\n").has_value());
  CHECK_FALSE(ParseServerDb(kItemDb, "[unclosed", kContext).has_value());
  // Une status_db d'un autre type est une erreur, pas une base sans états.
  CHECK_FALSE(ParseServerDb(kItemDb, kMobDb, kContext, kMobDb).has_value());
}

TEST_CASE("Taux réel : le ratio de la famille, puis les bornes", "[vfs][server_db][rates]") {
  const ServerDb db = Parsed();
  const ro::vfs::ServerMonster& poring = *db.FindMonster(kPoring);
  // Divers ×10 : 70 % devient 100 %, plafonné par le maximum.
  CHECK(db.RateOf(poring, poring.drops[0], false) == 10000);
  // Soin ×25 : 0,5 % devient 12,5 %.
  CHECK(db.RateOf(poring, poring.drops[1], false) == 1250);
  // Équipement ×100, borné à 5 % par un maximum égal au minimum.
  CHECK(db.RateOf(poring, poring.drops[2], false) == 500);
  // Carte ×100 : 0,01 % devient 1 %.
  CHECK(db.RateOf(poring, poring.drops[3], false) == 100);
  // Le MVP prend le ratio de soin des MVP ; sa récompense, celui des MVP.
  const ro::vfs::ServerMonster& baphomet = *db.FindMonster(kBaphomet);
  CHECK(db.RateOf(baphomet, baphomet.drops[0], false) == 500);
  CHECK(db.RateOf(baphomet, baphomet.mvp_drops[0], true) == 10000);
  // Le coffre au trésor a son propre ratio et ses propres bornes, quel que soit
  // son rang : 1 % ×12,5. Le minimum commun de 10 % ne le touche pas.
  const ro::vfs::ServerMonster& chest = *db.FindMonster(kChest);
  CHECK(db.RateOf(chest, chest.drops[0], false) == 1250);
}

TEST_CASE("Taux réel : drop_rateincrease, ratio nul, plancher et logarithme",
          "[vfs][server_db][rates]") {
  DropSettings settings;
  // Ratio de 100 : le taux écrit, tel quel.
  CHECK(EffectiveDropRate(settings, 50, "Etc", 0, false, false, nullptr) == 50);
  settings.rate_increase = true;
  CHECK(EffectiveDropRate(settings, 50, "Etc", 0, false, false, nullptr) == 51);
  // Seulement sous la moitié, et jamais pour une récompense de MVP.
  CHECK(EffectiveDropRate(settings, 6000, "Etc", 0, false, false, nullptr) == 6000);
  CHECK(EffectiveDropRate(settings, 50, "Etc", 0, false, true, nullptr) == 50);
  settings.rate_increase = false;
  // Un ratio de `mob_item_ratio` remplace celui de la famille ; nul, il retire.
  const std::int64_t doubled = 200;
  const std::int64_t removed = 0;
  CHECK(EffectiveDropRate(settings, 50, "Etc", 0, false, false, &doubled) == 100);
  CHECK(EffectiveDropRate(settings, 50, "Etc", 0, false, false, &removed) == 0);
  // Le plancher d'un centième, sauf drop_rate0item.
  settings.common.rate = 1;
  settings.common.least = 0;
  CHECK(EffectiveDropRate(settings, 50, "Etc", 0, false, false, nullptr) == 1);
  settings.rate_zero_allowed = true;
  CHECK(EffectiveDropRate(settings, 50, "Etc", 0, false, false, nullptr) == 0);
  // La formule logarithmique : 1 % ×5 donne 1 % × 3^1 + 0,5 = 300,5, arrondi.
  DropSettings logarithmic;
  logarithmic.logarithmic = true;
  logarithmic.common.rate = 500;
  CHECK(EffectiveDropRate(logarithmic, 100, "Etc", 0, false, false, nullptr) == 300);
  // `DelayConsume` et `Cash` sont des objets d'usage.
  DropSettings uses;
  uses.use.rate = 300;
  CHECK(EffectiveDropRate(uses, 10, "delayconsume", 0, false, false, nullptr) == 30);
  CHECK(EffectiveDropRate(uses, 10, "Cash", 0, false, false, nullptr) == 30);
}

TEST_CASE("EXP réelle : écrite × taux / 100, bornée", "[vfs][server_db][rates]") {
  CHECK(ro::vfs::EffectiveExp(2, 100000) == 2000);
  CHECK(ro::vfs::EffectiveExp(3, 250) == 7);
  CHECK(ro::vfs::EffectiveExp(50, 100) == 50);
  CHECK(ro::vfs::EffectiveExp(50, 0) == 0);
  CHECK(ro::vfs::EffectiveExp(UINT64_MAX, 1000) == static_cast<std::uint64_t>(INT64_MAX));
}

TEST_CASE("Taux réel : qui lâche un objet, du plus généreux au moins généreux",
          "[vfs][server_db][rates]") {
  const ServerDb db = Parsed();
  const std::vector<ro::vfs::ItemDropper> sword = db.DroppersOf(kSword);
  REQUIRE(sword.size() == 2);
  CHECK(sword[0].monster_id == kBaphomet);
  CHECK(sword[0].mvp_reward);
  CHECK(sword[0].rate_hundredths == 10000);
  CHECK(sword[1].monster_id == kPoring);
  CHECK(sword[1].rate_hundredths == 500);
  CHECK(db.DroppersOf(kPoringCard).size() == 1);
  CHECK(db.DroppersOf(12345).empty());
}

TEST_CASE("Recherche : une saisie toute en chiffres est un identifiant",
          "[vfs][server_db][search]") {
  const ServerDb db = Parsed();
  const std::vector<std::string> keys = ItemKeys(db);
  CHECK(ro::vfs::SearchedId(" 1101 ") == 1101U);
  CHECK_FALSE(ro::vfs::SearchedId("1101a").has_value());
  CHECK_FALSE(ro::vfs::SearchedId("").has_value());
  const auto by_id = ro::vfs::SearchItems(db, keys, {.text = "1101"});
  REQUIRE(by_id.size() == 1);
  CHECK(db.items()[by_id[0]].item_id == kSword);
  // Un identifiant ne se cherche pas dans les noms : « 50 » ne trouve rien.
  CHECK(ro::vfs::SearchItems(db, keys, {.text = "50"}).empty());
}

TEST_CASE("Recherche : un nom se trouve sans casse ni accents, AegisName compris",
          "[vfs][server_db][search]") {
  const ServerDb db = Parsed();
  const std::vector<std::string> keys = ItemKeys(db);
  CHECK(ro::vfs::SearchItems(db, keys, {.text = "POTION"}).size() == 1);
  CHECK(ro::vfs::SearchItems(db, keys, {.text = "red_pot"}).size() == 1);
  CHECK(ro::vfs::SearchItems(db, keys, {}).size() == db.items().size());
  const std::vector<std::string> monster_keys = MonsterKeys(db);
  CHECK(ro::vfs::SearchMonsters(db, monster_keys, {.text = "bapho"}).size() == 1);
}

TEST_CASE("Recherche : les filtres d'objet — type, emplacement, niveau requis",
          "[vfs][server_db][search]") {
  const ServerDb db = Parsed();
  const std::vector<std::string> keys = ItemKeys(db);
  CHECK(ro::vfs::SearchItems(db, keys, {.type = "weapon"}).size() == 1);
  // Un objet sans `Type` se trouve parmi les divers.
  CHECK(ro::vfs::SearchItems(db, keys, {.type = "Etc"}).size() == 1);
  const std::vector<std::string_view> hands = {"Right_Hand", "Both_Hand"};
  CHECK(ro::vfs::SearchItems(db, keys, {.locations = hands}).size() == 1);
  const std::vector<std::string_view> heads = {"Head_Top"};
  CHECK(ro::vfs::SearchItems(db, keys, {.locations = heads}).empty());
  CHECK(ro::vfs::SearchItems(db, keys, {.level_min = 2, .level_max = 10}).size() == 1);
  CHECK(ro::vfs::SearchItems(db, keys, {.level_min = 3}).empty());
}

TEST_CASE("Recherche : les filtres de monstre — race, élément, taille, rang, niveau",
          "[vfs][server_db][search]") {
  const ServerDb db = Parsed();
  const std::vector<std::string> keys = MonsterKeys(db);
  CHECK(ro::vfs::SearchMonsters(db, keys, {.race = 3}).size() == 1);
  CHECK(ro::vfs::SearchMonsters(db, keys, {.element = 7}).size() == 1);
  CHECK(ro::vfs::SearchMonsters(db, keys, {.size = 0}).size() == 1);
  CHECK(ro::vfs::SearchMonsters(db, keys, {.rank = ro::vfs::kServerMonsterMvp}).size() == 1);
  CHECK(ro::vfs::SearchMonsters(db, keys, {.rank = ro::vfs::kServerMonsterBoss}).size() == 1);
  CHECK(ro::vfs::SearchMonsters(db, keys, {.level_min = 80}).size() == 2);
  CHECK(ro::vfs::SearchMonsters(db, keys, {.level_min = 80, .level_max = 90}).size() == 1);
}

TEST_CASE("Base de données : les map drops se lisent, par objet et par monstre",
          "[vfs][server_db]") {
  constexpr std::string_view kMapDrops = R"yaml(# Les map drops
Header:
  Type: MAP_DROP_DB
  Version: 2

Body:
  - Map: pay_fild01
    Drops:
      - {Item: Jellopy, Rate: 50}
  - Map: prt_maze03
    Drops:
      - {Item: Sword, Rate: 150}
  - Map: gef_d01_i
    Monster: PORING
    Drops:
      - {Item: Sword, Rate: 5, RandomOptionGroup: GROUP_A}
      - {Item: Jellopy, Rate: 5000}
      - {Item: Unknown_Thing, Rate: 5000}
  - Map: gef_d01_i
    Monster: UNKNOWN_MOB
    Drops:
      - {Item: Jellopy, Rate: 100}
)yaml";
  auto read = ParseServerDb(kItemDb, kMobDb, kContext, {}, kMapDrops);
  REQUIRE(read.has_value());

  // Au taux décroissant ; le taux écrit est le taux réel, en millièmes de pour
  // cent ; une ligne d'un monstre ou d'un objet inconnu ne compte pas.
  const std::vector<ro::vfs::ServerMapDrop> jellopy = read->MapDropsOfItem(kJellopy);
  REQUIRE(jellopy.size() == 2);
  CHECK(jellopy[0].monster_id == kPoring);
  CHECK(jellopy[0].map_name == "gef_d01_i");
  CHECK(jellopy[0].rate_thousandths == 5000);
  CHECK_FALSE(jellopy[0].random_options);
  CHECK(jellopy[1].monster_id == ro::vfs::kServerMapDropAllMonsters);

  // Les siens, et ceux de TOUS les monstres d'une carte où il apparaît :
  // pay_fild01 oui, prt_maze03 non.
  const std::vector<ro::vfs::ServerMapDrop> poring = read->MapDropsOfMonster(kPoring);
  REQUIRE(poring.size() == 3);
  CHECK(poring[0].item_id == kJellopy);
  CHECK(poring[1].map_name == "pay_fild01");
  CHECK(poring[2].item_id == kSword);
  CHECK(poring[2].random_options);
  CHECK(poring[2].rate_thousandths == 5);

  // Absente, la base reste connue, sans map drops.
  CHECK(Parsed().MapDropsOfItem(kJellopy).empty());
  // D'un autre type, c'est une erreur.
  CHECK_FALSE(ParseServerDb(kItemDb, kMobDb, kContext, {}, kMobDb).has_value());
}
