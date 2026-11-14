// TOURNER SUR PLACE — et les deux choses qui ne se devinent pas.
//
// 🔴 LE DÉPLACEMENT ANNONCÉ SURVIT À L'ARRIVÉE, et c'est voulu : c'est lui qui
// garde la direction du dernier pas. Mais il dictait aussi le regard d'un
// acteur qui tournait ENSUITE, si bien que seule la tête bougeait — le corps
// restait figé sur le dernier pas d'une marche terminée depuis longtemps.
//
// 🔴 ET UN DEMI-TOUR NE VA PAS D'UN COUP À LA CASE VISÉE. Le client natif
// rapproche le corps jusqu'à un huitième du but et confie le dernier huitième
// au REGARD ; viser à quatre-vingt-dix degrés demande donc DEUX demandes.

#include <array>
#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/sim/movement.h"
#include "ro/sim/world.h"

using ro::sim::Actor;
using ro::sim::CellMovement;
using ro::sim::CellPosition;
using ro::sim::Facing;
using ro::sim::FacingAt;
using ro::sim::kGazeAhead;
using ro::sim::kGazeDownward;
using ro::sim::kGazeUpward;
using ro::sim::TurnToward;
using ro::sim::World;

namespace {

constexpr std::uint32_t kWalkerId = 7;
constexpr std::uint16_t kCellMs = 150;
// Les directions du serveur : zéro au nord, et la suite tourne à l'envers des
// aiguilles d'une montre.
constexpr std::uint8_t kNorth = 0;
constexpr std::uint8_t kWest = 2;
constexpr std::uint8_t kSouth = 4;
constexpr std::uint8_t kEast = 6;

// Quelqu'un, posé là, et rien de plus : les essais du REGARD n'ont besoin que
// de sa case et de son identifiant.
Actor Someone(std::uint32_t id, CellPosition where) {
  Actor actor;
  actor.id = id;
  actor.position = where;
  return actor;
}

// Un acteur posé en 100,100 qui vient de marcher d'une case vers l'est.
World WorldAfterAWalk() {
  World world;
  Actor actor;
  actor.id = kWalkerId;
  actor.position = CellPosition{100, 100, kNorth};
  actor.speed_ms_per_cell = kCellMs;
  world.Spawn(actor);
  world.StartMoving(
      kWalkerId, CellMovement{CellPosition{100, 100, kNorth}, CellPosition{101, 100, kNorth}, 8, 8},
      1000);
  return world;
}

}  // namespace

TEST_CASE("Demi-tour : le CORPS tourne, et pas seulement la tête", "[sim][turn]") {
  // 🔴 LE DÉFAUT QUE CE BANC GARDE. Avant, le regard d'un acteur arrivé venait
  // de la queue de son chemin — que rien n'efface — et la direction posée par
  // le paquet n'était visible que sur la tête. Un joueur qui tournait sur place
  // ne bougeait que les yeux.
  World world = WorldAfterAWalk();
  const std::uint32_t arrived_ms = 1000 + kCellMs + 1;
  const Actor* walker = world.Find(kWalkerId);
  REQUIRE(walker != nullptr);
  const std::uint8_t walked = FacingAt(*walker, arrived_ms);
  CHECK(walked != kSouth);

  REQUIRE(world.Turn(kWalkerId, kSouth, kGazeUpward));
  walker = world.Find(kWalkerId);
  REQUIRE(walker != nullptr);
  CHECK(FacingAt(*walker, arrived_ms) == kSouth);
  CHECK(walker->head_direction == kGazeUpward);
}

TEST_CASE("Demi-tour : le PAS EN COURS garde le regard", "[sim][turn]") {
  // ⚠ ET C'EST L'AUTRE MOITIÉ DE LA RÈGLE. Le serveur repose la direction de
  // l'unité à chaque case franchie ; un demi-tour demandé en marchant ne doit
  // pas figer le corps à contresens pendant tout le trajet.
  World world = WorldAfterAWalk();
  REQUIRE(world.Turn(kWalkerId, kSouth, kGazeAhead));
  const Actor* walker = world.Find(kWalkerId);
  REQUIRE(walker != nullptr);
  // À mi-chemin, le pas commande.
  CHECK(FacingAt(*walker, 1000 + kCellMs / 2) != kSouth);
  // Arrivé, le demi-tour reprend la main.
  CHECK(FacingAt(*walker, 1000 + kCellMs + 1) == kSouth);
}

