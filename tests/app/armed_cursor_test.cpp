#include "world/armed_cursor.h"

#include <catch2/catch_test_macros.hpp>

#include "ro/render/cursor.h"
#include "ro/sim/actor.h"
#include "ro/sim/skill_tree.h"

namespace {

using moonclient::ArmedBannerText;
using moonclient::ArmedCursor;
using moonclient::ArmedLevelAfterWheel;
using moonclient::SkillAim;
using ro::render::CursorShape;

ro::sim::Actor Monster(ro::sim::Posture posture) {
  ro::sim::Actor actor;
  actor.kind = ro::sim::ActorKind::kMonster;
  actor.posture = posture;
  return actor;
}

// Une fiche : le nom interne et le libellé affiché.
[[nodiscard]] ro::sim::SkillTree BashTree() {
  return ro::sim::SkillTree::FromTables({ro::sim::SkillNode{.id = 5,
                                                            .idname = "SM_BASH",
                                                            .display_name = "Bash",
                                                            .sp_by_level = {8, 8, 8, 8, 8, 15},
                                                            .level_matters = true}},
                                        {});
}

}  // namespace

TEST_CASE("Curseur armé : sans armement, rien ne change", "[app][cursor]") {
  // La forme ordinaire se calcule de toute façon ; l'armement ne fait que
  // la reprendre ou la remplacer.
  CHECK(ArmedCursor(SkillAim::kNothing, nullptr, CursorShape::kAttack) == CursorShape::kAttack);
  CHECK(ArmedCursor(SkillAim::kNothing, nullptr, CursorShape::kArrow) == CursorShape::kArrow);
  CHECK(ArmedCursor(SkillAim::kNothing, nullptr, CursorShape::kForbidden) ==
        CursorShape::kForbidden);
}

TEST_CASE("Curseur armé : l'épée ne paraît JAMAIS", "[app][cursor]") {
  // 🔴 C'EST LA PANNE QUE CET ESSAI GARDE. L'épée annonce le coup ORDINAIRE,
  // celui que l'armement vient de suspendre : la montrer promet le geste qu'on
  // a désactivé. Le client pose l'anneau, et lui seul.
  const ro::sim::Actor alive = Monster(ro::sim::Posture::kStanding);

  CHECK(ArmedCursor(SkillAim::kTarget, &alive, CursorShape::kAttack) != CursorShape::kAttack);
  CHECK(ArmedCursor(SkillAim::kGround, &alive, CursorShape::kAttack) != CursorShape::kAttack);
}

TEST_CASE("Curseur armé : l'anneau s'accélère sur une cible", "[app][cursor]") {
  const ro::sim::Actor alive = Monster(ro::sim::Posture::kStanding);

  // Ce qui se frappe est ce que la compétence atteindra : l'anneau VIF.
  CHECK(ArmedCursor(SkillAim::kTarget, &alive, CursorShape::kAttack) == CursorShape::kTargetLocked);
  // Ailleurs, l'anneau qui cherche encore.
  CHECK(ArmedCursor(SkillAim::kTarget, nullptr, CursorShape::kArrow) == CursorShape::kGroundTarget);
}

TEST_CASE("Curseur armé : un mort garde la flèche, une case interdite non", "[app][cursor]") {
  const ro::sim::Actor dead = Monster(ro::sim::Posture::kDead);

  // ⚠ UN CADAVRE N'EST PAS UNE CIBLE, et l'anneau y promettrait un lancement.
  CHECK(ArmedCursor(SkillAim::kTarget, &dead, CursorShape::kArrow) == CursorShape::kArrow);
  // Le panneau d'interdiction, lui, CÈDE : le client consulte le mode de
  // ciblage avant la case, et l'armement passe devant.
  CHECK(ArmedCursor(SkillAim::kTarget, nullptr, CursorShape::kForbidden) ==
        CursorShape::kGroundTarget);
}

TEST_CASE("Curseur armé : ce qui n'est pas une cible garde sa forme", "[app][cursor]") {
  // Les branches du client qui posent ces formes-là ne lisent jamais le mode de
  // ciblage : un clic sur un PNJ lui parle même une compétence armée, et un
  // objet au sol se ramasse. La forme doit le dire.
  CHECK(ArmedCursor(SkillAim::kTarget, nullptr, CursorShape::kTalk) == CursorShape::kTalk);
  CHECK(ArmedCursor(SkillAim::kTarget, nullptr, CursorShape::kWarp) == CursorShape::kWarp);
  CHECK(ArmedCursor(SkillAim::kTarget, nullptr, CursorShape::kPickUp) == CursorShape::kPickUp);
}

TEST_CASE("Curseur armé : le ciblage au sol ne regarde RIEN", "[app][cursor]") {
  // 🔴 LE CLIENT SORT DE SON ROUTEUR DE SURVOL dès l'entrée dans ce mode-là :
  // aucune branche par catégorie ne s'exécute, et l'anneau couvre tout — les
  // PNJ, les portails et les objets au sol compris.
  const ro::sim::Actor alive = Monster(ro::sim::Posture::kStanding);

  CHECK(ArmedCursor(SkillAim::kGround, &alive, CursorShape::kAttack) == CursorShape::kGroundTarget);
  CHECK(ArmedCursor(SkillAim::kGround, nullptr, CursorShape::kTalk) == CursorShape::kGroundTarget);
  CHECK(ArmedCursor(SkillAim::kGround, nullptr, CursorShape::kPickUp) ==
        CursorShape::kGroundTarget);
  CHECK(ArmedCursor(SkillAim::kGround, nullptr, CursorShape::kForbidden) ==
        CursorShape::kGroundTarget);
}

