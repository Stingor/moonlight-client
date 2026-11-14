#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_update.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/misc_effect.h"
#include "ro/sim/skill_book.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

// Ce que les paquets de COMPÉTENCE font du monde : les unités qu'elles posent
// au sol, les coups qu'elles portent, et l'incantation qui les précède.
using ro::net::ApplyFrame;
using ro::net::FrameOutcome;
using ro::test_support::FrameOf;
using ro::test_support::IdleUnit;

namespace {

// L'effet que cet acteur porte sous ce numéro, ou rien. Un acteur en porte
// plusieurs à la fois : le chercher par son numéro est la seule façon de dire
// ce qu'un paquet a fait.
const ro::sim::PlayedEffect* EffectOf(const ro::sim::Actor* actor, std::uint32_t effect_id) {
  if (actor == nullptr) {
    return nullptr;
  }
  const auto found = std::ranges::find(actor->effects, effect_id, &ro::sim::PlayedEffect::id);
  return found == actor->effects.end() ? nullptr : &*found;
}

}  // namespace

TEST_CASE("Mise à jour : une unité au sol se pose et se retire", "[net][world]") {
  // Un piège, un mur, une zone : le serveur les annonce par leur propre paquet,
  // et ce ne sont PAS des acteurs. Ils occupent une case et s'en vont.
  ro::sim::World world;

  // 0x09ca : longueur, identifiant, poseur, case, unité, portée, visible,
  // niveau. La longueur est recopiée en tête à l'imitation d'un paquet de
  // taille variable ; la table la donne fixe à vingt-trois.
  const std::vector<std::uint8_t> pose = {0xCA, 0x09, 0x17, 0x00,  // opcode, longueur
                                          0x39, 0x30, 0x00, 0x00,  // identifiant de l'unité
                                          0x01, 0x00, 0x00, 0x00,  // qui l'a posée
                                          0x64, 0x00, 0x2A, 0x00,  // case 100, 42
                                          0x91, 0x00, 0x00, 0x00,  // unité 145 : un piège
                                          0x01, 0x01, 0x05};       // portée, visible, niveau
  CHECK(ApplyFrame(FrameOf(pose), world, 900) == FrameOutcome::kApplied);
  REQUIRE(world.skill_units().size() == 1);
  const ro::sim::SkillUnit& unit = world.skill_units().begin()->second;
  CHECK(unit.id == 0x3039);
  CHECK(unit.creator_id == 1);
  CHECK(unit.cell.x == 100);
  CHECK(unit.cell.y == 42);
  CHECK(unit.unit_id == 145);
  CHECK(unit.range == 1);
  CHECK(unit.skill_level == 5);
  CHECK(unit.placed_ms == 900);
  // Une unité au sol n'est pas un acteur : rien ne doit être apparu.
  CHECK(world.actors().empty());

  // 0x0120 : le retrait, par le même identifiant.
  const std::vector<std::uint8_t> retrait = {0x20, 0x01, 0x39, 0x30, 0x00, 0x00};
  CHECK(ApplyFrame(FrameOf(retrait), world, 1500) == FrameOutcome::kApplied);
  CHECK(world.skill_units().empty());
  // Le même paquet sert à retirer d'autres choses : un identifiant inconnu
  // n'est pas une erreur, il ne nous concerne pas.
  CHECK(ApplyFrame(FrameOf(retrait), world, 1600) == FrameOutcome::kIgnored);
}

TEST_CASE("Mise à jour : un piège CACHÉ se pose sans dire ce qu'il est", "[net][world]") {
  // Le serveur remplace l'identifiant d'unité par celui de l'unité muette quand
  // le piège est cache : il ne dit pas ce qu'il cache. L'unite existe quand
  // meme — elle occupe une case, et son retrait passera par son identifiant.
  ro::sim::World world;
  const std::vector<std::uint8_t> cache = {0xCA, 0x09, 0x17, 0x00, 0x39, 0x30, 0x00, 0x00,
                                           0x01, 0x00, 0x00, 0x00, 0x64, 0x00, 0x2A, 0x00,
                                           0x91, 0x00, 0x00, 0x00, 0x01, 0x00, 0x05};
  CHECK(ApplyFrame(FrameOf(cache), world, 900) == FrameOutcome::kApplied);
  REQUIRE(world.skill_units().size() == 1);
  CHECK(world.skill_units().begin()->second.unit_id == ro::sim::kSilentSkillUnit);
  CHECK_FALSE(world.skill_units().begin()->second.visible);

  // LE NÔTRE SE MONTRE TOUJOURS, quoi que dise le drapeau.
  ro::sim::World own;
  own.set_player_id(1);
  CHECK(ApplyFrame(FrameOf(cache), own, 900) == FrameOutcome::kApplied);
  REQUIRE(own.skill_units().size() == 1);
  CHECK(own.skill_units().begin()->second.visible);
  CHECK(own.skill_units().begin()->second.unit_id == 145);
}

