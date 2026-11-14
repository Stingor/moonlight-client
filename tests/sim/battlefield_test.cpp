// LE CHAMP DE BATAILLE DE LA CARTE : camps, côté, score et coéquipiers.

#include "ro/sim/battlefield.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "ro/sim/world.h"

using ro::sim::Battlefield;
using ro::sim::BattleSide;

namespace {

constexpr std::uint32_t kSelf = 2'000'001;
constexpr std::uint32_t kMate = 2'000'002;
constexpr std::uint32_t kFoe = 2'000'003;
constexpr std::uint32_t kStranger = 2'000'004;
// Les identifiants d'équipe du serveur : l'équipe lion, créée d'abord, a le
// plus petit.
constexpr std::uint16_t kLionCamp = 7;
constexpr std::uint16_t kEagleCamp = 8;
constexpr std::uint16_t kKnightJob = 7;
constexpr std::int16_t kCellX = 120;
constexpr std::int16_t kCellY = 64;
constexpr std::int16_t kNextCellX = 121;
constexpr std::int16_t kGone = -1;

}  // namespace

TEST_CASE("Champ de bataille : neuf, il ne connaît ni camp ni score", "[sim][battleground]") {
  Battlefield battlefield;
  CHECK(battlefield.own_camp() == ro::sim::kNoBattleCamp);
  CHECK_FALSE(battlefield.score().shown());
  CHECK(battlefield.mates().empty());
  CHECK(battlefield.own_side() == BattleSide::kUnknown);
}

TEST_CASE("Champ de bataille : notre camp, puis l'ennemi, puis l'allié", "[sim][battleground]") {
  Battlefield battlefield;
  battlefield.TellCamp(kSelf, kLionCamp, kSelf);
  CHECK(battlefield.own_camp() == kLionCamp);
  // Aucun camp adverse vu : le côté reste inconnu.
  CHECK(battlefield.own_side() == BattleSide::kUnknown);

  battlefield.TellCamp(kFoe, kEagleCamp, kSelf);
  battlefield.TellCamp(kMate, kLionCamp, kSelf);
  CHECK(battlefield.CampOf(kFoe) == kEagleCamp);
  CHECK(battlefield.IsEnemy(kFoe));
  CHECK_FALSE(battlefield.IsAlly(kFoe));
  CHECK(battlefield.IsAlly(kMate));
  CHECK_FALSE(battlefield.IsEnemy(kMate));
  CHECK(battlefield.own_side() == BattleSide::kLion);
}

TEST_CASE("Champ de bataille : l'ennemi vu avant notre camp se juge à son arrivée",
          "[sim][battleground]") {
  Battlefield battlefield;
  battlefield.TellCamp(kFoe, kLionCamp, kSelf);
  // Sans notre camp, personne n'est ennemi.
  CHECK_FALSE(battlefield.IsEnemy(kFoe));
  CHECK_FALSE(battlefield.IsAlly(kFoe));
  battlefield.TellCamp(kSelf, kEagleCamp, kSelf);
  CHECK(battlefield.IsEnemy(kFoe));
  CHECK(battlefield.own_side() == BattleSide::kEagle);
}

TEST_CASE("Champ de bataille : un camp inconnu n'est ni ennemi ni allié", "[sim][battleground]") {
  Battlefield battlefield;
  battlefield.TellCamp(kSelf, kLionCamp, kSelf);
  CHECK(battlefield.CampOf(kStranger) == ro::sim::kNoBattleCamp);
  CHECK_FALSE(battlefield.IsEnemy(kStranger));
  CHECK_FALSE(battlefield.IsAlly(kStranger));
  // Un camp nul retire celui qu'on connaissait.
  battlefield.TellCamp(kFoe, kEagleCamp, kSelf);
  battlefield.TellCamp(kFoe, ro::sim::kNoBattleCamp, kSelf);
  CHECK_FALSE(battlefield.IsEnemy(kFoe));
}

TEST_CASE("Champ de bataille : le score se montre une fois dit", "[sim][battleground]") {
  Battlefield battlefield;
  battlefield.TellScore(0, 3);
  CHECK(battlefield.score().shown());
  CHECK(battlefield.score().lion == 0);
  CHECK(battlefield.score().eagle == 3);
}

TEST_CASE("Champ de bataille : un coéquipier se pose, se déplace et se retire",
          "[sim][battleground]") {
  Battlefield battlefield;
  battlefield.TellMate(kMate, "Brunhild", kKnightJob, kCellX, kCellY);
  REQUIRE(battlefield.mates().count(kMate) == 1);
  const ro::sim::BattleMate& mate = battlefield.mates().at(kMate);
  CHECK(mate.name == "Brunhild");
  CHECK(mate.job == kKnightJob);
  CHECK(mate.cell.x == kCellX);
  CHECK(mate.cell.y == kCellY);

  battlefield.TellMate(kMate, "Brunhild", kKnightJob, kNextCellX, kCellY);
  CHECK(battlefield.mates().size() == 1);
  CHECK(battlefield.mates().at(kMate).cell.x == kNextCellX);

  // Une seule coordonnée à −1 n'est pas un retrait.
  battlefield.TellMate(kMate, "Brunhild", kKnightJob, kGone, kCellY);
  CHECK(battlefield.mates().count(kMate) == 1);
  battlefield.TellMate(kMate, "", 0, kGone, kGone);
  CHECK(battlefield.mates().empty());
}

