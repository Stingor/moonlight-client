// LE BLOC DE FAITS D'UN OBJET TIRÉ DE L'ITEM_DB, sans fenêtre : ce qu'il écrit
// d'une arme, d'une armure, d'une carte, d'un consommable, d'un objet à jobs
// restreints et d'un objet à niveau maximal, les faits nuls qu'il tait, ceux
// que le script et la pet_db donnent — soin, guérison, élément, durée, délai,
// capture, œuf, nourriture, accessoire —, puis ce qu'il ajoute aux lignes
// d'une description selon que la base est lue et connaît l'objet. Les entrées
// sont celles de `dist/data/yaml_files/item_db.yml`.

#include "game/item_db_block.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/item_table.h"
#include "ro/vfs/pet_tables.h"
#include "ro/vfs/server_db.h"

using moonclient::ItemDbBlockOf;
using moonclient::JobsSummaryOf;
using ro::vfs::ServerItem;

namespace {

constexpr std::string_view kHeader = "^777777Données du serveur^000000";
constexpr std::string_view kRule = ro::vfs::kDescriptionRule;

[[nodiscard]] bool Holds(const std::vector<std::string>& lines, std::string_view line) {
  return std::ranges::find(lines, line) != lines.end();
}

// 1101, « Sword [3] ».
[[nodiscard]] ServerItem Sword() {
  return ServerItem{.item_id = 1101,
                    .type = "Weapon",
                    .sub_type = "1hSword",
                    .weight_tenths = 500,
                    .attack = 25,
                    .range_cells = 1,
                    .slots = 3,
                    .jobs = {"Alchemist", "Assassin", "Blacksmith", "Crusader", "Knight",
                             "Merchant", "Novice", "Rogue", "SuperNovice", "Swordman", "Thief"},
                    .locations = {"Right_Hand"},
                    .weapon_level = 1,
                    .equip_level_min = 2,
                    .refineable = true};
}

}  // namespace

TEST_CASE("Bloc de l'item_db : une arme", "[app][item_db_block]") {
  const std::vector<std::string> lines = ItemDbBlockOf(Sword());
  const std::vector<std::string> expected = {
      std::string(kHeader),
      "^0000CCType :^000000 Épée à une main",
      "^0000CCAttaque :^000000 25",
      "^0000CCPortée :^000000 1 case(s)",
      "^0000CCPoids :^000000 50",
      "^0000CCNiveau d'arme :^000000 1",
      "^0000CCEmplacements de carte :^000000 3",
      "^0000CCRaffinable :^000000 Oui",
      std::string(kRule),
      "^0000CCPrérequis :^000000",
      "Niveau de base 2",
      "Lignées Novice, Swordman, Merchant, Thief",
  };
  CHECK(lines == expected);
}

TEST_CASE("Bloc de l'item_db : une armure et un couvre-chef", "[app][item_db_block]") {
  // 2101, « Guard » : un bouclier, ouvert à tous.
  const ServerItem guard{.item_id = 2101,
                         .type = "Armor",
                         .weight_tenths = 300,
                         .defense = 3,
                         .locations = {"Left_Hand"},
                         .refineable = true};
  const std::vector<std::string> shield = ItemDbBlockOf(guard);
  CHECK(Holds(shield, "^0000CCType :^000000 Bouclier"));
  CHECK(Holds(shield, "^0000CCDéfense :^000000 3"));
  CHECK(Holds(shield, "^0000CCPosition :^000000 Bouclier"));
  CHECK(Holds(shield, "^0000CCPoids :^000000 30"));
  CHECK(Holds(shield, "Tous les jobs"));
  // 2201, « Sunglasses » : une défense nulle ne s'écrit pas, « Raffinable : Non » si.
  const ServerItem glasses{
      .item_id = 2201, .type = "Armor", .weight_tenths = 100, .locations = {"Head_Mid"}};
  const std::vector<std::string> headgear = ItemDbBlockOf(glasses);
  CHECK(Holds(headgear, "^0000CCType :^000000 Couvre-chef"));
  CHECK(std::ranges::none_of(
      headgear, [](const std::string& line) { return line.starts_with("^0000CCDéfense"); }));
  CHECK(Holds(headgear, "^0000CCPosition :^000000 Milieu"));
  CHECK(Holds(headgear, "^0000CCRaffinable :^000000 Non"));
  // 19500, « T Mr Smile » : un costume des trois pièces, sans défense ni
  // raffinement.
  const ServerItem smile{
      .item_id = 19500, .type = "Armor", .locations = {"Costume_Head"}, .view = 65};
  const std::vector<std::string> costume = ItemDbBlockOf(smile);
  CHECK(Holds(costume, "^0000CCType :^000000 Costume"));
  CHECK(Holds(costume, "^0000CCPosition :^000000 Haut / Milieu / Bas"));
  CHECK_FALSE(Holds(costume, "^0000CCDéfense :^000000 0"));
  CHECK(std::ranges::none_of(
      costume, [](const std::string& line) { return line.starts_with("^0000CCRaffinable"); }));
}