TEST_CASE("Demi-tour : une marche neuve reprend la main sur le regard", "[sim][turn]") {
  World world = WorldAfterAWalk();
  REQUIRE(world.Turn(kWalkerId, kSouth, kGazeAhead));
  world.StartMoving(
      kWalkerId, CellMovement{CellPosition{101, 100, kSouth}, CellPosition{101, 101, kSouth}, 8, 8},
      2000);
  const Actor* walker = world.Find(kWalkerId);
  REQUIRE(walker != nullptr);
  // Le dernier pas de la NOUVELLE marche décide, pas le demi-tour d'avant.
  CHECK(FacingAt(*walker, 2000 + kCellMs + 1) == kNorth);
}

TEST_CASE("Marche : un ordre vers SA PROPRE case ne met pas dos à la caméra",
          "[sim][turn][movement]") {
  // 🔴 LE SYMPTÔME, ET IL ARRIVE TOUT SEUL. La marche tenue se redemande toutes
  // les cent millisecondes vers la case sous la souris ; sur un pas d'UNE case,
  // on y est déjà arrivé, et le serveur répond par un déplacement dont les deux
  // bouts sont la même case. Le chemin est alors VIDE, le regard se replie sur
  // `position.direction` — et ce champ valait ZÉRO, le NORD, parce qu'un
  // déplacement ne porte aucune direction et qu'on l'affectait en bloc.
  World world;
  Actor actor;
  actor.id = kWalkerId;
  actor.position = CellPosition{100, 100, kNorth};
  actor.speed_ms_per_cell = kCellMs;
  world.Spawn(actor);

  // Un pas vers l'EST, terminé.
  world.StartMoving(
      kWalkerId, CellMovement{CellPosition{100, 100, kNorth}, CellPosition{101, 100, kNorth}, 8, 8},
      1000);
  const std::uint8_t est = FacingAt(*world.Find(kWalkerId), 1000 + kCellMs + 1);
  REQUIRE(est != kNorth);

  // La redemande, arrivée sur la case qu'on occupe déjà.
  world.StartMoving(
      kWalkerId, CellMovement{CellPosition{101, 100, kNorth}, CellPosition{101, 100, kNorth}, 8, 8},
      1200);
  CHECK(FacingAt(*world.Find(kWalkerId), 1200 + kCellMs + 1) == est);
}

TEST_CASE("Demi-tour : partir en marche REDRESSE la tête", "[sim][turn]") {
  // 🔴 ET LE SERVEUR NE LE DIT À PERSONNE. `unit_walktoxy_sub` écrit
  // `sd->head_dir = DIR_NORTH` au départ de chaque marche d'un joueur, et aucun
  // paquet de marche ne porte le regard : rien sur le fil ne l'annonce.
  //
  // ⚠ CE N'EST PAS QU'UNE AFFAIRE D'IMAGE. Une tête gardée de biais ici ferait
  // partir la prochaine demande de direction d'un regard que le serveur a déjà
  // remis à zéro, et le demi-tour s'y compterait de travers — la seconde
  // demande n'achèverait plus le tour.
  World world;
  Actor actor;
  actor.id = kWalkerId;
  actor.position = CellPosition{100, 100, kNorth};
  actor.speed_ms_per_cell = kCellMs;
  world.Spawn(actor);
  REQUIRE(world.Turn(kWalkerId, kNorth, kGazeUpward));
  REQUIRE(world.Find(kWalkerId)->head_direction == kGazeUpward);

  world.StartMoving(
      kWalkerId, CellMovement{CellPosition{100, 100, kNorth}, CellPosition{101, 100, kNorth}, 8, 8},
      1000);
  CHECK(world.Find(kWalkerId)->head_direction == kGazeAhead);
}

TEST_CASE("Demi-tour : un huitième ne bouge que la TÊTE", "[sim][turn]") {
  // 🔴 C'est la règle du client natif, et elle explique une plainte qui n'en
  // est pas une : un corps qui ne tourne pas d'un huitième est CORRECT, parce
  // que le sprite porte justement trois regards par direction.
  const Facing north{kNorth, kGazeAhead};
  const Facing once = TurnToward(north, 1);
  CHECK(once.direction == kNorth);
  CHECK(once.head_direction == kGazeUpward);

  // ⚠ ET LA SECONDE DEMANDE ACHÈVE LE CORPS : la tête déjà penchée du bon côté
  // veut dire « ce huitième a déjà été demandé ».
  const Facing twice = TurnToward(once, 1);
  CHECK(twice.direction == 1);
  CHECK(twice.head_direction == kGazeAhead);
}

TEST_CASE("Demi-tour : un quart de tour se fait en DEUX demandes", "[sim][turn]") {
  const Facing north{kNorth, kGazeAhead};
  const Facing first = TurnToward(north, kWest);
  // Le corps s'approche à un huitième, la tête couvre le reste.
  CHECK(first.direction == 1);
  CHECK(first.head_direction == kGazeUpward);

  const Facing second = TurnToward(first, kWest);
  CHECK(second.direction == kWest);
  CHECK(second.head_direction == kGazeAhead);
}

