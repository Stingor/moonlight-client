#include "ro/render/sprite_catalog.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"
#include "ro/vfs/encoding.h"
#include "ro/vfs/vfs.h"

#include "support/sprite_tables.h"
#include "support/temp_directory.h"

using ro::core::ErrorCode;
using ro::render::ActorFiles;
using ro::render::ActorLook;
using ro::render::ActorPart;
using ro::render::ActorPieceFiles;
using ro::render::BodyPalettePath;
using ro::render::BodySpritePath;
using ro::render::BodySpritePaths;
using ro::render::GarmentFiles;
using ro::render::GarmentPlacement;
using ro::render::GarmentPlacementFor;
using ro::render::HairPalettePath;
using ro::render::HeadgearSpritePaths;
using ro::render::HeadSpritePath;
using ro::render::IsPlayerClass;
using ro::render::LoadSpriteCatalog;
using ro::render::ShadowScaleFor;
using ro::render::ShieldSpritePaths;
using ro::render::SpriteCatalog;
using ro::render::SpriteTableFile;
using ro::render::WeaponSpritePaths;
using ro::render::WeaponTrailPaths;
using ro::test_support::SpriteTablesFrom;
using ro::test_support::TempDirectory;
using ro::vfs::Utf8ToCp949;
using ro::vfs::Vfs;

namespace {

// Les tables, réduites à quelques classes. Deux sont encombrantes (un rayon) ;
// le facteur d'ombre dit qu'un portail n'en a pas, une plante presque pas, un
// géant le double — et la 1002 en a un aussi, pour vérifier que le RAYON gagne.
constexpr std::string_view kClasses =
    "version: 1\n"
    "classes:\n"
    "  45: {sprite: \"1_ETC_01\", shadow: 0}\n"
    "  47: {sprite: \"1_M_01\", shadow: 1}\n"
    "  200: {sprite: \"jamais_charge\"}\n"
    "  1002: {sprite: \"Poring\", radius: 1, shadow: 0.5}\n"
    "  1078: {shadow: 0.2}\n"
    "  1288: {sprite: \"Empelium90_0.gr2\", radius: 2, shadow: 9}\n"
    "  10002: {sprite: \"4_M_KID\"}\n"
    "  10007: {sprite: \"portal\"}\n";
constexpr std::string_view kHeadgears =
    "version: 1\nheadgears:\n  1: {symbol: \"ACCESSORY_GOGGLES\", sprite: \"_고글\"}\n";
// Les capes : des ailes, un sac, une robe sans nom ; et les tables de couches,
// réduites à quelques images — celles où la cape passe derrière le corps.
constexpr std::string_view kGarments =
    "version: 1\n"
    "garments:\n"
    "  1: {symbol: \"ROBE_WINGS\", sprite: \"ailes\"}\n"
    "  2: {symbol: \"ROBE_BAG\", sprite: \"sac\", small_layer: true}\n"
    "  3: {symbol: \"ROBE_NAMELESS\", sprite: \"\"}\n";
constexpr std::string_view kLayers =
    "version: 1\n"
    "inherited_jobs:\n"
    "  4008: 7\n"
    "big_female:\n"
    "  0: {0: [0]}\n"
    "big_male:\n"
    "  7: {0: [0, 1], 8: [3]}\n"
    "  4124: {0: [1]}\n"
    "small_male:\n"
    "  0: {16: [0]}\n";

SpriteCatalog Catalog() {
  return SpriteCatalog{SpriteTablesFrom({{SpriteTableFile::kClasses, kClasses},
                                         {SpriteTableFile::kHeadgears, kHeadgears},
                                         {SpriteTableFile::kGarments, kGarments},
                                         {SpriteTableFile::kLayers, kLayers}})};
}

ActorLook Player(std::uint32_t job, std::uint8_t sex) {
  ActorLook look;
  look.player = true;
  look.job = job;
  look.sex = sex;
  return look;
}

ActorLook Creature(std::uint32_t job) {
  ActorLook look;
  look.job = job;
  return look;
}

constexpr std::uint8_t kFemale = 0;
constexpr std::uint8_t kMale = 1;
constexpr std::uint32_t kNovice = 0;
constexpr std::uint32_t kRuneKnight = 4054;
constexpr std::uint32_t kSummoner = 4218;

}  // namespace

TEST_CASE("Catalogue : le corps d'un joueur suit son métier, son sexe et sa race",
          "[render][catalog]") {
  const SpriteCatalog catalog = Catalog();
  CHECK(BodySpritePath(catalog, Player(kNovice, kMale)) ==
        "data\\sprite\\인간족\\몸통\\남\\초보자_남");
  CHECK(BodySpritePath(catalog, Player(kRuneKnight, kFemale)) ==
        "data\\sprite\\인간족\\몸통\\여\\룬나이트_여");
  // Un Doram range tout sous un autre dossier de race.
  CHECK(BodySpritePath(catalog, Player(kSummoner, kMale)) ==
        "data\\sprite\\도람족\\몸통\\남\\summoner_남");
  // Un métier que le client ne nomme pas porte le corps du novice, comme
  // chez lui : sa table est remplie de ce nom avant d'être renseignée.
  CHECK(BodySpritePath(catalog, Player(4999, kMale)) ==
        "data\\sprite\\인간족\\몸통\\남\\초보자_남");
  // Une TENUE ALTERNATIVE a son propre dossier et son numéro dans le nom.
  constexpr std::uint32_t kTranscendentRuneKnight = 4060;
  ActorLook costumed = Player(kTranscendentRuneKnight, kMale);
  costumed.body_style = 4332;
  CHECK(BodySpritePath(catalog, costumed) ==
        "data\\sprite\\인간족\\몸통\\남\\costume_1\\룬나이트_남_1");
  // Et l'on se rabat sur le corps ordinaire si l'installation ne l'a pas.
  CHECK(BodySpritePaths(catalog, costumed) ==
        std::vector<std::string>{"data\\sprite\\인간족\\몸통\\남\\costume_1\\룬나이트_남_1",
                                 "data\\sprite\\인간족\\몸통\\남\\룬나이트_남"});
  // La tenue habille SON métier, quel que soit celui du personnage.
  costumed.job = kNovice;
  CHECK(BodySpritePaths(catalog, costumed) ==
        std::vector<std::string>{"data\\sprite\\인간족\\몸통\\남\\costume_1\\룬나이트_남_1",
                                 "data\\sprite\\인간족\\몸통\\남\\룬나이트_남"});

  // Hors de cette plage, le style de corps n'est PAS un costume : le serveur y
  // met la classe du personnage quand il ne porte aucune tenue alternative, et
  // la lire comme un numéro faisait disparaître tout le monde.
  ActorLook plain = Player(kNovice, kMale);
  plain.body_style = kNovice;
  CHECK(BodySpritePath(catalog, plain) == "data\\sprite\\인간족\\몸통\\남\\초보자_남");
  CHECK(BodySpritePaths(catalog, plain).size() == 1);
}