TEST_CASE("Curseur armé : la visée d'un piège garde l'épée sur ce qui se frappe", "[app][cursor]") {
  // Le client ne consulte le mode de ciblage, sur un monstre, que pour une
  // visée de cible ou de soutien : Remove Trap armé y laisse l'épée, et
  // l'anneau lent partout ailleurs.
  const ro::sim::Actor alive = Monster(ro::sim::Posture::kStanding);
  CHECK(ArmedCursor(SkillAim::kTrap, &alive, CursorShape::kAttack) == CursorShape::kAttack);
  CHECK(ArmedCursor(SkillAim::kTrap, nullptr, CursorShape::kArrow) == CursorShape::kGroundTarget);
  CHECK(ArmedCursor(SkillAim::kTrap, nullptr, CursorShape::kForbidden) ==
        CursorShape::kGroundTarget);
  CHECK(ArmedCursor(SkillAim::kTrap, nullptr, CursorShape::kTalk) == CursorShape::kTalk);
  CHECK(ArmedCursor(SkillAim::kTrap, nullptr, CursorShape::kPickUp) == CursorShape::kPickUp);
}

// ── LE RANG ARMÉ, RÉGLÉ À LA MOLETTE ────────────────────────────────────────

TEST_CASE("Rang armé : un cran monte d'UN rang, quel qu'en soit le nombre",
          "[app][cursor][skill]") {
  // 🔴 C'EST LA PANNE QUE CET ESSAI GARDE. Le client ne lit que le SIGNE du
  // cumul de l'image : une molette rendue en crans ferait sauter le rang de
  // trois d'un seul geste, là où le jeu en monte un.
  CHECK(ArmedLevelAfterWheel(5, 10, true, 1.0F) == 6);
  CHECK(ArmedLevelAfterWheel(5, 10, true, 3.0F) == 6);
  CHECK(ArmedLevelAfterWheel(5, 10, true, 0.25F) == 6);
  CHECK(ArmedLevelAfterWheel(5, 10, true, -1.0F) == 4);
  CHECK(ArmedLevelAfterWheel(5, 10, true, -3.0F) == 4);
}

TEST_CASE("Rang armé : les bornes sont l'appris et UN", "[app][cursor][skill]") {
  // Le plafond est le rang APPRIS, et non le rang maximal de la compétence : on
  // ne lance pas plus haut que ce que l'on sait.
  CHECK(ArmedLevelAfterWheel(7, 7, true, 1.0F) == 7);
  CHECK(ArmedLevelAfterWheel(1, 7, true, -1.0F) == 1);
  // Un rang armé DÉJÀ au-dessus de l'appris redescend : le serveur peut avoir
  // changé la ligne entre le choix et le cran.
  CHECK(ArmedLevelAfterWheel(9, 7, true, 1.0F) == 7);
}

TEST_CASE("Rang armé : sans molette, rien ne bouge", "[app][cursor][skill]") {
  CHECK(ArmedLevelAfterWheel(5, 10, true, 0.0F) == 5);
}

TEST_CASE("Rang armé : ce qui ne se dose pas ne bouge pas", "[app][cursor][skill]") {
  // ⚠ `IsLevelUseSkill` EST UNE LISTE BLANCHE, et le client la consulte à chaque
  // cran. Sans elle, le rang changerait sous le joueur pour un sort où il ne
  // veut rien dire — et partirait plus bas qu'il ne l'a voulu.
  CHECK(ArmedLevelAfterWheel(5, 10, false, 1.0F) == 5);
  CHECK(ArmedLevelAfterWheel(5, 10, false, -1.0F) == 5);
}

TEST_CASE("Rang armé : un rang appris nul ne borne rien", "[app][cursor][skill]") {
  // Une compétence que le grimoire ne porte pas n'a pas de plafond à donner :
  // la borner à zéro puis la remonter à un armerait un rang qu'on n'a pas.
  CHECK(ArmedLevelAfterWheel(3, 0, true, 1.0F) == 3);
  CHECK(ArmedLevelAfterWheel(3, 0, true, -1.0F) == 3);
}

TEST_CASE("Étiquette armée : le nom des tables, et le rang qui partira", "[app][cursor][skill]") {
  const ro::sim::SkillTree tree = BashTree();

  CHECK(ArmedBannerText(&tree, 5, 3, true) == "Bash Nv3");
  // ⚠ LE CLIENT N'Y MET QUE LE NOM, son rang se lisant au curseur : qui garde
  // les deux affichages ne veut pas du rang deux fois.
  CHECK(ArmedBannerText(&tree, 5, 3, false) == "Bash");
  // ⚠ SANS TABLES, ON DIT LE NUMÉRO : un bandeau sans nom ressemblerait à une
  // compétence sans nom, et les deux se corrigent à des endroits opposés.
  CHECK(ArmedBannerText(nullptr, 5, 3, true) == "#5 Nv3");
  CHECK(ArmedBannerText(&tree, 4242, 1, true) == "#4242 Nv1");
}
