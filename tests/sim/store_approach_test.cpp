#include "ro/sim/store_approach.h"

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/long_walk.h"
#include "ro/sim/pending_action.h"
#include "ro/sim/signboard.h"
#include "ro/sim/walk_map.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"

using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::AdvancePendingAction;
using ro::sim::ApproachWalkClock;
using ro::sim::ArmFarStoreVisit;
using ro::sim::CellPosition;
using ro::sim::DropErrandApproach;
using ro::sim::kRangeToleranceCells;
using ro::sim::PendingAction;
using ro::sim::PendingDeed;
using ro::sim::PendingMove;
using ro::sim::PendingStep;
using ro::sim::Signboard;
using ro::sim::SignboardKind;
using ro::sim::StepsApart;
using ro::sim::StoreApproachRangeCells;
using ro::sim::WalkMap;
using ro::sim::WalkPlanner;
using ro::sim::World;

namespace {

constexpr std::uint32_t kPlayerId = 150'001;
constexpr std::uint32_t kMerchantId = 150'002;
constexpr std::uint16_t kSpeedMsPerCell = 150;
// Les réglages d'usine de rAthena : `area_size` et `max_walk_path`.
constexpr int kGameAreaCells = 14;
constexpr int kServerSteps = 17;
// La vue de moonclient, plus large que la portée du jeu.
constexpr int kViewCells = 40;
constexpr std::int32_t kOpenSide = 128;
constexpr std::uint16_t kRow = 60;
constexpr std::uint16_t kMerchantX = 90;

CellPosition At(std::uint16_t x, std::uint16_t y) { return CellPosition{x, y, 0}; }

WalkMap Open(std::int32_t size) {
  const std::vector<std::string> rows(static_cast<std::size_t>(size),
                                      std::string(static_cast<std::size_t>(size), '.'));
  const std::vector<std::string_view> views(rows.begin(), rows.end());
  return WalkMap::FromPicture(views);
}

// Un monde où le personnage se tient en `player_x` sur la ligne, et le marchand
// en `kMerchantX`, sous l'enseigne `kind`.
World Market(const WalkMap& map, std::uint16_t player_x, SignboardKind kind) {
  World world;
  world.SetWalkMap(&map);
  world.set_player_id(kPlayerId);
  Actor player;
  player.id = kPlayerId;
  player.kind = ActorKind::kPlayer;
  player.position = At(player_x, kRow);
  player.speed_ms_per_cell = kSpeedMsPerCell;
  world.Spawn(player);
  Actor merchant;
  merchant.id = kMerchantId;
  merchant.kind = ActorKind::kPlayer;
  merchant.position = At(kMerchantX, kRow);
  world.Spawn(merchant);
  Signboard board;
  board.kind = kind;
  board.title = "Potions";
  REQUIRE(world.SetSignboard(kMerchantId, board));
  world.session().game_area_cells = kGameAreaCells;
  world.session().view_cells = kViewCells;
  world.session().max_walk_path_steps = kServerSteps;
  return world;
}

std::uint16_t Away(int cells) { return static_cast<std::uint16_t>(kMerchantX - cells); }

PendingMove Step(World& world, const WalkMap& map, PendingAction& action, std::uint32_t now_ms) {
  WalkPlanner planner(&map);
  ApproachWalkClock clock;
  return AdvancePendingAction(world, planner, map, action, clock, now_ms);
}

}  // namespace

TEST_CASE("Échoppe lointaine : à la portée du jeu, la demande part d'ici",
          "[sim][store_approach]") {
  const WalkMap map = Open(kOpenSide);
  for (const SignboardKind kind : {SignboardKind::kVendingShop, SignboardKind::kBuyingStore}) {
    const World near = Market(map, Away(kGameAreaCells), kind);
    CHECK_FALSE(ArmFarStoreVisit(near, *near.Find(kMerchantId)).has_value());
  }
}

TEST_CASE("Échoppe lointaine : au-delà, la visite s'inscrit, de vente comme d'achat",
          "[sim][store_approach]") {
  const WalkMap map = Open(kOpenSide);
  for (const SignboardKind kind : {SignboardKind::kVendingShop, SignboardKind::kBuyingStore}) {
    const World far = Market(map, Away(kGameAreaCells + 1), kind);
    const std::optional<PendingAction> armed = ArmFarStoreVisit(far, *far.Find(kMerchantId));
    REQUIRE(armed.has_value());
    CHECK(armed->target_id == kMerchantId);
    CHECK(armed->deed == PendingDeed::kVisitStore);
    CHECK(armed->store_kind == kind);
    CHECK(armed->range_cells == StoreApproachRangeCells(far));
  }
}