TEST_CASE("Catalogue : le style de corps prête le corps d'un autre métier", "[render][catalog]") {
  const SpriteCatalog catalog = Catalog();
  constexpr std::uint32_t kKnight = 4008;
  constexpr std::uint32_t kKnightOnPeco = 4014;
  constexpr std::uint32_t kRuneKnightOnGreenDragon = 4080;
  constexpr std::uint32_t kRuneKnightOnBlackDragon = 4088;
  constexpr std::uint32_t kDragonKnight = 4252;

  // Le styliste du serveur pose un métier de troisième ou de quatrième classe
  // sur un personnage qui ne l'exerce pas : c'est ce corps qui se dessine.
  ActorLook skinned = Player(kKnight, kMale);
  skinned.body_style = kRuneKnight;
  CHECK(BodySpritePath(catalog, skinned) == "data\\sprite\\인간족\\몸통\\남\\룬나이트_남");
  skinned.body_style = kDragonKnight;
  CHECK(BodySpritePath(catalog, skinned) == "data\\sprite\\인간족\\몸통\\남\\DRAGON_KNIGHT_남");

  // La machine d'un Mechanic ne passe pas à un corps qui n'en a pas : ni
  // Madogear, ni sa lueur.
  constexpr std::uint32_t kMechanic = 4058;
  constexpr std::uint32_t kMechanicOnMadogear = 4086;
  ActorLook machine = Player(kMechanicOnMadogear, kMale);
  machine.body_style = kMechanic;
  CHECK(BodySpritePath(catalog, machine) == "data\\sprite\\인간족\\몸통\\남\\마도기어_남");
  CHECK_FALSE(WeaponTrailPaths(catalog, machine).empty());
  machine.body_style = kRuneKnight;
  CHECK(BodySpritePath(catalog, machine) == "data\\sprite\\인간족\\몸통\\남\\룬나이트_남");
  CHECK(WeaponTrailPaths(catalog, machine).empty());

  // Le Peco du métier passe au corps emprunté.
  ActorLook riding = Player(kKnightOnPeco, kMale);
  riding.body_style = kRuneKnight;
  CHECK(BodySpritePath(catalog, riding) == "data\\sprite\\인간족\\몸통\\남\\룬나이트쁘띠_남");

  // Le corps du métier lui-même ne change rien : un dragon de couleur garde la
  // sienne, alors qu'un corps EMPRUNTÉ ne reçoit que le dragon vert.
  ActorLook dragon = Player(kRuneKnightOnBlackDragon, kMale);
  dragon.body_style = kRuneKnight;
  CHECK(BodySpritePath(catalog, dragon) == "data\\sprite\\인간족\\몸통\\남\\룬나이트쁘띠2_남");
  dragon.job = kRuneKnightOnGreenDragon;
  dragon.body_style = kDragonKnight;
  CHECK(BodySpritePath(catalog, dragon) ==
        "data\\sprite\\인간족\\몸통\\남\\DRAGON_KNIGHT_CHICKEN_남");

  // La tête reste celle du personnage.
  ActorLook doram_skin = Player(kSummoner, kMale);
  doram_skin.body_style = kRuneKnight;
  CHECK(BodySpritePath(catalog, doram_skin) == "data\\sprite\\인간족\\몸통\\남\\룬나이트_남");
  CHECK(HeadSpritePath(doram_skin).starts_with("data\\sprite\\도람족\\"));
}

