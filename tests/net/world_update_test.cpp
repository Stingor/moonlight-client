#include "ro/net/world_update.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/map_packets.h"
#include "ro/sim/movement.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::net::ActorKindOf;
using ro::net::ApplyFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::ActorKind;
using ro::sim::CellMovement;
using ro::sim::CellPosition;
using ro::sim::World;
using ro::test_support::FrameOf;
using ro::test_support::IdleUnit;

TEST_CASE("Traduction : un acteur qui entre en vue arrive avec ses états", "[net][update]") {
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(150'001, CellPosition{10, 10, 0}))), world, 0) ==
          FrameOutcome::kApplied);

  // Le serveur envoie un état par trame, APRÈS le paquet d'unité.
  ro::proto::ZcEfstSetEnter worn;
  worn.targetID = 150'001;
  worn.type = 613;  // EFST_ALL_RIDING
  worn.duration = 9999;

  CHECK(ApplyFrame(FrameOf(Encode(worn)), world, 0) == FrameOutcome::kApplied);
  const auto* actor = world.Find(150'001);
  REQUIRE(actor != nullptr);
  CHECK(actor->status_effects.contains(ro::sim::kStatusAllRiding));
}

TEST_CASE("Traduction : un marchand qui entre en vue dit le type de sa charrette",
          "[net][update]") {
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(150'003, CellPosition{10, 10, 0}))), world, 0) ==
          FrameOutcome::kApplied);

  // EFST_ON_PUSH_CART, dont la première valeur est le type.
  constexpr std::uint16_t kOnPushCart = 673;
  constexpr std::int32_t kGiftBoxCart = 10;
  ro::proto::ZcEfstSetEnter pushed;
  pushed.targetID = 150'003;
  pushed.type = kOnPushCart;
  pushed.val1 = kGiftBoxCart;

  CHECK(ApplyFrame(FrameOf(Encode(pushed)), world, 0) == FrameOutcome::kApplied);
  const auto* actor = world.Find(150'003);
  REQUIRE(actor != nullptr);
  REQUIRE(actor->status_values.contains(kOnPushCart));
  CHECK(actor->status_values.at(kOnPushCart) == kGiftBoxCart);
}

TEST_CASE("Traduction : l'état d'un inconnu n'invente pas d'acteur", "[net][update]") {
  World world;
  ro::proto::ZcEfstSetEnter worn;
  worn.targetID = 999'999;
  worn.type = 613;

  CHECK(ApplyFrame(FrameOf(Encode(worn)), world, 0) == FrameOutcome::kIgnored);
  CHECK(world.Find(999'999) == nullptr);
}

TEST_CASE("Traduction : la vie d'un compagnon de groupe", "[net][update]") {
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(150'002, CellPosition{10, 10, 0}))), world, 0) ==
          FrameOutcome::kApplied);

  ro::proto::ZcNotifyHpToGroupm health;
  health.AID = 150'002;
  health.hp = 340;
  health.maxhp = 900;

  CHECK(ApplyFrame(FrameOf(Encode(health)), world, 0) == FrameOutcome::kApplied);
  const auto* actor = world.Find(150'002);
  REQUIRE(actor != nullptr);
  CHECK(actor->health == 340);
  CHECK(actor->max_health == 900);

  // Un membre hors de vue est compris et sans effet : ce n'est pas du travail
  // qui reste, c'est une fenêtre de groupe qui n'existe pas encore.
  health.AID = 150'999;
  CHECK(ApplyFrame(FrameOf(Encode(health)), world, 0) == FrameOutcome::kIgnored);
}

TEST_CASE("Traduction : le genre d'acteur suit la table du serveur", "[net][update]") {
  // Les valeurs viennent de clif_bl_type. Un joueur déguisé reste un joueur ;
  // une valeur inconnue reste un acteur, parce que le serveur en ajoute au fil
  // des versions et qu'il faut quand même le placer.
  CHECK(ActorKindOf(0x0) == ActorKind::kPlayer);
  CHECK(ActorKindOf(0x1) == ActorKind::kPlayer);
  CHECK(ActorKindOf(0x2) == ActorKind::kItem);
  CHECK(ActorKindOf(0x3) == ActorKind::kSkillUnit);
  CHECK(ActorKindOf(0x5) == ActorKind::kMonster);
  CHECK(ActorKindOf(0x6) == ActorKind::kNonPlayer);
  CHECK(ActorKindOf(0x8) == ActorKind::kNonPlayer);
  // Un PNJ vêtu d'une classe de monstre reste un PNJ ; les invocations de
  // Meister et de Biolo restent des monstres.
  CHECK(ActorKindOf(0xC) == ActorKind::kNonPlayer);
  CHECK(ActorKindOf(0xD) == ActorKind::kMonster);
  CHECK(ActorKindOf(0xE) == ActorKind::kMonster);
  CHECK(ActorKindOf(0xFF) == ActorKind::kUnknown);
}

TEST_CASE("Traduction : une unité au repos entre dans le monde", "[net][update]") {
  World world;
  const auto frame = FrameOf(Encode(IdleUnit(150'001, CellPosition{150, 100, 4})));

  CHECK(ApplyFrame(frame, world, 0) == FrameOutcome::kApplied);
  const auto* actor = world.Find(150'001);
  REQUIRE(actor != nullptr);
  CHECK(actor->kind == ActorKind::kPlayer);
  CHECK(actor->position.x == 150);
  CHECK(actor->position.y == 100);
  CHECK(actor->position.direction == 4);
  CHECK(actor->name == "Alice");
  CHECK(actor->look.job == 4054);
  CHECK(actor->speed_ms_per_cell == 150);
  CHECK_FALSE(actor->movement.has_value());
}

TEST_CASE("Traduction : c'est l'AID qui nomme un acteur, pas le GID", "[net][update]") {
  // Depuis PACKETVER 20131223, GID porte l'identifiant de PERSONNAGE, nul pour
  // tout ce qui n'est pas un joueur. S'en servir rangerait tous les PNJ et tous
  // les monstres sous le même identifiant : ils s'effaceraient les uns les
  // autres, et le monde n'en montrerait qu'un.
  World world;
  ro::proto::PacketIdleUnit first = IdleUnit(110'000'001, CellPosition{150, 100, 0});
  ro::proto::PacketIdleUnit second = IdleUnit(110'000'002, CellPosition{151, 100, 0});
  first.objecttype = 6;   // un PNJ
  second.objecttype = 6;  // un autre
  first.GID = 0;
  second.GID = 0;
  REQUIRE(ApplyFrame(FrameOf(Encode(first)), world, 0) == FrameOutcome::kApplied);
  REQUIRE(ApplyFrame(FrameOf(Encode(second)), world, 0) == FrameOutcome::kApplied);

  CHECK(world.size() == 2);
  CHECK(world.Find(110'000'001) != nullptr);
  CHECK(world.Find(110'000'002) != nullptr);
  CHECK(world.Find(0) == nullptr);
}

TEST_CASE("Traduction : les trois accessoires arrivent bas, HAUT, milieu", "[net][update]") {
  // Le serveur recopie LOOK_HEAD_BOTTOM, LOOK_HEAD_TOP puis LOOK_HEAD_MID :
  // le deuxième accessoire est le HAUT. Les intervertir met le chapeau à la
  // place des lunettes.
  World world;
  ro::proto::PacketIdleUnit unit = IdleUnit(2'000'001, CellPosition{150, 100, 0});
  unit.accessory = 11;
  unit.accessory2 = 22;
  unit.accessory3 = 33;
  REQUIRE(ApplyFrame(FrameOf(Encode(unit)), world, 0) == FrameOutcome::kApplied);

  const auto* actor = world.Find(2'000'001);
  REQUIRE(actor != nullptr);
  CHECK(actor->look.lower_headgear == 11);
  CHECK(actor->look.upper_headgear == 22);
  CHECK(actor->look.middle_headgear == 33);
}

TEST_CASE("Traduction : une unité qui marche entre ET se met en route", "[net][update]") {
  // Elle est annoncée ENTIÈRE : c'est une apparition et un déplacement à la
  // fois. Ne traiter que le déplacement laisserait sans apparence quelqu'un qui
  // entre dans le champ de vision en marchant.
  World world;
  ro::proto::PacketUnitWalking walking;
  walking.objecttype = 5;  // un monstre
  walking.AID = 200'002;
  walking.speed = 200;
  walking.name = "Poring";
  walking.MoveData = ro::sim::PackMovement(
      CellMovement{CellPosition{150, 100, 0}, CellPosition{158, 104, 0}, 8, 8});

  CHECK(ApplyFrame(FrameOf(Encode(walking)), world, 0) == FrameOutcome::kApplied);
  const auto* actor = world.Find(200'002);
  REQUIRE(actor != nullptr);
  CHECK(actor->kind == ActorKind::kMonster);
  CHECK(actor->name == "Poring");
  CHECK(actor->position.x == 150);
  REQUIRE(actor->movement.has_value());
  CHECK(actor->movement->to.x == 158);
  CHECK(actor->movement->to.y == 104);
}

TEST_CASE("Traduction : un ARRÊT garde le regard du dernier pas", "[net][update]") {
  // 🔴 LE PAQUET D'ARRÊT NE DIT PAS LA DIRECTION, et le client relisait
  // `position.direction` — qui ne vaut PAS ce que l'acteur montre en marchant.
  // Un déplacement ne porte aucune direction : les six octets de MoveData sont
  // deux cases et deux sous-cases, si bien que ce champ garde le regard d'AVANT
  // le départ, souvent le ZÉRO d'un acteur neuf. L'acteur arrêté pivotait donc
  // vers le NORD, dos à la caméra.
  World world;
  ro::proto::PacketUnitWalking walking;
  walking.objecttype = 5;  // un monstre
  walking.AID = 200'003;
  walking.speed = 150;
  walking.name = "Poring";
  // Cap à l'EST, cinq cases.
  walking.MoveData = ro::sim::PackMovement(
      CellMovement{CellPosition{100, 100, 0}, CellPosition{105, 100, 0}, 8, 8});
  REQUIRE(ApplyFrame(FrameOf(Encode(walking)), world, 0) == FrameOutcome::kApplied);

  // En pleine course, il regarde ailleurs que le nord.
  const std::uint32_t mid_course_ms = 300;
  const std::uint8_t shown_facing = ro::sim::FacingAt(*world.Find(200'003), mid_course_ms);
  REQUIRE(shown_facing != 0);

  ro::proto::ZcStopmove stop;
  stop.AID = 200'003;
  stop.xPos = 102;
  stop.yPos = 100;
  CHECK(ApplyFrame(FrameOf(Encode(stop)), world, mid_course_ms) == FrameOutcome::kApplied);

  const auto* stopped = world.Find(200'003);
  REQUIRE(stopped != nullptr);
  CHECK_FALSE(stopped->movement.has_value());
  CHECK(stopped->position.direction == shown_facing);
}

TEST_CASE("Traduction : une unité qui disparaît est retirée", "[net][update]") {
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(150'001, CellPosition{1, 1, 0}))), world, 0) ==
          FrameOutcome::kApplied);

  ro::proto::ZcNotifyVanish vanish;
  vanish.gid = 150'001;
  CHECK(ApplyFrame(FrameOf(Encode(vanish)), world, 0) == FrameOutcome::kApplied);
  CHECK(world.Find(150'001) == nullptr);
}

TEST_CASE("Traduction : un arrêt repose l'acteur et garde sa direction", "[net][update]") {
  // Le paquet d'arrêt ne dit pas où l'on regarde. La remettre à zéro ferait
  // pivoter tout le monde vers le nord à chaque arrêt.
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(150'001, CellPosition{150, 100, 6}))), world, 0) ==
          FrameOutcome::kApplied);

  ro::proto::ZcStopmove stop;
  stop.AID = 150'001;
  stop.xPos = 155;
  stop.yPos = 104;
  CHECK(ApplyFrame(FrameOf(Encode(stop)), world, 0) == FrameOutcome::kApplied);

  const auto* actor = world.Find(150'001);
  REQUIRE(actor != nullptr);
  CHECK(actor->position.x == 155);
  CHECK(actor->position.direction == 6);
}

TEST_CASE("Traduction : tourner sur place n'annule pas la marche", "[net][update]") {
  // On peut regarder ailleurs en marchant. Annuler le déplacement figerait le
  // personnage à chaque fois qu'il tourne la tête.
  World world;
  ro::proto::PacketUnitWalking walking;
  walking.AID = 150'001;
  walking.MoveData = ro::sim::PackMovement(
      CellMovement{CellPosition{150, 100, 0}, CellPosition{158, 104, 0}, 0, 0});
  REQUIRE(ApplyFrame(FrameOf(Encode(walking)), world, 0) == FrameOutcome::kApplied);

  ro::proto::ZcChangeDirection turn;
  turn.srcId = 150'001;
  turn.dir = 2;
  CHECK(ApplyFrame(FrameOf(Encode(turn)), world, 0) == FrameOutcome::kApplied);

  const auto* actor = world.Find(150'001);
  REQUIRE(actor != nullptr);
  CHECK(actor->position.direction == 2);
  CHECK(actor->movement.has_value());
}

TEST_CASE("Traduction : la tête regarde où le corps ne regarde pas", "[net][update]") {
  // Le serveur donne deux directions, et le client les distingue : le corps
  // reste où il est pendant que la tête tourne. C'est /doridori.
  World world;
  ro::proto::PacketIdleUnit unit = IdleUnit(150'001, CellPosition{150, 100, 4});
  unit.headDir = 2;
  REQUIRE(ApplyFrame(FrameOf(Encode(unit)), world, 0) == FrameOutcome::kApplied);
  REQUIRE(world.Find(150'001) != nullptr);
  CHECK(world.Find(150'001)->head_direction == 2);

  ro::proto::ZcChangeDirection turn;
  turn.srcId = 150'001;
  turn.dir = 4;
  turn.headDir = 1;
  REQUIRE(ApplyFrame(FrameOf(Encode(turn)), world, 0) == FrameOutcome::kApplied);
  CHECK(world.Find(150'001)->position.direction == 4);
  CHECK(world.Find(150'001)->head_direction == 1);
}

TEST_CASE("Traduction : la marche du joueur se date à l'heure du serveur", "[net][update]") {
  World world;
  world.set_player_id(150'001);
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(150'001, CellPosition{1, 1, 0}))), world, 0) ==
          FrameOutcome::kApplied);
  // Le trajet le plus rapide : la milliseconde locale 1000 répond au tic 5000.
  world.server_clock().NoteServerTick(5000, 1000);

  ro::proto::ZcNotifyPlayermove move;
  move.moveStartTime = 5100;
  move.moveData =
      ro::sim::PackMovement(CellMovement{CellPosition{1, 1, 0}, CellPosition{9, 1, 0}, 8, 8});
  // La trame arrive cinquante millisecondes plus tard que ce trajet-là.
  REQUIRE(ApplyFrame(FrameOf(Encode(move)), world, 1150) == FrameOutcome::kApplied);

  const auto* player = world.player();
  REQUIRE(player != nullptr);
  CHECK(player->movement_started_ms == 1100);
}

TEST_CASE("Traduction : le déplacement du joueur ne nomme personne", "[net][update]") {
  // Le paquet ne porte aucun identifiant : il ne parle que du joueur. Sans
  // savoir qui il est, il n'y a rien à faire — et surtout personne à inventer.
  World world;
  ro::proto::ZcNotifyPlayermove move;
  move.moveData = ro::sim::PackMovement(
      CellMovement{CellPosition{150, 100, 0}, CellPosition{160, 110, 0}, 0, 0});
  CHECK(ApplyFrame(FrameOf(Encode(move)), world, 0) == FrameOutcome::kIgnored);

  world.set_player_id(150'001);
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(150'001, CellPosition{1, 1, 0}))), world, 0) ==
          FrameOutcome::kApplied);
  CHECK(ApplyFrame(FrameOf(Encode(move)), world, 0) == FrameOutcome::kApplied);

  const auto* player = world.player();
  REQUIRE(player != nullptr);
  CHECK(player->position.x == 150);
  REQUIRE(player->movement.has_value());
  CHECK(player->movement->to.y == 110);
}

TEST_CASE("Traduction : changer de carte nomme la carte et repose le joueur", "[net][update]") {
  // Le serveur nomme les cartes du nom de leur fichier de marchabilité ; le
  // reste du client les nomme sans extension.
  World world;
  world.set_player_id(2'000'001);
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(2'000'001, CellPosition{150, 100, 2}))), world, 0) ==
          FrameOutcome::kApplied);

  ro::proto::ZcNpcackMapmove move;
  move.mapName = "prontera.gat";
  move.xPos = 53;
  move.yPos = 111;
  CHECK(ApplyFrame(FrameOf(Encode(move)), world, 0) == FrameOutcome::kApplied);

  CHECK(world.map_name() == "prontera");
  REQUIRE(world.player() != nullptr);
  CHECK(world.player()->position.x == 53);
  CHECK(world.player()->position.y == 111);
}

TEST_CASE("Traduction : changer de carte vide le monde", "[net][update]") {
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(150'001, CellPosition{1, 1, 0}))), world, 0) ==
          FrameOutcome::kApplied);
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(200'002, CellPosition{2, 2, 0}))), world, 0) ==
          FrameOutcome::kApplied);

  ro::proto::ZcNpcackMapmove warp;
  warp.mapName = "geffen.gat";
  warp.xPos = 100;
  warp.yPos = 100;
  CHECK(ApplyFrame(FrameOf(Encode(warp)), world, 0) == FrameOutcome::kApplied);
  CHECK(world.size() == 0);
}

TEST_CASE("Traduction : une trame inconnue se distingue d'une trame ignorée", "[net][update]") {
  // La distinction dit ce qu'il reste à écrire. Les confondre laisserait croire
  // que tout est traité.
  World world;
  CHECK(ApplyFrame(Frame{0xFFFF, {0xFF, 0xFF}}, world, 0) == FrameOutcome::kUnhandled);

  // Une trame dont l'opcode est connu mais dont les octets sont trop courts.
  CHECK(ApplyFrame(Frame{ro::proto::ZcNotifyVanish::kOpcode, {0x80, 0x00}}, world, 0) ==
        FrameOutcome::kUndecodable);
}

TEST_CASE("Traduction : un état se pose par l'un des trois paquets, et se retire",
          "[net][update]") {
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(150'001, CellPosition{1, 1, 0}))), world, 0) ==
          FrameOutcome::kApplied);

  ro::proto::PacketStatusChange riding;
  riding.index = static_cast<std::int16_t>(ro::sim::kStatusAllRiding);
  riding.AID = 150'001;
  riding.state = 1;
  CHECK(ApplyFrame(FrameOf(Encode(riding)), world, 0) == FrameOutcome::kApplied);
  CHECK(world.Find(150'001)->status_effects.contains(ro::sim::kStatusAllRiding));

  ro::proto::PacketStatusChangeEnd ended;
  ended.index = riding.index;
  ended.AID = 150'001;
  ended.state = 0;
  CHECK(ApplyFrame(FrameOf(Encode(ended)), world, 0) == FrameOutcome::kApplied);
  CHECK_FALSE(world.Find(150'001)->status_effects.contains(ro::sim::kStatusAllRiding));

  ro::proto::PacketStatusChange2 again;
  again.index = riding.index;
  again.AID = 150'001;
  again.state = 1;
  CHECK(ApplyFrame(FrameOf(Encode(again)), world, 0) == FrameOutcome::kApplied);
  CHECK(world.Find(150'001)->status_effects.contains(ro::sim::kStatusAllRiding));

  // Un inconnu n'est pas créé.
  riding.AID = 999;
  CHECK(ApplyFrame(FrameOf(Encode(riding)), world, 0) == FrameOutcome::kIgnored);
  CHECK(world.Find(999) == nullptr);
}

TEST_CASE("Traduction : un changement d'apparence ne touche qu'à l'aspect qu'il nomme",
          "[net][update]") {
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(150'001, CellPosition{1, 1, 0}))), world, 0) ==
          FrameOutcome::kApplied);
  const auto look = [&world]() { return world.Find(150'001)->look; };

  // Les valeurs d'aspect sont celles de enum _look : LOOK_WEAPON dit les
  // deux mains, LOOK_HAIR la coiffure, LOOK_ROBE la cape, LOOK_BODY2 le
  // costume, LOOK_BASE le métier, LOOK_HAIR_COLOR la palette des cheveux.
  ro::proto::ZcSpriteChange change;
  change.AID = 150'001;
  change.type = 2;
  change.val = 1201;
  change.val2 = 2101;
  CHECK(ApplyFrame(FrameOf(Encode(change)), world, 0) == FrameOutcome::kApplied);
  CHECK(look().weapon == 1201);
  CHECK(look().shield == 2101);
  CHECK(look().job == 4054);

  change.type = 1;
  change.val = 7;
  change.val2 = 0;
  CHECK(ApplyFrame(FrameOf(Encode(change)), world, 0) == FrameOutcome::kApplied);
  CHECK(look().head == 7);
  CHECK(look().weapon == 1201);

  change.type = 12;
  change.val = 3;
  CHECK(ApplyFrame(FrameOf(Encode(change)), world, 0) == FrameOutcome::kApplied);
  CHECK(look().robe == 3);
  change.type = 13;
  change.val = 1;
  CHECK(ApplyFrame(FrameOf(Encode(change)), world, 0) == FrameOutcome::kApplied);
  CHECK(look().body_style == 1);
  change.type = 0;
  change.val = 4060;
  CHECK(ApplyFrame(FrameOf(Encode(change)), world, 0) == FrameOutcome::kApplied);
  CHECK(look().job == 4060);
  change.type = 6;
  change.val = 5;
  CHECK(ApplyFrame(FrameOf(Encode(change)), world, 0) == FrameOutcome::kApplied);
  CHECK(look().head_palette == 5);

  // Les coiffes se posent une à une, et tombent d'un coup (LOOK_RESET_COSTUMES).
  change.type = 4;
  change.val = 100;
  CHECK(ApplyFrame(FrameOf(Encode(change)), world, 0) == FrameOutcome::kApplied);
  CHECK(look().upper_headgear == 100);
  change.type = 11;
  change.val = 0;
  CHECK(ApplyFrame(FrameOf(Encode(change)), world, 0) == FrameOutcome::kApplied);
  CHECK(look().upper_headgear == 0);

  // Les chaussures ne changent rien de ce qu'on dessine ; un inconnu n'est
  // pas créé.
  change.type = 9;
  CHECK(ApplyFrame(FrameOf(Encode(change)), world, 0) == FrameOutcome::kIgnored);
  change.type = 2;
  change.AID = 999;
  CHECK(ApplyFrame(FrameOf(Encode(change)), world, 0) == FrameOutcome::kIgnored);
  CHECK(world.Find(999) == nullptr);
}

TEST_CASE("Traduction : s'asseoir, se relever, mourir et ressusciter", "[net][update]") {
  using ro::sim::Posture;
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(150'001, CellPosition{1, 1, 0}))), world, 0) ==
          FrameOutcome::kApplied);

  // Les valeurs d'action sont celles de e_damage_type : 2 s'assoit, 3 se
  // relève, 0 est un coup — que rien ne traduit encore.
  ro::proto::ZcNotifyAct act;
  act.srcID = 150'001;
  act.type = 2;
  CHECK(ApplyFrame(FrameOf(Encode(act)), world, 500) == FrameOutcome::kApplied);
  CHECK(world.Find(150'001)->posture == Posture::kSitting);
  CHECK(world.Find(150'001)->posture_since_ms == 500);
  act.type = 3;
  CHECK(ApplyFrame(FrameOf(Encode(act)), world, 600) == FrameOutcome::kApplied);
  CHECK(world.Find(150'001)->posture == Posture::kStanding);
  // Le ramassage (1) se note avec son instant.
  act.type = 1;
  CHECK(ApplyFrame(FrameOf(Encode(act)), world, 700) == FrameOutcome::kApplied);
  CHECK(world.Find(150'001)->pickup_since_ms == 700);

  // Mort (clr_type 1), l'acteur reste, à terre ; ressuscité, il se relève ;
  // rafraîchi (4), il s'en va sur-le-champ, et le serveur le refait paraître.
  ro::proto::ZcNotifyVanish vanish;
  vanish.gid = 150'001;
  vanish.type = 1;
  CHECK(ApplyFrame(FrameOf(Encode(vanish)), world, 800) == FrameOutcome::kApplied);
  REQUIRE(world.Find(150'001) != nullptr);
  CHECK(world.Find(150'001)->posture == Posture::kDead);
  CHECK(world.Find(150'001)->posture_since_ms == 800);
  ro::proto::ZcResurrection alive;
  alive.gid = 150'001;
  CHECK(ApplyFrame(FrameOf(Encode(alive)), world, 900) == FrameOutcome::kApplied);
  CHECK(world.Find(150'001)->posture == Posture::kStanding);
  vanish.type = 4;
  CHECK(ApplyFrame(FrameOf(Encode(vanish)), world, 1000) == FrameOutcome::kApplied);
  CHECK(world.Find(150'001) == nullptr);

  // Une unité au repos peut arriver assise, ou morte.
  ro::proto::PacketIdleUnit seated = IdleUnit(150'002, CellPosition{2, 2, 0});
  seated.state = 2;
  CHECK(ApplyFrame(FrameOf(Encode(seated)), world, 1150) == FrameOutcome::kApplied);
  CHECK(world.Find(150'002)->posture == Posture::kSitting);
  CHECK(world.Find(150'002)->posture_since_ms == 1150);

  // La mort d'un inconnu ne le crée pas.
  vanish.type = 1;
  CHECK(ApplyFrame(FrameOf(Encode(vanish)), world, 1200) == FrameOutcome::kIgnored);
  CHECK(world.Find(150'001) == nullptr);
}

TEST_CASE("Traduction : un coup se note sur qui le porte et qui le reçoit", "[net][update]") {
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(150'001, CellPosition{10, 10, 0}))), world, 0) ==
          FrameOutcome::kApplied);
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(150'002, CellPosition{10, 13, 0}))), world, 0) ==
          FrameOutcome::kApplied);

  // Un coup ordinaire (0) : les durées et les dégâts passent, la cible bronche.
  ro::proto::ZcNotifyAct blow;
  blow.srcID = 150'001;
  blow.targetID = 150'002;
  blow.srcSpeed = 500;
  blow.dmgSpeed = 288;
  blow.damage = 42;
  blow.type = 0;
  CHECK(ApplyFrame(FrameOf(Encode(blow)), world, 900) == FrameOutcome::kApplied);
  REQUIRE(world.Find(150'001)->attack.has_value());
  CHECK(world.Find(150'001)->attack->started_ms == 900);
  CHECK(world.Find(150'001)->attack->attack_motion_ms == 500);
  CHECK(world.Find(150'001)->position.direction == 0);
  REQUIRE(world.Find(150'002)->hit.has_value());
  CHECK(world.Find(150'002)->hit->hurt_motion_ms == 288);
  CHECK(world.Find(150'002)->hit->flinches);

  // Un coup encaissé (4) ne fait pas broncher ; un coup raté non plus.
  blow.type = 4;
  CHECK(ApplyFrame(FrameOf(Encode(blow)), world, 1000) == FrameOutcome::kApplied);
  CHECK_FALSE(world.Find(150'002)->hit->flinches);
  blow.type = 0;
  blow.damage = 0;
  CHECK(ApplyFrame(FrameOf(Encode(blow)), world, 1100) == FrameOutcome::kApplied);
  CHECK_FALSE(world.Find(150'002)->hit->flinches);

  // Un coup que le client ne joue pas (6) attend ; un coup entre inconnus
  // ne crée personne.
  blow.type = 6;
  CHECK(ApplyFrame(FrameOf(Encode(blow)), world, 1200) == FrameOutcome::kUnhandled);
  blow.type = 0;
  blow.srcID = 998;
  blow.targetID = 999;
  CHECK(ApplyFrame(FrameOf(Encode(blow)), world, 1300) == FrameOutcome::kIgnored);
  CHECK(world.size() == 2);
}

TEST_CASE("Traduction : un objet au sol entre, tombe, et se ramasse", "[net][update]") {
  World world;
  ro::proto::ZcItemEntry lying;
  lying.AID = 200'001;
  lying.itemId = 501;
  lying.x = 10;
  lying.y = 12;
  lying.subX = 12;
  lying.subY = 4;
  CHECK(ApplyFrame(FrameOf(Encode(lying)), world, 0) == FrameOutcome::kApplied);
  const ro::sim::Actor* potion = world.Find(200'001);
  REQUIRE(potion != nullptr);
  CHECK(potion->kind == ActorKind::kItem);
  REQUIRE(potion->ground_item.has_value());
  CHECK(potion->ground_item->item_id == 501);
  CHECK(potion->position.x == 10);
  CHECK(potion->position.y == 12);
  CHECK(potion->sub_x == 12);
  CHECK(potion->sub_y == 4);

  ro::proto::PacketDropflooritem falling;
  falling.ITAID = 200'002;
  falling.ITID = 512;
  falling.xPos = 11;
  falling.yPos = 12;
  falling.subX = 3;
  falling.subY = 9;
  CHECK(ApplyFrame(FrameOf(Encode(falling)), world, 0) == FrameOutcome::kApplied);
  const ro::sim::Actor* apple = world.Find(200'002);
  REQUIRE(apple != nullptr);
  CHECK(apple->kind == ActorKind::kItem);
  REQUIRE(apple->ground_item.has_value());
  CHECK(apple->ground_item->item_id == 512);
  CHECK(apple->sub_x == 3);

  ro::proto::ZcItemDisappear taken;
  taken.itemAid = 200'001;
  CHECK(ApplyFrame(FrameOf(Encode(taken)), world, 0) == FrameOutcome::kApplied);
  CHECK(world.Find(200'001) == nullptr);
  CHECK(world.size() == 1);
}

TEST_CASE("Traduction : un objet au sol garde ses 32 bits d'identifiant", "[net][update]") {
  // 🔴 L'IDENTIFIANT PASSAIT PAR LE « MÉTIER », qui n'a que 16 bits : au-delà
  // de 65 535, un objet prenait le sprite et le nom d'un autre.
  constexpr std::uint32_t kWideItemId = 400'123;
  World world;
  ro::proto::PacketDropflooritem wide;
  wide.ITAID = 200'003;
  wide.ITID = kWideItemId;
  CHECK(ApplyFrame(FrameOf(Encode(wide)), world, 0) == FrameOutcome::kApplied);
  const ro::sim::Actor* item = world.Find(200'003);
  REQUIRE(item != nullptr);
  REQUIRE(item->ground_item.has_value());
  CHECK(item->ground_item->item_id == kWideItemId);
}

TEST_CASE("Traduction : les états arrivent avec l'acteur, et changent à part", "[net][update]") {
  World world;
  ro::proto::PacketIdleUnit frozen = IdleUnit(150'001, CellPosition{1, 1, 0});
  frozen.bodyState = static_cast<std::int16_t>(ro::sim::kBodyFrozen);
  frozen.healthState = 0x10;
  frozen.effectState = static_cast<std::int32_t>(ro::sim::kOptionCloak);
  // Et le mode PK — le karma du porteur — arrive par le même paquet.
  frozen.isPKModeON = 1;
  REQUIRE(ApplyFrame(FrameOf(Encode(frozen)), world, 300) == FrameOutcome::kApplied);
  CHECK(world.Find(150'001)->body_state == ro::sim::kBodyFrozen);
  CHECK(world.Find(150'001)->body_state_since_ms == 300);
  CHECK(world.Find(150'001)->health_state == 0x10);
  CHECK(world.Find(150'001)->option == ro::sim::kOptionCloak);
  CHECK(world.Find(150'001)->pk_mode);

  ro::proto::ZcStateChange change;
  change.AID = 150'001;
  change.bodyState = 0;
  change.healthState = 0;
  change.effectState = static_cast<std::int32_t>(ro::sim::kOptionHide);
  change.isPKModeON = 0;
  CHECK(ApplyFrame(FrameOf(Encode(change)), world, 900) == FrameOutcome::kApplied);
  CHECK(world.Find(150'001)->body_state == 0);
  CHECK(world.Find(150'001)->body_state_since_ms == 900);
  CHECK(world.Find(150'001)->option == ro::sim::kOptionHide);
  CHECK_FALSE(world.Find(150'001)->pk_mode);
  change.isPKModeON = 1;
  CHECK(ApplyFrame(FrameOf(Encode(change)), world, 950) == FrameOutcome::kApplied);
  CHECK(world.Find(150'001)->pk_mode);
  change.AID = 999;
  CHECK(ApplyFrame(FrameOf(Encode(change)), world, 1000) == FrameOutcome::kIgnored);
}

TEST_CASE("Traduction : un coup porte ses dégâts, ses coups et sa sorte", "[net][update]") {
  // Le monde n'en fait rien ; ce qu'on dessine — les chiffres au-dessus de la
  // cible — en a besoin, et les perdre ici les perdrait pour de bon.
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(150'001, CellPosition{100, 100, 0}))), world, 0) ==
          FrameOutcome::kApplied);
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(150'002, CellPosition{101, 100, 0}))), world, 0) ==
          FrameOutcome::kApplied);

  ro::proto::ZcNotifyAct blow;
  blow.srcID = 150'001;
  blow.targetID = 150'002;
  blow.srcSpeed = 432;
  blow.dmgSpeed = 288;
  blow.damage = 128;
  blow.damage2 = 32;
  blow.div = 2;
  blow.type = 8;
  blow.isSPDamage = 0;
  REQUIRE(ApplyFrame(FrameOf(Encode(blow)), world, 700) == FrameOutcome::kApplied);

  const auto* target = world.Find(150'002);
  REQUIRE(target != nullptr);
  REQUIRE(target->hit.has_value());
  CHECK(target->hit->damage == 128);
  CHECK(target->hit->second_damage == 32);
  CHECK(target->hit->hits == 2);
  CHECK(target->hit->type == 8);
  CHECK_FALSE(target->hit->sp_damage);
  CHECK(target->hit->started_ms == 700);
}

TEST_CASE("Mise à jour : la vie d'un monstre arrive par son paquet", "[net][world]") {
  // Le serveur ne l'envoie que sous un réglage et seulement d'un monstre déjà
  // entamé. Une vie maximale nulle vaut « on ne sait pas » : sans cela, tout
  // monstre intact porterait une jauge vide.
  ro::sim::World world;
  ro::sim::Actor actor;
  actor.id = 11;
  actor.kind = ro::sim::ActorKind::kMonster;
  world.Spawn(actor);

  const std::vector<std::uint8_t> entame = {0x77, 0x09, 0x0B, 0x00, 0x00, 0x00, 0x40,
                                            0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00};
  CHECK(ApplyFrame(FrameOf(entame), world, 0) == FrameOutcome::kApplied);
  REQUIRE(world.Find(11) != nullptr);
  CHECK(world.Find(11)->health == 64);
  CHECK(world.Find(11)->max_health == 100);

  const std::vector<std::uint8_t> unknown = {0x77, 0x09, 0x0B, 0x00, 0x00, 0x00, 0x00,
                                             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  CHECK(ApplyFrame(FrameOf(unknown), world, 0) == FrameOutcome::kApplied);
  CHECK(world.Find(11)->max_health == ro::sim::kUnknownHealth);
}

TEST_CASE("Traduction : les effets de CHAPEAU se posent et se retirent un par un",
          "[net][update]") {
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(150'001, CellPosition{1, 1, 0}))), world, 0) ==
          FrameOutcome::kApplied);

  // ⚠ LA LISTE N'EST PAS UN REMPLACEMENT, et c'est ce que cet essai garde. Le
  // serveur envoie ce paquet de deux façons : la liste ENTIÈRE à l'entrée en vue,
  // et UN seul effet quand un costume se pose ou s'enlève. Le lire comme
  // « voici tout ce qu'il porte » effacerait les autres à chaque changement.
  ro::proto::ZcEquipmentEffect ensemble;
  ensemble.aid = 150'001;
  ensemble.status = 1;
  ensemble.effects = {1, 5, 9};
  CHECK(ApplyFrame(FrameOf(Encode(ensemble)), world, 0) == FrameOutcome::kApplied);
  CHECK(world.Find(150'001)->hat_effects.size() == 3);

  ro::proto::ZcEquipmentEffect retire;
  retire.aid = 150'001;
  retire.status = 0;
  retire.effects = {5};
  CHECK(ApplyFrame(FrameOf(Encode(retire)), world, 0) == FrameOutcome::kApplied);
  CHECK(world.Find(150'001)->hat_effects.size() == 2);
  CHECK(world.Find(150'001)->hat_effects.contains(1));
  CHECK_FALSE(world.Find(150'001)->hat_effects.contains(5));
  CHECK(world.Find(150'001)->hat_effects.contains(9));

  // ⚠ UN EFFET DE CHAPEAU N'EST PAS UN ÉTAT. Les deux numérotations n'ont rien à
  // voir — l'une est un rang dans `HatEFID`, l'autre un EFST du serveur — et les
  // confondre ferait jouer l'effet d'un état homonyme.
  CHECK(world.Find(150'001)->status_effects.empty());

  // Un inconnu n'est pas créé.
  ensemble.aid = 999;
  CHECK(ApplyFrame(FrameOf(Encode(ensemble)), world, 0) == FrameOutcome::kIgnored);
}