TEST_CASE("Demi-tour : le dos tourné y va d'un COUP", "[sim][turn]") {
  // ⚠ À quatre huitièmes, aucun regard ne peut couvrir l'écart : le client
  // pivote le corps entier. C'est le seul cas où une demande suffit pour un
  // grand angle.
  const Facing north{kNorth, kGazeAhead};
  const Facing about = TurnToward(north, kSouth);
  CHECK(about.direction == kSouth);
  CHECK(about.head_direction == kGazeAhead);
}

TEST_CASE("Demi-tour : viser où l'on regarde déjà ne demande rien", "[sim][turn]") {
  const Facing north{kNorth, kGazeAhead};
  CHECK(TurnToward(north, kNorth) == north);
  // ⚠ Mais une tête penchée se REDRESSE : le corps est déjà au but, et c'est
  // le regard qui ne l'est pas.
  const Facing penchee{kNorth, kGazeDownward};
  CHECK(TurnToward(penchee, kNorth) == north);
}

TEST_CASE("Demi-tour : les deux sens sont symétriques", "[sim][turn]") {
  // Le même quart de tour dans l'autre sens, pour que le signe ne s'inverse
  // pas en silence.
  const Facing west{kWest, kGazeAhead};
  const Facing first = TurnToward(west, kNorth);
  CHECK(first.direction == 1);
  CHECK(first.head_direction == kGazeDownward);
  CHECK(TurnToward(first, kNorth) == Facing{kNorth, kGazeAhead});
}