TEST_CASE("Bloc de l'item_db : une carte", "[app][item_db_block]") {
  // 4035, « Hydra Card » : se sertit sur une arme, sans prérequis.
  const ServerItem hydra{.item_id = 4035,
                         .type = "Card",
                         .sub_type = "Normal",
                         .weight_tenths = 10,
                         .locations = {"Right_Hand"}};
  const std::vector<std::string> lines = ItemDbBlockOf(hydra);
  const std::vector<std::string> expected = {
      std::string(kHeader),
      "^0000CCType :^000000 Carte",
      "^0000CCSe sertit sur :^000000 Arme",
      "^0000CCPoids :^000000 1",
  };
  CHECK(lines == expected);
  // Une carte de couvre-chef se dit d'un mot, quelle que soit la pièce.
  const ServerItem head_card{
      .type = "Card", .sub_type = "Normal", .locations = {"Head_Top", "Head_Mid", "Head_Low"}};
  CHECK(Holds(ItemDbBlockOf(head_card), "^0000CCSe sertit sur :^000000 Couvre-chef"));
}

TEST_CASE("Bloc de l'item_db : un consommable", "[app][item_db_block]") {
  // 501, « Red Potion » : un poids de dixièmes, et rien à prérequérir.
  const ServerItem potion{.item_id = 501, .type = "Healing", .weight_tenths = 70};
  const std::vector<std::string> expected = {
      std::string(kHeader),
      "^0000CCType :^000000 Soin",
      "^0000CCPoids :^000000 7",
  };
  CHECK(ItemDbBlockOf(potion) == expected);
  // 1750, « Arrow » : un poids nul ne s'écrit pas.
  const ServerItem arrow{.item_id = 1750,
                         .type = "Ammo",
                         .sub_type = "Arrow",
                         .attack = 25,
                         .jobs = {"Archer", "Assassin", "BardDancer", "Hunter", "Rogue", "Thief"},
                         .locations = {"Ammo"}};
  const std::vector<std::string> ammo = ItemDbBlockOf(arrow);
  CHECK(Holds(ammo, "^0000CCType :^000000 Flèche"));
  CHECK(std::ranges::none_of(
      ammo, [](const std::string& line) { return line.starts_with("^0000CCPoids"); }));
  CHECK(Holds(ammo, "Lignées Archer, Thief"));
  // Un poids qui n'est pas entier garde son dixième.
  const ServerItem light{.type = "Etc", .weight_tenths = 15};
  CHECK(Holds(ItemDbBlockOf(light), "^0000CCPoids :^000000 1,5"));
}

