// Les états des membres, tirés du fil : la réponse complète qui ne concerne que
// les membres, le refus, les transitions de la zone, et la cible qui reçoit la
// même trame sans rien perdre.

#include "ro/net/member_statuses.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/target_info.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/actor.h"
#include "ro/sim/world.h"

#include "support/wire_bytes.h"

using ro::net::ApplyMemberStatusFrame;
using ro::net::ApplyTargetStatusFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::ActiveStatus;
using ro::sim::MemberStatuses;
using ro::sim::PartyMember;
using ro::sim::PartyRoster;
using ro::test_support::PutU16;
using ro::test_support::PutU32;

namespace {

constexpr std::uint32_t kMe = 2'000'001;
constexpr std::uint32_t kAlice = 2'000'002;
constexpr std::uint32_t kStranger = 2'000'999;
constexpr std::uint32_t kNow = 500'000;
constexpr std::uint16_t kFirstEffect = 100;
constexpr std::uint32_t kRemainingStepMs = 1000;
constexpr std::uint32_t kTotalMs = 60000;

// `ZC 0x0F2D` : bloc fixe de 10 octets, puis `count` entrées de 10.
[[nodiscard]] Frame StatusFrame(std::uint32_t gid, std::uint8_t status, std::uint8_t count) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::ZcMoonlightStatusList::kOpcode);
  PutU16(bytes, 0);
  PutU32(bytes, gid);
  bytes.push_back(status);
  bytes.push_back(count);
  for (std::size_t rank = 0; rank < count; ++rank) {
    PutU16(bytes, static_cast<std::uint16_t>(kFirstEffect + rank));
    PutU32(bytes, static_cast<std::uint32_t>(kRemainingStepMs * (rank + 1)));
    PutU32(bytes, kTotalMs);
  }
  const auto size = static_cast<std::uint16_t>(bytes.size());
  bytes[2] = static_cast<std::uint8_t>(size & 0xFF);
  bytes[3] = static_cast<std::uint8_t>(size >> 8);
  Frame frame;
  frame.opcode = ro::proto::ZcMoonlightStatusList::kOpcode;
  frame.bytes = std::move(bytes);
  return frame;
}

[[nodiscard]] Frame ChangeFrame(std::uint32_t account_id, std::uint16_t effect, bool active,
                                std::uint32_t left_ms) {
  ro::proto::PacketStatusChange packet;
  packet.index = static_cast<std::int16_t>(effect);
  packet.AID = account_id;
  packet.state = active ? 1 : 0;
  packet.Total = left_ms;
  packet.Left = left_ms;
  Frame frame;
  frame.opcode = ro::proto::PacketStatusChange::kOpcode;
  frame.bytes = ro::proto::Encode(packet);
  return frame;
}

[[nodiscard]] PartyRoster Party() {
  PartyRoster party;
  party.Replace("Les Lunes", {PartyMember{.account_id = kMe}, PartyMember{.account_id = kAlice}});
  return party;
}

}  // namespace

TEST_CASE("États des membres (fil) : la réponse sur un membre se range, avec ses durées",
          "[net][party]") {
  const PartyRoster party = Party();
  MemberStatuses statuses;
  REQUIRE(ApplyMemberStatusFrame(StatusFrame(kAlice, 0, 2), statuses, party, kMe, kNow) ==
          FrameOutcome::kApplied);
  const std::vector<ActiveStatus>* list = statuses.Find(kAlice);
  REQUIRE(list != nullptr);
  REQUIRE(list->size() == 2);
  CHECK((*list)[0].effect == kFirstEffect);
  CHECK((*list)[0].expires_ms == kNow + kRemainingStepMs);
  CHECK((*list)[0].total_ms == kTotalMs);
  CHECK((*list)[1].expires_ms == kNow + 2 * kRemainingStepMs);
}

TEST_CASE("États des membres (fil) : ni moi ni un inconnu n'y entrent", "[net][party]") {
  const PartyRoster party = Party();
  MemberStatuses statuses;
  CHECK(ApplyMemberStatusFrame(StatusFrame(kStranger, 0, 1), statuses, party, kMe, kNow) ==
        FrameOutcome::kUnhandled);
  CHECK(ApplyMemberStatusFrame(StatusFrame(kMe, 0, 1), statuses, party, kMe, kNow) ==
        FrameOutcome::kUnhandled);
  CHECK(ApplyMemberStatusFrame(ChangeFrame(kStranger, kFirstEffect, true, kTotalMs), statuses,
                               party, kMe, kNow) == FrameOutcome::kUnhandled);
  CHECK(statuses.Find(kStranger) == nullptr);
  CHECK(statuses.Find(kMe) == nullptr);
}

TEST_CASE("États des membres (fil) : un refus fait taire, une absence fait oublier",
          "[net][party]") {
  const PartyRoster party = Party();
  MemberStatuses statuses;
  REQUIRE(ApplyMemberStatusFrame(StatusFrame(kAlice, 0, 1), statuses, party, kMe, kNow) ==
          FrameOutcome::kApplied);
  REQUIRE(ApplyMemberStatusFrame(StatusFrame(kAlice, 1, 0), statuses, party, kMe, kNow) ==
          FrameOutcome::kApplied);
  CHECK(statuses.Find(kAlice) == nullptr);
  CHECK_FALSE(statuses.Refused(kAlice, kNow));

  REQUIRE(ApplyMemberStatusFrame(StatusFrame(kAlice, ro::net::kStatusListRefused, 0), statuses,
                                 party, kMe, kNow) == FrameOutcome::kApplied);
  CHECK(statuses.Find(kAlice) == nullptr);
  CHECK(statuses.Refused(kAlice, kNow));
}

TEST_CASE("États des membres (fil) : une transition de la zone touche le membre", "[net][party]") {
  const PartyRoster party = Party();
  MemberStatuses statuses;
  REQUIRE(ApplyMemberStatusFrame(ChangeFrame(kAlice, kFirstEffect, true, kTotalMs), statuses, party,
                                 kMe, kNow) == FrameOutcome::kApplied);
  const std::vector<ActiveStatus>* list = statuses.Find(kAlice);
  REQUIRE(list != nullptr);
  REQUIRE(list->size() == 1);
  CHECK(list->front().expires_ms == kNow + kTotalMs);
  CHECK(list->front().total_ms == kTotalMs);

  REQUIRE(ApplyMemberStatusFrame(ChangeFrame(kAlice, kFirstEffect, false, 0), statuses, party, kMe,
                                 kNow) == FrameOutcome::kApplied);
  CHECK(statuses.Find(kAlice)->empty());
}

TEST_CASE("États des membres (fil) : la cible reçoit la même réponse", "[net][party]") {
  const PartyRoster party = Party();
  MemberStatuses statuses;
  ro::sim::World world;
  ro::sim::Actor alice;
  alice.id = kAlice;
  world.Spawn(std::move(alice));
  REQUIRE(world.Select(kAlice));

  const Frame frame = StatusFrame(kAlice, 0, 1);
  REQUIRE(ApplyTargetStatusFrame(frame, world, kNow) == FrameOutcome::kApplied);
  REQUIRE(ApplyMemberStatusFrame(frame, statuses, party, kMe, kNow) == FrameOutcome::kApplied);
  CHECK(world.target_detail().statuses_told);
  CHECK(world.target_detail().statuses.size() == 1);
  REQUIRE(statuses.Find(kAlice) != nullptr);
  CHECK(statuses.Find(kAlice)->size() == 1);
}