TEST_CASE("Monde : le regard d'un frappeur SE VOIT, même après avoir marché", "[sim][world]") {
  // 🔴 TOURNER N'EST PAS ÊTRE DESSINÉ TOURNÉ. `Locate` garde la direction de la
  // queue du chemin pour quiconque a marché une fois, et ne rend la main au
  // regard posé que sous `Actor::turned_since_walk_began`. Le coup écrivait la
  // direction sans lever ce drapeau : le champ avait l'air mesuré et le sprite
  // restait de dos, ce que seul le joueur devant les deux clients pouvait voir.
  World world;
  world.Spawn(Someone(160'001, CellPosition{10, 10, 0}));
  world.Spawn(Someone(160'002, CellPosition{13, 10, 0}));

  // Le frappeur MARCHE d'abord vers le nord : sans marche, la queue du chemin
  // n'existe pas et le défaut ne se reproduit pas.
  //
  // ⚠ ET IL LUI FAUT UNE VITESSE, sans quoi `Locate` rend l'arrivée sans même
  // consulter le drapeau : l'essai passerait à côté du chemin ordinaire.
  REQUIRE(world.SetSpeed(160'001, 150));
  CellMovement march;
  march.from = CellPosition{10, 10, 0};
  march.to = CellPosition{10, 13, 0};
  REQUIRE(world.StartMoving(160'001, march, 0));
  const ro::sim::Actor* after_walk = world.Find(160'001);
  REQUIRE(after_walk != nullptr);
  const std::uint8_t step_facing = ro::sim::FacingAt(*after_walk, 100'000);

  ro::sim::Strike strike;
  strike.attacker = 160'001;
  strike.target = 160'002;
  strike.started_ms = 100'000;
  REQUIRE(world.RecordStrike(strike));

  const ro::sim::Actor* striker = world.Find(160'001);
  REQUIRE(striker != nullptr);
  // ⚠ LE REPÈRE EST LA CASE OÙ L'ON SE TIENT, pas celle qu'on annonce : lire
  // `position` des deux côtés reviendrait à comparer le calcul à lui-même, et
  // l'essai passerait quelle que soit la case dont on part.
  const std::uint8_t aimed_facing =
      DirectionTowards(ro::sim::StandingCell(*striker), ro::sim::StandingCell(*world.Find(160'002)),
                       striker->position.direction);
  CHECK(striker->position.direction == aimed_facing);
  // Et c'est bien CELUI-LÀ qui se dessine, une fois la marche finie.
  CHECK(ro::sim::FacingAt(*striker, 100'000) == aimed_facing);
  CHECK(ro::sim::FacingAt(*striker, 100'000) != step_facing);
}

TEST_CASE("Monde : un frappeur qui a DÉPASSÉ sa cible ne lui tourne pas le dos", "[sim][world]") {
  // 🔴 CE QUE LE JOUEUR A VU, DEVANT LES DEUX CLIENTS : son personnage frappait
  // en tournant le dos à son adversaire, « et quand je change de côté, ça tourne
  // le dos de l'autre sens ».
  //
  // `Actor::position` est le DÉPART d'une marche et le reste jusqu'à la
  // prochaine annonce du serveur. Viser depuis lui fait regarder où la cible
  // était PAR RAPPORT AU POINT DE DÉPART — et la visée s'inverse exactement
  // quand on a dépassé ce point, ce qui arrive à chaque approche au corps à
  // corps, le serveur nous menant à portée par le chemin qu'il veut.
  World world;
  world.Spawn(Someone(162'001, CellPosition{10, 10, 0}));
  world.Spawn(Someone(162'002, CellPosition{12, 10, 0}));
  REQUIRE(world.SetSpeed(162'001, 150));

  // Le frappeur part de l'ouest de sa cible et s'arrête à l'EST : il l'a
  // dépassée. Depuis son départ elle est à l'est, depuis son arrivée à l'ouest.
  CellMovement approche;
  approche.from = CellPosition{10, 10, 0};
  approche.to = CellPosition{13, 10, 0};
  REQUIRE(world.StartMoving(162'001, approche, 0));

  ro::sim::Strike strike;
  strike.attacker = 162'001;
  strike.target = 162'002;
  strike.started_ms = 100'000;
  REQUIRE(world.RecordStrike(strike));

  const ro::sim::Actor* striker = world.Find(162'001);
  REQUIRE(striker != nullptr);
  CHECK(striker->position.direction == kWest);
  CHECK(striker->position.direction != kEast);
  CHECK(FacingAt(*striker, 100'000) == kWest);
}

TEST_CASE("Monde : une cible qui a fini de courir se vise où elle est arrivée", "[sim][world]") {
  // L'autre bout de la même règle : la cible est annoncée à son DÉPART tant
  // qu'elle marche, et la viser là ferait frapper derrière elle.
  World world;
  world.Spawn(Someone(163'001, CellPosition{10, 10, 0}));
  world.Spawn(Someone(163'002, CellPosition{12, 10, 0}));
  REQUIRE(world.SetSpeed(163'002, 150));

  // Elle passe de l'est du frappeur à son ouest.
  CellMovement fuite;
  fuite.from = CellPosition{12, 10, 0};
  fuite.to = CellPosition{7, 10, 0};
  REQUIRE(world.StartMoving(163'002, fuite, 0));

  ro::sim::Strike strike;
  strike.attacker = 163'001;
  strike.target = 163'002;
  strike.started_ms = 100'000;
  REQUIRE(world.RecordStrike(strike));

  const ro::sim::Actor* striker = world.Find(163'001);
  REQUIRE(striker != nullptr);
  CHECK(striker->position.direction == kWest);
}

TEST_CASE("Monde : sans vitesse connue, le demi-tour se voit quand même", "[sim][world]") {
  // ⚠ `Locate` A DEUX SORTIES, et celle de la vitesse nulle sautait le regard
  // posé. Un acteur paru sans que le serveur ait dit son pas gardait donc la
  // direction de son chemin, quoi qu'on lui demande.
  World world;
  world.Spawn(Someone(161'001, CellPosition{10, 10, 0}));
  world.Spawn(Someone(161'002, CellPosition{13, 10, 0}));
  CellMovement march;
  march.from = CellPosition{10, 10, 0};
  march.to = CellPosition{10, 13, 0};
  REQUIRE(world.StartMoving(161'001, march, 0));
  REQUIRE(world.Find(161'001)->speed_ms_per_cell == 0);

  ro::sim::Strike strike;
  strike.attacker = 161'001;
  strike.target = 161'002;
  strike.started_ms = 500;
  REQUIRE(world.RecordStrike(strike));

  const ro::sim::Actor* striker = world.Find(161'001);
  CHECK(ro::sim::FacingAt(*striker, 500) == striker->position.direction);
}

TEST_CASE("Monde : le regard garde qui le lui a donné", "[sim][world]") {
  // Le serveur, la marche ou la visée donnent la même boussole, mais le client
  // n'en tient pas le même angle : le monde note donc l'origine, et l'écart
  // exact vers ce qui est visé.
  using ro::sim::FacingOrigin;
  World world;
  world.Spawn(Someone(164'001, CellPosition{10, 10, kSouth}));
  world.Spawn(Someone(164'002, CellPosition{9, 7, 0}));
  CHECK(world.Find(164'001)->facing_origin == FacingOrigin::kServer);

  ro::sim::Strike strike;
  strike.attacker = 164'001;
  strike.target = 164'002;
  strike.started_ms = 500;
  REQUIRE(world.RecordStrike(strike));
  const Actor* striker = world.Find(164'001);
  CHECK(striker->facing_origin == FacingOrigin::kAim);
  CHECK(striker->aimed_dx_cells == -1);
  CHECK(striker->aimed_dy_cells == -3);

  // Un demi-tour annoncé reprend la main.
  REQUIRE(world.Turn(164'001, kEast, kGazeAhead));
  CHECK(world.Find(164'001)->facing_origin == FacingOrigin::kServer);

  // Et une marche arrêtée garde le regard de son dernier pas.
  CellMovement march;
  march.from = CellPosition{10, 10, 0};
  march.to = CellPosition{10, 12, 0};
  REQUIRE(world.StartMoving(164'001, march, 1000));
  REQUIRE(world.PlaceAt(164'001, CellPosition{10, 11, kNorth}, 1200));
  CHECK(world.Find(164'001)->facing_origin == FacingOrigin::kWalk);
}

// ── LE REGARD AU COUP : ce que le client fait, et que le signe des écarts ne
// disait pas ────────────────────────────────────────────────────────────────

namespace {

constexpr std::uint8_t kSouthEast = 5;
constexpr std::uint8_t kNorthEast = 7;

ro::sim::Strike BlowFrom(std::uint32_t attacker, std::uint32_t target, std::uint32_t at_ms) {
  ro::sim::Strike strike;
  strike.attacker = attacker;
  strike.target = target;
  strike.started_ms = at_ms;
  strike.damage = 1;
  return strike;
}

}  // namespace

TEST_CASE("Visée : l'angle exact décide de la boussole, pas le signe des écarts",
          "[sim][turn][aim]") {
  // 🔴 LE DÉFAUT QUE CE BANC GARDE. La direction se prenait sur le SIGNE de
  // chaque écart, ce qui met en diagonale toute cible hors des axes : cinq cases
  // à l'est et une au nord regardaient le nord-est. Le client mesure l'angle
  // et l'arrondit au huitième : c'est l'EST.
  CHECK(ro::sim::DirectionOfAim(5.0F, 1.0F, kNorth) == kEast);
  CHECK(ro::sim::DirectionOfAim(1.0F, 5.0F, kEast) == kNorth);
  CHECK(ro::sim::DirectionOfAim(1.0F, 2.0F, kNorth) == kNorthEast);
  CHECK(ro::sim::DirectionOfAim(3.0F, -3.0F, kNorth) == kSouthEast);
  CHECK(ro::sim::DirectionOfAim(-1.0F, 0.0F, kNorth) == kWest);
  // Un écart nul ne tourne personne.
  CHECK(ro::sim::DirectionOfAim(0.0F, 0.0F, kWest) == kWest);
  // Le sud vaut zéro chez le client, l'est quatre-vingt-dix, le nord cent
  // quatre-vingts.
  CHECK(ro::sim::ClientAimDegrees(0.0F, -1.0F) == Catch::Approx(0.0F));
  CHECK(ro::sim::ClientAimDegrees(1.0F, 0.0F) == Catch::Approx(90.0F));
  CHECK(ro::sim::ClientAimDegrees(0.0F, 1.0F) == Catch::Approx(180.0F));
  CHECK(ro::sim::ClientAimDegrees(-1.0F, 0.0F) == Catch::Approx(270.0F));

  World world;
  world.Spawn(Someone(170'001, CellPosition{10, 10, kNorth}));
  world.Spawn(Someone(170'002, CellPosition{15, 11, kNorth}));
  REQUIRE(world.RecordStrike(BlowFrom(170'001, 170'002, 500)));
  const Actor* attacker = world.Find(170'001);
  CHECK(attacker->position.direction == kEast);
  CHECK(attacker->aimed_dx_cells == 5.0F);
  CHECK(attacker->aimed_dy_cells == 1.0F);
}

TEST_CASE("Visée : une cible qui court se vise LÀ OÙ ELLE EST au coup", "[sim][turn][aim]") {
  // 🔴 LE CLIENT VISE LA POSITION DU MOMENT, au milieu d'un pas s'il le faut,
  // et non la case où la cible s'arrêtera. Une cible qui passe de l'est à
  // l'ouest du frappeur se vise à l'EST tant qu'elle y est encore — même quand
  // le coup ne l'arrête pas.
  World world;
  world.Spawn(Someone(171'001, CellPosition{9, 10, kNorth}));
  world.Spawn(Someone(171'002, CellPosition{12, 10, kNorth}));
  REQUIRE(world.SetSpeed(171'002, kCellMs));
  const CellMovement crossing{CellPosition{12, 10, 0}, CellPosition{7, 10, 0}, 8, 8};
  REQUIRE(world.StartMoving(171'002, crossing, 0));

  // Un coup encaissé sans broncher, une case franchie : la cible est en 11,10.
  ro::sim::Strike endured = BlowFrom(171'001, 171'002, kCellMs);
  endured.flinches = false;
  REQUIRE(world.RecordStrike(endured));
  const Actor* attacker = world.Find(171'001);
  CHECK(attacker->position.direction == kEast);
  CHECK(attacker->aimed_dx_cells == Catch::Approx(2.0F));
  // Et la cible court toujours.
  CHECK(world.Find(171'002)->movement.has_value());
}

TEST_CASE(
    "Visée : une cible hors de vue — l'attaque tourne le dos au joueur, la compétence ne "
    "tourne pas",
    "[sim][turn][aim]") {
  // 🔴 CE QUE LE CLIENT FAIT D'UNE CIBLE QU'IL NE VOIT PAS : le frappeur d'une
  // attaque ordinaire se tourne vers le point symétrique du JOUEUR par rapport
  // à lui — ce qu'on ne voit pas est au-delà de la vue, donc de ce côté-là. Le
  // coup d'une compétence, lui, ne tourne personne.
  World world;
  world.set_player_id(172'000);
  world.Spawn(Someone(172'000, CellPosition{10, 5, kNorth}));
  world.Spawn(Someone(172'001, CellPosition{10, 10, kEast}));
  world.Spawn(Someone(172'002, CellPosition{20, 20, kEast}));

  REQUIRE(world.RecordStrike(BlowFrom(172'001, 999'999, 500)));
  CHECK(world.Find(172'001)->position.direction == kNorth);

  ro::sim::Strike skill = BlowFrom(172'002, 999'999, 600);
  skill.skill_id = 14;
  REQUIRE(world.RecordStrike(skill));
  CHECK(world.Find(172'002)->position.direction == kEast);

  // Le joueur lui-même vise alors sa propre place : rien ne bouge.
  REQUIRE(world.RecordStrike(BlowFrom(172'000, 999'999, 700)));
  CHECK(world.Find(172'000)->position.direction == kNorth);
  CHECK(world.Find(172'000)->facing_origin == ro::sim::FacingOrigin::kServer);
}

TEST_CASE("Visée : une unité au sol se frappe en face", "[sim][turn][aim]") {
  // Un mur de glace, un piège : ce n'est pas un acteur, et le client le
  // cherche dans sa seconde liste pour s'y tourner quand même.
  World world;
  world.Spawn(Someone(173'001, CellPosition{10, 10, kNorth}));
  ro::sim::SkillUnit wall;
  wall.id = 0x3039;
  wall.cell = CellPosition{13, 10, 0};
  wall.unit_id = 0x7F;
  world.PlaceSkillUnit(wall);
  ro::sim::Strike strike = BlowFrom(173'001, 0x3039, 500);
  strike.skill_id = 14;
  REQUIRE(world.RecordStrike(strike));
  CHECK(world.Find(173'001)->position.direction == kEast);
}

TEST_CASE("Visée : le coup d'une compétence du registre ne tourne pas son auteur",
          "[sim][turn][aim]") {
  // Douze classes du registre des actions laissent le lanceur tel quel, motion
  // et regard compris.
  World world;
  world.Spawn(Someone(174'001, CellPosition{10, 10, kNorth}));
  world.Spawn(Someone(174'002, CellPosition{13, 10, kNorth}));
  ro::sim::Strike strike = BlowFrom(174'001, 174'002, 500);
  strike.skill_id = 5411;
  REQUIRE(world.RecordStrike(strike));
  CHECK(world.Find(174'001)->position.direction == kNorth);
  strike.skill_id = 5;
  REQUIRE(world.RecordStrike(strike));
  CHECK(world.Find(174'001)->position.direction == kEast);
}

TEST_CASE("Incantation : une annonce à durée nulle ne tourne pas, sauf Asura et Kyrie",
          "[sim][turn][aim]") {
  // Le client sort du geste d'incantation avant de tourner le lanceur quand la
  // durée vaut zéro ; le poing d'Asura enjambe cette sortie, et Kyrie Eleison
  // se tourne vers sa cible avant même de l'atteindre.
  World world;
  world.Spawn(Someone(175'001, CellPosition{10, 10, kNorth}));
  world.Spawn(Someone(175'002, CellPosition{13, 10, kNorth}));
  ro::sim::Cast cast;
  cast.skill_id = 14;
  cast.target_id = 175'002;
  cast.started_ms = 500;
  cast.duration_ms = 0;
  REQUIRE(world.RecordCast(175'001, cast));
  CHECK(world.Find(175'001)->position.direction == kNorth);

  cast.skill_id = 271;
  REQUIRE(world.RecordCast(175'001, cast));
  CHECK(world.Find(175'001)->position.direction == kEast);

  REQUIRE(world.Turn(175'001, kNorth, kGazeAhead));
  cast.skill_id = 73;
  REQUIRE(world.RecordCast(175'001, cast));
  CHECK(world.Find(175'001)->position.direction == kEast);

  // Au sol et sans durée : rien non plus ; avec une durée, la case.
  REQUIRE(world.Turn(175'001, kNorth, kGazeAhead));
  cast.skill_id = 89;
  cast.target_id = 0;
  cast.target_cell = CellPosition{10, 4, 0};
  REQUIRE(world.RecordCast(175'001, cast));
  CHECK(world.Find(175'001)->position.direction == kNorth);
  cast.duration_ms = 1500;
  REQUIRE(world.RecordCast(175'001, cast));
  CHECK(world.Find(175'001)->position.direction == kSouth);
}

TEST_CASE("Emploi : un soin tourne le prêtre vers le soigné", "[sim][turn][aim]") {
  // Une compétence sans dégâts tourne son lanceur vers sa cible, comme un
  // coup ; le retour d'un homoncule et deux emplois à effet n'en font rien, et
  // qui se soigne lui-même ne bouge pas.
  World world;
  world.Spawn(Someone(176'001, CellPosition{10, 10, kNorth}));
  world.Spawn(Someone(176'002, CellPosition{13, 10, kNorth}));
  ro::sim::SupportHit heal;
  heal.skill_id = 28;
  heal.source_id = 176'001;
  heal.started_ms = 500;
  REQUIRE(world.RecordSupport(176'002, heal));
  CHECK(world.Find(176'001)->position.direction == kEast);

  REQUIRE(world.Turn(176'001, kNorth, kGazeAhead));
  heal.skill_id = 244;
  REQUIRE(world.RecordSupport(176'002, heal));
  CHECK(world.Find(176'001)->position.direction == kNorth);

  heal.skill_id = 28;
  REQUIRE(world.RecordSupport(176'001, heal));
  CHECK(world.Find(176'001)->position.direction == kNorth);
}

TEST_CASE("Incantation : le lanceur suit sa cible des yeux", "[sim][turn][aim]") {
  // 🔴 TANT QUE L'INCANTATION DURE, le client retourne le lanceur vers là où sa
  // cible se trouve, à chaque image ; ni après la fin, ni après un coup qui
  // l'a fait broncher.
  World world;
  world.Spawn(Someone(177'001, CellPosition{10, 10, kNorth}));
  world.Spawn(Someone(177'002, CellPosition{13, 10, kNorth}));
  REQUIRE(world.SetSpeed(177'002, 100));
  ro::sim::Cast cast;
  cast.skill_id = 14;
  cast.target_id = 177'002;
  cast.started_ms = 1000;
  cast.duration_ms = 2000;
  REQUIRE(world.RecordCast(177'001, cast));
  CHECK(world.Find(177'001)->position.direction == kEast);

  // La cible descend vers le sud pendant l'incantation.
  const CellMovement south{CellPosition{13, 10, 0}, CellPosition{13, 0, 0}, 8, 8};
  REQUIRE(world.StartMoving(177'002, south, 1000));
  world.TrackCastTargets(1400);
  CHECK(world.Find(177'001)->position.direction == kSouthEast);
  world.TrackCastTargets(1900);
  CHECK(world.Find(177'001)->position.direction == kSouth);

  // L'incantation finie, le regard ne suit plus.
  REQUIRE(world.PlaceAt(177'002, CellPosition{7, 10, 0}, 3100));
  world.TrackCastTargets(3100);
  CHECK(world.Find(177'001)->position.direction == kSouth);

  // Ni après un coup qui a fait broncher le lanceur.
  cast.started_ms = 4000;
  REQUIRE(world.RecordCast(177'001, cast));
  CHECK(world.Find(177'001)->position.direction == kWest);
  ro::sim::Strike blow = BlowFrom(177'002, 177'001, 4100);
  blow.flinches = true;
  blow.hurt_motion_ms = 288;
  REQUIRE(world.RecordStrike(blow));
  REQUIRE(world.PlaceAt(177'002, CellPosition{13, 10, 0}, 4200));
  world.TrackCastTargets(4300);
  CHECK(world.Find(177'001)->position.direction == kWest);
}

TEST_CASE("Case : le lanceur se tourne vers la case, par l'angle exact", "[sim][turn][aim]") {
  // 🔴 LE PAQUET D'UNE COMPÉTENCE POSÉE SUR UNE CASE TOURNE SON LANCEUR vers
  // elle, joueur comme monstre, avant de poser la motion d'emploi. Il restait
  // planté dans son regard d'avant.
  World world;
  world.Spawn(Someone(178'001, CellPosition{10, 10, kNorth}));
  ro::sim::GroundSkillUse wall;
  wall.skill_id = 87;
  wall.level = 5;
  wall.cell = CellPosition{11, 12, 0};
  wall.started_ms = 500;
  REQUIRE(world.RecordGroundSkill(178'001, wall));
  const Actor* caster = world.Find(178'001);
  CHECK(caster->position.direction == kNorthEast);
  CHECK(caster->facing_origin == ro::sim::FacingOrigin::kAim);
  // L'ANGLE EXACT, et non le huitième : c'est lui que le client garde.
  CHECK(caster->aimed_dx_cells == Catch::Approx(1.0F));
  CHECK(caster->aimed_dy_cells == Catch::Approx(2.0F));

  // 🔴 UN REGARD DU SERVEUR QUI SUIT L'EMPORTE, comme chez le client : le
  // paquet de direction réécrit le corps par-dessus.
  REQUIRE(world.Turn(178'001, kSouth, kGazeAhead));
  caster = world.Find(178'001);
  CHECK(caster->position.direction == kSouth);
  CHECK(caster->facing_origin == ro::sim::FacingOrigin::kServer);

  // Viser sa propre case ne change rien, pas même l'origine du regard.
  wall.cell = CellPosition{10, 10, 0};
  wall.started_ms = 900;
  REQUIRE(world.RecordGroundSkill(178'001, wall));
  CHECK(world.Find(178'001)->position.direction == kSouth);
  CHECK(world.Find(178'001)->facing_origin == ro::sim::FacingOrigin::kServer);
}

TEST_CASE("Case : onze compétences ne tournent pas leur lanceur", "[sim][turn][aim]") {
  // Dix en dur — dont 83 et 769 —, et 5455 par sa classe du registre : le
  // client leur pose une autre action que la motion d'emploi, sans regard.
  World world;
  world.Spawn(Someone(179'001, CellPosition{10, 10, kNorth}));
  ro::sim::GroundSkillUse use;
  use.cell = CellPosition{14, 10, 0};
  use.started_ms = 500;
  constexpr std::array<std::uint16_t, 11> kKeepingFacing = {83,   769,  774,  5028, 5042, 5219,
                                                            5223, 5224, 5231, 5236, 5455};
  for (const std::uint16_t skill_id : kKeepingFacing) {
    use.skill_id = skill_id;
    REQUIRE(world.RecordGroundSkill(179'001, use));
    CHECK(world.Find(179'001)->position.direction == kNorth);
  }
  use.skill_id = 5220;
  REQUIRE(world.RecordGroundSkill(179'001, use));
  CHECK(world.Find(179'001)->position.direction == kEast);
}

TEST_CASE("Case : le lanceur oublie la cible qu'il suivait", "[sim][turn][aim]") {
  // Le client efface la cible suivie d'une incantation quand une compétence
  // tombe sur une case : le regard ne revient plus vers elle.
  World world;
  world.Spawn(Someone(180'001, CellPosition{10, 10, kNorth}));
  world.Spawn(Someone(180'002, CellPosition{13, 10, kNorth}));
  ro::sim::Cast cast;
  cast.skill_id = 14;
  cast.target_id = 180'002;
  cast.started_ms = 1000;
  cast.duration_ms = 2000;
  REQUIRE(world.RecordCast(180'001, cast));
  ro::sim::GroundSkillUse use;
  use.skill_id = 87;
  use.cell = CellPosition{10, 5, 0};
  use.started_ms = 1200;
  REQUIRE(world.RecordGroundSkill(180'001, use));
  CHECK(world.Find(180'001)->position.direction == kSouth);
  world.TrackCastTargets(1400);
  CHECK(world.Find(180'001)->position.direction == kSouth);
}

TEST_CASE("Case : un lanceur hors de vue laisse sa compétence au journal", "[sim][turn]") {
  // 🔴 LE PAQUET N'EST PLUS PERDU : la case se montre sans son lanceur.
  World world;
  ro::sim::GroundSkillUse use;
  use.skill_id = 2417;
  use.cell = CellPosition{20, 20, 0};
  use.started_ms = 700;
  CHECK_FALSE(world.RecordGroundSkill(181'001, use));
  ro::sim::UnseenGroundSkillCursor cursor;
  const auto fresh = cursor.Take(world.unseen_ground_skills());
  REQUIRE(fresh.size() == 1);
  CHECK(fresh.front().caster_id == 181'001);
  CHECK(fresh.front().use.skill_id == 2417);
  CHECK(fresh.front().use.cell.x == 20);
  CHECK(cursor.Take(world.unseen_ground_skills()).empty());
  // Une carte quittée emporte ce qui n'a pas été lu.
  CHECK_FALSE(world.RecordGroundSkill(181'001, use));
  world.EnterMap("prontera", 1, 1);
  CHECK(cursor.Take(world.unseen_ground_skills()).empty());
}