TEST_CASE("Champ de bataille : l'entrée sur une autre carte vide tout", "[sim][battleground]") {
  ro::sim::World world;
  world.EnterMap("bat_a01", 50, 50);
  world.battlefield().TellCamp(kSelf, kLionCamp, kSelf);
  world.battlefield().TellCamp(kFoe, kEagleCamp, kSelf);
  world.battlefield().TellScore(1, 2);
  world.battlefield().TellMate(kMate, "Brunhild", kKnightJob, kCellX, kCellY);
  world.EnterMap("prontera", 150, 150);
  CHECK(world.battlefield().own_camp() == ro::sim::kNoBattleCamp);
  CHECK(world.battlefield().CampOf(kFoe) == ro::sim::kNoBattleCamp);
  CHECK_FALSE(world.battlefield().score().shown());
  CHECK(world.battlefield().mates().empty());
}

TEST_CASE("Champ de bataille : réapparaître sur la même carte garde coéquipiers et score",
          "[sim][battleground]") {
  constexpr std::uint16_t kLionScore = 1;
  constexpr std::uint16_t kEagleScore = 2;
  ro::sim::World world;
  world.EnterMap("bat_a01", 50, 50);
  world.battlefield().TellCamp(kSelf, kLionCamp, kSelf);
  world.battlefield().TellScore(kLionScore, kEagleScore);
  world.battlefield().TellMate(kMate, "Brunhild", kKnightJob, kCellX, kCellY);

  world.EnterMap("bat_a01", 60, 60);
  CHECK(world.battlefield().own_camp() == kLionCamp);
  CHECK(world.battlefield().score().lion == kLionScore);
  CHECK(world.battlefield().score().eagle == kEagleScore);
  REQUIRE(world.battlefield().mates().count(kMate) == 1);
  CHECK(world.battlefield().mates().at(kMate).cell.x == kCellX);
}

TEST_CASE("Champ de bataille : une nouvelle manche sur la même carte oublie les anciens camps",
          "[sim][battleground]") {
  // Les équipes de la manche suivante reçoivent de nouveaux identifiants, plus
  // grands : un camp de la manche précédente fausserait le côté.
  constexpr std::uint16_t kNextLionCamp = 9;
  constexpr std::uint16_t kNextEagleCamp = 10;
  constexpr std::uint16_t kLionScore = 1;
  constexpr std::uint16_t kEagleScore = 2;
  const bool through_no_camp = GENERATE(false, true);
  CAPTURE(through_no_camp);

  Battlefield battlefield;
  battlefield.TellCamp(kSelf, kEagleCamp, kSelf);
  battlefield.TellCamp(kFoe, kLionCamp, kSelf);
  battlefield.TellCamp(kMate, kEagleCamp, kSelf);
  battlefield.TellScore(kLionScore, kEagleScore);
  battlefield.TellMate(kMate, "Brunhild", kKnightJob, kCellX, kCellY);
  REQUIRE(battlefield.own_side() == BattleSide::kEagle);

  // Entre deux manches, le serveur peut nous dire hors de toute équipe.
  if (through_no_camp) {
    battlefield.TellCamp(kSelf, ro::sim::kNoBattleCamp, kSelf);
  }
  battlefield.TellCamp(kSelf, kNextLionCamp, kSelf);
  CHECK(battlefield.own_camp() == kNextLionCamp);
  CHECK(battlefield.CampOf(kSelf) == kNextLionCamp);
  CHECK(battlefield.CampOf(kFoe) == ro::sim::kNoBattleCamp);
  CHECK(battlefield.CampOf(kMate) == ro::sim::kNoBattleCamp);
  CHECK_FALSE(battlefield.IsEnemy(kFoe));
  CHECK(battlefield.own_side() == BattleSide::kUnknown);
  // Le score et les coéquipiers restent jusqu'à ce que le serveur les redise.
  CHECK(battlefield.score().lion == kLionScore);
  CHECK(battlefield.score().eagle == kEagleScore);
  CHECK(battlefield.mates().count(kMate) == 1);

  battlefield.TellCamp(kStranger, kNextEagleCamp, kSelf);
  CHECK(battlefield.IsEnemy(kStranger));
  CHECK(battlefield.own_side() == BattleSide::kLion);
}

TEST_CASE("Champ de bataille : notre camp redit à l'identique ne fait rien oublier",
          "[sim][battleground]") {
  Battlefield battlefield;
  battlefield.TellCamp(kFoe, kEagleCamp, kSelf);
  battlefield.TellCamp(kSelf, kLionCamp, kSelf);
  battlefield.TellCamp(kSelf, kLionCamp, kSelf);
  CHECK(battlefield.IsEnemy(kFoe));
  CHECK(battlefield.own_side() == BattleSide::kLion);
}