TEST_CASE("Bloc de l'item_db : les jobs, en résumé", "[app][item_db_block]") {
  // Sans `Jobs`, le serveur ouvre l'objet à tous.
  CHECK(JobsSummaryOf(ServerItem{}) == "Tous les jobs");
  CHECK_FALSE(moonclient::JobsRestricted(ServerItem{}));
  // Un `Jobs` sans clé levée n'ouvre l'objet à personne.
  CHECK(JobsSummaryOf(ServerItem{.jobs = {}, .jobs_lowered = {"All"}}) == "Aucun job");
  CHECK(moonclient::JobsRestricted(ServerItem{.jobs = {}}));
  // 2601, « Ring » : `{All: true, Novice: false, SuperNovice: false}`.
  CHECK(JobsSummaryOf(ServerItem{.jobs = {"All"}, .jobs_lowered = {"Novice", "SuperNovice"}}) ==
        "Tous les jobs sauf lignée Novice");
  // 1201, « Knife » : une liste plus longue que son complément.
  const ServerItem knife{.jobs = {"Alchemist", "Archer", "Assassin", "BardDancer", "Blacksmith",
                                  "Crusader", "Hunter", "Knight", "Mage", "Merchant", "Ninja",
                                  "Novice", "Rogue", "Sage", "SoulLinker", "SuperNovice",
                                  "Swordman", "Thief", "Wizard"}};
  CHECK(JobsSummaryOf(knife) ==
        "Tous les jobs sauf lignée Acolyte ; Taekwon, Star Gladiator, "
        "Gunslinger");
  // 13101, « Six Shooter » : un seul job se nomme seul.
  CHECK(JobsSummaryOf(ServerItem{.jobs = {"Gunslinger"}}) == "Gunslinger");
  // 1601, « Rod » : trois lignées complètes et un job seul.
  const ServerItem rod{.jobs = {"Acolyte", "Mage", "Monk", "Novice", "Priest", "Sage", "SoulLinker",
                                "SuperNovice", "Wizard"}};
  CHECK(JobsSummaryOf(rod) == "Lignées Novice, Magician, Acolyte ; Soul Linker");
  // 1707, « Great Bow » : la lignée Archer, puis Thief et Rogue SANS Assassin.
  // Les jobs seuls ne se lisent pas comme une lignée.
  const ServerItem great_bow{.jobs = {"Archer", "BardDancer", "Hunter", "Rogue", "Thief"}};
  CHECK(JobsSummaryOf(great_bow) == "Lignée Archer ; Thief, Rogue");
  // Une lignée complète et le job seul d'une autre lignée.
  const ServerItem merchant_and_priest{.jobs = {"Alchemist", "Blacksmith", "Merchant", "Priest"}};
  CHECK(JobsSummaryOf(merchant_and_priest) == "Lignée Merchant ; Priest");
  // 15000, « Bone Plate » : trois lignées, au niveau 85.
  const ServerItem bone{.jobs = {"Alchemist", "Assassin", "Blacksmith", "Crusader", "Knight",
                                 "Merchant", "Rogue", "Swordman", "Thief"},
                        .locations = {"Armor"},
                        .equip_level_min = 85};
  CHECK(JobsSummaryOf(bone) == "Lignées Swordman, Merchant, Thief");
  const std::vector<std::string> lines = ItemDbBlockOf(bone);
  CHECK(Holds(lines, "Niveau de base 85"));
  CHECK(std::ranges::none_of(lines,
                             [](const std::string& line) { return line.starts_with("Classes"); }));
  // Des classes restreintes se nomment.
  const ServerItem upper{.classes = {"Upper", "Third_Upper"}, .locations = {"Armor"}};
  CHECK(Holds(ItemDbBlockOf(upper), "Classes : Transcendante, Troisième transcendante"));
  // Une table qui retire toutes les classes : personne.
  CHECK(Holds(ItemDbBlockOf(ServerItem{.type = "Usable", .classes = {}}), "Classes : aucune"));
  // Les instruments sont aux hommes : le sexe se dit dans les prérequis.
  CHECK(
      Holds(ItemDbBlockOf(ServerItem{
                .type = "Weapon", .sub_type = "Musical", .jobs = {"BardDancer"}, .gender = "Male"}),
            "Hommes seulement"));
}

TEST_CASE("Bloc de l'item_db : un niveau maximal", "[app][item_db_block]") {
  // 23280, « Novice Fly Wing » : `EquipLevelMax: 98`, le niveau 1 par défaut.
  const ServerItem wing{.item_id = 23280, .type = "Delayconsume", .equip_level_max = 98};
  const std::vector<std::string> lines = ItemDbBlockOf(wing);
  CHECK(Holds(lines, "^0000CCType :^000000 Consommable différé"));
  CHECK(Holds(lines, "Niveau de base 1 à 98"));
  // Un consommable sans `Jobs` ne dit rien des jobs.
  CHECK_FALSE(Holds(lines, "Tous les jobs"));
}

