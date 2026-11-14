// Les trames d'instance traduites dans l'état du joueur : la réservation
// annulée par 0xFFFF, la création refusée qui ne dit rien, et le détail maison.

#include "ro/net/memorial_dungeon.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/map_packets.h"
#include "ro/proto/memorial_packets.h"

#include "support/net_frames.h"

using ro::net::ApplyMemorialDungeonFrame;
using ro::net::FrameOutcome;
using ro::sim::MemorialDungeon;
using ro::sim::MemorialEnding;
using ro::sim::MemorialOwnerKind;
using ro::sim::MemorialPhase;
using ro::test_support::FrameOf;

namespace {

constexpr std::int64_t kNow = 1'800'000'000;
// Le type de `0x02CE` qui dit une création refusée, et celui d'une
// destruction demandée.
constexpr std::uint32_t kWireCreateFailed = 4;
constexpr std::uint32_t kWireDestroyed = 3;

[[nodiscard]] FrameOutcome Apply(std::vector<std::uint8_t> bytes, MemorialDungeon& dungeon) {
  return ApplyMemorialDungeonFrame(FrameOf(std::move(bytes)), dungeon, kNow);
}

}  // namespace

TEST_CASE("Instance : 0xFFFF annule la réservation", "[net][instance]") {
  MemorialDungeon dungeon;
  REQUIRE(Apply(ro::proto::Encode(
                    ro::proto::InstanceCreatedNotice{.name = "Old Glast Heim", .wait_position = 1}),
                dungeon) == FrameOutcome::kApplied);
  CHECK(dungeon.phase() == MemorialPhase::kWaiting);
  REQUIRE(Apply(ro::proto::Encode(ro::proto::InstanceWaitNotice{
                    .wait_position = ro::proto::InstanceWaitNotice::kCancelled}),
                dungeon) == FrameOutcome::kApplied);
  CHECK(dungeon.phase() == MemorialPhase::kNone);
  CHECK(dungeon.TakeEndings() == std::vector{MemorialEnding::kReservationCancelled});
}

TEST_CASE("Instance : une création refusée ferme sans un mot", "[net][instance]") {
  MemorialDungeon dungeon;
  REQUIRE(Apply(ro::proto::Encode(ro::proto::InstanceCreatedNotice{.name = "Old Glast Heim"}),
                dungeon) == FrameOutcome::kApplied);
  REQUIRE(Apply(ro::proto::Encode(ro::proto::InstanceChangeNotice{.type = kWireCreateFailed}),
                dungeon) == FrameOutcome::kApplied);
  CHECK(dungeon.phase() == MemorialPhase::kNone);
  CHECK(dungeon.TakeEndings().empty());

  REQUIRE(Apply(ro::proto::Encode(ro::proto::InstanceStatusNotice{.name = "Old Glast Heim"}),
                dungeon) == FrameOutcome::kApplied);
  REQUIRE(Apply(ro::proto::Encode(ro::proto::InstanceChangeNotice{.type = kWireDestroyed}),
                dungeon) == FrameOutcome::kApplied);
  CHECK(dungeon.TakeEndings() == std::vector{MemorialEnding::kDestroyed});
}

TEST_CASE("Instance : le détail maison, nom en page de codes", "[net][instance]") {
  MemorialDungeon dungeon;
  REQUIRE(Apply(ro::proto::Encode(ro::proto::InstanceStatusNotice{.name = "Airship Assault"}),
                dungeon) == FrameOutcome::kApplied);
  ro::proto::ZcMoonlightInstanceInfo info;
  info.server_time = static_cast<std::uint32_t>(kNow + 30);
  info.duration_limit = 7200;
  info.idle_timeout = 300;
  info.hardmode = 10;
  info.turbo = 1;
  info.mode = 2;
  info.destroyable = 1;
  info.may_destroy = 1;
  info.players_inside = 4;
  info.owner_name = "Les Gu\xE9riers";
  REQUIRE(Apply(ro::proto::Encode(info), dungeon) == FrameOutcome::kApplied);
  REQUIRE(dungeon.details().has_value());
  const ro::sim::MemorialDetails& details = *dungeon.details();
  CHECK(details.server_ahead_s == 30);
  CHECK(details.duration_limit_s == 7200U);
  CHECK(details.hard_mode == 10);
  CHECK(details.turbo);
  CHECK(details.owner_kind == MemorialOwnerKind::kParty);
  CHECK(details.owner_name == "Les Guériers");
  CHECK(details.may_destroy);
  CHECK_FALSE(details.on_instance_map);
  CHECK(details.players_inside == 4);

  // UN MODE QUE CE CLIENT NE CONNAÎT PAS se lit « inconnu ».
  info.mode = 9;
  REQUIRE(Apply(ro::proto::Encode(info), dungeon) == FrameOutcome::kApplied);
  CHECK(dungeon.details()->owner_kind == MemorialOwnerKind::kUnknown);
}

TEST_CASE("Instance : le reste n'est pas à elle", "[net][instance]") {
  MemorialDungeon dungeon;
  CHECK(Apply({0x01, 0x00}, dungeon) == FrameOutcome::kUnhandled);
}
