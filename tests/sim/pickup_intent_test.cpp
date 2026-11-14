#include "ro/sim/pickup_intent.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/pending_action.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"

using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::AdvancePickup;
using ro::sim::CellMovement;
using ro::sim::CellPosition;
using ro::sim::kPickupReachCells;
using ro::sim::kWalkAnswerGraceMs;
using ro::sim::PickupIntent;
using ro::sim::PickupStep;
using ro::sim::StandingCell;
using ro::sim::StraightWalkPath;
using ro::sim::WalkPath;
using ro::sim::WithinPickupReach;
using ro::sim::World;

namespace {

constexpr std::uint32_t kPlayerId = 150'001;
constexpr std::uint32_t kItemId = 700'001;
constexpr std::uint16_t kSpeedMsPerCell = 150;

Actor Player(CellPosition where) {
  Actor actor;
  actor.id = kPlayerId;
  actor.kind = ActorKind::kPlayer;
  actor.position = where;
  actor.speed_ms_per_cell = kSpeedMsPerCell;
  return actor;
}

Actor Item(CellPosition where) {
  Actor actor;
  actor.id = kItemId;
  actor.kind = ActorKind::kItem;
  actor.position = where;
  return actor;
}

// Un monde où le joueur se tient en (100, 100) et l'objet en (110, 100), dix
// cases plus à l'est.
World FarApart() {
  World world;
  world.set_player_id(kPlayerId);
  world.Spawn(Player(CellPosition{100, 100, 0}));
  world.Spawn(Item(CellPosition{110, 100, 0}));
  return world;
}

}  // namespace

TEST_CASE("Ramassage : la portée est de deux cases, dans les deux axes à la fois",
          "[sim][pickup]") {
  // C'est check_distance_bl : la plus grande des deux différences, pas leur
  // somme. En diagonale de deux, on tient encore ; à trois, plus.
  const CellPosition here{100, 100, 0};
  CHECK(WithinPickupReach(here, here));
  CHECK(WithinPickupReach(here, CellPosition{102, 102, 0}));
  CHECK(WithinPickupReach(here, CellPosition{98, 100, 0}));
  CHECK_FALSE(WithinPickupReach(here, CellPosition{103, 100, 0}));
  CHECK_FALSE(WithinPickupReach(here, CellPosition{100, 97, 0}));
  CHECK(kPickupReachCells == 2);
}

TEST_CASE("Ramassage : on se tient à l'arrivée de sa marche, sinon sur sa case", "[sim][pickup]") {
  Actor actor = Player(CellPosition{100, 100, 0});
  CHECK(StandingCell(actor).x == 100);
  actor.movement = CellMovement{CellPosition{100, 100, 0}, CellPosition{110, 100, 0}, 8, 8};
  CHECK(StandingCell(actor).x == 110);
}

TEST_CASE("Ramassage : un objet disparu abandonne l'intention", "[sim][pickup]") {
  World world = FarApart();
  world.Remove(kItemId);
  const WalkPath none;
  CHECK(AdvancePickup(world, none, PickupIntent{kItemId, 0}, 10) == PickupStep::kGiveUp);
}

TEST_CASE("Ramassage : déjà à portée, on prend tout de suite", "[sim][pickup]") {
  World world;
  world.set_player_id(kPlayerId);
  world.Spawn(Player(CellPosition{100, 100, 0}));
  world.Spawn(Item(CellPosition{101, 99, 0}));
  const WalkPath none;
  CHECK(AdvancePickup(world, none, PickupIntent{kItemId, 0}, 0) == PickupStep::kPickUp);
}