TEST_CASE("Bloc de l'item_db : ce qu'il ajoute à une description", "[app][item_db_block]") {
  const std::vector<std::string> table = {"A basic one-handed sword.", "Weight: 50"};
  // Une base non lue n'ajoute rien.
  std::vector<std::string> lines = table;
  moonclient::AppendItemDbBlock(lines, nullptr, 1101);
  CHECK(lines == table);
  const ro::vfs::ServerDb unread;
  moonclient::AppendItemDbBlock(lines, &unread, 1101);
  CHECK(lines == table);

  // Lue, elle ajoute un trait puis le bloc.
  const ro::vfs::ServerDb db = ro::vfs::ServerDb::Known({Sword()}, {}, {});
  moonclient::AppendItemDbBlock(lines, &db, 1101);
  REQUIRE(lines.size() > table.size() + 1);
  CHECK(lines[table.size()] == kRule);
  CHECK(lines[table.size() + 1] == kHeader);
  CHECK(lines.back() == "Lignées Novice, Swordman, Merchant, Thief");

  // Un objet que la base ignore le dit d'une ligne grise.
  std::vector<std::string> orphan = table;
  moonclient::AppendItemDbBlock(orphan, &db, 31000);
  REQUIRE(orphan.size() == table.size() + 2);
  CHECK(orphan.back() == "^777777Données du serveur : item inconnu^000000");

  // Sans ligne de table, pas de trait en tête.
  std::vector<std::string> bare;
  moonclient::AppendItemDbBlock(bare, &db, 1101);
  REQUIRE_FALSE(bare.empty());
  CHECK(bare.front() == kHeader);
}

TEST_CASE("Bloc de l'item_db : la base lit les jobs abaissés", "[app][item_db_block]") {
  constexpr std::string_view kItemDb = R"yaml(Header:
  Type: ITEM_DB
  Version: 3

Body:
  - Id: 2601
    AegisName: Ring
    Name: Ring
    Type: Armor
    Jobs: {All: true, Novice: false, SuperNovice: false}
    Locations: {Both_Accessory: true}
    EquipLevelMin: 20
  - Id: 2301
    AegisName: Cotton_Shirt
    Name: Cotton Shirt
    Type: Armor
    Locations: {Armor: true}
  - Id: 9001
    AegisName: Nobody_Item
    Name: Nobody Item
    Jobs: {All: false}
  - Id: 1681
    AegisName: Short_Foxtail_Staff
    Name: Short Foxtail Staff
    Type: Weapon
    Jobs: {}
    Locations: {Right_Hand: true}
)yaml";
  constexpr std::string_view kMobDb = R"yaml(Header:
  Type: MOB_DB
  Version: 5

Body:
  - Id: 1002
    AegisName: PORING
    Name: Poring
)yaml";
  constexpr std::string_view kContext = R"yaml(version: 1
battle:
  base_exp_rate: 100
)yaml";
  const auto db = ro::vfs::ParseServerDb(kItemDb, kMobDb, kContext);
  INFO((db.has_value() ? std::string() : db.error().detail));
  REQUIRE(db.has_value());
  const ServerItem* const ring = db->FindItem(2601);
  REQUIRE(ring != nullptr);
  CHECK(ring->jobs == std::vector<std::string>{"All"});
  CHECK(ring->jobs_lowered == std::vector<std::string>{"Novice", "SuperNovice"});
  CHECK(JobsSummaryOf(*ring) == "Tous les jobs sauf lignée Novice");
  // Sans `Jobs`, le défaut du serveur : tous.
  const ServerItem* const shirt = db->FindItem(2301);
  REQUIRE(shirt != nullptr);
  CHECK(shirt->jobs == std::vector<std::string>{"All"});
  CHECK(JobsSummaryOf(*shirt) == "Tous les jobs");
  // Tout abaissé : personne.
  const ServerItem* const nobody = db->FindItem(9001);
  REQUIRE(nobody != nullptr);
  CHECK(nobody->jobs.empty());
  CHECK(JobsSummaryOf(*nobody) == "Aucun job");
  // 1681 : la base du serveur écrit `Jobs: {}` — personne non plus.
  const ServerItem* const foxtail = db->FindItem(1681);
  REQUIRE(foxtail != nullptr);
  CHECK(foxtail->jobs.empty());
  CHECK(Holds(ItemDbBlockOf(*foxtail), "Aucun job"));
}

