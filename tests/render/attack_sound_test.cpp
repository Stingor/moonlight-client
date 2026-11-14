#include "ro/render/attack_sound.h"

#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/class_hit_sound_table.h"
#include "ro/render/sprite_catalog.h"
#include "ro/vfs/item_table.h"

using ro::render::AttackSound;
using ro::render::ClassHasHitSound;
using ro::render::ClassHitSound;
using ro::render::kBareHandsWeapon;
using ro::render::kDefaultClassHitSound;
using ro::render::kEnemyHitSounds;
using ro::render::kFistHitSounds;
using ro::render::kLargestHitCount;
using ro::render::SpriteCatalog;
using ro::render::WeaponHitSound;

namespace {

// La table des armes, réduite à ce qu'il faut. ⚠ SES CLÉS SONT DES SEAUX
// D'ANIMATION, pas des identifiants d'objet : c'est tout le sujet de ce
// fichier. Deux seaux nommés, un troisième laissé vide.
constexpr std::string_view kTable =
    "version: 1\n"
    "weapons:\n"
    "  2: {hit_sound: \"_hit_sword.wav\"}\n"
    "  10: {hit_sound: \"_hit_rod.wav\"}\n";

// Trois objets : une épée, un bâton, et une arme dont le seau n'est pas nommé.
constexpr std::uint32_t kSwordItem = 1101;
constexpr std::uint32_t kRodItem = 1601;
constexpr std::uint32_t kAxeItem = 1301;

// Les classes que la table des bruits distingue — et une qu'elle écarte.
// `kMonsterJob` ne sert qu'à nommer l'argument quand la cible n'est pas un
// joueur : la table ne le lit alors pas.
constexpr std::int32_t kNoviceJob = 0;
constexpr std::int32_t kSwordmanJob = 1;
constexpr std::int32_t kArcherJob = 3;
constexpr std::int32_t kLastBasicJob = 30;
// 4012 = index 62, 4316 = index 366 : les deux bouts de la seconde indexation.
constexpr std::int32_t kAdvancedArcherJob = 4012;
constexpr std::int32_t kLastAdvancedJob = 4316;
constexpr std::int32_t kOutOfBoundsJob = 100;
constexpr std::int32_t kMonsterJob = 1002;

SpriteCatalog CatalogWithTable() {
  SpriteCatalog catalog;
  REQUIRE(ro::render::ParseSpriteTableText(ro::render::SpriteTableFile::kWeapons, kTable,
                                           catalog.tables)
              .has_value());
  ro::vfs::ItemEntry sword;
  sword.view_id = 2;
  catalog.items.Put(kSwordItem, sword);
  ro::vfs::ItemEntry rod;
  rod.view_id = 10;
  catalog.items.Put(kRodItem, rod);
  ro::vfs::ItemEntry axe;
  axe.view_id = 6;
  catalog.items.Put(kAxeItem, axe);
  return catalog;
}

}  // namespace

TEST_CASE("Bruit d'un coup : l'identifiant d'objet se traduit avant la table",
          "[render][attacksound]") {
  // 🔴 DEUX TRADUCTIONS, ET LES SAUTER REND LE COUP MUET. Le serveur envoie
  // l'objet — un bâton porte 1601 —, le client en tire la classe d'arme puis le
  // seau d'animation, et sa table n'a que trente entrées. `table[1601]` ne rend
  // rien, et c'est ainsi qu'un mage frappait sans bruit.
  const SpriteCatalog catalog = CatalogWithTable();
  CHECK(WeaponHitSound(catalog, kRodItem, 0) == "_hit_rod.wav");
  CHECK(WeaponHitSound(catalog, kSwordItem, 0) == "_hit_sword.wav");
  // Le tirage ne change rien à une arme : ses quatre bruits n'existent pas.
  CHECK(WeaponHitSound(catalog, kSwordItem, 3) == "_hit_sword.wav");
  // Un seau que la table ne porte pas ne sonne pas, plutôt que de sonner faux.
  CHECK(WeaponHitSound(catalog, kAxeItem, 0).empty());
  // Et un objet que la table ignore vaut les mains nues.
  CHECK(WeaponHitSound(catalog, 999999, 0) == std::string(kFistHitSounds[0]));
}