TEST_CASE("Échoppe lointaine : un salon ou un passant sans enseigne ne s'approchent pas",
          "[sim][store_approach]") {
  const WalkMap map = Open(kOpenSide);
  World room = Market(map, Away(kViewCells - 1), SignboardKind::kChatRoom);
  CHECK_FALSE(ArmFarStoreVisit(room, *room.Find(kMerchantId)).has_value());
  REQUIRE(room.ClearSignboard(kMerchantId));
  CHECK_FALSE(ArmFarStoreVisit(room, *room.Find(kMerchantId)).has_value());
}

TEST_CASE("Échoppe lointaine : on s'arrête à la portée du serveur, pas sur le marchand",
          "[sim][store_approach]") {
  const WalkMap map = Open(kOpenSide);
  // La demande part dès qu'on se voit à `portée + tolérance` ; la case du
  // serveur retarde d'un pas au plus. Les deux ensemble tiennent dans
  // `area_size`, que le serveur compare en Tchebychev.
  const World world = Market(map, Away(kGameAreaCells + 1), SignboardKind::kBuyingStore);
  const float worst_cells =
      static_cast<float>(StoreApproachRangeCells(world)) + kRangeToleranceCells + 1.0F;
  CHECK(std::floor(worst_cells) <= static_cast<float>(kGameAreaCells));

  // Juste hors de portée : on marche vers la case d'approche, à la portée
  // d'approche du marchand.
  World outside = Market(map, Away(kGameAreaCells), SignboardKind::kBuyingStore);
  PendingAction action = *ArmFarStoreVisit(world, *world.Find(kMerchantId));
  const PendingMove walk = Step(outside, map, action, 0);
  REQUIRE(walk.step == PendingStep::kApproach);
  CHECK(StepsApart(walk.toward, At(kMerchantX, kRow)) == StoreApproachRangeCells(world));
  CHECK(StepsApart(walk.toward, At(kMerchantX, kRow)) > 0);

  // Arrivé là, la demande part.
  World arrived = Market(map, Away(StoreApproachRangeCells(world)), SignboardKind::kBuyingStore);
  CHECK(Step(arrived, map, action, 0).step == PendingStep::kRelease);
}

TEST_CASE("Échoppe lointaine : hors d'une marche, l'approche part par étapes",
          "[sim][store_approach]") {
  const WalkMap map = Open(kOpenSide);
  World world = Market(map, Away(kViewCells - 1), SignboardKind::kVendingShop);
  PendingAction action = *ArmFarStoreVisit(world, *world.Find(kMerchantId));
  const PendingMove leg = Step(world, map, action, 0);
  REQUIRE(leg.step == PendingStep::kApproach);
  CHECK(StepsApart(At(Away(kViewCells - 1), kRow), leg.toward) <=
        static_cast<std::size_t>(kServerSteps));
  CHECK(action.leg_to.has_value());
}

TEST_CASE("Échoppe lointaine : fermée ou disparue en route, on renonce", "[sim][store_approach]") {
  const WalkMap map = Open(kOpenSide);
  const auto armed = [&](World& world) {
    return *ArmFarStoreVisit(world, *world.Find(kMerchantId));
  };

  World closed = Market(map, Away(kViewCells - 1), SignboardKind::kVendingShop);
  PendingAction to_closed = armed(closed);
  REQUIRE(closed.ClearSignboard(kMerchantId));
  CHECK(Step(closed, map, to_closed, 0).step == PendingStep::kGiveUp);

  World gone = Market(map, Away(kViewCells - 1), SignboardKind::kBuyingStore);
  PendingAction to_gone = armed(gone);
  REQUIRE(gone.Remove(kMerchantId));
  CHECK(Step(gone, map, to_gone, 0).step == PendingStep::kGiveUp);

  // L'enseigne d'une autre sorte n'est plus l'échoppe qu'on allait voir.
  World changed = Market(map, Away(kViewCells - 1), SignboardKind::kVendingShop);
  PendingAction to_changed = armed(changed);
  Signboard room;
  room.kind = SignboardKind::kChatRoom;
  REQUIRE(changed.SetSignboard(kMerchantId, room));
  CHECK(Step(changed, map, to_changed, 0).step == PendingStep::kGiveUp);
}

TEST_CASE("Échoppe lointaine : Échap défait l'approche, pas une poursuite d'attaque",
          "[sim][store_approach]") {
  const WalkMap map = Open(kOpenSide);
  World world = Market(map, Away(kViewCells - 1), SignboardKind::kVendingShop);
  std::optional<PendingAction> visit = ArmFarStoreVisit(world, *world.Find(kMerchantId));
  CHECK(DropErrandApproach(visit));
  CHECK_FALSE(visit.has_value());

  std::optional<PendingAction> strike = PendingAction{};
  CHECK_FALSE(DropErrandApproach(strike));
  CHECK(strike.has_value());

  std::optional<PendingAction> nothing;
  CHECK_FALSE(DropErrandApproach(nothing));
}