TEST_CASE("Mise à jour : un piège qui se déclenche change d'apparence", "[net][world]") {
  // Le serveur l'annonce par le paquet de changement d'apparence d'un ACTEUR,
  // sur l'identifiant de l'UNITÉ : l'apparence de base devient celle du piège
  // usé, et le modèle joue son animation.
  ro::sim::World world;
  const std::vector<std::uint8_t> pose = {0xCA, 0x09, 0x17, 0x00, 0x39, 0x30, 0x00, 0x00,
                                          0x01, 0x00, 0x00, 0x00, 0x64, 0x00, 0x2A, 0x00,
                                          0x91, 0x00, 0x00, 0x00, 0x01, 0x01, 0x05};
  REQUIRE(ApplyFrame(FrameOf(pose), world, 900) == FrameOutcome::kApplied);
  CHECK(world.skill_units().at(0x3039).look_since_ms == 900);
  CHECK(world.skill_units().at(0x3039).placed_unit_id == 0x91);

  // 0x01d7 : identifiant, sorte 0 (la base), valeur 140, seconde valeur.
  const std::vector<std::uint8_t> usee = {0xD7, 0x01, 0x39, 0x30, 0x00, 0x00, 0x00, 0x8C,
                                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  CHECK(ApplyFrame(FrameOf(usee), world, 1500) == FrameOutcome::kApplied);
  const ro::sim::SkillUnit& unit = world.skill_units().at(0x3039);
  CHECK(unit.unit_id == 140);
  CHECK(unit.look_since_ms == 1500);
  // L'apparence de la pose reste : c'est elle qui dit ce que le piège était.
  CHECK(unit.placed_unit_id == 0x91);
  REQUIRE(unit.sprung_ms.has_value());
  CHECK(*unit.sprung_ms == 1500);

  // Une autre sorte d'apparence ne concerne pas une unité.
  std::vector<std::uint8_t> cheveux = usee;
  cheveux[6] = 1;
  CHECK(ApplyFrame(FrameOf(cheveux), world, 1600) == FrameOutcome::kIgnored);
}

TEST_CASE("Mise à jour : un coup de COMPÉTENCE est un coup", "[net][world]") {
  // Le serveur l'annonce par son propre paquet, mais c'est le même fait :
  // quelqu'un frappe quelqu'un, la cible bronche, le nombre paraît. Ce qui
  // change, c'est le numéro de la compétence, que le monde garde pour que ce
  // qui dessine et ce qui sonne sachent quoi jouer.
  ro::sim::World world;
  ro::sim::Actor caster;
  caster.id = 100;
  world.Spawn(caster);
  ro::sim::Actor target;
  target.id = 200;
  target.kind = ro::sim::ActorKind::kMonster;
  world.Spawn(target);

  // 0x01de : compétence, lanceur, cible, instant, délais, dégâts, niveau,
  // compte, sorte.
  const std::vector<std::uint8_t> strike = {0xDE, 0x01, 0x59, 0x00,  // opcode, compétence 89
                                            0x64, 0x00, 0x00, 0x00,  // lanceur
                                            0xC8, 0x00, 0x00, 0x00,  // cible
                                            0x00, 0x00, 0x00, 0x00,  // horloge du serveur
                                            0x2C, 0x01, 0x00, 0x00,  // délai du lanceur : 300
                                            0xC8, 0x00, 0x00, 0x00,  // délai de la cible : 200
                                            0xE8, 0x03, 0x00, 0x00,  // dégâts : 1000
                                            0x05, 0x00,              // niveau
                                            0x03, 0x00,              // trois coups
                                            0x08};                   // sorte : coup multiple
  CHECK(ApplyFrame(FrameOf(strike), world, 700) == FrameOutcome::kApplied);

  const auto* frappe = world.Find(200);
  REQUIRE(frappe != nullptr);
  REQUIRE(frappe->hit.has_value());
  CHECK(frappe->hit->skill_id == 89);
  CHECK(frappe->hit->skill_level == 5);
  CHECK(frappe->hit->damage == 1000);
  CHECK(frappe->hit->hits == 3);
  CHECK(frappe->hit->type == 8);
  CHECK(frappe->hit->attack_motion_ms == 300);
  CHECK(frappe->hit->hurt_motion_ms == 200);
  CHECK(frappe->hit->flinches);
  // Une compétence ne frappe pas de la seconde main : le paquet n'a pas de
  // champ pour elle, et en inventer un ferait paraître un second nombre à zéro.
  CHECK(frappe->hit->second_damage == 0);
}

TEST_CASE("Mise à jour : une incantation commence, puis se coupe", "[net][world]") {
  // Le serveur annonce le début d'un sort avec sa durée, et n'annonce sa fin
  // QUE si elle est interrompue. Le paquet d'interruption s'appelle « dispel »
  // mais ne dissipe rien : clif_skillcastcancel en est le seul émetteur.
  ro::sim::World world;
  ro::sim::Actor caster;
  caster.id = 100;
  world.Spawn(caster);
  ro::sim::Actor target;
  target.id = 200;
  target.kind = ro::sim::ActorKind::kMonster;
  world.Spawn(target);

  ro::proto::ZcUseskillAck ack;
  ack.srcId = 100;
  ack.dstId = 200;
  ack.skillId = 14;
  ack.element = 2;
  ack.delayTime = 1500;
  CHECK(ApplyFrame(FrameOf(Encode(ack)), world, 4000) == FrameOutcome::kApplied);

  const ro::sim::Actor* caster_actor = world.Find(100);
  REQUIRE(caster_actor != nullptr);
  REQUIRE(caster_actor->cast.has_value());
  CHECK(caster_actor->cast->skill_id == 14);
  CHECK(caster_actor->cast->target_id == 200);
  CHECK(caster_actor->cast->element == 2);
  CHECK(caster_actor->cast->duration_ms == 1500);
  // L'instant est celui de l'APPELANT, pas celui du paquet : il n'en porte pas.
  CHECK(caster_actor->cast->started_ms == 4000);

  ro::proto::ZcDispel coupe;
  coupe.gid = 100;
  CHECK(ApplyFrame(FrameOf(Encode(coupe)), world, 4600) == FrameOutcome::kApplied);
  // L'incantation reste connue, arrêtée là où elle en était : le repos qui
  // suit sait qu'on la quitte.
  REQUIRE(world.Find(100)->cast.has_value());
  CHECK(world.Find(100)->cast->duration_ms == 600);

  // Le même paquet pour quelqu'un qui n'incantait pas ne change rien.
  coupe.gid = 200;
  CHECK(ApplyFrame(FrameOf(Encode(coupe)), world, 4700) == FrameOutcome::kIgnored);
}

TEST_CASE("Mise à jour : une incantation instantanée ne tourne pas son lanceur", "[net][world]") {
  // Le client sort du geste d'incantation avant de tourner le lanceur quand la
  // durée vaut zéro : le coup ou l'emploi qui suit s'en charge.
  ro::sim::World world;
  ro::sim::Actor caster;
  caster.id = 100;
  caster.position = ro::sim::CellPosition{10, 10, 0};
  world.Spawn(caster);
  ro::sim::Actor target;
  target.id = 200;
  target.position = ro::sim::CellPosition{13, 10, 0};
  world.Spawn(target);

  ro::proto::ZcUseskillAck ack;
  ack.srcId = 100;
  ack.dstId = 200;
  ack.skillId = 14;
  ack.delayTime = 0;
  CHECK(ApplyFrame(FrameOf(Encode(ack)), world, 4000) == FrameOutcome::kApplied);
  CHECK(world.Find(100)->position.direction == 0);

  // Avec une durée, le lanceur se tourne vers sa cible : l'est, six.
  ack.delayTime = 1500;
  CHECK(ApplyFrame(FrameOf(Encode(ack)), world, 4100) == FrameOutcome::kApplied);
  CHECK(world.Find(100)->position.direction == 6);
}

TEST_CASE("Mise à jour : un soin tourne le prêtre vers le soigné", "[net][world]") {
  // L'emploi d'une compétence sans dégâts tourne son lanceur vers sa cible,
  // comme un coup.
  ro::sim::World world;
  ro::sim::Actor healer;
  healer.id = 3;
  healer.position = ro::sim::CellPosition{10, 10, 0};
  world.Spawn(healer);
  ro::sim::Actor wounded;
  wounded.id = 9;
  wounded.position = ro::sim::CellPosition{13, 10, 0};
  world.Spawn(wounded);

  ro::proto::ZcUseSkill heal;
  heal.SKID = 28;
  heal.level = 137;
  heal.targetAID = 9;
  heal.srcAID = 3;
  heal.result = 1;
  CHECK(ApplyFrame(FrameOf(Encode(heal)), world, 2500) == FrameOutcome::kApplied);
  CHECK(world.Find(3)->position.direction == 6);
  CHECK(world.Find(3)->facing_origin == ro::sim::FacingOrigin::kAim);
}

TEST_CASE("Mise à jour : l'attaque d'une cible hors de vue tourne le dos au joueur",
          "[net][world]") {
  // 🔴 CE QUE LE CLIENT FAIT D'UNE CIBLE QU'IL NE VOIT PAS : le frappeur se
  // tourne vers le point symétrique du joueur par rapport à lui — ce qu'on ne
  // voit pas est au-delà de la vue. Le joueur est au sud du frappeur : celui-ci
  // regarde le nord.
  ro::sim::World world;
  world.set_player_id(150'000);
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(150'000, ro::sim::CellPosition{10, 5, 0}))), world,
                     0) == FrameOutcome::kApplied);
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(150'001, ro::sim::CellPosition{10, 10, 6}))), world,
                     0) == FrameOutcome::kApplied);

  ro::proto::ZcNotifyAct blow;
  blow.srcID = 150'001;
  blow.targetID = 999'999;
  blow.damage = 42;
  blow.type = 0;
  CHECK(ApplyFrame(FrameOf(Encode(blow)), world, 900) == FrameOutcome::kApplied);
  CHECK(world.Find(150'001)->position.direction == 0);
}

TEST_CASE("Mise à jour : un sort visé au SOL n'a pas de cible", "[net][world]") {
  // Le serveur met alors zéro dans l'identifiant de cible et donne la case.
  // Prendre l'un pour l'autre ferait tourner le lanceur vers un acteur absent.
  ro::sim::World world;
  ro::sim::Actor caster;
  caster.id = 7;
  world.Spawn(caster);

  ro::proto::ZcUseskillAck ack;
  ack.srcId = 7;
  ack.dstId = 0;
  ack.x = 153;
  ack.y = 97;
  ack.skillId = 89;
  ack.delayTime = 2000;
  CHECK(ApplyFrame(FrameOf(Encode(ack)), world, 100) == FrameOutcome::kApplied);

  const ro::sim::Actor* caster_actor = world.Find(7);
  REQUIRE(caster_actor != nullptr);
  REQUIRE(caster_actor->cast.has_value());
  CHECK(caster_actor->cast->target_id == 0);
  CHECK(caster_actor->cast->target_cell.x == 153);
  CHECK(caster_actor->cast->target_cell.y == 97);
}

TEST_CASE("Mise à jour : une compétence SANS dégâts porte son montant", "[net][world]") {
  // Le serveur les annonce toutes par le même paquet, et le champ qui s'appelle
  // « niveau » y porte en réalité le montant quand la compétence en a un. Le
  // monde garde les deux sans juger : c'est le numéro de la compétence qui dira
  // ce que le nombre veut dire.
  ro::sim::World world;
  ro::sim::Actor healer;
  healer.id = 3;
  world.Spawn(healer);
  ro::sim::Actor wounded;
  wounded.id = 9;
  world.Spawn(wounded);

  ro::proto::ZcUseSkill heal;
  heal.SKID = 28;  // AL_HEAL
  heal.level = 137;
  heal.targetAID = 9;
  heal.srcAID = 3;
  heal.result = 1;
  CHECK(ApplyFrame(FrameOf(Encode(heal)), world, 2500) == FrameOutcome::kApplied);

  const ro::sim::Actor* healed = world.Find(9);
  REQUIRE(healed != nullptr);
  REQUIRE(healed->support.has_value());
  CHECK(healed->support->skill_id == 28);
  CHECK(healed->support->amount == 137);
  CHECK(healed->support->source_id == 3);
  CHECK(healed->support->succeeded);
  CHECK(healed->support->started_ms == 2500);
  // Elle se pose sur la CIBLE, pas sur le lanceur.
  CHECK_FALSE(world.Find(3)->support.has_value());

  // Une compétence qui ÉCHOUE s'annonce quand même.
  ro::proto::ZcUseSkill failed;
  failed.SKID = 34;
  failed.level = 3;
  failed.targetAID = 9;
  failed.srcAID = 3;
  failed.result = 0;
  CHECK(ApplyFrame(FrameOf(Encode(failed)), world, 2600) == FrameOutcome::kApplied);
  CHECK_FALSE(world.Find(9)->support->succeeded);

  // Sur quelqu'un qu'on ne voit pas, rien à noter.
  ro::proto::ZcUseSkill elsewhere;
  elsewhere.SKID = 28;
  elsewhere.targetAID = 4242;
  CHECK(ApplyFrame(FrameOf(Encode(elsewhere)), world, 2700) == FrameOutcome::kIgnored);
}

TEST_CASE("Mise à jour : une compétence lancée AU SOL se pose sur son lanceur", "[net][world]") {
  // Le serveur l'annonce à part de tout le reste : ce n'est ni un coup, ni une
  // unité au sol, ni un effet porté par quelqu'un. Il ne dit que la
  // compétence, son niveau et la case — c'est à qui dessine d'en tirer ce qui
  // se montre, et là où ça se montre.
  ro::sim::World world;
  ro::sim::Actor caster;
  caster.id = 42;
  world.Spawn(caster);

  ro::proto::ZcNotifyGroundskill placed;
  placed.SKID = 27;  // AL_WARP
  placed.AID = 42;
  placed.level = 4;
  placed.xPos = 151;
  placed.yPos = 88;
  CHECK(ApplyFrame(FrameOf(Encode(placed)), world, 900) == FrameOutcome::kApplied);

  const ro::sim::Actor* found = world.Find(42);
  REQUIRE(found != nullptr);
  REQUIRE(found->ground_skill.has_value());
  CHECK(found->ground_skill->skill_id == 27);
  CHECK(found->ground_skill->level == 4);
  CHECK(found->ground_skill->cell.x == 151);
  CHECK(found->ground_skill->cell.y == 88);
  CHECK(found->ground_skill->started_ms == 900);

  // 🔴 UN LANCEUR QU'ON NE VOIT PAS N'ÉTEINT PAS LE PAQUET : la case se montre
  // sans lui, et la compétence attend au journal qu'on la joue.
  ro::proto::ZcNotifyGroundskill elsewhere_unit;
  elsewhere_unit.SKID = 27;
  elsewhere_unit.AID = 7777;
  elsewhere_unit.xPos = 30;
  elsewhere_unit.yPos = 40;
  CHECK(ApplyFrame(FrameOf(Encode(elsewhere_unit)), world, 1000) == FrameOutcome::kApplied);
  ro::sim::UnseenGroundSkillCursor cursor;
  const auto unseen = cursor.Take(world.unseen_ground_skills());
  REQUIRE(unseen.size() == 1);
  CHECK(unseen.front().caster_id == 7777);
  CHECK(unseen.front().use.cell.x == 30);
  CHECK(unseen.front().use.cell.y == 40);
  CHECK(world.Find(7777) == nullptr);
}

TEST_CASE("Mise à jour : une compétence AU SOL n'annonce pas une créature", "[net][world]") {
  // Le gestionnaire du paquet ne demande la bannière du nom que sur le chemin
  // d'un joueur ; celui d'une créature ne la demande pas.
  constexpr std::uint16_t kCreatureJob = 1002;
  constexpr std::uint16_t kPlayerJob = 4;
  constexpr std::uint16_t kSkill = 27;
  ro::sim::World world;
  for (const auto& [id, job] : {std::pair<std::uint32_t, std::uint16_t>{42, kCreatureJob},
                                std::pair<std::uint32_t, std::uint16_t>{43, kPlayerJob}}) {
    ro::sim::Actor caster;
    caster.id = id;
    caster.look.job = job;
    world.Spawn(caster);
    ro::proto::ZcNotifyGroundskill placed;
    placed.SKID = kSkill;
    placed.AID = id;
    CHECK(ApplyFrame(FrameOf(Encode(placed)), world, 900) == FrameOutcome::kApplied);
  }
  CHECK(world.Find(42)->announced_skill_id == 0);
  CHECK(world.Find(43)->announced_skill_id == kSkill);
}

TEST_CASE("Mise à jour : une INCANTATION n'annonce pas une créature", "[net][world]") {
  // Follower Summon n'envoie que ce paquet : un monstre qui appelle sa suite ne
  // montre aucun nom.
  constexpr std::uint16_t kCreatureJob = 1002;
  constexpr std::uint16_t kPlayerJob = 4;
  constexpr std::uint16_t kFollowerSummon = 196;
  constexpr std::uint16_t kBash = 5;
  ro::sim::World world;
  for (const auto& [id, job] : {std::pair<std::uint32_t, std::uint16_t>{42, kCreatureJob},
                                std::pair<std::uint32_t, std::uint16_t>{43, kPlayerJob},
                                std::pair<std::uint32_t, std::uint16_t>{44, kPlayerJob}}) {
    ro::sim::Actor caster;
    caster.id = id;
    caster.look.job = job;
    world.Spawn(caster);
  }
  ro::proto::ZcUseskillAck summon;
  summon.srcId = 42;
  summon.skillId = kFollowerSummon;
  CHECK(ApplyFrame(FrameOf(Encode(summon)), world, 900) == FrameOutcome::kApplied);
  ro::proto::ZcUseskillAck bash;
  bash.srcId = 43;
  bash.skillId = kBash;
  CHECK(ApplyFrame(FrameOf(Encode(bash)), world, 900) == FrameOutcome::kApplied);
  // Un paquet jetable se tait, même lancé par un joueur.
  bash.srcId = 44;
  bash.disposable = 1;
  CHECK(ApplyFrame(FrameOf(Encode(bash)), world, 900) == FrameOutcome::kApplied);
  CHECK(world.Find(42)->announced_skill_id == 0);
  CHECK(world.Find(43)->announced_skill_id == kBash);
  CHECK(world.Find(44)->announced_skill_id == 0);
}

TEST_CASE("Mise à jour : un refus se consigne avec tout ce que le paquet en dit", "[net][world]") {
  // La phrase s'écrit dans `app`, qui a le nom affiché de la compétence et
  // celui de l'objet : le monde n'en garde que le fait. Pour un objet manquant,
  // le paquet porte l'objet ET la quantité — un Medicine Bowl pour Prepare
  // Potion —, et les perdre réduisait la ligne à « cause 71 ».
  ro::sim::World world;
  constexpr std::uint16_t kPreparePotion = 228;
  constexpr std::uint32_t kMedicineBowl = 7134;
  constexpr std::uint8_t kNeedItem = 71;

  ro::proto::ZcAckTouseskill refus;
  refus.skillId = kPreparePotion;
  refus.btype = 1;
  refus.itemId = kMedicineBowl;
  refus.flag = 0;
  refus.cause = kNeedItem;
  CHECK(ApplyFrame(FrameOf(Encode(refus)), world, 100) == FrameOutcome::kApplied);
  REQUIRE(world.skill_book().refusals().size() == 1);
  const ro::sim::SkillRefusal& kept = world.skill_book().refusals()[0];
  CHECK(kept.seq == 1);
  CHECK(kept.skill_id == kPreparePotion);
  CHECK(kept.cause == kNeedItem);
  CHECK(kept.count == 1);
  CHECK(kept.item_id == kMedicineBowl);
  CHECK(kept.refused_ms == 100);
  CHECK(world.skill_book().last_refusal_seq() == 1);
  // ⚠ RIEN DANS LA CONVERSATION À CET ÉTAGE : la ligne y entrerait deux fois.
  CHECK(world.chat_lines().empty());

  // Le MÊME paquet annonce aussi les compétences qui partent : seul l'échec se
  // consigne, sans quoi chaque sort lancé écrirait une ligne.
  ro::proto::ZcAckTouseskill reussite;
  reussite.skillId = 28;
  reussite.flag = 1;
  CHECK(ApplyFrame(FrameOf(Encode(reussite)), world, 300) == FrameOutcome::kIgnored);
  CHECK(world.skill_book().refusals().size() == 1);
}

TEST_CASE("Grimoire : les refus gardés s'écrêtent, et leur rang ne recule jamais", "[net][world]") {
  ro::sim::SkillBook book;
  for (std::size_t sent = 0; sent < ro::sim::kSkillRefusalsKept + 3; ++sent) {
    book.RecordRefusal(ro::sim::SkillRefusal{.skill_id = 28});
  }
  CHECK(book.refusals().size() == ro::sim::kSkillRefusalsKept);
  CHECK(book.last_refusal_seq() == ro::sim::kSkillRefusalsKept + 3);
  CHECK(book.refusals().back().seq == book.last_refusal_seq());
  // Vider le grimoire au saut de carte ne rend pas un rang déjà donné.
  book.Clear();
  book.RecordRefusal(ro::sim::SkillRefusal{.skill_id = 28});
  CHECK(book.last_refusal_seq() == ro::sim::kSkillRefusalsKept + 4);
}

TEST_CASE("Mise à jour : traverser une carte rend sa taille au joueur", "[net][world]") {
  // ⚠ LE SERVEUR N'ENVOIE AUCUN EFFET POUR REVENIR À LA TAILLE NORMALE.
  // `@size 0` téléporte le joueur SUR PLACE — `pc_setpos` avec `CLR_TELEPORT`,
  // suivi de `clif_changemap` même quand la carte ne change pas — et compte sur
  // le client pour détruire son acteur. C'est la contrepartie exacte de
  // l'ADR 0024 : un drapeau d'apparence ne se baissant jamais, seul un acteur
  // recréé revient à lui-même.
  ro::sim::World world;
  world.set_player_id(2'000'001);
  ro::sim::Actor self;
  self.id = 2'000'001;
  self.kind = ro::sim::ActorKind::kPlayer;
  world.Spawn(self);

  const std::vector<std::uint8_t> rapetisse = {0xF3, 0x01, 0x81, 0x84, 0x1E,
                                               0x00, 0xA4, 0x01, 0x00, 0x00};
  CHECK(ApplyFrame(FrameOf(rapetisse), world, 1000) == FrameOutcome::kApplied);
  REQUIRE(EffectOf(world.player(), 420) != nullptr);

  // Le paquet du changement de carte, celui que la téléportation sur place
  // envoie quand même : nom de carte sur seize octets, puis les deux cases.
  std::vector<std::uint8_t> traverse(22, 0);
  traverse[0] = 0x91;
  traverse[1] = 0x00;
  const std::string carte = "prontera.gat";
  std::copy(carte.begin(), carte.end(), traverse.begin() + 2);
  traverse[18] = 53;
  traverse[20] = 111;
  CHECK(ApplyFrame(FrameOf(traverse), world, 2000) == FrameOutcome::kApplied);

  REQUIRE(world.player() != nullptr);
  CHECK(world.player()->effects.empty());
}

TEST_CASE("Mise à jour : l'effet de taille se note comme les autres", "[net][world]") {
  // Le protocole n'a pas de champ de taille : le serveur envoie un EFFET, et le
  // redit à chaque apparition. `net` n'en déduit rien — il note le fait, et la
  // table du client dit ce qu'il montre.
  ro::sim::World world;
  ro::sim::Actor actor;
  actor.id = 7;
  actor.kind = ro::sim::ActorKind::kMonster;
  world.Spawn(actor);

  const std::vector<std::uint8_t> grandit = {0xF3, 0x01, 0x07, 0x00, 0x00,
                                             0x00, 0xA7, 0x01, 0x00, 0x00};
  CHECK(ApplyFrame(FrameOf(grandit), world, 1000) == FrameOutcome::kApplied);
  REQUIRE(world.Find(7) != nullptr);
  REQUIRE(EffectOf(world.Find(7), 423) != nullptr);
  CHECK(EffectOf(world.Find(7), 423)->since_ms == 1000);

  // ⚠ LE REDIRE RELANCE L'EFFET, et c'est voulu. Le client construit une
  // instance neuve à chaque annonce ; c'est le NUMÉRO qui distingue les deux
  // cas, pas le monde. 423 est la variante réémise à chaque apparition et pose
  // la taille d'un coup, 422 vient du changement et y va par une courbe : une
  // annonce répétée ne fait donc rien voir de nouveau.
  CHECK(ApplyFrame(FrameOf(grandit), world, 5000) == FrameOutcome::kApplied);
  CHECK(EffectOf(world.Find(7), 423)->since_ms == 5000);

  // Un effet qui ne dit rien de la taille se note pareillement.
  const std::vector<std::uint8_t> other = {0xF3, 0x01, 0x07, 0x00, 0x00,
                                           0x00, 0x01, 0x00, 0x00, 0x00};
  CHECK(ApplyFrame(FrameOf(other), world, 6000) == FrameOutcome::kApplied);
  REQUIRE(EffectOf(world.Find(7), 1) != nullptr);
  CHECK(EffectOf(world.Find(7), 1)->since_ms == 6000);

  // Personne à cet identifiant : rien à noter.
  const std::vector<std::uint8_t> elsewhere_unit = {0xF3, 0x01, 0x63, 0x00, 0x00,
                                                    0x00, 0x59, 0x00, 0x00, 0x00};
  CHECK(ApplyFrame(FrameOf(elsewhere_unit), world, 7000) == FrameOutcome::kIgnored);
}

TEST_CASE("Mise à jour : un effet peut porter un NOMBRE", "[net][world]") {
  // Le paquet 0x0284 : l'acteur, l'effet, et une valeur sur trente-deux bits
  // signés. Le monde garde les trois sans trier — savoir quels numéros font
  // paraître un nombre est une table du client.
  ro::sim::World world;
  ro::sim::Actor actor;
  actor.id = 42;
  world.Spawn(actor);

  const std::vector<std::uint8_t> soigne = {0x84, 0x02, 0x2A, 0x00, 0x00, 0x00, 0x91,
                                            0x02, 0x00, 0x00, 0x2C, 0x01, 0x00, 0x00};
  CHECK(ApplyFrame(FrameOf(soigne), world, 3000) == FrameOutcome::kApplied);
  const ro::sim::Actor* target_actor = world.Find(42);
  REQUIRE(target_actor != nullptr);
  const ro::sim::PlayedEffect* heal_effect = EffectOf(target_actor, 657);
  REQUIRE(heal_effect != nullptr);
  CHECK(heal_effect->since_ms == 3000);
  REQUIRE(heal_effect->value.has_value());
  CHECK(*heal_effect->value == 300);

  // Un effet SANS valeur s'ajoute sans toucher à celui qui en portait une :
  // la valeur appartient à SON effet, pas à l'acteur.
  const std::vector<std::uint8_t> nu = {0xF3, 0x01, 0x2A, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
  CHECK(ApplyFrame(FrameOf(nu), world, 3500) == FrameOutcome::kApplied);
  REQUIRE(EffectOf(world.Find(42), 1) != nullptr);
  CHECK_FALSE(EffectOf(world.Find(42), 1)->value.has_value());
  REQUIRE(EffectOf(world.Find(42), 657) != nullptr);
  CHECK(*EffectOf(world.Find(42), 657)->value == 300);

  // Personne à cet identifiant : rien à noter, et personne n'est inventé.
  const std::vector<std::uint8_t> elsewhere_unit = {0x84, 0x02, 0x63, 0x00, 0x00, 0x00, 0x91,
                                                    0x02, 0x00, 0x00, 0x2C, 0x01, 0x00, 0x00};
  CHECK(ApplyFrame(FrameOf(elsewhere_unit), world, 4000) == FrameOutcome::kIgnored);
  CHECK(world.Find(99) == nullptr);
}

TEST_CASE("Mise à jour : un effet se retire par son numéro", "[net][world]") {
  // Le retrait NOMME son effet, et un acteur en porte plusieurs : celui qu'un
  // autre a recouvert se retire quand même, et lui seul.
  ro::sim::World world;
  ro::sim::Actor actor;
  actor.id = 42;
  world.Spawn(actor);

  const std::vector<std::uint8_t> pose = {0xF3, 0x01, 0x2A, 0x00, 0x00,
                                          0x00, 0x64, 0x00, 0x00, 0x00};
  REQUIRE(ApplyFrame(FrameOf(pose), world, 1000) == FrameOutcome::kApplied);

  ro::proto::ZcRemoveEffect other;
  other.aid = 42;
  other.effectId = 101;
  CHECK(ApplyFrame(FrameOf(Encode(other)), world, 1100) == FrameOutcome::kIgnored);
  CHECK(EffectOf(world.Find(42), 100) != nullptr);

  ro::proto::ZcRemoveEffect retire;
  retire.aid = 42;
  retire.effectId = 100;
  CHECK(ApplyFrame(FrameOf(Encode(retire)), world, 1200) == FrameOutcome::kApplied);
  CHECK(EffectOf(world.Find(42), 100) == nullptr);

  // Deux fois de suite : le second n'a plus rien à retirer.
  CHECK(ApplyFrame(FrameOf(Encode(retire)), world, 1300) == FrameOutcome::kIgnored);

  // Personne à cet identifiant.
  ro::proto::ZcRemoveEffect unknown;
  unknown.aid = 99;
  unknown.effectId = 100;
  CHECK(ApplyFrame(FrameOf(Encode(unknown)), world, 1400) == FrameOutcome::kIgnored);
}

TEST_CASE("Mise à jour : la portée d'un sort se centre sur la case VISÉE", "[net][world]") {
  // Chez Moonlight, le centre est la case visée et non le lanceur : kRO ne
  // posait ShowScale que sur des sorts visés sur soi, où les deux coïncident.
  // La portée vit sur son LANCEUR, comme l'incantation qu'elle accompagne.
  ro::sim::World world;
  ro::sim::Actor caster;
  caster.id = 100;
  caster.position = ro::sim::CellPosition{150, 100, 0};
  world.Spawn(caster);

  ro::proto::ZcSkillScale scale;
  scale.AID = 100;
  scale.skill_id = 89;
  scale.skill_lv = 10;
  scale.x = 160;
  scale.y = 112;
  scale.casttime = 2500;
  CHECK(ApplyFrame(FrameOf(Encode(scale)), world, 700) == FrameOutcome::kApplied);

  const ro::sim::Actor* caster_actor = world.Find(100);
  REQUIRE(caster_actor != nullptr);
  REQUIRE(caster_actor->skill_scale.has_value());
  CHECK(caster_actor->skill_scale->skill_id == 89);
  CHECK(caster_actor->skill_scale->level == 10);
  CHECK(caster_actor->skill_scale->centre.x == 160);
  CHECK(caster_actor->skill_scale->centre.y == 112);
  CHECK(caster_actor->skill_scale->started_ms == 700);
  CHECK(caster_actor->skill_scale->duration_ms == 2500);

  // Elle SURVIT à une réannonce du lanceur : le serveur le réannonce dès qu'il
  // se met en marche, et la perdre effacerait le carré en plein sort.
  ro::sim::Actor still_there;
  still_there.id = 100;
  still_there.position = ro::sim::CellPosition{151, 100, 0};
  world.Spawn(still_there);
  REQUIRE(world.Find(100)->skill_scale.has_value());
  CHECK(world.Find(100)->skill_scale->centre.x == 160);

  // Personne à cet identifiant.
  ro::proto::ZcSkillScale elsewhere_unit;
  elsewhere_unit.AID = 999;
  CHECK(ApplyFrame(FrameOf(Encode(elsewhere_unit)), world, 800) == FrameOutcome::kIgnored);
}

TEST_CASE("Mise à jour : l'effet DIVERS dit son type, pas son effet", "[net][world]") {
  // 🔴 LE CHAMP S'APPELLE `effectId` ET N'EN EST PAS UN. ZC_NOTIFY_EFFECT porte
  // un rang de `e_notify_effect` — montée de niveau, raffinement, fabrication —,
  // que le client traduit par un switch fixe. Le poser tel quel montrerait
  // l'effet 0, qui est un impact de coup, à chaque montée de niveau.
  ro::sim::World world;
  ro::sim::Actor player;
  player.id = 7;
  world.Spawn(player);

  ro::proto::ZcNotifyEffect level_up;
  level_up.aid = 7;
  level_up.effectId = 0;  // NOTIFYEFFECT_BASE_LEVEL_UP
  CHECK(ApplyFrame(FrameOf(Encode(level_up)), world, 900) == FrameOutcome::kApplied);
  CHECK(EffectOf(world.Find(7), 371) != nullptr);
  CHECK(EffectOf(world.Find(7), 0) == nullptr);

  ro::proto::ZcNotifyEffect job_up;
  job_up.aid = 7;
  job_up.effectId = 1;  // NOTIFYEFFECT_JOB_LEVEL_UP
  CHECK(ApplyFrame(FrameOf(Encode(job_up)), world, 1000) == FrameOutcome::kApplied);
  CHECK(EffectOf(world.Find(7), 158) != nullptr);

  // ⚠ LA FIN DE PARTIE NE MONTRE RIEN chez le client non plus : elle y lève un
  // drapeau. Lui donner un effet inventerait une image que le natif n'a pas.
  ro::proto::ZcNotifyEffect game_over;
  game_over.aid = 7;
  game_over.effectId = 4;  // NOTIFYEFFECT_GAME_OVER
  CHECK(ApplyFrame(FrameOf(Encode(game_over)), world, 1100) == FrameOutcome::kIgnored);

  // Sur quelqu'un qu'on ne voit pas, rien à noter.
  ro::proto::ZcNotifyEffect elsewhere_unit;
  elsewhere_unit.aid = 4242;
  elsewhere_unit.effectId = 0;
  CHECK(ApplyFrame(FrameOf(Encode(elsewhere_unit)), world, 1200) == FrameOutcome::kIgnored);
}

TEST_CASE("Mise à jour : l'effet divers ne touche pas le joueur", "[net][world]") {
  // ⚠ LE CLIENT NE CHERCHE CET ACTEUR QUE PARMI LES AUTRES. Sa propre montée
  // de niveau se montre par le paramètre qui la précède : la semer ici aussi
  // la doublerait.
  ro::sim::World world;
  ro::sim::Actor player;
  player.id = 7;
  world.Spawn(player);
  world.set_player_id(7);

  ro::proto::ZcNotifyEffect level_up;
  level_up.aid = 7;
  level_up.effectId = 0;  // NOTIFYEFFECT_BASE_LEVEL_UP
  CHECK(ApplyFrame(FrameOf(Encode(level_up)), world, 900) == FrameOutcome::kIgnored);
  CHECK(EffectOf(world.Find(7), 371) == nullptr);
}

TEST_CASE("Mise à jour : les dix types d'effet divers sont ceux du client", "[net][world]") {
  // Les neuf qui montrent quelque chose, tels que le client les associe — et
  // le dixième, qui n'en montre aucun.
  CHECK(ro::sim::MiscEffectId(0) == 371);
  CHECK(ro::sim::MiscEffectId(1) == 158);
  CHECK(ro::sim::MiscEffectId(2) == 155);
  CHECK(ro::sim::MiscEffectId(3) == 154);
  CHECK(ro::sim::MiscEffectId(4) == 0);
  CHECK(ro::sim::MiscEffectId(5) == 305);
  CHECK(ro::sim::MiscEffectId(6) == 306);
  CHECK(ro::sim::MiscEffectId(7) == 338);
  CHECK(ro::sim::MiscEffectId(8) == 337);
  CHECK(ro::sim::MiscEffectId(9) == 582);
  // Au-delà des dix, le client tombe au défaut de son switch et ne fait rien.
  CHECK(ro::sim::MiscEffectId(10) == 0);
  CHECK(ro::sim::MiscEffectId(9999) == 0);
}

TEST_CASE("Mise à jour : le délai de combo passe par l'aiguillage général", "[net][world]") {
  // ⚠ UN CAS DE FAMILLE N'EST RIEN SANS SON AIGUILLAGE : l'essai passe par
  // l'entrée générale, et non par la fonction de la famille.
  constexpr std::uint32_t kMonk = 5;
  constexpr std::uint32_t kComboDelayMs = 600;
  constexpr std::uint32_t kNowMs = 4200;
  ro::sim::World world;
  ro::sim::Actor monk;
  monk.id = kMonk;
  world.Spawn(monk);
  ro::proto::ZcCombodelay combo;
  combo.AID = kMonk;
  combo.delay = kComboDelayMs;
  CHECK(ApplyFrame(FrameOf(Encode(combo)), world, kNowMs) == FrameOutcome::kApplied);
  const ro::sim::Actor* found = world.Find(kMonk);
  REQUIRE(found != nullptr);
  CHECK(found->combo_delay_ms == kComboDelayMs);
  CHECK(found->combo_delay_since_ms == kNowMs);
}