TEST_CASE("Bloc de l'item_db : le soin et les états guéris, tirés du script",
          "[app][item_db_block]") {
  // 526, « Royal Jelly ».
  const ServerItem jelly{
      .item_id = 526,
      .type = "Healing",
      .weight_tenths = 150,
      .script =
          "itemheal rand(325,405),rand(40,60); sc_end SC_POISON; sc_end SC_SILENCE; sc_end "
          "SC_BLIND; sc_end SC_CONFUSION; sc_end SC_CURSE; sc_end SC_Hallucination;\n"};
  const std::vector<std::string> expected = {
      std::string(kHeader),
      "^0000CCType :^000000 Soin",
      "^0000CCSoin :^000000 325 ~ 405 PV, 40 ~ 60 SP",
      "^0000CCGuérit :^000000 Poison, Silence, Cécité, Confusion, Malédiction, Hallucination",
      "^0000CCPoids :^000000 15",
  };
  CHECK(ItemDbBlockOf(jelly) == expected);
  // 523, « Holy Water ».
  const ServerItem water{
      .item_id = 523, .type = "Healing", .weight_tenths = 30, .script = "sc_end SC_CURSE;\n"};
  CHECK(Holds(ItemDbBlockOf(water), "^0000CCGuérit :^000000 Malédiction"));
  // 500, « Choko Gangjeong » : des pour cent ; 505 : des SP seuls.
  CHECK(Holds(ItemDbBlockOf(ServerItem{.type = "Healing", .script = "percentheal 10,10;"}),
              "^0000CCSoin :^000000 10 % des PV, 10 % des SP"));
  CHECK(Holds(ItemDbBlockOf(ServerItem{.type = "Healing", .script = "itemheal 0,rand(40,60);"}),
              "^0000CCSoin :^000000 40 ~ 60 SP"));
  // Un soin nul ne s'écrit pas.
  const std::vector<std::string> nothing =
      ItemDbBlockOf(ServerItem{.type = "Healing", .script = "itemheal 0,0;"});
  CHECK(std::ranges::none_of(
      nothing, [](const std::string& line) { return line.starts_with("^0000CCSoin"); }));
}

TEST_CASE("Bloc de l'item_db : l'élément, la durée et le délai", "[app][item_db_block]") {
  // 1131, « Ice Falchion ».
  const ServerItem falchion{.item_id = 1131,
                            .type = "Weapon",
                            .sub_type = "1hSword",
                            .weight_tenths = 600,
                            .attack = 100,
                            .script = "bonus bAtkEle,Ele_Water; bonus2 bAddEff,Eff_Freeze,500;"};
  CHECK(Holds(ItemDbBlockOf(falchion), "^0000CCÉlément :^000000 Eau"));
  const ServerItem shield{
      .type = "Armor", .locations = {"Left_Hand"}, .script = "bonus bDefEle,Ele_Holy;"};
  CHECK(Holds(ItemDbBlockOf(shield), "^0000CCÉlément d'armure :^000000 Sacré"));
  // 645, « Concentration Potion » : trente minutes.
  CHECK(Holds(ItemDbBlockOf(ServerItem{.type = "Usable",
                                       .script = "sc_start SC_ATTHASTE_POTION1,1800000,0;"}),
              "^0000CCDurée :^000000 30 minute(s)"));
  CHECK(Holds(ItemDbBlockOf(ServerItem{.type = "Healing", .reuse_delay_ms = 1000}),
              "^0000CCDélai de réutilisation :^000000 1 seconde(s)"));
  CHECK(Holds(ItemDbBlockOf(ServerItem{.type = "Healing", .reuse_delay_ms = 500}),
              "^0000CCDélai de réutilisation :^000000 500 ms"));
}