TEST_CASE("Catalogue : la tête d'un joueur passe par la table de numéros", "[render][catalog]") {
  // Les douze premières coupes sont permutées, différemment selon le sexe :
  // la coiffure 2 est le fichier 1 chez un homme et le fichier 4 chez une
  // femme. Lire l'identité mettrait une autre coupe sur la tête.
  ActorLook look = Player(kNovice, kMale);
  look.hair = 2;
  CHECK(HeadSpritePath(look) == "data\\sprite\\인간족\\머리통\\남\\1_남");
  look.sex = kFemale;
  CHECK(HeadSpritePath(look) == "data\\sprite\\인간족\\머리통\\여\\4_여");
  look.hair = 12;
  CHECK(HeadSpritePath(look) == "data\\sprite\\인간족\\머리통\\여\\8_여");
  // La coiffure zéro n'existe pas : la table y met le fichier 2.
  look.hair = 0;
  CHECK(HeadSpritePath(look) == "data\\sprite\\인간족\\머리통\\여\\2_여");
  // Au-delà de la table, le numéro tel quel — le client de ce serveur le veut.
  look.hair = 50;
  CHECK(HeadSpritePath(look) == "data\\sprite\\인간족\\머리통\\여\\50_여");

  ActorLook doram = Player(kSummoner, kMale);
  doram.hair = 3;
  CHECK(HeadSpritePath(doram) == "data\\sprite\\도람족\\머리통\\남\\3_남");
  // Hors de 1..10, un Doram porte la dixième coupe.
  doram.hair = 0;
  CHECK(HeadSpritePath(doram) == "data\\sprite\\도람족\\머리통\\남\\10_남");
  doram.hair = 11;
  CHECK(HeadSpritePath(doram) == "data\\sprite\\도람족\\머리통\\남\\10_남");
  // Au-delà de la table humaine, le numéro tel quel : le patch des coiffures du
  // client du joueur ne distingue pas les races.
  doram.hair = 50;
  CHECK(HeadSpritePath(doram) == "data\\sprite\\도람족\\머리통\\남\\50_남");
}

TEST_CASE("Catalogue : un Doram sans coupe a l'animation de la dixième et l'image de la deuxième",
          "[render][catalog]") {
  // Le client du joueur cherche le .act de la dixième coupe, réécrit la coupe à
  // deux, puis cherche le .spr avec elle.
  const SpriteCatalog catalog = Catalog();
  ActorLook doram = Player(kSummoner, kMale);
  doram.hair = 0;
  ActorLook second = doram;
  second.hair = 2;
  const std::vector<ActorPieceFiles> files = ro::render::ActorFiles(catalog, doram);
  const auto head = std::ranges::find(files, ActorPart::kHead, &ActorPieceFiles::part);
  REQUIRE(head != files.end());
  REQUIRE(head->candidates.size() == 1);
  CHECK(head->candidates.front() == HeadSpritePath(second));
  CHECK(head->animation_path == HeadSpritePath(doram));

  // Une coupe ordinaire garde ses deux fichiers sous le même nom.
  doram.hair = 3;
  const std::vector<ActorPieceFiles> plain = ro::render::ActorFiles(catalog, doram);
  const auto plain_head = std::ranges::find(plain, ActorPart::kHead, &ActorPieceFiles::part);
  REQUIRE(plain_head != plain.end());
  CHECK(plain_head->animation_path.empty());
}

TEST_CASE("Catalogue : la tête d'orc remplace la tête, sans palette de cheveux",
          "[render][catalog]") {
  // 🔴 LE BIT 0x800 DES OPTIONS : le client charge le visage d'orc des effets à
  // la place de la tête de la coupe, et celui des Doram pour un Doram.
  const SpriteCatalog catalog = Catalog();
  for (const auto& [job, expected] :
       {std::pair{kNovice, "data\\sprite\\이팩트\\orcface"},
        std::pair{kSummoner, "data\\sprite\\이팩트\\orcface_doram"}}) {
    ActorLook look = Player(job, kMale);
    look.hair = 3;
    look.hair_palette = 4;
    look.orc_head = true;
    const std::vector<ActorPieceFiles> files = ro::render::ActorFiles(catalog, look);
    const auto head = std::ranges::find(files, ActorPart::kHead, &ActorPieceFiles::part);
    REQUIRE(head != files.end());
    REQUIRE(head->candidates.size() == 1);
    CHECK(head->candidates.front() == expected);
    CHECK(head->palette_path.empty());
    CHECK(head->animation_path.empty());
  }
}

TEST_CASE("Catalogue : un monstre ou un PNJ se nomme par la table, son dossier par sa classe",
          "[render][catalog]") {
  const SpriteCatalog catalog = Catalog();
  CHECK(BodySpritePath(catalog, Creature(1002)) == "data\\sprite\\몬스터\\Poring");
  // 🔴 UN WARP NE REND AUCUN CHEMIN : son image est un EFFET, pas un sprite.
  // Un warp (classe 45) pose l'effet 321 à son apparition. Lui donner le sprite
  // du portail, classe 10007, parce que l'installation le porte serait une
  // INFÉRENCE que le client ne fait pas.
  //
  // ⚠ ET RENDRE LE NOM DE LA TABLE SERAIT PIRE : elle dit « 1_ETC_01 », dont le
  // sprite existe et montre un villageois.
  CHECK_FALSE(BodySpritePath(catalog, Creature(45)).has_value());
  CHECK(BodySpritePath(catalog, Creature(47)) == "data\\sprite\\npc\\1_M_01");
  CHECK(BodySpritePath(catalog, Creature(10002)) == "data\\sprite\\npc\\4_M_KID");
  // Les homoncules et les mercenaires sont nommés dans les tables du client.
  CHECK(BodySpritePath(catalog, Creature(6001)) == "data\\sprite\\homun\\LIF");
  CHECK(BodySpritePath(catalog, Creature(6050)) == "data\\sprite\\homun\\MER_SERA");
  CHECK(BodySpritePath(catalog, Creature(6017)) == "data\\sprite\\인간족\\몸통\\여\\활용병");
  CHECK(BodySpritePath(catalog, Creature(6040)) == "data\\sprite\\인간족\\몸통\\남\\검용병");
}

TEST_CASE("Catalogue : ce que le client ne nomme pas est un Poring", "[render][catalog]") {
  const SpriteCatalog catalog = Catalog();
  // Absent de la table.
  CHECK(BodySpritePath(catalog, Creature(1999)) == "data\\sprite\\몬스터\\Poring");
  // Présent dans la table, mais dans une plage que le client ne charge jamais :
  // un Poring, rangé dans le dossier de sa plage.
  CHECK(BodySpritePath(catalog, Creature(200)) == "data\\sprite\\npc\\Poring");
  // Un trou dans les classes nommées en dur garde le dossier de sa plage.
  CHECK(BodySpritePath(catalog, Creature(6047)) == "data\\sprite\\homun\\Poring");
}