TEST_CASE("Bruit d'un coup : une SOURCE INTROUVABLE sonne sourd", "[render][attacksound]") {
  // 🔴 ET C'EST LE CAS DE TOUTE UNITÉ POSÉE AU SOL. Le client cherche l'acteur
  // qui frappe dans sa liste ; ne l'y trouvant pas, il garde la classe d'arme à
  // MOINS UN et sonne des quatre bruits sourds. Abandonner au lieu de sonner
  // rendait chaque coup de Storm Gust muet.
  const SpriteCatalog catalog = CatalogWithTable();
  for (std::uint32_t draw = 0; draw < kEnemyHitSounds.size(); ++draw) {
    CHECK(WeaponHitSound(catalog, -1, draw) == std::string(kEnemyHitSounds[draw]));
    CHECK(AttackSound(catalog, false, kMonsterJob, -1, draw) == std::string(kEnemyHitSounds[draw]));
  }
}

TEST_CASE("Bruit d'un coup : les mains nues ont le leur", "[render][attacksound]") {
  const SpriteCatalog catalog = CatalogWithTable();
  for (std::uint32_t draw = 0; draw < kFistHitSounds.size(); ++draw) {
    CHECK(WeaponHitSound(catalog, kBareHandsWeapon, draw) == std::string(kFistHitSounds[draw]));
  }
  // Le tirage revient à son point de départ : c'est un choix parmi quatre, pas
  // un compteur qui déborde.
  CHECK(WeaponHitSound(catalog, kBareHandsWeapon, 4) == std::string(kFistHitSounds[0]));
}

TEST_CASE("Bruit d'un coup : c'est la CIBLE qui décide", "[render][attacksound]") {
  // 🔴 ET NON QUI FRAPPE. Le client lit le métier d'affichage de la CIBLE : un
  // joueur touché sonne de sa classe, tout le reste sonne de l'arme qui le
  // touche. Lire l'attaquant donnait le bruit de l'arme quand un monstre nous
  // frappait, et rien quand nous frappions un monstre.
  const SpriteCatalog catalog = CatalogWithTable();
  CHECK(AttackSound(catalog, false, kMonsterJob, kRodItem, 0) == "_hit_rod.wav");
  // 🔴 ET LE BRUIT D'UN JOUEUR NE VIENT PAS DE L'ARME QUI LE TOUCHE : quoi que
  // tienne celui qui frappe, c'est la classe du frappé qui parle. Le bâton ne
  // s'entend plus.
  CHECK(AttackSound(catalog, true, kNoviceJob, kRodItem, 0) == std::string(kDefaultClassHitSound));
  CHECK(AttackSound(catalog, true, kSwordmanJob, kRodItem, 0) == "player_metal.wav");
  // Une classe HORS des bornes de la table — un costume, une monture — retombe
  // sur celle des armes, comme chez le client.
  CHECK(AttackSound(catalog, true, kOutOfBoundsJob, kRodItem, 0) == "_hit_rod.wav");
}

TEST_CASE("Bruit d'un coup : la table des classes, et ses deux indexations",
          "[render][attacksound]") {
  // 🔴 CE BRUIT NE VIENT D'AUCUN LUA. Le client porte en dur un vecteur de
  // 28 053 entrées, toutes égales à `player_clothes.wav`, dont il ne réassigne
  // que 138 — et c'est pourquoi le premier port ne l'a pas trouvé et rendait
  // les quatre bruits sourds d'un monstre pour un joueur frappé.
  CHECK(ClassHitSound(kNoviceJob) == kDefaultClassHitSound);
  CHECK(ClassHitSound(kSwordmanJob) == "player_metal.wav");
  CHECK(ClassHitSound(kArcherJob) == "player_wooden_male.wav");
  // Une classe que la table ne nomme pas garde ce dont le vecteur est rempli.
  CHECK(ClassHitSound(kLastBasicJob) == kDefaultClassHitSound);

  // ⚠ LES CLASSES AVANCÉES S'INDEXENT PAR `classe − 3950`, et le seuil n'est
  // pas celui des bornes : lire la table à l'index de la classe rendrait le
  // défaut pour toutes.
  CHECK(ClassHitSound(kAdvancedArcherJob) == "player_wooden_male.wav");
  CHECK(ClassHitSound(kLastAdvancedJob) == "player_metal.wav");

  // ⚠ ET LES CLASSES ÉCARTÉES NE RÉPONDENT RIEN : c'est à l'appelant de
  // retomber sur la table des armes, comme le client.
  CHECK(ClassHasHitSound(kNoviceJob));
  CHECK(ClassHasHitSound(kAdvancedArcherJob));
  CHECK_FALSE(ClassHasHitSound(kOutOfBoundsJob));
  CHECK_FALSE(ClassHasHitSound(-1));
  CHECK_FALSE(ClassHasHitSound(3998));
  CHECK_FALSE(ClassHasHitSound(20021));
  CHECK_FALSE(ClassHasHitSound(31999));
  CHECK(ClassHasHitSound(32000));
  // Le grand-novice, seul rescapé de sa plage.
  CHECK(ClassHasHitSound(1000));
  CHECK(ClassHitSound(kOutOfBoundsJob).empty());
}

