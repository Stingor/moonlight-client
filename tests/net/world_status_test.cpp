// Ce que le serveur dit des NOMBRES du personnage : vitesse, bourse, feuille.
//
// Separes de `world_update_test.cpp` parce qu'ils ne parlent pas des acteurs
// mais du joueur lui-meme, et parce que ce fichier-la avait atteint son plafond.

#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_update.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/movement.h"
#include "ro/sim/player_status.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::net::ApplyFrame;
using ro::net::FrameOutcome;
using ro::sim::World;
using ro::test_support::FrameOf;

TEST_CASE("Traduction : la vitesse qui change laisse le joueur où il en est", "[net][update]") {
  World world;
  world.set_player_id(2'000'001);
  ro::sim::Actor self;
  self.id = 2'000'001;
  self.speed_ms_per_cell = 150;
  world.Spawn(std::move(self));
  REQUIRE(world.StartMoving(2'000'001,
                            ro::sim::CellMovement{ro::sim::CellPosition{100, 100, 0},
                                                  ro::sim::CellPosition{110, 100, 0}, 8, 8},
                            10'000));

  ro::proto::ZcParChange speed;
  speed.varID = 0;  // SP_SPEED
  speed.count = 300;
  REQUIRE(ApplyFrame(FrameOf(Encode(speed)), world, 12'000) == FrameOutcome::kApplied);

  REQUIRE(world.player() != nullptr);
  CHECK(world.player()->speed_ms_per_cell == 300);
  CHECK_FALSE(ro::sim::IsWalking(*world.player(), 12'000));
  CHECK(ro::sim::InterpolatedCell(*world.player(), 12'000).x == 110.0F);
}

TEST_CASE("Traduction : la vitesse du joueur arrive par un paramètre", "[net][update]") {
  // Aucun paquet d'unité n'annonce le joueur à lui-même : sa vitesse de marche
  // n'arrive que par SP_SPEED, et sans elle il se téléporte.
  World world;
  ro::proto::ZcParChange speed;
  speed.varID = 0;  // SP_SPEED
  speed.count = 140;
  // ⚠ IGNORÉE, ET NON « NON TRAITÉE » : le paramètre est reconnu, il n'y a
  // simplement personne à qui l'appliquer. Le ranger dans le travail restant
  // ferait croire à un paquet qu'on ne porte pas.
  CHECK(ApplyFrame(FrameOf(Encode(speed)), world, 0) == FrameOutcome::kIgnored);

  world.set_player_id(2'000'001);
  ro::sim::Actor self;
  self.id = 2'000'001;
  world.Spawn(std::move(self));
  CHECK(ApplyFrame(FrameOf(Encode(speed)), world, 0) == FrameOutcome::kApplied);
  REQUIRE(world.player() != nullptr);
  CHECK(world.player()->speed_ms_per_cell == 140);

  // Les autres paramètres vont au PERSONNAGE, et non à l'acteur.
  ro::proto::ZcParChange zeny;
  zeny.varID = 20;  // SP_ZENY
  zeny.count = 5'000;
  CHECK(ApplyFrame(FrameOf(Encode(zeny)), world, 0) == FrameOutcome::kApplied);
  CHECK(world.player_status().zeny == 5'000);
  CHECK(world.player()->speed_ms_per_cell == 140);

  // Et un numéro que personne n'a classé reste VISIBLE dans le relevé : c'est
  // la seule liste qui dise ce qu'il reste à porter.
  ro::proto::ZcParChange unknown;
  unknown.varID = 999;
  unknown.count = 1;
  CHECK(ApplyFrame(FrameOf(Encode(unknown)), world, 0) == FrameOutcome::kUnhandled);
}

TEST_CASE("Traduction : la bourse arrive par un TROISIEME paquet de paramètre", "[net][update]") {
  // 🔴 CE PAQUET NE SE DISTINGUE DES DEUX AUTRES QUE PAR SON OPCODE. Même taille,
  // mêmes deux champs, mêmes identifiants de paramètre — et c'est par lui que
  // passe SP_ZENY (`clif_longpar_change`, src/map/clif.cpp). L'oublier ne
  // produisait aucune erreur : la bourse gardait simplement sa sentinelle « pas
  // encore dit », et le pied du sac affichait « -1z ».
  World world;
  world.set_player_id(2'000'002);
  ro::sim::Actor self;
  self.id = 2'000'002;
  world.Spawn(std::move(self));

  ro::proto::ZcLongparChange purse;
  purse.varID = 20;  // SP_ZENY
  purse.amount = 10'900'000;
  CHECK(ApplyFrame(FrameOf(Encode(purse)), world, 0) == FrameOutcome::kApplied);
  CHECK(world.player_status().zeny == 10'900'000);

  // Et il range dans la MÊME table que les deux autres : un identifiant que
  // personne n'a classé y reste visible.
  ro::proto::ZcLongparChange unknown;
  unknown.varID = 999;
  unknown.amount = 1;
  CHECK(ApplyFrame(FrameOf(Encode(unknown)), world, 0) == FrameOutcome::kUnhandled);
}

TEST_CASE("Traduction : la feuille de statut porte AUSSI les six caracteristiques",
          "[net][update][fiche]") {
  // 🔴 ELLES ARRIVAIENT ET N'ETAIENT PAS LUES. Dix-huit champs sur vingt-quatre
  // etaient ranges ; les six qui manquaient sont celles que la fiche montre en
  // premier, et c'est la seule trame qui les porte toutes d'un coup.
  World world;
  world.set_player_id(2'000'001);

  ro::proto::ZcStatus sheet;
  sheet.point = 42;
  sheet.str = 99;
  sheet.standardStr = 16;
  sheet.agi = 1;
  sheet.standardAgi = 2;
  sheet.vit = 70;
  sheet.standardVit = 11;
  sheet.int_ = 30;
  sheet.standardInt = 5;
  sheet.dex = 88;
  sheet.standardDex = 14;
  sheet.luk = 7;
  sheet.standardLuk = 2;
  sheet.ASPD = 165;
  CHECK(ApplyFrame(FrameOf(Encode(sheet)), world, 0) == FrameOutcome::kApplied);

  const auto& status = world.player_status();
  CHECK(status.str == 99);
  CHECK(status.agi == 1);
  CHECK(status.vit == 70);
  CHECK(status.int_ == 30);
  CHECK(status.dex == 88);
  CHECK(status.luk == 7);

  // ⚠ LA VALEUR ET SON COUT NE SE CONFONDENT PAS, et ils voyagent cote a cote
  // dans la trame : un decalage d'un champ montrerait le prix du point suivant a
  // la place de la caracteristique.
  CHECK(status.str_cost == 16);
  CHECK(status.dex_cost == 14);
  CHECK(status.status_point == 42);

  // Et le delai de motion reste brut : c'est `DisplayedAspd` qui le retourne.
  CHECK(status.amotion == 165);
}

TEST_CASE("Traduction : l'accuse d'une montee PORTE la nouvelle valeur", "[net][update][fiche]") {
  // 🔴 CE N'EST PAS UN SIMPLE ACCUSE DE RECEPTION. pc_statusup n'envoie
  // clif_updatestatus(type) QUE si la valeur depasse 255, pour corriger la
  // troncature ; en dessous -- c'est-a-dire toujours -- la caracteristique
  // montee ne voyage QUE dans ce paquet. L'ignorer laisserait le bouton « + »
  // sans effet visible, alors que le serveur aurait bien agi.
  World world;
  world.set_player_id(2'000'003);

  ro::proto::ZcStatusChangeAck accepted;
  accepted.sp = 13;  // SP_STR
  accepted.ok = 1;
  accepted.value = 41;
  CHECK(ApplyFrame(FrameOf(Encode(accepted)), world, 0) == FrameOutcome::kApplied);
  CHECK(world.player_status().str == 41);

  // ⚠ ET LE REFUS NE TOUCHE A RIEN. Le serveur refuse pour trois raisons et n'en
  // nomme aucune ; avancer la valeur montrerait une force que le monde ne
  // partage pas.
  ro::proto::ZcStatusChangeAck refused;
  refused.sp = 13;
  refused.ok = 0;
  refused.value = 99;
  CHECK(ApplyFrame(FrameOf(Encode(refused)), world, 0) == FrameOutcome::kApplied);
  CHECK(world.player_status().str == 41);
}

TEST_CASE("Traduction : le prix du point suivant arrive par ZC_STATUS_CHANGE",
          "[net][update][fiche]") {
  // 🔴 APRES UNE MONTEE OU UNE REMISE A ZERO, le serveur redit le cout par
  // clif_updatestatus(SP_USTR...), qui n'emprunte QUE ce paquet. Perdu, le cout
  // restait nul, et « Max » et « + » restaient eteints malgre des milliers de
  // points.
  World world;
  world.set_player_id(2'000'005);

  ro::proto::ZcStatusChange cost;
  cost.statusID = 32;  // SP_USTR
  cost.value = 2;
  CHECK(ApplyFrame(FrameOf(Encode(cost)), world, 0) == FrameOutcome::kApplied);
  CHECK(world.player_status().str_cost == 2);
}

TEST_CASE("Traduction : la paire des MATK est dans l'autre sens que ses voisines",
          "[net][update][fiche]") {
  // 🔴 LE SERVEUR INVERSE CELLE-LA ET ELLE SEULE. SP_ATK1, SP_DEF1 et SP_MDEF1
  // portent tous le cote GAUCHE ; SP_MATK1 porte le cote DROIT, donc le MAXIMUM
  // (clif_updatestatus, src/map/clif.cpp). Les noms du paquet, eux, sont
  // honnetes : clif_initialstatus ecrit max_mattPower = pc_rightside_matk.
  //
  // Ranger ces deux champs dans l'ordre de leur nom mettait le maximum sous le
  // minimum, et la fiche affichait un intervalle a l'envers -- que rien ne
  // signalait, parce que les deux nombres restent plausibles.
  World world;
  world.set_player_id(2'000'004);

  ro::proto::ZcStatus sheet;
  sheet.min_mattPower = 120;  // pc_leftside_matk
  sheet.max_mattPower = 480;  // pc_rightside_matk
  // Les trois voisines, pour montrer que c'est bien la SEULE inversee.
  sheet.attPower = 100;      // pc_leftside_atk  -> SP_ATK1
  sheet.refiningPower = 40;  // pc_rightside_atk -> SP_ATK2
  sheet.itemdefPower = 10;   // pc_leftside_def  -> SP_DEF1
  sheet.plusdefPower = 55;   // pc_rightside_def -> SP_DEF2
  CHECK(ApplyFrame(FrameOf(Encode(sheet)), world, 0) == FrameOutcome::kApplied);

  const auto& status = world.player_status();
  CHECK(status.matk1 == 480);
  CHECK(status.matk2 == 120);
  CHECK(status.matk1 > status.matk2);

  CHECK(status.atk1 == 100);
  CHECK(status.atk2 == 40);
  CHECK(status.def1 == 10);
  CHECK(status.def2 == 55);
}

TEST_CASE("Traduction : le niveau n'arrive JAMAIS du serveur de carte", "[net][update][fiche]") {
  // 🔴 CE QUE CET ESSAI TIENT EST UNE ABSENCE. clif_updatestatus(SP_BASELEVEL)
  // n'est appele qu'a la MONTEE (pc_baselevelup) : ni la feuille de statut ni
  // l'entree en carte ne le portent. Le niveau vient de la liste des
  // personnages, et c'est app qui l'y prend.
  //
  // Sans cela la fiche affichait « Nv 0 / 0 » jusqu'au premier niveau gagne, et
  // le zero passait pour un defaut d'affichage.
  World world;
  world.set_player_id(2'000'005);

  ro::proto::ZcStatus sheet;
  sheet.point = 7;
  CHECK(ApplyFrame(FrameOf(Encode(sheet)), world, 0) == FrameOutcome::kApplied);
  CHECK(world.player_status().status_point == 7);
  CHECK(world.player_status().base_level == ro::sim::kUnknownStat);
  CHECK(world.player_status().job_level == ro::sim::kUnknownStat);

  // Et quand il arrive -- a la montee -- c'est par la porte ordinaire.
  ro::proto::ZcParChange levelled;
  levelled.varID = 11;  // SP_BASELEVEL
  levelled.count = 42;
  CHECK(ApplyFrame(FrameOf(Encode(levelled)), world, 0) == FrameOutcome::kApplied);
  CHECK(world.player_status().base_level == 42);
}