TEST_CASE("Catalogue : un modèle 3D n'a pas de sprite", "[render][catalog]") {
  const SpriteCatalog catalog = Catalog();
  CHECK_FALSE(BodySpritePath(catalog, Creature(1288)).has_value());
  CHECK(ActorFiles(catalog, Creature(1288)).empty());
}

TEST_CASE("Catalogue : une coiffe se nomme par sa table, sa variante Doram d'abord",
          "[render][catalog]") {
  const SpriteCatalog catalog = Catalog();
  // Le nom de la table porte déjà son « _ » : rien ne s'intercale.
  CHECK(HeadgearSpritePaths(catalog, Player(kNovice, kMale), 1) ==
        std::vector<std::string>{"data\\sprite\\악세사리\\남\\남_고글"});
  CHECK(HeadgearSpritePaths(catalog, Player(kSummoner, kFemale), 1) ==
        std::vector<std::string>{"data\\sprite\\악세사리\\여_doram\\여_고글",
                                 "data\\sprite\\악세사리\\여\\여_고글"});
  CHECK(HeadgearSpritePaths(catalog, Player(kNovice, kMale), 999).empty());
}

TEST_CASE("Catalogue : les palettes suivent la race, et zéro n'en est pas une",
          "[render][catalog]") {
  ActorLook look = Player(kNovice, kMale);
  CHECK(BodyPalettePath(look).empty());
  CHECK(HairPalettePath(look).empty());
  look.body_palette = 5;
  look.hair_palette = 3;
  CHECK(BodyPalettePath(look) == "data\\palette\\몸\\body_5.pal");
  CHECK(HairPalettePath(look) == "data\\palette\\머리\\head_3.pal");
  look.job = kSummoner;
  CHECK(BodyPalettePath(look) == "data\\palette\\도람족\\body_5.pal");
  CHECK(HairPalettePath(look) == "data\\palette\\도람족\\머리\\head_3.pal");

  // UNE TENUE ALTERNATIVE A SON DOSSIER, comme au client du joueur ; la
  // coiffure, elle, n'en change pas.
  look.body_style = 4332;
  CHECK(BodyPalettePath(look) == "data\\palette\\도람족\\costume_1\\body_5.pal");
  look.job = kNovice;
  CHECK(BodyPalettePath(look) == "data\\palette\\몸\\costume_1\\body_5_1.pal");
  CHECK(HairPalettePath(look) == "data\\palette\\머리\\head_3.pal");
}

TEST_CASE("Catalogue : la couleur 1 ne lit body_1 que chez une première classe transcendante",
          "[render][catalog]") {
  constexpr std::uint32_t kHighNovice = 4001;
  constexpr std::uint32_t kHighMerchant = 4007;
  constexpr std::uint32_t kAssassinCross = 4013;
  ActorLook look = Player(kAssassinCross, kMale);
  look.body_palette = 1;
  CHECK(BodyPalettePath(look) == "data\\palette\\몸\\body_0.pal");
  look.job = kNovice;
  CHECK(BodyPalettePath(look) == "data\\palette\\몸\\body_0.pal");
  look.job = kHighNovice;
  CHECK(BodyPalettePath(look) == "data\\palette\\몸\\body_1.pal");
  look.job = kHighMerchant;
  CHECK(BodyPalettePath(look) == "data\\palette\\몸\\body_1.pal");

  // À partir de 2, la couleur nomme son fichier, quel que soit le métier.
  look.job = kAssassinCross;
  look.body_palette = 2;
  CHECK(BodyPalettePath(look) == "data\\palette\\몸\\body_2.pal");

  // En tenue alternative, la couleur 1 nomme aussi le sien.
  look.body_palette = 1;
  look.body_style = 4332;
  CHECK(BodyPalettePath(look) == "data\\palette\\몸\\costume_1\\body_1_1.pal");

  // Une recette de style désigne le fichier, sans la règle du serveur.
  look.body_style = 0;
  look.body_palette_names_file = true;
  CHECK(BodyPalettePath(look) == "data\\palette\\몸\\body_1.pal");
}

TEST_CASE("Catalogue : les pièces d'un joueur viennent dans l'ordre des ancrages",
          "[render][catalog]") {
  const SpriteCatalog catalog = Catalog();
  ActorLook look = Player(kNovice, kMale);
  look.hair = 1;
  look.body_palette = 7;
  // Une coiffe inconnue de la table est passée sous silence ; les autres
  // gardent leur rang, bas, milieu, haut.
  look.headgears = {0, 999, 1};
  const std::vector<ActorPieceFiles> files = ActorFiles(catalog, look);
  REQUIRE(files.size() == 3);
  CHECK(files[0].part == ActorPart::kBody);
  CHECK(files[0].candidates ==
        std::vector<std::string>{"data\\sprite\\인간족\\몸통\\남\\초보자_남"});
  CHECK(files[0].palette_path == "data\\palette\\몸\\body_7.pal");
  CHECK(files[1].part == ActorPart::kHead);
  CHECK(files[1].candidates == std::vector<std::string>{"data\\sprite\\인간족\\머리통\\남\\2_남"});
  CHECK(files[1].palette_path.empty());
  CHECK(files[2].part == ActorPart::kHeadgearTop);
  CHECK(files[2].candidates == std::vector<std::string>{"data\\sprite\\악세사리\\남\\남_고글"});

  // Un monstre n'a qu'un corps, et pas de palette.
  const std::vector<ActorPieceFiles> creature = ActorFiles(catalog, Creature(1002));
  REQUIRE(creature.size() == 1);
  CHECK(creature[0].part == ActorPart::kBody);
  CHECK(creature[0].palette_path.empty());
}

