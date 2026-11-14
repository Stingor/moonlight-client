#include "ro/sim/world.h"

#include <array>
#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/sim/movement.h"
#include "ro/sim/skill_banner.h"

using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::CellMovement;
using ro::sim::CellPosition;
using ro::sim::DirectionTowards;
using ro::sim::World;

namespace {

Actor Someone(std::uint32_t id, CellPosition where) {
  Actor actor;
  actor.id = id;
  actor.kind = ActorKind::kPlayer;
  actor.position = where;
  actor.name = "quelqu'un";
  return actor;
}

}  // namespace

TEST_CASE("Monde : un acteur qui entre se retrouve", "[sim][world]") {
  World world;
  world.Spawn(Someone(150'001, CellPosition{150, 100, 4}));

  const Actor* found = world.Find(150'001);
  REQUIRE(found != nullptr);
  CHECK(found->position.x == 150);
  CHECK(found->position.direction == 4);
  CHECK(world.size() == 1);
  CHECK(world.Find(999) == nullptr);
}

TEST_CASE("Monde : réannoncer un acteur REMPLACE son état", "[sim][world]") {
  // Le serveur réannonce quelqu'un qui rentre dans le champ de vision. Refuser
  // le doublon garderait l'apparence d'avant, donc l'équipement d'avant.
  World world;
  world.Spawn(Someone(150'001, CellPosition{150, 100, 4}));

  Actor changed = Someone(150'001, CellPosition{200, 210, 2});
  changed.look.job = 4054;
  world.Spawn(changed);

  const Actor* found = world.Find(150'001);
  REQUIRE(found != nullptr);
  CHECK(found->position.x == 200);
  CHECK(found->look.job == 4054);
  CHECK(world.size() == 1);
}

TEST_CASE("Monde : une réannonce garde la coiffe, l'habit et l'élément des sphères",
          "[sim][world]") {
  constexpr std::uint32_t kPasserby = 170'040;
  constexpr ro::sim::HatEffectId kHat{7};
  constexpr std::int16_t kFireElement = 3;
  constexpr std::int16_t kCharmCount = 2;
  constexpr std::uint32_t kCharmMs = 100;
  World world;
  world.Spawn(Someone(kPasserby, {10, 10}));
  REQUIRE(world.SetHatEffect(kPasserby, kHat, true));
  REQUIRE(world.TellGmOutfit(kPasserby, true));
  REQUIRE(world.RecordSpiritCharm(kPasserby, kFireElement, kCharmCount, kCharmMs));

  world.Spawn(Someone(kPasserby, {11, 10}));

  const Actor* passerby = world.Find(kPasserby);
  REQUIRE(passerby != nullptr);
  CHECK(passerby->hat_effects.contains(kHat));
  CHECK(passerby->gm_outfit);
  CHECK(passerby->spirit_charm_element == kFireElement);
}

TEST_CASE("Monde : un acteur qui sort disparaît, et une seule fois", "[sim][world]") {
  World world;
  world.Spawn(Someone(150'001, CellPosition{150, 100, 0}));

  CHECK(world.Remove(150'001));
  CHECK(world.Find(150'001) == nullptr);
  // Le second retrait ne trouve plus rien, et le dit.
  CHECK_FALSE(world.Remove(150'001));
}

TEST_CASE("Monde : mettre en marche cale la position sur ce que dit le serveur", "[sim][world]") {
  // Le serveur fait autorité : sa position de départ corrige un client qui
  // aurait dérivé, et l'ignorer laisserait l'écart grandir à chaque pas.
  World world;
  world.Spawn(Someone(150'001, CellPosition{150, 100, 0}));

  const CellMovement path{CellPosition{152, 101, 0}, CellPosition{160, 108, 0}, 8, 8};
  REQUIRE(world.StartMoving(150'001, path, 1000));

  const Actor* found = world.Find(150'001);
  REQUIRE(found != nullptr);
  CHECK(found->position.x == 152);
  CHECK(found->position.y == 101);
  REQUIRE(found->movement.has_value());
  CHECK(found->movement->to.x == 160);
}

TEST_CASE("Monde : reposer un acteur ANNULE son déplacement", "[sim][world]") {
  // C'est le sens même d'un arrêt. Garder le déplacement ferait glisser le
  // personnage vers une destination abandonnée.
  World world;
  world.Spawn(Someone(150'001, CellPosition{150, 100, 0}));
  REQUIRE(world.StartMoving(
      150'001, CellMovement{CellPosition{150, 100, 0}, CellPosition{160, 108, 0}, 0, 0}, 0));
  REQUIRE(world.Find(150'001)->movement.has_value());

  REQUIRE(world.PlaceAt(150'001, CellPosition{155, 104, 3}));
  const Actor* found = world.Find(150'001);
  REQUIRE(found != nullptr);
  CHECK_FALSE(found->movement.has_value());
  CHECK(found->position.x == 155);
  CHECK(found->position.direction == 3);
}

TEST_CASE("Monde : agir sur un inconnu ne crée personne", "[sim][world]") {
  // Un paquet peut parler d'un acteur qu'on n'a jamais vu apparaître — il est
  // arrivé avant nous, ou son apparition s'est perdue. L'inventer donnerait un
  // acteur sans apparence ni nom, qu'aucun sprite ne pourrait dessiner.
  World world;
  CHECK_FALSE(world.PlaceAt(42, CellPosition{1, 1, 0}));
  CHECK_FALSE(world.StartMoving(42, CellMovement{}, 0));
  CHECK(world.size() == 0);
}

TEST_CASE("Monde : changer de carte vide tout, sauf QUI on est", "[sim][world]") {
  // Les acteurs de l'ancienne carte n'ont rien à faire sur la nouvelle. Mais
  // l'identifiant du joueur vient de la phase de connexion, pas de la carte :
  // le perdre obligerait à le redemander à chaque porte franchie.
  World world;
  world.set_player_id(150'001);
  world.Spawn(Someone(150'001, CellPosition{150, 100, 0}));
  world.Spawn(Someone(200'002, CellPosition{151, 100, 0}));
  REQUIRE(world.size() == 2);

  world.Clear();
  CHECK(world.size() == 0);
  CHECK(world.player_id() == 150'001);
  // Le joueur lui-même n'est plus là tant que la nouvelle carte ne l'a pas
  // annoncé : son identifiant n'est pas sa présence.
  CHECK(world.player() == nullptr);
}

TEST_CASE("Monde : une enseigne se pose, se retire, et survit à une réannonce", "[sim][world]") {
  World world;
  world.Spawn(Someone(150'001, CellPosition{1, 1, 0}));

  ro::sim::Signboard shop;
  shop.kind = ro::sim::SignboardKind::kVendingShop;
  shop.title = "Potions pas cheres";
  CHECK(world.SetSignboard(150'001, shop));
  REQUIRE(world.Find(150'001)->signboard.has_value());
  CHECK(world.Find(150'001)->signboard->title == "Potions pas cheres");

  // ⚠ ELLE SURVIT À UNE RÉANNONCE, contrairement aux états : le serveur ne la
  // redit qu'à l'entrée en vue, et la perdre serait définitif.
  world.Spawn(Someone(150'001, CellPosition{2, 2, 0}));
  REQUIRE(world.Find(150'001)->signboard.has_value());
  CHECK(world.Find(150'001)->signboard->title == "Potions pas cheres");

  CHECK(world.ClearSignboard(150'001));
  CHECK_FALSE(world.Find(150'001)->signboard.has_value());
  // Retirer deux fois, ou chez un inconnu, n'est pas une panne.
  CHECK_FALSE(world.ClearSignboard(150'001));
  CHECK_FALSE(world.ClearSignboard(999));
  CHECK(world.size() == 1);
}

TEST_CASE("Monde : un salon se ferme par SON identifiant, pas celui de son hôte", "[sim][world]") {
  // ⚠ C'EST LE PIÈGE DE CETTE FAMILLE. Les deux identifiants sont des entiers de
  // même largeur, côte à côte dans le paquet ; les confondre fermerait le salon
  // d'un autre, ou ne fermerait rien.
  World world;
  world.Spawn(Someone(150'001, CellPosition{1, 1, 0}));
  world.Spawn(Someone(150'002, CellPosition{2, 2, 0}));

  ro::sim::Signboard room;
  room.kind = ro::sim::SignboardKind::kChatRoom;
  room.title = "Groupe pour Orcs";
  room.room_id = 77'000;
  room.users = 3;
  room.limit = 12;
  room.status = ro::sim::kRoomPublic;
  REQUIRE(world.SetSignboard(150'001, room));

  // L'identifiant de l'ACTEUR ne ferme aucun salon.
  CHECK_FALSE(world.ClearRoom(150'001));
  CHECK(world.Find(150'001)->signboard.has_value());
  // Celui d'un salon qui n'existe pas non plus.
  CHECK_FALSE(world.ClearRoom(77'001));

  CHECK(world.ClearRoom(77'000));
  CHECK_FALSE(world.Find(150'001)->signboard.has_value());

  // Et une ÉCHOPPE ne se ferme pas comme un salon : elle n'a pas d'identifiant
  // de salon, et zéro n'en est pas un.
  ro::sim::Signboard shop;
  shop.kind = ro::sim::SignboardKind::kVendingShop;
  shop.title = "Cartes rares";
  REQUIRE(world.SetSignboard(150'002, shop));
  CHECK_FALSE(world.ClearRoom(0));
  CHECK(world.Find(150'002)->signboard.has_value());
}

TEST_CASE("Monde : un état se pose, se retire, et ne crée personne", "[sim][world]") {
  World world;
  world.Spawn(Someone(150'001, CellPosition{1, 1, 0}));
  CHECK(world.SetStatusEffect(150'001, ro::sim::kStatusAllRiding, true));
  CHECK(world.Find(150'001)->status_effects.contains(ro::sim::kStatusAllRiding));
  // Poser deux fois ne change rien ; retirer l'efface.
  CHECK(world.SetStatusEffect(150'001, ro::sim::kStatusAllRiding, true));
  CHECK(world.SetStatusEffect(150'001, ro::sim::kStatusAllRiding, false));
  CHECK_FALSE(world.Find(150'001)->status_effects.contains(ro::sim::kStatusAllRiding));
  CHECK_FALSE(world.SetStatusEffect(999, ro::sim::kStatusAllRiding, true));
  CHECK(world.size() == 1);
  // Une réannonce les garde : voir status_since_test.cpp.
}

TEST_CASE("Monde : changer d'apparence ne touche à rien d'autre", "[sim][world]") {
  World world;
  world.Spawn(Someone(150'001, CellPosition{1, 1, 3}));
  REQUIRE(world.SetStatusEffect(150'001, ro::sim::kStatusAllRiding, true));
  ro::sim::Appearance look = world.Find(150'001)->look;
  look.weapon = 1201;
  CHECK(world.SetLook(150'001, look));
  const Actor* found = world.Find(150'001);
  REQUIRE(found != nullptr);
  CHECK(found->look.weapon == 1201);
  CHECK(found->position.direction == 3);
  CHECK(found->status_effects.contains(ro::sim::kStatusAllRiding));
  CHECK_FALSE(world.SetLook(999, look));
  CHECK(world.size() == 1);
}

TEST_CASE("Monde : la posture se pose avec son instant, et la redire ne le change pas",
          "[sim][world]") {
  using ro::sim::Posture;
  World world;
  world.Spawn(Someone(150'001, CellPosition{1, 1, 0}));
  CHECK(world.Find(150'001)->posture == Posture::kStanding);
  CHECK(world.SetPosture(150'001, Posture::kSitting, 1000));
  CHECK(world.Find(150'001)->posture == Posture::kSitting);
  CHECK(world.Find(150'001)->posture_since_ms == 1000);
  // La même posture redite garde son instant : l'animation ne repart pas.
  CHECK(world.SetPosture(150'001, Posture::kSitting, 2000));
  CHECK(world.Find(150'001)->posture_since_ms == 1000);
  CHECK(world.SetPosture(150'001, Posture::kDead, 3000));
  CHECK(world.Find(150'001)->posture == Posture::kDead);
  CHECK(world.Find(150'001)->posture_since_ms == 3000);
  CHECK_FALSE(world.SetPosture(999, Posture::kDead, 0));
  CHECK(world.size() == 1);
}

TEST_CASE("Monde : un coup se note des deux côtés, et tourne qui le porte", "[sim][world]") {
  World world;
  world.Spawn(Someone(150'001, CellPosition{10, 10, 0}));
  world.Spawn(Someone(150'002, CellPosition{13, 10, 0}));
  ro::sim::Strike strike;
  strike.attacker = 150'001;
  strike.target = 150'002;
  strike.started_ms = 500;
  strike.attack_motion_ms = 432;
  strike.hurt_motion_ms = 288;
  strike.flinches = true;
  CHECK(world.RecordStrike(strike));
  REQUIRE(world.Find(150'001)->attack.has_value());
  CHECK(world.Find(150'001)->attack->target == 150'002);
  CHECK_FALSE(world.Find(150'001)->hit.has_value());
  REQUIRE(world.Find(150'002)->hit.has_value());
  CHECK(world.Find(150'002)->hit->started_ms == 500);
  // La cible est à l'est : le frappeur s'y tourne (6 chez le serveur).
  CHECK(world.Find(150'001)->position.direction == 6);
  CHECK(world.Find(150'002)->position.direction == 0);

  // Qui se frappe lui-même ne porte pas de coup.
  ro::sim::Strike self = strike;
  self.attacker = 150'002;
  CHECK(world.RecordStrike(self));
  CHECK_FALSE(world.Find(150'002)->attack.has_value());
  CHECK(world.Find(150'002)->hit->attacker == 150'002);

  // Un coup entre inconnus ne crée personne.
  strike.attacker = 998;
  strike.target = 999;
  CHECK_FALSE(world.RecordStrike(strike));
  CHECK(world.size() == 2);
}

TEST_CASE("Monde : un coup qui fait broncher COUPE la marche de sa cible", "[sim][world]") {
  // Le serveur arrête la marche d'une cible qui bronche et ne l'annonce par
  // AUCUN paquet. Un client qui poursuit la marche annoncée montre le monstre
  // plusieurs cases plus loin, jusqu'à ce que l'annonce suivante le ramène d'un
  // coup : c'est la téléportation qu'on voit en attaquant.
  const CellMovement east{CellPosition{10, 10, 0}, CellPosition{15, 10, 0}, 8, 8};
  const auto walking = [&east](std::uint32_t started_ms) {
    World world;
    Actor mob = Someone(150'002, CellPosition{10, 10, 0});
    mob.speed_ms_per_cell = 200;
    world.Spawn(mob);
    world.Spawn(Someone(150'001, CellPosition{20, 10, 0}));
    REQUIRE(world.StartMoving(150'002, east, started_ms));
    return world;
  };
  ro::sim::Strike strike;
  strike.attacker = 150'001;
  strike.target = 150'002;
  strike.hurt_motion_ms = 288;
  strike.flinches = true;

  SECTION("un pas franchi à moitié se TERMINE") {
    World world = walking(1000);
    // Deux cases faites, la troisième à moitié : le serveur la termine.
    strike.started_ms = 1500;
    REQUIRE(world.RecordStrike(strike));
    const Actor* stopped = world.Find(150'002);
    REQUIRE(stopped != nullptr);
    CHECK_FALSE(stopped->movement.has_value());
    CHECK(stopped->position.x == 13);
    CHECK(stopped->position.y == 10);
    // Le regard reste celui du pas en cours : l'est, six chez le serveur.
    CHECK(stopped->position.direction == 6);
    // Ce qu'il a marché reste au compteur du cycle de marche.
    CHECK(stopped->travelled_before_cells == Catch::Approx(3.0F));
  }

  SECTION("un pas à peine entamé se DÉFAIT") {
    World world = walking(1000);
    strike.started_ms = 1450;
    REQUIRE(world.RecordStrike(strike));
    CHECK(world.Find(150'002)->position.x == 12);
  }

  SECTION("un coup encaissé sans broncher laisse marcher") {
    World world = walking(1000);
    strike.started_ms = 1500;
    strike.flinches = false;
    REQUIRE(world.RecordStrike(strike));
    const Actor* still = world.Find(150'002);
    REQUIRE(still->movement.has_value());
    CHECK(still->position.x == 10);
  }
}

TEST_CASE("Monde : une émoticône se note, et survit à une réannonce", "[sim][world]") {
  // Le serveur réannonce une unité à chaque fois qu'elle se met en marche, et
  // son annonce ne dit rien de l'émoticône : tout écraser ferait disparaître
  // la bulle de qui salue en marchant.
  World world;
  world.Spawn(Someone(150'001, CellPosition{10, 10, 0}));
  CHECK_FALSE(world.Find(150'001)->emotion.has_value());

  REQUIRE(world.RecordEmotion(150'001, ro::sim::Emotion{ro::sim::kBaseEmotionPack, 8}, 4000));
  REQUIRE(world.Find(150'001)->emotion.has_value());
  CHECK(world.Find(150'001)->emotion->id == 8);
  CHECK(world.Find(150'001)->emotion->pack == ro::sim::kBaseEmotionPack);
  CHECK(world.Find(150'001)->emotion_since_ms == 4000);

  world.Spawn(Someone(150'001, CellPosition{11, 10, 0}));
  REQUIRE(world.Find(150'001)->emotion.has_value());
  CHECK(world.Find(150'001)->emotion_since_ms == 4000);

  // La MÊME émoticône redite repart de zéro : on peut la répéter.
  REQUIRE(world.RecordEmotion(150'001, ro::sim::Emotion{ro::sim::kBaseEmotionPack, 8}, 9000));
  CHECK(world.Find(150'001)->emotion_since_ms == 9000);

  CHECK_FALSE(world.RecordEmotion(999, ro::sim::Emotion{}, 0));
}

TEST_CASE("Monde : un ramassage se note avec son instant", "[sim][world]") {
  World world;
  world.Spawn(Someone(150'001, CellPosition{1, 1, 0}));
  CHECK_FALSE(world.Find(150'001)->pickup_since_ms.has_value());
  CHECK(world.RecordPickup(150'001, 700));
  CHECK(world.Find(150'001)->pickup_since_ms == 700);
  CHECK_FALSE(world.RecordPickup(999, 700));
}

TEST_CASE("Monde : les états se posent, et l'instant ne suit que le corps", "[sim][world]") {
  World world;
  world.Spawn(Someone(150'001, CellPosition{1, 1, 0}));
  CHECK(world.SetStates(150'001, ro::sim::kBodyFrozen, 0x1, ro::sim::kOptionHide, 400));
  const Actor* found = world.Find(150'001);
  REQUIRE(found != nullptr);
  CHECK(found->body_state == ro::sim::kBodyFrozen);
  CHECK(found->body_state_since_ms == 400);
  CHECK(found->health_state == 0x1);
  CHECK(found->option == ro::sim::kOptionHide);
  // Le même corps redit garde son instant ; un autre le renouvelle.
  CHECK(world.SetStates(150'001, ro::sim::kBodyFrozen, 0, 0, 500));
  CHECK(world.Find(150'001)->body_state_since_ms == 400);
  CHECK(world.Find(150'001)->option == 0);
  CHECK(world.SetStates(150'001, 0, 0, 0, 600));
  CHECK(world.Find(150'001)->body_state_since_ms == 600);
  CHECK_FALSE(world.SetStates(999, 0, 0, 0, 0));
}

TEST_CASE("Monde : un bit d'option qui s'allume se compte, même s'il tenait déjà", "[sim][world]") {
  // 🔴 CE QUE LE MOT SEUL NE DIT PAS. Un bit qui tombe puis remonte entre deux
  // images ne laisse aucune trace dans le mot : un client qui compare le mot
  // d'une image à celui de la précédente ne voit rien passer. Le natif tranche
  // sur les deux mots du PAQUET, et c'est ce que ce compte reproduit.
  constexpr std::uint32_t kSight = 0x1;
  World world;
  world.Spawn(Someone(150'001, CellPosition{1, 1, 0}));
  const Actor* watched = world.Find(150'001);
  REQUIRE(watched != nullptr);
  CHECK(watched->option_rise_count == 0);

  CHECK(world.SetStates(150'001, 0, 0, kSight, 100));
  CHECK(watched->option_rise_count == 1);
  CHECK(watched->option_risen == kSight);

  // Le MÊME mot redit ne compte pas : aucun bit ne s'allume.
  CHECK(world.SetStates(150'001, 0, 0, kSight, 200));
  CHECK(watched->option_rise_count == 1);

  // La chute ne compte pas non plus, et la remontée oui — c'est le cas qu'une
  // comparaison d'images manquerait si les deux paquets tombaient dans la même.
  CHECK(world.SetStates(150'001, 0, 0, 0, 300));
  CHECK(watched->option_rise_count == 1);
  CHECK(world.SetStates(150'001, 0, 0, kSight, 400));
  CHECK(watched->option_rise_count == 2);
  CHECK(watched->option_risen == kSight);

  // Un SECOND bit qui s'allume à côté du premier ne laisse que lui dans les
  // montants : le mot, lui, porte les deux.
  constexpr std::uint32_t kRuwach = 0x2000;
  CHECK(world.SetStates(150'001, 0, 0, kSight | kRuwach, 500));
  CHECK(watched->option_rise_count == 3);
  CHECK(watched->option_risen == kRuwach);
  CHECK(watched->option == (kSight | kRuwach));
}

TEST_CASE("Monde : changer de carte emporte tout le monde sauf le joueur", "[sim][world]") {
  // Le serveur réannonce les acteurs de la nouvelle carte, mais jamais le
  // joueur : lui traverse, avec son identifiant et son apparence.
  World world;
  world.set_player_id(2'000'001);

  Actor self;
  self.id = 2'000'001;
  self.kind = ActorKind::kPlayer;
  self.position = CellPosition{150, 100, 4};
  self.look.job = 4009;
  self.movement = CellMovement{CellPosition{150, 100, 0}, CellPosition{160, 110, 0}, 0, 0};
  self.pickup_since_ms = 42;
  world.Spawn(std::move(self));
  // Un corps rapetissé : l'effet 420, celui qu'envoie `@size 1`.
  REQUIRE(world.RecordEffect(2'000'001, 420, 1000));

  Actor other;
  other.id = 3'000'123;
  other.kind = ActorKind::kNonPlayer;
  world.Spawn(std::move(other));

  REQUIRE(world.size() == 2);
  CHECK(world.map_name().empty());

  world.EnterMap("prontera", 53, 111);

  CHECK(world.map_name() == "prontera");
  CHECK(world.size() == 1);
  CHECK(world.Find(3'000'123) == nullptr);

  const Actor* player = world.player();
  REQUIRE(player != nullptr);
  CHECK(player->position.x == 53);
  CHECK(player->position.y == 111);
  // La direction du regard traverse : le paquet ne la porte pas.
  CHECK(player->position.direction == 4);
  CHECK(player->look.job == 4009);
  // Ce qu'il faisait sur l'ancienne carte s'oublie : ces faits nomment des
  // acteurs qui ne sont plus là.
  CHECK_FALSE(player->movement.has_value());
  CHECK_FALSE(player->pickup_since_ms.has_value());
  // ⚠ ET SES EFFETS AUSSI, ce qui est la SEULE façon d'en éteindre un. Aucun
  // drapeau d'apparence ne se baisse : le client natif ne les efface qu'en
  // détruisant l'acteur, et le serveur s'appuie dessus — `@size 0` n'envoie
  // aucun effet, il téléporte sur place. Les garder laissait le corps
  // rapetissé pour le reste de la séance.
  CHECK(player->effects.empty());
}

TEST_CASE("Monde : les unités au sol restent sur la carte quittée", "[sim][world]") {
  // Le serveur n'annonce aucune disparition pour les pièges qu'on laisse
  // derrière soi : les garder les dessinerait aux mêmes cases, ailleurs.
  World world;
  ro::sim::SkillUnit trap;
  trap.id = 90'001;
  world.PlaceSkillUnit(trap);
  REQUIRE(world.skill_units().size() == 1);

  world.EnterMap("payon", 10, 20);
  CHECK(world.skill_units().empty());
}

TEST_CASE("Monde : changer de carte sans joueur ne laisse personne", "[sim][world]") {
  // Un monde sans joueur — le décor de l'écran de connexion — se vide sans
  // rien inventer.
  World world;
  Actor stranger;
  stranger.id = 7;
  world.Spawn(std::move(stranger));

  world.EnterMap("gonryun", 10, 20);
  CHECK(world.map_name() == "gonryun");
  CHECK(world.size() == 0);
}

TEST_CASE("Monde : l'horloge du serveur se cale sur le trajet le plus rapide", "[sim][world]") {
  // Le serveur date ses annonces dans SA propre horloge. Le décalage se mesure
  // sur le trajet le plus court observé : tout le reste est de la latence, et la
  // latence ne retranche jamais rien.
  ro::sim::World world;
  CHECK_FALSE(world.server_clock().known());
  // Sans mesure, l'instant demandé vaut celui de l'appelant.
  CHECK(world.server_clock().LocalTimeOf(1000, 5000) == 5000);

  // Une trame partie à 1000 chez le serveur, reçue à 5300 chez nous : décalage
  // de 4300.
  world.server_clock().NoteServerTick(1000, 5300);
  CHECK(world.server_clock().known());
  CHECK(world.server_clock().LocalTimeOf(1000, 5300) == 5300);

  // Une trame plus lente ne dégrade pas l'estimation.
  world.server_clock().NoteServerTick(2000, 6600);
  CHECK(world.server_clock().LocalTimeOf(2000, 6600) == 6300);

  // Une plus rapide l'améliore : le décalage tombe à 4200.
  world.server_clock().NoteServerTick(3000, 7200);
  CHECK(world.server_clock().LocalTimeOf(3000, 7200) == 7200);
  CHECK(world.server_clock().LocalTimeOf(2000, 8000) == 6200);

  // Et rien ne commence dans le futur.
  CHECK(world.server_clock().LocalTimeOf(9000, 7000) == 7000);
}

TEST_CASE("Monde : une unité réannoncée garde ce que son paquet ne dit pas", "[sim][world]") {
  // Le serveur réannonce une unité ENTIÈRE à chaque mise en marche, et son
  // annonce ne porte ni la vie ni la taille : celles-là arrivent par des paquets
  // à elles. Tout écraser ferait clignoter la jauge d'un monstre à chaque pas,
  // et un corps rapetissé rejouerait son rétrécissement en marchant.
  ro::sim::World world;
  ro::sim::Actor mob;
  mob.id = 3;
  mob.kind = ro::sim::ActorKind::kMonster;
  world.Spawn(mob);
  CHECK(world.SetHealth(3, 40, 100));
  CHECK(world.RecordEffect(3, 420, 500));

  ro::sim::Actor reannonce;
  reannonce.id = 3;
  reannonce.kind = ro::sim::ActorKind::kMonster;
  reannonce.name = "Poring";
  world.Spawn(reannonce);

  REQUIRE(world.Find(3) != nullptr);
  CHECK(world.Find(3)->name == "Poring");
  CHECK(world.Find(3)->health == 40);
  CHECK(world.Find(3)->max_health == 100);
  REQUIRE(world.Find(3)->effects.size() == 1);
  CHECK(world.Find(3)->effects.front().id == 420);
  CHECK(world.Find(3)->effects.front().since_ms == 500);
}

TEST_CASE("Monde : le compteur de distance ne repart pas d'un nouvel ordre", "[sim][world]") {
  // Le jeu anime la marche sur la distance parcourue, et son compteur court sur
  // toute la vie de l'acteur. Le serveur, lui, reannonce un deplacement a chaque
  // changement de cap : remettre le compteur a zero ferait repartir le cycle de
  // sa premiere image, et le personnage marcherait d'un seul pied.
  ro::sim::World world;
  ro::sim::Actor mob;
  mob.id = 5;
  mob.kind = ro::sim::ActorKind::kMonster;
  mob.position = ro::sim::CellPosition{10, 10, 0};
  world.Spawn(mob);
  CHECK(world.Find(5)->travelled_before_cells == 0.0F);

  // Trois cases plus loin, le serveur annonce un nouveau depart.
  ro::sim::Actor reannonce = mob;
  reannonce.position = ro::sim::CellPosition{13, 10, 0};
  world.Spawn(reannonce);
  CHECK(world.Find(5)->travelled_before_cells == Catch::Approx(3.0F));

  // Et quatre de plus, en diagonale : la racine de deux par pas.
  ro::sim::Actor still_there = mob;
  still_there.position = ro::sim::CellPosition{16, 13, 0};
  world.Spawn(still_there);
  CHECK(world.Find(5)->travelled_before_cells == Catch::Approx(3.0F + 4.2426F).margin(0.01F));
}

TEST_CASE("Monde : l'effet joué sur un acteur se note, et se rejoue", "[sim][world]") {
  // Le monde ne sait pas ce qu'un numéro d'effet MONTRE — c'est une table du
  // client qui le dit — et ne garde que le fait : celui-ci, sur celui-là, à cet
  // instant. Le même effet relancé repart de zéro, sans quoi un sort lancé deux
  // fois de suite ne se verrait qu'une.
  ro::sim::World world;
  ro::sim::Actor actor;
  actor.id = 11;
  world.Spawn(actor);

  CHECK(world.Find(11)->effects.empty());
  CHECK(world.RecordEffect(11, 89, 400));
  REQUIRE(world.Find(11)->effects.size() == 1);
  CHECK(world.Find(11)->effects[0].id == 89);
  CHECK(world.Find(11)->effects[0].since_ms == 400);

  // Le MÊME effet renoté ne s'ajoute pas une seconde fois : il repart de zéro.
  CHECK(world.RecordEffect(11, 89, 900));
  REQUIRE(world.Find(11)->effects.size() == 1);
  CHECK(world.Find(11)->effects[0].since_ms == 900);

  CHECK_FALSE(world.RecordEffect(12, 89, 900));
}

TEST_CASE("Monde : un acteur porte PLUSIEURS effets à la fois", "[sim][world]") {
  // Un corps rouge tient pendant qu'un coup l'atteint, et le client lui-même en
  // empile autant qu'il en reçoit. N'en garder qu'un perdait le premier dès que
  // le second arrivait — et le retrait par 0x0B0D ne pouvait plus viser que le
  // dernier.
  ro::sim::World world;
  ro::sim::Actor actor;
  actor.id = 11;
  world.Spawn(actor);

  REQUIRE(world.RecordEffect(11, 368, 400));
  REQUIRE(world.RecordEffect(11, 1, 500));
  REQUIRE(world.Find(11)->effects.size() == 2);

  // Le retrait NOMME son effet : celui que l'autre a recouvert s'en va quand
  // même, et lui seul.
  CHECK(world.RemoveEffect(11, 368));
  REQUIRE(world.Find(11)->effects.size() == 1);
  CHECK(world.Find(11)->effects[0].id == 1);
  CHECK_FALSE(world.RemoveEffect(11, 368));

  // ⚠ UN EFFET NE S'EN VA PAS TOUT SEUL : le client ne baisse jamais le bit
  // d'apparence qu'un effet lève. Sans plafond, la liste grandirait tant que
  // l'acteur vit ; au-delà, le plus ancien s'en va.
  for (std::uint32_t index = 0; index < ro::sim::kEffectsKeptPerActor; ++index) {
    REQUIRE(world.RecordEffect(11, 1000 + index, 600 + index));
  }
  CHECK(world.Find(11)->effects.size() == ro::sim::kEffectsKeptPerActor);
  // Le seul qui soit parti est le PLUS ANCIEN, celui qui restait du début.
  CHECK(world.Find(11)->effects.front().id == 1000);
  CHECK(world.Find(11)->effects.back().id == 1000 + ro::sim::kEffectsKeptPerActor - 1);
}

TEST_CASE("Monde : une réapparition garde l'effet en cours", "[sim][world]") {
  // Le serveur réannonce une unité sans rien dire de son effet. L'oublier
  // couperait net une aura qui joue encore, à chaque réannonce.
  ro::sim::World world;
  ro::sim::Actor actor;
  actor.id = 11;
  world.Spawn(actor);
  REQUIRE(world.RecordEffect(11, 89, 400));

  world.Spawn(actor);
  REQUIRE(world.Find(11)->effects.size() == 1);
  CHECK(world.Find(11)->effects[0].id == 89);
  CHECK(world.Find(11)->effects[0].since_ms == 400);
}

TEST_CASE("Monde : la bannière du nom suit la règle du client", "[sim][world]") {
  // 🔴 UN MONSTRE ANNONCE CE QU'UN JOUEUR TAIT. Le client ne consulte sa liste
  // d'exceptions que pour un joueur : un monstre ou un PNJ a le drapeau forcé
  // à vrai. Écarter ces neuf pour tout le monde priverait
  // les monstres d'une annonce qu'ils font.
  CHECK(ro::sim::SkillBannerShows(5, false));         // SM_BASH, un joueur
  CHECK_FALSE(ro::sim::SkillBannerShows(40, false));  // dans la liste du joueur
  CHECK(ro::sim::SkillBannerShows(40, true));         // le monstre l'annonce
  // Les quatre au-delà de 2247, que la liste a longtemps oubliés.
  for (const std::uint16_t tue : std::array<std::uint16_t, 4>{2290, 2342, 2535, 3001}) {
    CHECK_FALSE(ro::sim::SkillBannerShows(tue, false));
    CHECK(ro::sim::SkillBannerShows(tue, true));
  }
  // Et les trois que PERSONNE n'annonce.
  CHECK_FALSE(ro::sim::SkillBannerShows(318, false));
  CHECK_FALSE(ro::sim::SkillBannerShows(318, true));
  CHECK_FALSE(ro::sim::SkillBannerShows(326, true));
  CHECK_FALSE(ro::sim::SkillBannerShows(492, true));
  // Zéro n'est pas une compétence : c'est ce que porte un coup ordinaire.
  CHECK_FALSE(ro::sim::SkillBannerShows(0, false));
  CHECK_FALSE(ro::sim::SkillBannerShows(0, true));
  // Les compétences de couple nomment leur partenaire.
  CHECK(ro::sim::SkillBannerNamesTarget(334));
  CHECK(ro::sim::SkillBannerNamesTarget(335));
  CHECK_FALSE(ro::sim::SkillBannerNamesTarget(28));
}

TEST_CASE("Monde : l'annonce retient qui la compétence vise", "[sim][world]") {
  World world;
  Actor caster = Someone(170'011, CellPosition{10, 10, 0});
  caster.kind = ro::sim::ActorKind::kPlayer;
  world.Spawn(caster);
  constexpr std::uint16_t kWeMale = 334;
  constexpr std::uint32_t kPartner = 170'012;
  REQUIRE(world.AnnounceSkill(170'011, kWeMale, 4'000, kPartner));
  CHECK(world.Find(170'011)->announced_target_id == kPartner);
}

TEST_CASE("Monde : l'annonce se pose sur le LANCEUR", "[sim][world]") {
  World world;
  Actor caster = Someone(170'001, CellPosition{10, 10, 0});
  caster.kind = ro::sim::ActorKind::kPlayer;
  world.Spawn(caster);

  CHECK(world.AnnounceSkill(170'001, 5, 4'000));
  const ro::sim::Actor* found = world.Find(170'001);
  REQUIRE(found != nullptr);
  CHECK(found->announced_skill_id == 5);
  CHECK(found->announced_skill_since_ms == 4'000);

  // ⚠ UNE COMPÉTENCE ÉCARTÉE NE REMPLACE PAS LA PRÉCÉDENTE : le client ne
  // touche pas à la bulle quand il n'annonce rien.
  CHECK_FALSE(world.AnnounceSkill(170'001, 40, 5'000));
  CHECK(world.Find(170'001)->announced_skill_id == 5);
  CHECK(world.Find(170'001)->announced_skill_since_ms == 4'000);

  // Sur quelqu'un qu'on ne voit pas, rien à noter.
  CHECK_FALSE(world.AnnounceSkill(999, 5, 6'000));
}

TEST_CASE("Bannière : le nom, le suffixe, et le numéro pour le staff", "[sim][world]") {
  // ⚠ SANS TABLES DU CLIENT, ON DIT LE NUMÉRO plutôt que de se taire : une
  // bannière vide ressemblerait à une compétence qui n'annonce rien.
  CHECK(ro::sim::SkillBannerText(nullptr, 5, false) == "#5 !!");
  // 🔴 LE NUMÉRO S'AJOUTE AU NOM, il ne le remplace pas : le staff lit les deux.
  CHECK(ro::sim::SkillBannerText(nullptr, 5, true) == "#5 (5) !!");
  // Les suffixes sont ceux du client, à l'octet près : derrière un partenaire,
  // deux espaces de queue ; derrière une compétence, aucun.
  CHECK(ro::sim::kSkillBannerSuffix == " !!  ");
  CHECK(ro::sim::kSkillNameBannerSuffix == " !!");
}