TEST_CASE("Bruit d'un coup : sans table, seul le poing répond", "[render][attacksound]") {
  const SpriteCatalog empty;
  // Sans table d'objets, toute arme vaut les mains nues.
  CHECK(WeaponHitSound(empty, kRodItem, 0) == std::string(kFistHitSounds[0]));
  CHECK(WeaponHitSound(empty, kBareHandsWeapon, 0) == std::string(kFistHitSounds[0]));
}

TEST_CASE("Bruit d'un coup : une compétence sonne sourd, quoi que tienne le lanceur",
          "[render][attacksound]") {
  // Le client demande la classe d'arme MOINS UN pour ses messages de dégâts de
  // compétence : les quatre bruits des créatures, au tirage.
  for (std::uint32_t draw = 0; draw < 4; ++draw) {
    CHECK(ro::render::SkillBlowSound(draw) == std::string(kEnemyHitSounds[draw]));
  }
  CHECK(ro::render::SkillBlowSound(7) == std::string(kEnemyHitSounds[3]));
}

TEST_CASE("Bruit d'un coup : quatre sortes de coup rendent son métier à la cible",
          "[render][attacksound]") {
  // Les sortes qui mènent aux messages 39, 93 et 148 : le client y rejoint le
  // corps commun, celui qui lit le métier de la CIBLE.
  CHECK_FALSE(ro::render::SkillBlowUsesEnemySounds(4, 1));   // encaissé, message 39
  CHECK_FALSE(ro::render::SkillBlowUsesEnemySounds(9, 1));   // multiple-encaissé, 39
  CHECK_FALSE(ro::render::SkillBlowUsesEnemySounds(10, 1));  // critique, message 93
  CHECK_FALSE(ro::render::SkillBlowUsesEnemySounds(13, 1));  // multiple-critique, 148
  // Celles qui mènent aux messages 96 et 97, seuls à poser la classe d'arme à
  // moins un — tant que le coup ne porte qu'une frappe.
  CHECK(ro::render::SkillBlowUsesEnemySounds(0, 1));
  CHECK(ro::render::SkillBlowUsesEnemySounds(5, 1));
  CHECK(ro::render::SkillBlowUsesEnemySounds(6, 1));
  CHECK(ro::render::SkillBlowUsesEnemySounds(7, 1));
  CHECK(ro::render::SkillBlowUsesEnemySounds(8, 1));
  CHECK(ro::render::SkillBlowUsesEnemySounds(11, 1));
  CHECK(ro::render::SkillBlowUsesEnemySounds(12, 1));
  CHECK(ro::render::SkillBlowUsesEnemySounds(14, 1));
}

TEST_CASE("Bruit d'un coup : un coup multiple rend son métier à la cible",
          "[render][attacksound]") {
  // Chez le client d'origine, un Cold Bolt de High Wizard sur un High Wizard
  // arrive en sorte 8 — que le seul switch range en 96 — et le message
  // réellement délivré est 111, parce qu'il porte le CUMUL du coup multiple.
  CHECK_FALSE(ro::render::SkillBlowUsesEnemySounds(8, 3));
  CHECK_FALSE(ro::render::SkillBlowUsesEnemySounds(6, 2));
  CHECK_FALSE(ro::render::SkillBlowUsesEnemySounds(8, kLargestHitCount));
  // Une seule frappe ne porte aucun cumul : la sorte décide seule.
  CHECK(ro::render::SkillBlowUsesEnemySounds(8, 1));
  // ⚠ AU-DELÀ DE LA BORNE, LE CLIENT RAMÈNE LE COMPTE À UN, et le cumul
  // disparaît avec lui : le coup sonne de nouveau comme une compétence simple.
  CHECK(ro::render::SkillBlowUsesEnemySounds(8, kLargestHitCount + 1));
  // Et les quatre sortes qui rendent déjà son métier à la cible le font quel
  // que soit le compte.
  CHECK_FALSE(ro::render::SkillBlowUsesEnemySounds(10, 1));
  CHECK_FALSE(ro::render::SkillBlowUsesEnemySounds(10, 4));
}