TEST_CASE("Catalogue : le chargement tolère un fichier absent", "[render][catalog]") {
  TempDirectory client;
  client.WriteText("data/yaml_files/class_sprites.yml", kClasses);
  auto vfs = Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());

  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  CHECK(BodySpritePath(*catalog, Creature(1002)) == "data\\sprite\\몬스터\\Poring");
  // Sans table d'accessoires, une coiffe n'a pas de chemin — et rien d'autre
  // ne manque.
  CHECK(HeadgearSpritePaths(*catalog, Player(kNovice, kMale), 1).empty());
}

TEST_CASE("Catalogue : un fichier illisible refuse le catalogue en le nommant",
          "[render][catalog]") {
  TempDirectory client;
  // Un nom sans guillemets : du YAML valide, que la forme stricte refuse.
  client.WriteText("data/yaml_files/class_sprites.yml",
                   "version: 1\nclasses:\n  1002: {sprite: Poring}\n");
  auto vfs = Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());

  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE_FALSE(catalog.has_value());
  CHECK(catalog.error().code == ErrorCode::kCorruptData);
  CHECK(catalog.error().detail.find("class_sprites.yml") != std::string::npos);
}

namespace {

// Le catalogue, avec ce qu'il faut pour les mains : la table des armes et la
// vue de quelques objets. Les noms d'armes sont en ASCII ici — la règle qu'on
// éprouve est celle des chemins, pas celle de l'encodage.
struct ItemViewSpec {
  std::uint32_t item;
  std::uint16_t view;
};
constexpr std::array<ItemViewSpec, 8> kItemViews = {
    {{1201, 1}, {1207, 31}, {1101, 2}, {1301, 6}, {1501, 8}, {2101, 1}, {2104, 2}, {2189, 6}}};

SpriteCatalog HeldCatalog() {
  SpriteCatalog catalog = Catalog();
  const auto parsed = ro::render::ParseSpriteTableText(
      SpriteTableFile::kWeapons,
      "version: 1\nweapons:\n  0: {sprite: \"\"}\n  1: {sprite: \"_dagger\"}\n"
      "  2: {sprite: \"_sword\"}\n  6: {sprite: \"_axe\"}\n  8: {sprite: \"_club\"}\n"
      "  10: {sprite: \"_rod\"}\n  25: {sprite: \"_dagger_dagger\"}\n"
      "  31: {sprite: \"_1207\", family: 1}\n",
      catalog.tables);
  REQUIRE(parsed.has_value());
  for (const auto& [item, view] : kItemViews) {
    catalog.items.Put(item, ro::vfs::ItemEntry{.view_id = view});
  }
  return catalog;
}

ActorLook Armed(std::uint32_t job, std::uint32_t weapon, std::uint32_t shield) {
  ActorLook look = Player(job, kMale);
  look.weapon = weapon;
  look.shield = shield;
  return look;
}

constexpr std::uint32_t kSwordman = 1;
constexpr std::uint32_t kThief = 6;
constexpr std::uint32_t kKnight = 7;
constexpr std::uint32_t kPecoKnight = 13;
constexpr std::uint32_t kAssassin = 12;
constexpr std::uint32_t kLordKnight = 4008;
constexpr std::uint32_t kBabySwordman = 4024;
constexpr std::uint32_t kCartSummoner = 4219;
constexpr std::uint32_t kKnife = 1201;
constexpr std::uint32_t kLeftHand = 1207;
constexpr std::uint32_t kSword = 1101;
constexpr std::uint32_t kAxe = 1301;
constexpr std::uint32_t kClub = 1501;
constexpr std::uint32_t kGuard = 2101;
constexpr std::uint32_t kBuckler = 2104;

}  // namespace

TEST_CASE("Catalogue : une arme se cherche par son nom propre, puis par sa famille",
          "[render][catalog]") {
  const SpriteCatalog catalog = HeldCatalog();
  // La dague de parade a un sprite à elle, nommé par son identifiant ; sa
  // famille, la dague, reste en repli.
  CHECK(WeaponSpritePaths(catalog, Armed(kNovice, kLeftHand, 0)) ==
        std::vector<std::string>{"data\\sprite\\인간족\\초보자\\초보자_남_1207",
                                 "data\\sprite\\인간족\\초보자\\초보자_남_dagger"});
  // Un couteau ordinaire : le nom propre EST celui de la famille.
  CHECK(WeaponSpritePaths(catalog, Armed(kNovice, kKnife, 0)) ==
        std::vector<std::string>{"data\\sprite\\인간족\\초보자\\초보자_남_dagger"});
  // Sans objet, ou sans vue connue, rien.
  CHECK(WeaponSpritePaths(catalog, Armed(kNovice, 0, 0)).empty());
  CHECK(WeaponSpritePaths(catalog, Armed(kNovice, 9999, 0)).empty());
}

TEST_CASE("Catalogue : le métier choisit le dossier de ses armes", "[render][catalog]") {
  const SpriteCatalog catalog = HeldCatalog();
  // Un seigneur chevalier tient les armes du chevalier ; l'invocateur à
  // charrette celles de l'invocateur, sous son propre préfixe.
  CHECK(WeaponSpritePaths(catalog, Armed(kLordKnight, kSword, 0)) ==
        std::vector<std::string>{"data\\sprite\\인간족\\기사\\기사_남_sword"});
  CHECK(WeaponSpritePaths(catalog, Armed(kCartSummoner, kSword, 0)) ==
        std::vector<std::string>{"data\\sprite\\도람족\\summoner\\cart_summoner_남_sword",
                                 "data\\sprite\\인간족\\summoner\\cart_summoner_남_sword"});
}