TEST_CASE("Bloc de l'item_db : la capture, l'œuf et la nourriture, tirés de la pet_db",
          "[app][item_db_block]") {
  constexpr std::string_view kPets = R"yaml(version: 1
pets: {
  1107: {egg: 9010, food: 537, tame: 628, accessory: 10003, auto_feed: false, evolutions: []},
  1002: {egg: 9001, food: 531, tame: 619, accessory: 0, auto_feed: false, evolutions: []}
}
)yaml";
  ro::vfs::PetTables pets;
  REQUIRE(ro::vfs::ParsePetDatabase(kPets, pets).has_value());
  pets.species_by_class[1107] = "Baby Desert Wolf";
  const ro::vfs::ServerDb db =
      ro::vfs::ServerDb::Known({ServerItem{.item_id = 531, .name = "Apple Juice"}},
                               {ro::vfs::ServerMonster{.monster_id = 1002, .name = "Poring"}}, {});
  const moonclient::ItemDbNames names{.db = &db, .pets = &pets};

  // 628, « Well-Dried Bone ».
  const ServerItem bone{.item_id = 628, .type = "Usable", .weight_tenths = 50, .script = "pet;\n"};
  const std::vector<std::string> expected = {
      std::string(kHeader),
      "^0000CCType :^000000 Item de capture",
      "^0000CCCapture :^000000 Baby Desert Wolf",
      "^0000CCPoids :^000000 5",
  };
  CHECK(ItemDbBlockOf(bone, names) == expected);
  // Sans pet_db, l'objet reste un objet de capture, sans monstre nommé.
  CHECK(Holds(ItemDbBlockOf(bone), "^0000CCType :^000000 Item de capture"));
  CHECK_FALSE(Holds(ItemDbBlockOf(bone), "^0000CCCapture :^000000 Baby Desert Wolf"));

  // 9001, « Poring Egg » : le familier, nommé par le serveur, et sa nourriture.
  const std::vector<std::string> egg =
      ItemDbBlockOf(ServerItem{.item_id = 9001, .type = "PetEgg"}, names);
  CHECK(Holds(egg, "^0000CCFamilier :^000000 Poring"));
  CHECK(Holds(egg, "^0000CCNourriture :^000000 Apple Juice"));
  // 531, « Apple Juice » : le familier qu'il nourrit.
  CHECK(Holds(ItemDbBlockOf(ServerItem{.item_id = 531, .type = "Usable"}, names),
              "^0000CCNourrit :^000000 Poring"));
  // L'espèce nomme le familier avant le serveur ; un objet que rien ne nomme
  // s'écrit par son identifiant.
  CHECK(Holds(ItemDbBlockOf(ServerItem{.item_id = 537, .type = "Usable"}, names),
              "^0000CCNourrit :^000000 Baby Desert Wolf"));
  CHECK(Holds(ItemDbBlockOf(ServerItem{.item_id = 9010, .type = "PetEgg"}, names),
              "^0000CCNourriture :^000000 #537"));
  // 10003, « Transparent Head Protector » : le familier qui le porte.
  CHECK(Holds(ItemDbBlockOf(ServerItem{.item_id = 10003, .type = "PetArmor"}, names),
              "^0000CCFamilier :^000000 Baby Desert Wolf"));
}

TEST_CASE("Bloc de l'item_db : la base lit le délai de réutilisation", "[app][item_db_block]") {
  constexpr std::string_view kItemDb = R"yaml(Header:
  Type: ITEM_DB
  Version: 3

Body:
  - Id: 12016
    AegisName: Speed_Up_Potion
    Name: Speed Potion
    Type: Healing
    Delay: {Duration: 1000, Status: Reuse_Limit_F}
  - Id: 501
    AegisName: Red_Potion
    Name: Red Potion
    Type: Healing
)yaml";
  constexpr std::string_view kMobDb = R"yaml(Header:
  Type: MOB_DB
  Version: 5

Body:
  - Id: 1002
    AegisName: PORING
    Name: Poring
)yaml";
  constexpr std::string_view kContext = R"yaml(version: 1
battle:
  base_exp_rate: 100
)yaml";
  const auto db = ro::vfs::ParseServerDb(kItemDb, kMobDb, kContext);
  REQUIRE(db.has_value());
  REQUIRE(db->FindItem(12016) != nullptr);
  CHECK(db->FindItem(12016)->reuse_delay_ms == 1000);
  REQUIRE(db->FindItem(501) != nullptr);
  CHECK(db->FindItem(501)->reuse_delay_ms == 0);
}
