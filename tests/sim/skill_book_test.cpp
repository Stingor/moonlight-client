#include "ro/sim/skill_book.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/world.h"

// LE GRIMOIRE : ce que le personnage sait lancer.
using ro::sim::KnownSkill;
using ro::sim::SkillBook;

namespace {

KnownSkill Learned(std::uint16_t id, std::string name, std::uint16_t targeting,
                   std::uint16_t level = 1) {
  KnownSkill skill;
  skill.id = id;
  skill.name = std::move(name);
  skill.targeting = targeting;
  skill.level = level;
  return skill;
}

}  // namespace

TEST_CASE("Grimoire : les compétences se rangent par identifiant", "[sim][skills]") {
  // Le serveur les envoie dans l'ordre de sa propre table, qui n'est ni celui
  // de son fichier ni un ordre stable d'une session à l'autre. Un ordre fixe
  // est ce qui permet de cliquer deux fois au même endroit sur la même ligne.
  SkillBook book;
  book.Learn(Learned(28, "AL_HEAL", ro::sim::kSkillOnFriend));
  book.Learn(Learned(14, "MG_COLDBOLT", ro::sim::kSkillOnEnemy));
  book.Learn(Learned(80, "MG_FIREBALL", ro::sim::kSkillOnEnemy));

  REQUIRE(book.size() == 3);
  CHECK(book.skills()[0].id == 14);
  CHECK(book.skills()[1].id == 28);
  CHECK(book.skills()[2].id == 80);
}

TEST_CASE("Grimoire : une mise à jour garde le nom qu'elle ne porte pas", "[sim][skills]") {
  // Le paquet de mise à jour n'a PAS de champ de nom : le serveur ne le redit
  // qu'à l'ajout. L'écraser laisserait une ligne sans titre dès la première
  // montée de rang, et le grimoire deviendrait illisible en montant.
  SkillBook book;
  book.Learn(Learned(14, "MG_COLDBOLT", ro::sim::kSkillOnEnemy, 3));

  KnownSkill raised;
  raised.id = 14;
  raised.level = 4;
  raised.targeting = ro::sim::kSkillOnEnemy;
  book.Learn(raised);

  REQUIRE(book.size() == 1);
  const KnownSkill* known = book.Find(14);
  REQUIRE(known != nullptr);
  CHECK(known->level == 4);
  CHECK(known->name == "MG_COLDBOLT");

  // Un nom NOUVEAU, lui, remplace : le serveur peut renommer une compétence
  // d'une version à l'autre.
  book.Learn(Learned(14, "MG_COLDBOLT2", ro::sim::kSkillOnEnemy, 5));
  CHECK(book.Find(14)->name == "MG_COLDBOLT2");
}

TEST_CASE("Grimoire : ce qu'on oublie s'en va, et le reste tient", "[sim][skills]") {
  SkillBook book;
  book.Learn(Learned(14, "MG_COLDBOLT", ro::sim::kSkillOnEnemy));
  book.Learn(Learned(28, "AL_HEAL", ro::sim::kSkillOnFriend));

  CHECK(book.Forget(14));
  CHECK(book.Find(14) == nullptr);
  REQUIRE(book.size() == 1);
  CHECK(book.skills()[0].id == 28);

  // Oublier deux fois ne se fait pas passer pour un succès : le même paquet
  // sert à retirer d'autres choses.
  CHECK_FALSE(book.Forget(14));

  book.Clear();
  CHECK(book.size() == 0);
}

TEST_CASE("Grimoire : ce qu'une compétence vise décide de comment la lancer", "[sim][skills]") {
  // Le serveur REFUSE une demande qui se trompe, et sans un mot : une
  // compétence de zone envoyée comme visant quelqu'un ne fait rien du tout.
  const KnownSkill bolt = Learned(14, "MG_COLDBOLT", ro::sim::kSkillOnEnemy);
  CHECK(ro::sim::SkillNeedsTarget(bolt));
  CHECK_FALSE(ro::sim::SkillNeedsGround(bolt));
  CHECK_FALSE(ro::sim::SkillCastsOnSelf(bolt));
  CHECK_FALSE(ro::sim::SkillIsPassive(bolt));

  const KnownSkill heal = Learned(28, "AL_HEAL", ro::sim::kSkillOnFriend);
  CHECK(ro::sim::SkillNeedsTarget(heal));

  const KnownSkill wall = Learned(21, "MG_SAFETYWALL", ro::sim::kSkillOnGround);
  CHECK(ro::sim::SkillNeedsGround(wall));
  CHECK_FALSE(ro::sim::SkillNeedsTarget(wall));

  // POSER UN PIÈGE, C'EST VISER UNE CASE : le serveur annonce Sandman par le
  // drapeau du sol.
  const KnownSkill sandman = Learned(115, "HT_SANDMAN", ro::sim::kSkillOnGround);
  CHECK(ro::sim::SkillNeedsGround(sandman));
  CHECK_FALSE(ro::sim::SkillTargetsTrap(sandman));

  // 🔴 LE DRAPEAU DU PIÈGE VISE UN PIÈGE DÉJÀ POSÉ, et part par la demande
  // visant quelqu'un : envoyé au sol, le serveur le jetait sans un mot.
  const KnownSkill remove_trap = Learned(124, "HT_REMOVETRAP", ro::sim::kSkillTrap);
  CHECK_FALSE(ro::sim::SkillNeedsGround(remove_trap));
  CHECK(ro::sim::SkillTargetsTrap(remove_trap));
  CHECK_FALSE(ro::sim::SkillNeedsTarget(remove_trap));
  CHECK_FALSE(ro::sim::SkillCastsOnSelf(remove_trap));

  // Une compétence qui vise le SOL et porte aussi le drapeau « sur soi » reste
  // une compétence de sol : le serveur écarte les compétences de zone de la
  // demande visant quelqu'un AVANT de regarder quoi que ce soit d'autre.
  const KnownSkill both =
      Learned(83, "MG_THUNDERSTORM", ro::sim::kSkillOnGround | ro::sim::kSkillOnSelf);
  CHECK(ro::sim::SkillNeedsGround(both));
  CHECK_FALSE(ro::sim::SkillCastsOnSelf(both));

  const KnownSkill blessing = Learned(34, "AL_BLESSING", ro::sim::kSkillOnSelf);
  CHECK(ro::sim::SkillCastsOnSelf(blessing));
  CHECK_FALSE(ro::sim::SkillNeedsTarget(blessing));

  const KnownSkill passive = Learned(1, "NV_BASIC", ro::sim::kSkillPassive);
  CHECK(ro::sim::SkillIsPassive(passive));
  CHECK_FALSE(ro::sim::SkillNeedsTarget(passive));
  CHECK_FALSE(ro::sim::SkillNeedsGround(passive));
  CHECK_FALSE(ro::sim::SkillCastsOnSelf(passive));
}

TEST_CASE("Grimoire : changer de carte n'efface pas ce qu'on sait", "[sim][skills]") {
  // Le serveur redit le grimoire à chaque entrée, mais plus tard que la carte
  // elle-même : le vider au saut laisserait un panneau blanc entre les deux
  // annonces.
  ro::sim::World world;
  world.skill_book().Learn(Learned(14, "MG_COLDBOLT", ro::sim::kSkillOnEnemy));

  world.EnterMap("prontera", 150, 150);
  CHECK(world.skill_book().size() == 1);

  world.Clear();
  CHECK(world.skill_book().size() == 1);
}