TEST_CASE("Catalogue : un voleur tient une masse comme une épée", "[render][catalog]") {
  const SpriteCatalog catalog = HeldCatalog();
  CHECK(WeaponSpritePaths(catalog, Armed(kThief, kClub, 0)) ==
        std::vector<std::string>{"data\\sprite\\인간족\\도둑\\도둑_남_club",
                                 "data\\sprite\\인간족\\도둑\\도둑_남_sword"});
}

TEST_CASE("Catalogue : deux armes en main se nomment par le couple", "[render][catalog]") {
  const SpriteCatalog catalog = HeldCatalog();
  // Deux dagues : une seule pièce, nommée par les plages d'identifiants des
  // deux objets, et rien dans la main du bouclier.
  const ActorLook assassin = Armed(kAssassin, kKnife, kLeftHand);
  CHECK(WeaponSpritePaths(catalog, assassin) ==
        std::vector<std::string>{"data\\sprite\\인간족\\어세신\\어세신_남_dagger_dagger"});
  CHECK(ShieldSpritePaths(catalog, assassin).empty());
}

TEST_CASE("Catalogue : la traînée suit l'arme", "[render][catalog]") {
  const SpriteCatalog catalog = HeldCatalog();
  CHECK(WeaponTrailPaths(catalog, Armed(kNovice, kSword, 0)) ==
        std::vector<std::string>{"data\\sprite\\인간족\\초보자\\초보자_남_sword_검광"});
  CHECK(WeaponTrailPaths(catalog, Armed(kNovice, kLeftHand, 0)) ==
        std::vector<std::string>{"data\\sprite\\인간족\\초보자\\초보자_남_1207_검광",
                                 "data\\sprite\\인간족\\초보자\\초보자_남_dagger_검광"});
}

TEST_CASE("Catalogue : un bouclier se cherche par son objet, puis par sa forme",
          "[render][catalog]") {
  const SpriteCatalog catalog = HeldCatalog();
  // Le chevalier sait dessiner un bouclier rond : la variante de l'objet, puis
  // la forme.
  CHECK(ShieldSpritePaths(catalog, Armed(kKnight, 0, kBuckler)) ==
        std::vector<std::string>{"data\\sprite\\방패\\기사\\기사_남_2104_방패",
                                 "data\\sprite\\방패\\기사\\기사_남_버클러"});
  // Le novice ne dessine que la garde : la variante de l'objet, et c'est tout.
  CHECK(ShieldSpritePaths(catalog, Armed(kNovice, 0, kBuckler)) ==
        std::vector<std::string>{"data\\sprite\\방패\\초보자\\초보자_남_2104_방패"});
  CHECK(ShieldSpritePaths(catalog, Armed(kNovice, 0, kGuard)) ==
        std::vector<std::string>{"data\\sprite\\방패\\초보자\\초보자_남_2101_방패",
                                 "data\\sprite\\방패\\초보자\\초보자_남_가드"});
  // Monté, le chevalier n'en dessine aucun ; un bébé tient comme son aîné.
  CHECK(ShieldSpritePaths(catalog, Armed(kPecoKnight, 0, kGuard)).empty());
  CHECK(ShieldSpritePaths(catalog, Armed(kBabySwordman, 0, kGuard)) ==
        ShieldSpritePaths(catalog, Armed(kSwordman, 0, kGuard)));
  CHECK(WeaponSpritePaths(catalog, Armed(kBabySwordman, kSword, 0)) ==
        std::vector<std::string>{"data\\sprite\\인간족\\검사\\검사_남_sword"});
}

TEST_CASE("Catalogue : ce que les mains tiennent est facultatif", "[render][catalog]") {
  const SpriteCatalog catalog = HeldCatalog();
  ActorLook look = Armed(kNovice, kKnife, kGuard);
  look.hair = 1;
  const std::vector<ActorPieceFiles> files = ActorFiles(catalog, look);
  REQUIRE(files.size() == 5);
  CHECK(files[0].part == ActorPart::kBody);
  CHECK_FALSE(files[0].optional);
  CHECK(files[1].part == ActorPart::kHead);
  CHECK(files[2].part == ActorPart::kWeapon);
  CHECK(files[2].optional);
  CHECK(files[3].part == ActorPart::kWeaponTrail);
  CHECK(files[3].optional);
  CHECK(files[4].part == ActorPart::kShield);
  CHECK(files[4].optional);
  // Sans table des objets, l'arme ne se cherche plus ; le bouclier, lui, se
  // cherche encore par son objet, comme le client le fait sans connaître sa
  // forme.
  SpriteCatalog blind = HeldCatalog();
  blind.items = ro::vfs::ItemTable{};
  const std::vector<ActorPieceFiles> without = ActorFiles(blind, look);
  REQUIRE(without.size() == 3);
  CHECK(without[2].part == ActorPart::kShield);
  CHECK(without[2].candidates ==
        std::vector<std::string>{"data\\sprite\\방패\\초보자\\초보자_남_2101_방패"});
}

namespace {

constexpr std::uint32_t kMadogear = 4086;
constexpr std::uint32_t kSanta = 26;
constexpr std::uint16_t kWings = 1;
constexpr std::uint16_t kBag = 2;
constexpr std::uint16_t kNamelessRobe = 3;

ActorLook Caped(std::uint32_t job, std::uint8_t sex, std::uint16_t robe) {
  ActorLook look = Player(job, sex);
  look.robe = robe;
  return look;
}

}  // namespace