TEST_CASE("Ramassage : on attend la fin de la marche, puis on prend", "[sim][pickup]") {
  // Le clic à l'instant zéro ; le serveur annonce la marche à cinquante, dix
  // cases vers l'est, qui dure mille cinq cents millisecondes.
  World world = FarApart();
  const CellMovement walk{CellPosition{100, 100, 0}, CellPosition{110, 100, 0}, 8, 8};
  REQUIRE(world.StartMoving(kPlayerId, walk, 50));
  const WalkPath path = StraightWalkPath(walk.from, walk.to);
  const PickupIntent intent{kItemId, 0};

  CHECK(AdvancePickup(world, path, intent, 60) == PickupStep::kWait);
  CHECK(AdvancePickup(world, path, intent, 1'000) == PickupStep::kWait);
  CHECK(AdvancePickup(world, path, intent, 50 + 10 * kSpeedMsPerCell + 1) == PickupStep::kPickUp);
}

TEST_CASE("Ramassage : arrivé trop loin, on renonce", "[sim][pickup]") {
  // Le serveur a fait marcher le joueur ailleurs — vers l'ouest — depuis le
  // clic : la marche demandée n'est pas celle-là, et il n'y en aura pas d'autre.
  World world = FarApart();
  const CellMovement elsewhere{CellPosition{100, 100, 0}, CellPosition{95, 100, 0}, 8, 8};
  REQUIRE(world.StartMoving(kPlayerId, elsewhere, 50));
  const WalkPath path = StraightWalkPath(elsewhere.from, elsewhere.to);
  const PickupIntent intent{kItemId, 0};

  CHECK(AdvancePickup(world, path, intent, 100) == PickupStep::kWait);
  CHECK(AdvancePickup(world, path, intent, 50 + 5 * kSpeedMsPerCell + 1) == PickupStep::kGiveUp);
}

TEST_CASE("Ramassage : sans réponse du serveur, on renonce après le délai", "[sim][pickup]") {
  // Une marche refusée ne se dit pas : le joueur reste planté, et seule
  // l'attente permet de conclure.
  World world = FarApart();
  const WalkPath none;
  const PickupIntent intent{kItemId, 200};
  CHECK(AdvancePickup(world, none, intent, 200) == PickupStep::kWait);
  CHECK(AdvancePickup(world, none, intent, 200 + kWalkAnswerGraceMs - 1) == PickupStep::kWait);
  CHECK(AdvancePickup(world, none, intent, 200 + kWalkAnswerGraceMs) == PickupStep::kGiveUp);
}

TEST_CASE("Ramassage : une marche d'AVANT le clic ne compte pas pour une réponse",
          "[sim][pickup]") {
  // Le joueur marchait déjà vers l'ouest quand il a cliqué ; arrivé, il est
  // loin de l'objet, mais cette marche-là ne dit rien de la demande : on laisse
  // au serveur son délai.
  World world = FarApart();
  const CellMovement before{CellPosition{100, 100, 0}, CellPosition{99, 100, 0}, 8, 8};
  REQUIRE(world.StartMoving(kPlayerId, before, 10));
  const WalkPath path = StraightWalkPath(before.from, before.to);
  const PickupIntent intent{kItemId, 300};
  CHECK(AdvancePickup(world, path, intent, 400) == PickupStep::kWait);
  CHECK(AdvancePickup(world, path, intent, 300 + kWalkAnswerGraceMs) == PickupStep::kGiveUp);
}

// ── L'ACTION EN ATTENTE ─────────────────────────────────────────────────────
//
// La seconde intention du client, et de la même famille que le ramassage : le
// serveur n'en sait rien, et c'est le client qui marche avant d'émettre. Voir
// `ro/sim/pending_action.h`.

namespace {

constexpr std::uint32_t kMonsterId = 300'001;
// Une carte carrée entièrement franchissable ; `Bare` en perce une case.
constexpr std::size_t kTestMapSide = 16;

ro::sim::WalkMap OpenGround() {
  std::vector<std::string> rows(kTestMapSide, std::string(kTestMapSide, '.'));
  std::vector<std::string_view> views;
  views.reserve(rows.size());
  for (const std::string& row : rows) {
    views.emplace_back(row);
  }
  return ro::sim::WalkMap::FromPicture(views);
}

// La même, avec une case murée. ⚠ La PREMIÈRE ligne du dessin est le NORD :
// la ligne d'indice `y` se lit donc à `côté - 1 - y`.
ro::sim::WalkMap GroundWithWall(std::uint16_t wall_x, std::uint16_t wall_y) {
  std::vector<std::string> rows(kTestMapSide, std::string(kTestMapSide, '.'));
  rows[kTestMapSide - 1 - wall_y][wall_x] = '#';
  std::vector<std::string_view> views;
  views.reserve(rows.size());
  for (const std::string& row : rows) {
    views.emplace_back(row);
  }
  return ro::sim::WalkMap::FromPicture(views);
}

Actor Monster(CellPosition where) {
  Actor actor;
  actor.id = kMonsterId;
  actor.kind = ActorKind::kMonster;
  actor.position = where;
  actor.speed_ms_per_cell = kSpeedMsPerCell;
  return actor;
}

// Le joueur en (2, 8), un monstre dix cases plus à l'est.
World TenCellsApart() {
  World world;
  world.set_player_id(kPlayerId);
  world.Spawn(Player(CellPosition{2, 8, 0}));
  world.Spawn(Monster(CellPosition{12, 8, 0}));
  return world;
}

ro::sim::PendingAction BashOn(std::uint32_t target_id, std::uint16_t range_cells) {
  ro::sim::PendingAction action;
  action.target_id = target_id;
  action.skill_id = 5;
  action.skill_level = 10;
  action.range_cells = range_cells;
  return action;
}

}  // namespace

TEST_CASE("Attente : la case d'approche est au bord de la portée, du côté du lanceur",
          "[sim][pending]") {
  // Le client pose le point à `portée` cases de la CIBLE, dans la direction du
  // demandeur — il ne s'arrête donc pas sur la case du monstre.
  const ro::sim::WalkMap ground = OpenGround();
  const CellPosition where = ro::sim::ApproachCell({2.0F, 8.0F}, {12.0F, 8.0F}, 2, ground);
  CHECK(where.x == 10);
  CHECK(where.y == 8);
}

TEST_CASE("Attente : à portée nulle, on vise la case de la cible", "[sim][pending]") {
  const ro::sim::WalkMap ground = OpenGround();
  const CellPosition where = ro::sim::ApproachCell({2.0F, 8.0F}, {12.0F, 8.0F}, 0, ground);
  CHECK(where.x == 12);
  CHECK(where.y == 8);
}

TEST_CASE("Attente : une case d'approche murée se replie vers l'EST d'abord", "[sim][pending]") {
  // L'ordre de repli du client n'est pas un tour de cadran : est, nord-est,
  // sud-est, ouest… Le premier essai part donc VERS la cible.
  const ro::sim::WalkMap ground = GroundWithWall(10, 8);
  const CellPosition where = ro::sim::ApproachCell({2.0F, 8.0F}, {12.0F, 8.0F}, 2, ground);
  CHECK(where.x == 11);
  CHECK(where.y == 8);
}

TEST_CASE("Attente : sur la case de la cible, on ne bouge pas", "[sim][pending]") {
  // Le client y divise par zéro et lit une case indéterminée ; nous restons.
  const ro::sim::WalkMap ground = OpenGround();
  const CellPosition where = ro::sim::ApproachCell({7.0F, 7.0F}, {7.0F, 7.0F}, 3, ground);
  CHECK(where.x == 7);
  CHECK(where.y == 7);
}

TEST_CASE("Attente : la portée s'étire d'un dixième de case au-delà d'une case entière",
          "[sim][pending]") {
  // `5 × portée + 5,5` en unités du monde, cinq unités par case : la tolérance
  // vaut 1,1 case et pas une.
  CHECK(ro::sim::WithinSkillRange({0.0F, 0.0F}, {3.0F, 0.0F}, 2));
  CHECK(ro::sim::WithinSkillRange({0.0F, 0.0F}, {3.1F, 0.0F}, 2));
  CHECK_FALSE(ro::sim::WithinSkillRange({0.0F, 0.0F}, {3.5F, 0.0F}, 2));
  // En diagonale, c'est bien la distance EUCLIDIENNE : deux cases sur chaque
  // axe en font 2,83, que la portée deux couvre encore.
  CHECK(ro::sim::WithinSkillRange({0.0F, 0.0F}, {2.0F, 2.0F}, 2));
  CHECK_FALSE(ro::sim::WithinSkillRange({0.0F, 0.0F}, {3.0F, 3.0F}, 2));
}

TEST_CASE("Attente : hors de portée, on marche jusqu'au bord de la portée", "[sim][pending]") {
  World world = TenCellsApart();
  const ro::sim::WalkMap ground = OpenGround();
  ro::sim::WalkPlanner planner(&ground);
  ro::sim::ApproachWalkClock clock;
  ro::sim::PendingAction action = BashOn(kMonsterId, 2);

  const ro::sim::PendingMove first =
      ro::sim::AdvancePendingAction(world, planner, ground, action, clock, 1000);
  CHECK(first.step == ro::sim::PendingStep::kApproach);
  CHECK(first.toward.x == 10);
  CHECK(first.toward.y == 8);
  // La case demandée est retenue : la redemander à chaque image noierait le
  // serveur sous des marches identiques.
  REQUIRE(action.walking_to.has_value());
  CHECK(action.walking_to->x == 10);
  const ro::sim::PendingMove again =
      ro::sim::AdvancePendingAction(world, planner, ground, action, clock, 1010);
  CHECK(again.step == ro::sim::PendingStep::kWait);
}

TEST_CASE("Attente : une compétence au sol s'approche de sa CASE, puis se lâche",
          "[sim][pending]") {
  // 🔴 LE SERVEUR IGNORE EN SILENCE UNE DEMANDE AU SOL HORS DE PORTÉE : Volcano
  // ou Hammerfall, à une ou deux cases, ne partiraient presque jamais. La case
  // visée s'approche donc comme une cible, et aucune entité n'y est requise.
  World world;
  world.set_player_id(kPlayerId);
  world.Spawn(Player(CellPosition{2, 8, 0}));
  const ro::sim::WalkMap ground = OpenGround();
  ro::sim::WalkPlanner planner(&ground);
  ro::sim::ApproachWalkClock clock;
  ro::sim::PendingAction action;
  action.skill_id = 285;
  action.skill_level = 5;
  action.range_cells = 2;
  action.ground_cell = CellPosition{12, 8, 0};

  const ro::sim::PendingMove first =
      ro::sim::AdvancePendingAction(world, planner, ground, action, clock, 1000);
  CHECK(first.step == ro::sim::PendingStep::kApproach);
  CHECK(first.toward.x == 10);
  CHECK(first.toward.y == 8);

  // Arrivé au bord de la portée : la demande part.
  World near;
  near.set_player_id(kPlayerId);
  near.Spawn(Player(CellPosition{10, 8, 0}));
  ro::sim::PendingAction there = action;
  there.walking_to.reset();
  CHECK(ro::sim::AdvancePendingAction(near, planner, ground, there, clock, 2000).step ==
        ro::sim::PendingStep::kRelease);
}

TEST_CASE("Attente : une marche d'approche trop proche de la précédente tombe", "[sim][pending]") {
  // Le client du joueur ne laisse pas partir deux marches d'approche à moins de
  // cent millisecondes, et retient la case visée même quand la marche tombe.
  World world = TenCellsApart();
  const ro::sim::WalkMap ground = OpenGround();
  ro::sim::WalkPlanner planner(&ground);
  ro::sim::ApproachWalkClock clock;
  ro::sim::PendingAction action = BashOn(kMonsterId, 2);
  CHECK(ro::sim::AdvancePendingAction(world, planner, ground, action, clock, 1000).step ==
        ro::sim::PendingStep::kApproach);

  // La case visée « a changé » : sans plancher, la marche repartirait.
  action.walking_to = CellPosition{9, 8, 0};
  CHECK(ro::sim::AdvancePendingAction(world, planner, ground, action, clock, 1000 + 50).step ==
        ro::sim::PendingStep::kWait);
  REQUIRE(action.walking_to.has_value());
  CHECK(action.walking_to->x == 10);

  action.walking_to = CellPosition{9, 8, 0};
  const std::uint32_t after_floor =
      1000 + static_cast<std::uint32_t>(ro::sim::kApproachWalkFloorMs) + 1;
  CHECK(ro::sim::AdvancePendingAction(world, planner, ground, action, clock, after_floor).step ==
        ro::sim::PendingStep::kApproach);

  // 🔴 L'HORLOGE EST CELLE DU PERSONNAGE : une action NEUVE, armée juste après,
  // tombe sur le même plancher.
  ro::sim::PendingAction fresh = BashOn(kMonsterId, 2);
  CHECK(ro::sim::AdvancePendingAction(world, planner, ground, fresh, clock, after_floor + 1).step ==
        ro::sim::PendingStep::kWait);

  // Et elle repart avec le personnage, à l'entrée sur une carte.
  world.EnterMap("prontera", 2, 8);
  world.Spawn(Monster(CellPosition{12, 8, 0}));
  ro::sim::PendingAction arrived = BashOn(kMonsterId, 2);
  CHECK(
      ro::sim::AdvancePendingAction(world, planner, ground, arrived, clock, after_floor + 2).step ==
      ro::sim::PendingStep::kApproach);
}

TEST_CASE("Marche : le clic au sol et la poursuite passent la même porte", "[sim][pending]") {
  // Le client n'a qu'un chemin vers le paquet de marche, et le plancher est
  // posé dessus : un clic au sol retient la poursuite qui le suit de trop près,
  // et une poursuite retient le clic. Sous confusion, chaque marche partie est
  // un nouveau tirage de direction chez le serveur.
  World world = TenCellsApart();
  const ro::sim::WalkMap ground = OpenGround();
  ro::sim::WalkPlanner planner(&ground);
  ro::sim::ApproachWalkClock clock;

  CHECK(ro::sim::AdmitWalkRequest(world, clock, 1000));
  ro::sim::PendingAction action = BashOn(kMonsterId, 2);
  CHECK(ro::sim::AdvancePendingAction(world, planner, ground, action, clock, 1000 + 50).step ==
        ro::sim::PendingStep::kWait);

  // À égalité avec le plancher, la marche tombe encore : il faut le DÉPASSER.
  const std::uint32_t at_floor = 1000 + ro::sim::kApproachWalkFloorMs;
  CHECK_FALSE(ro::sim::AdmitWalkRequest(world, clock, at_floor));
  CHECK(ro::sim::AdmitWalkRequest(world, clock, at_floor + 1));

  // Et un second clic, cinquante millisecondes après le premier, est perdu.
  CHECK_FALSE(ro::sim::AdmitWalkRequest(world, clock, at_floor + 1 + 50));
}

TEST_CASE("Marche : celle d'un objet que le plancher retient se redemande au tic suivant",
          "[sim][pending][pickup]") {
  World world = FarApart();
  const WalkPath none;
  ro::sim::ApproachWalkClock clock;
  CHECK(ro::sim::AdmitWalkRequest(world, clock, 1000));

  // Le clic sur l'objet arrive vingt millisecondes après une autre marche : la
  // sienne attend, et l'intention ne s'abandonne pas pour autant.
  PickupIntent intent{
      .item_id = kItemId, .since_ms = 1020, .walk_toward = CellPosition{110, 100, 0}};
  CHECK_FALSE(ro::sim::DuePickupWalk(world, intent, clock, 1020).has_value());
  CHECK(intent.walk_toward.has_value());
  CHECK(AdvancePickup(world, none, intent, 1020 + kWalkAnswerGraceMs) == PickupStep::kWait);

  // Le plancher passé, elle part, une seule fois, et le délai de réponse repart
  // d'elle.
  const std::uint32_t due_ms = 1000 + ro::sim::kApproachWalkFloorMs + 1;
  const auto toward = ro::sim::DuePickupWalk(world, intent, clock, due_ms);
  REQUIRE(toward.has_value());
  CHECK(toward->x == 110);
  CHECK_FALSE(intent.walk_toward.has_value());
  CHECK(intent.since_ms == due_ms);
  CHECK_FALSE(ro::sim::DuePickupWalk(world, intent, clock, due_ms + 500).has_value());
  // Et elle compte pour la marche suivante.
  CHECK_FALSE(ro::sim::AdmitWalkRequest(world, clock, due_ms + 50));
}

TEST_CASE("Attente : à portée, la compétence part", "[sim][pending]") {
  World world;
  world.set_player_id(kPlayerId);
  world.Spawn(Player(CellPosition{10, 8, 0}));
  world.Spawn(Monster(CellPosition{12, 8, 0}));
  const ro::sim::WalkMap ground = OpenGround();
  ro::sim::WalkPlanner planner(&ground);
  ro::sim::ApproachWalkClock clock;
  ro::sim::PendingAction action = BashOn(kMonsterId, 2);
  const ro::sim::PendingMove move =
      ro::sim::AdvancePendingAction(world, planner, ground, action, clock, 1000);
  CHECK(move.step == ro::sim::PendingStep::kRelease);
}

TEST_CASE("Attente : arrivé sur la case visée, on lance même hors de portée", "[sim][pending]") {
  // Le second critère de tir. Sans lui, une cible qu'on ne peut pas approcher
  // davantage ferait piétiner sans fin.
  World world = TenCellsApart();
  const ro::sim::WalkMap ground = OpenGround();
  ro::sim::WalkPlanner planner(&ground);
  ro::sim::ApproachWalkClock clock;
  ro::sim::PendingAction action = BashOn(kMonsterId, 2);
  action.walking_to = CellPosition{2, 8, 0};
  const ro::sim::PendingMove move =
      ro::sim::AdvancePendingAction(world, planner, ground, action, clock, 1000);
  CHECK(move.step == ro::sim::PendingStep::kRelease);
}

TEST_CASE("Attente : une cible disparue fait abandonner, une compétence vise un mort",
          "[sim][pending]") {
  const ro::sim::WalkMap ground = OpenGround();
  ro::sim::WalkPlanner planner(&ground);
  ro::sim::ApproachWalkClock clock;

  World world = TenCellsApart();
  ro::sim::PendingAction absent = BashOn(kMonsterId + 1, 2);
  CHECK(ro::sim::AdvancePendingAction(world, planner, ground, absent, clock, 1000).step ==
        ro::sim::PendingStep::kGiveUp);

  // 🔴 LE CAS DE LA COMPÉTENCE CIBLÉE NE LIT PAS L'ÉTAT DE SA CIBLE : on
  // marche vers le mort pour le relever. Seule l'attaque renonce sur un cadavre.
  world.SetPosture(kMonsterId, ro::sim::Posture::kDead, 1000);
  constexpr std::uint16_t kResurrection = 54;
  ro::sim::PendingAction raise = BashOn(kMonsterId, 2);
  raise.skill_id = kResurrection;
  CHECK(ro::sim::AdvancePendingAction(world, planner, ground, raise, clock, 1000).step ==
        ro::sim::PendingStep::kApproach);

  ro::sim::PendingAction strike;
  strike.target_id = kMonsterId;
  strike.range_cells = 2;
  CHECK(ro::sim::AdvancePendingAction(world, planner, ground, strike, clock, 1000).step ==
        ro::sim::PendingStep::kGiveUp);
}

// ── LA POURSUITE D'UNE ATTAQUE ──────────────────────────────────────────────
//
// Elle passe par la même machine, et s'en distingue par UNE chose : une
// cadence de relance de la marche, que la compétence n'a pas. Voir
// `ro/sim/pending_action.h`.

namespace {

// Un coup ordinaire : pas de compétence, et la cadence que le genre décide.
ro::sim::PendingAction StrikeOn(std::uint32_t target_id, std::uint16_t range_cells,
                                std::uint32_t chase_repeat_ms) {
  ro::sim::PendingAction action;
  action.target_id = target_id;
  action.range_cells = range_cells;
  action.chase_repeat_ms = chase_repeat_ms;
  return action;
}

}  // namespace

TEST_CASE("Poursuite : le genre se décide sur les deux vitesses", "[sim][pending]") {
  // `vitesse × 4 / 3 >= vitesse de la cible` donne la cadence rapide. Une
  // vitesse est un TEMPS par case : le grand nombre est le lent.
  CHECK(ro::sim::ChaseRepeatMs(150, 150) == ro::sim::kChaseRepeatQuickMs);
  // 150 × 4 / 3 = 200, qui couvre encore une cible à 200.
  CHECK(ro::sim::ChaseRepeatMs(150, 200) == ro::sim::kChaseRepeatQuickMs);
  // Mais pas une à 201 : la cible est alors bien plus lente que nous.
  CHECK(ro::sim::ChaseRepeatMs(150, 201) == ro::sim::kChaseRepeatSlowMs);
  // « Pas encore dite » ne se devine pas : on parle le moins possible.
  CHECK(ro::sim::ChaseRepeatMs(0, 150) == ro::sim::kChaseRepeatSlowMs);
  CHECK(ro::sim::ChaseRepeatMs(150, 0) == ro::sim::kChaseRepeatSlowMs);
}

TEST_CASE("Poursuite : la première marche part tout de suite", "[sim][pending]") {
  // La cadence ne retient que les RELANCES : un clic sur une cible lointaine
  // ne doit pas attendre presque une demi-seconde avant le premier pas.
  World world = TenCellsApart();
  const ro::sim::WalkMap ground = OpenGround();
  ro::sim::WalkPlanner planner(&ground);
  ro::sim::ApproachWalkClock clock;
  ro::sim::PendingAction action = StrikeOn(kMonsterId, 1, ro::sim::kChaseRepeatQuickMs);
  const ro::sim::PendingMove first =
      ro::sim::AdvancePendingAction(world, planner, ground, action, clock, 0);
  CHECK(first.step == ro::sim::PendingStep::kApproach);
  CHECK(first.toward.x == 11);
}

TEST_CASE("Poursuite : une cible qui bouge ne relance la marche qu'à la cadence",
          "[sim][pending]") {
  World world = TenCellsApart();
  const ro::sim::WalkMap ground = OpenGround();
  ro::sim::WalkPlanner planner(&ground);
  ro::sim::ApproachWalkClock clock;
  ro::sim::PendingAction action = StrikeOn(kMonsterId, 1, ro::sim::kChaseRepeatQuickMs);
  REQUIRE(ro::sim::AdvancePendingAction(world, planner, ground, action, clock, 1000).step ==
          ro::sim::PendingStep::kApproach);

  // La cible s'éloigne : la case d'approche change, ce qui suffirait à relancer
  // une compétence. Pour un coup, la cadence retient la demande.
  world.PlaceAt(kMonsterId, CellPosition{13, 8, 0});
  CHECK(ro::sim::AdvancePendingAction(world, planner, ground, action, clock, 1100).step ==
        ro::sim::PendingStep::kWait);
  // Et la case retenue n'a pas bougé : la marche d'après doit bien repartir.
  REQUIRE(action.walking_to.has_value());
  CHECK(action.walking_to->x == 11);

  const ro::sim::PendingMove relance = ro::sim::AdvancePendingAction(
      world, planner, ground, action, clock, 1000 + ro::sim::kChaseRepeatQuickMs);
  CHECK(relance.step == ro::sim::PendingStep::kApproach);
  CHECK(relance.toward.x == 12);
}

TEST_CASE("Poursuite : la compétence n'a aucune cadence", "[sim][pending]") {
  // Le même scénario, avec une compétence : la relance est immédiate. C'est
  // mesuré — le cas 3 du tic ne compare aucune horloge.
  World world = TenCellsApart();
  const ro::sim::WalkMap ground = OpenGround();
  ro::sim::WalkPlanner planner(&ground);
  ro::sim::ApproachWalkClock clock;
  ro::sim::PendingAction action = BashOn(kMonsterId, 1);
  REQUIRE(ro::sim::AdvancePendingAction(world, planner, ground, action, clock, 1000).step ==
          ro::sim::PendingStep::kApproach);

  // ⚠ SEUL LE PLANCHER DE LA MARCHE la retient — voir `kApproachWalkFloorMs` :
  // il vaut pour toute approche, et il est bien plus court qu'une cadence.
  world.PlaceAt(kMonsterId, CellPosition{13, 8, 0});
  const ro::sim::PendingMove relance = ro::sim::AdvancePendingAction(
      world, planner, ground, action, clock,
      1000 + static_cast<std::uint32_t>(ro::sim::kApproachWalkFloorMs) + 1);
  CHECK(relance.step == ro::sim::PendingStep::kApproach);
  CHECK(relance.toward.x == 12);
}

TEST_CASE("Attente : un bloc d'Ice Wall se vise par sa case, et son retrait fait renoncer",
          "[sim][pending]") {
  // Le client range l'unité au sol dans sa propre liste et en lit la case ;
  // sans elle, la poursuite abandonnait avant le premier pas.
  constexpr std::uint32_t kIceBlockId = 880'001;
  constexpr std::uint32_t kIceWallUnit = 141;
  World world;
  world.set_player_id(kPlayerId);
  world.Spawn(Player(CellPosition{2, 8, 0}));
  world.PlaceSkillUnit(ro::sim::SkillUnit{
      .id = kIceBlockId, .cell = CellPosition{12, 8, 0}, .unit_id = kIceWallUnit});
  const ro::sim::WalkMap ground = OpenGround();
  ro::sim::WalkPlanner planner(&ground);
  ro::sim::ApproachWalkClock clock;
  ro::sim::PendingAction action;
  action.target_id = kIceBlockId;
  constexpr std::uint16_t kGroundUnitReachCells = 5;
  action.range_cells = kGroundUnitReachCells;
  const ro::sim::PendingMove first =
      ro::sim::AdvancePendingAction(world, planner, ground, action, clock, 1000);
  CHECK(first.step == ro::sim::PendingStep::kApproach);
  CHECK(first.toward.x == 7);
  CHECK(first.toward.y == 8);

  REQUIRE(world.RemoveSkillUnit(kIceBlockId));
  CHECK(ro::sim::AdvancePendingAction(world, planner, ground, action, clock, 1010).step ==
        ro::sim::PendingStep::kGiveUp);
}