TEST_CASE("Catalogue : la cape se cherche à plat puis par métier, son .act toujours par métier",
          "[render][catalog]") {
  const SpriteCatalog catalog = Catalog();
  const std::optional<ActorPieceFiles> wings = GarmentFiles(catalog, Caped(kNovice, kMale, kWings));
  REQUIRE(wings.has_value());
  CHECK(wings->part == ActorPart::kGarment);
  CHECK(wings->optional);
  CHECK(wings->candidates == std::vector<std::string>{"data\\sprite\\로브\\ailes\\ailes",
                                                      "data\\sprite\\로브\\ailes\\남\\초보자_남"});
  CHECK(wings->animation_path == "data\\sprite\\로브\\ailes\\남\\초보자_남");
  // La cape vient en dernier, rapportée au corps.
  const std::vector<ActorPieceFiles> files = ActorFiles(catalog, Caped(kNovice, kMale, kWings));
  REQUIRE(files.size() == 3);
  CHECK(files.back().part == ActorPart::kGarment);

  // Un Doram cherche à plat la variante qui porte son nom, à la place de l'autre.
  const std::optional<ActorPieceFiles> doram =
      GarmentFiles(catalog, Caped(kSummoner, kMale, kWings));
  REQUIRE(doram.has_value());
  CHECK(doram->candidates ==
        std::vector<std::string>{"data\\sprite\\로브\\ailes\\ailes_Doram",
                                 "data\\sprite\\로브\\ailes\\남\\summoner_남"});

  // Sans cape, pour une robe sans nom ou inconnue, et pour la mécanicienne en
  // Madogear : rien. Le mécanicien, lui, en porte une.
  CHECK_FALSE(GarmentFiles(catalog, Caped(kNovice, kMale, 0)).has_value());
  CHECK_FALSE(GarmentFiles(catalog, Caped(kNovice, kMale, kNamelessRobe)).has_value());
  CHECK_FALSE(GarmentFiles(catalog, Caped(kNovice, kMale, 99)).has_value());
  CHECK_FALSE(GarmentFiles(catalog, Caped(kMadogear, kFemale, kWings)).has_value());
  CHECK(GarmentFiles(catalog, Caped(kMadogear, kMale, kWings)).has_value());
  CHECK(ActorFiles(catalog, Caped(kNovice, kMale, kNamelessRobe)).size() == 2);
}

TEST_CASE("Catalogue : la cape passe derrière le corps pour les images que sa table liste",
          "[render][catalog]") {
  const SpriteCatalog catalog = Catalog();
  const GarmentPlacement knight = GarmentPlacementFor(catalog, Caped(kKnight, kMale, kWings));
  CHECK(knight.DrawsBehind(0, 0));
  CHECK(knight.DrawsBehind(0, 1));
  CHECK_FALSE(knight.DrawsBehind(0, 2));
  CHECK(knight.DrawsBehind(8, 3));
  CHECK_FALSE(knight.DrawsBehind(8, 0));
  CHECK_FALSE(knight.DrawsBehind(4, 0));

  // Un seigneur chevalier prend la table du chevalier.
  CHECK(GarmentPlacementFor(catalog, Caped(kLordKnight, kMale, kWings)).DrawsBehind(0, 1));

  // La table suit le sexe, et la taille de la robe : le sac est petit.
  const GarmentPlacement woman = GarmentPlacementFor(catalog, Caped(kNovice, kFemale, kWings));
  CHECK(woman.DrawsBehind(0, 0));
  CHECK_FALSE(woman.DrawsBehind(0, 1));
  const GarmentPlacement bagged = GarmentPlacementFor(catalog, Caped(kNovice, kMale, kBag));
  CHECK(bagged.DrawsBehind(16, 0));
  CHECK_FALSE(bagged.DrawsBehind(0, 0));

  // Un métier que la table ne connaît pas, une apparence sans cape, un
  // monstre : devant, toujours.
  CHECK(GarmentPlacementFor(catalog, Caped(kNovice, kMale, kWings)).behind_frames.empty());
  CHECK(GarmentPlacementFor(catalog, Caped(kKnight, kMale, 0)).behind_frames.empty());
  ActorLook creature = Creature(1002);
  creature.robe = kWings;
  CHECK(GarmentPlacementFor(catalog, creature).behind_frames.empty());
}

TEST_CASE("Catalogue : monté sur une monture cash-shop, le corps est celui de la monture",
          "[render][catalog]") {
  const SpriteCatalog catalog = HeldCatalog();
  ActorLook look = Armed(kNovice, kKnife, kGuard);
  look.robe = kWings;
  look.cash_mount = true;
  // Le novice chevauche un Poring ; ses mains se vident ; sa cape suit le
  // corps, jusque dans la table de couches ; sa tête ne change pas.
  CHECK(BodySpritePath(catalog, look) == "data\\sprite\\인간족\\몸통\\남\\노비스포링_남");
  CHECK(HeadSpritePath(look) == HeadSpritePath(Armed(kNovice, 0, 0)));
  CHECK(WeaponSpritePaths(catalog, look).empty());
  CHECK(ShieldSpritePaths(catalog, look).empty());
  const std::optional<ActorPieceFiles> garment = GarmentFiles(catalog, look);
  REQUIRE(garment.has_value());
  CHECK(garment->animation_path == "data\\sprite\\로브\\ailes\\남\\노비스포링_남");
  CHECK(GarmentPlacementFor(catalog, look).DrawsBehind(0, 1));
  CHECK(ActorFiles(catalog, look).size() == 3);

  look.cash_mount = false;
  CHECK_FALSE(GarmentPlacementFor(catalog, look).DrawsBehind(0, 1));
  CHECK(ActorFiles(catalog, look).size() == 6);

  // Un métier sans monture garde son corps.
  ActorLook santa = Player(kSanta, kMale);
  santa.cash_mount = true;
  CHECK(BodySpritePath(catalog, santa) == BodySpritePath(catalog, Player(kSanta, kMale)));
}

TEST_CASE("Catalogue : un objet au sol se dessine par sa ressource, sous le dossier des objets",
          "[render][catalog]") {
  SpriteCatalog catalog = Catalog();
  auto potion = Utf8ToCp949("빨간포션");
  REQUIRE(potion.has_value());
  catalog.items.Put(501, ro::vfs::ItemEntry{.resource = *potion});
  ActorLook lying;
  lying.item = true;
  lying.job = 501;
  CHECK(BodySpritePath(catalog, lying) == "data\\sprite\\아이템\\빨간포션");
  // Un objet n'a qu'un corps ; un objet inconnu de la table n'a rien.
  CHECK(ActorFiles(catalog, lying).size() == 1);
  lying.job = 999;
  CHECK_FALSE(BodySpritePath(catalog, lying).has_value());
  CHECK(ActorFiles(catalog, lying).empty());
}

TEST_CASE("Catalogue : l'ombre vient du rayon, puis du facteur", "[render][catalog]") {
  // DEUX tables, dans cet ordre. Le rayon au sol ne concerne que les créatures
  // encombrantes, et vaut alors deux fois le rayon plus un. Partout ailleurs
  // c'est le FACTEUR d'ombre qui décide, classe par classe : sans lui, tous les
  // monstres portent la même ombre.
  const SpriteCatalog catalog = Catalog();
  CHECK(ShadowScaleFor(catalog, Player(kNovice, kMale)) == 1.0F);
  CHECK(ShadowScaleFor(catalog, Creature(47)) == 1.0F);
  // Le rayon l'emporte sur le facteur quand les deux sont là.
  CHECK(ShadowScaleFor(catalog, Creature(1002)) == 3.0F);
  CHECK(ShadowScaleFor(catalog, Creature(1288)) == 5.0F);
  // Le facteur seul, quand aucun rayon ne parle : une plante en a une petite.
  CHECK(ShadowScaleFor(catalog, Creature(1078)) == 0.2F);
  // Une classe que ni l'une ni l'autre ne nomme garde son ombre entière.
  CHECK(ShadowScaleFor(catalog, Creature(1999)) == 1.0F);

  // La tombe d'un monstre n'en porte pas ; un objet au sol en porte une
  // petite, même quand son numéro est celui de la tombe.
  CHECK(ShadowScaleFor(catalog, Creature(565)) == 0.0F);
  ActorLook lying;
  lying.item = true;
  lying.job = 501;
  CHECK(ShadowScaleFor(catalog, lying) == 0.4F);
  lying.job = 565;
  CHECK(ShadowScaleFor(catalog, lying) == 0.4F);
}

TEST_CASE("Catalogue : un métier joueur se compose, une classe de PNJ non", "[render][catalog]") {
  // C'est ce qui décide qu'un PNJ vêtu d'un métier — une Gypsy, un novice —
  // prend tête, coiffes et armes au lieu d'un sprite unique.
  constexpr std::uint32_t kGypsy = 4021;
  constexpr std::uint32_t kWarp = 45;
  constexpr std::uint32_t kPoring = 1002;
  CHECK(IsPlayerClass(kNovice));
  CHECK(IsPlayerClass(kGypsy));
  CHECK_FALSE(IsPlayerClass(kWarp));
  CHECK_FALSE(IsPlayerClass(kPoring));
}

TEST_CASE("Catalogue : l'habit du GM remplace le corps et l'arme, et éteint la traînée",
          "[render][catalog]") {
  const SpriteCatalog catalog = HeldCatalog();
  ActorLook gm = Armed(kKnight, kSword, kGuard);
  gm.gm_outfit = true;
  CHECK(BodySpritePath(catalog, gm) == "data\\sprite\\인간족\\몸통\\남\\운영자_남");
  CHECK(WeaponSpritePaths(catalog, gm) ==
        std::vector<std::string>{"data\\sprite\\인간족\\운영자\\운영자_남_검"});
  CHECK(WeaponTrailPaths(catalog, gm).empty());
  // La couleur de vêtement ne le teint pas : il garde celles de son .spr.
  gm.body_palette = 3;
  CHECK(BodyPalettePath(gm).empty());
  // Ni la couleur de cheveux : la tête garde la palette de son .spr.
  gm.hair_palette = 5;
  CHECK(HairPalettePath(gm).empty());
  // Le bouclier reste celui du métier.
  CHECK(ShieldSpritePaths(catalog, gm) ==
        ShieldSpritePaths(catalog, Armed(kKnight, kSword, kGuard)));

  // Désarmé, il tient encore son épée ; un Doram porte l'habit humain.
  ActorLook doram = Player(kSummoner, kFemale);
  doram.gm_outfit = true;
  CHECK(BodySpritePath(catalog, doram) == "data\\sprite\\인간족\\몸통\\여\\운영자_여");
  CHECK(WeaponSpritePaths(catalog, doram) ==
        std::vector<std::string>{"data\\sprite\\인간족\\운영자\\운영자_여_검"});

  // Une monture vide les mains, la sienne aussi.
  gm.cash_mount = true;
  CHECK(WeaponSpritePaths(catalog, gm).empty());

  // Hors d'un joueur, l'habit ne veut rien dire.
  ActorLook creature = Creature(kNovice);
  creature.gm_outfit = true;
  CHECK(WeaponSpritePaths(catalog, creature).empty());
}
