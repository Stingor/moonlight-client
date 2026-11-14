// LES SUCCÈS, décodés — et la réclamation d'une récompense, sur le fil.
//
// 🔴 CE BANC PORTE CE QU'UNE SESSION NE MONTRERAIT PAS VITE :
//
//   · la bannière ne se lève qu'au PASSAGE à l'achèvement ;
//   · l'octet de l'accusé vaut un pour une réussite, zéro pour un refus ;
//   · une réclamation hors des gardes du serveur ne part pas.

#include "ro/net/achievement.h"

#include <cstdint>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/title_packets.h"
#include "ro/sim/achievement_book.h"

#include "support/loopback_server.h"

using ro::net::ApplyAchievementFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::AchievementBook;
using ro::test::LoopbackServer;

namespace {

constexpr std::uint32_t kDone = 120001;
constexpr std::uint32_t kRunning = 120002;

[[nodiscard]] Frame ListFrame() {
  ro::proto::AchievementList list;
  list.total_points = 565;
  list.rank = 7;
  list.current_rank_points = 15;
  list.next_rank_points = 178;
  ro::proto::AchievementEntry done;
  done.achievement_id = kDone;
  done.completed = true;
  done.completed_at = 1585008079;
  ro::proto::AchievementEntry running;
  running.achievement_id = kRunning;
  running.objectives[1] = 4;
  list.entries = {done, running};
  return Frame{.opcode = ro::proto::AchievementList::kOpcode,
               .bytes = ro::proto::EncodeAchievementList(list)};
}

[[nodiscard]] Frame UpdateFrame(std::uint32_t achievement_id, bool completed, bool rewarded) {
  ro::proto::PacketAchievementUpdate packet;
  packet.total_points = 575;
  packet.rank = 7;
  packet.current_rank_points = 25;
  packet.next_rank_points = 178;
  packet.ach.ach_id = achievement_id;
  packet.ach.completed = completed ? 1 : 0;
  packet.ach.reward = rewarded ? 1 : 0;
  packet.ach.objective[0] = 1;
  return Frame{.opcode = ro::proto::PacketAchievementUpdate::kOpcode,
               .bytes = ro::proto::Encode(packet)};
}

[[nodiscard]] Frame AckFrame(std::uint8_t granted, std::uint32_t achievement_id) {
  ro::proto::PacketAchievementRewardAck packet;
  packet.failed = granted;
  packet.ach_id = achievement_id;
  return Frame{.opcode = ro::proto::PacketAchievementRewardAck::kOpcode,
               .bytes = ro::proto::Encode(packet)};
}

}  // namespace

TEST_CASE("Succès : la liste pose le score, le rang et chaque progression", "[net][succes]") {
  AchievementBook book;
  const auto result = ApplyAchievementFrame(ListFrame(), book);
  CHECK(result.outcome == FrameOutcome::kApplied);
  CHECK_FALSE(result.completed.has_value());
  CHECK(book.standing().total_points == 565);
  CHECK(book.standing().rank_points == 15);
  CHECK(book.standing().rank_span == 178);
  REQUIRE(book.Find(kRunning) != nullptr);
  CHECK(book.Find(kRunning)->counts[1] == 4);
  CHECK(book.Find(kDone)->completed_at == 1585008079);
}

TEST_CASE("Succès : la mise à jour qui achève le dit, une seule fois", "[net][succes]") {
  AchievementBook book;
  static_cast<void>(ApplyAchievementFrame(ListFrame(), book));
  const auto first = ApplyAchievementFrame(UpdateFrame(kRunning, true, false), book);
  CHECK(first.completed == kRunning);
  CHECK(book.standing().total_points == 575);
  const auto again = ApplyAchievementFrame(UpdateFrame(kRunning, true, false), book);
  CHECK_FALSE(again.completed.has_value());
}

TEST_CASE("Succès : zéro dans l'accusé est un refus, un est une réussite", "[net][succes]") {
  AchievementBook book;
  static_cast<void>(ApplyAchievementFrame(ListFrame(), book));
  const auto refused = ApplyAchievementFrame(AckFrame(0, kDone), book);
  CHECK(refused.refused_claim == kDone);
  const auto granted = ApplyAchievementFrame(AckFrame(1, kDone), book);
  CHECK_FALSE(granted.refused_claim.has_value());
}

TEST_CASE("Succès : une trame étrangère n'est pas lue", "[net][succes]") {
  AchievementBook book;
  const Frame other{.opcode = ro::proto::ChangeTitleAck::kOpcode,
                    .bytes = ro::proto::EncodeChangeTitleAck({})};
  CHECK(ApplyAchievementFrame(other, book).outcome == FrameOutcome::kUnhandled);
}

TEST_CASE("Succès : la réclamation ne part que dans les gardes du serveur", "[net][succes]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  auto link = ro::net::Connection::Open("127.0.0.1", server.port());
  REQUIRE(link.has_value());
  REQUIRE(server.Accept());

  AchievementBook book;
  static_cast<void>(ApplyAchievementFrame(ListFrame(), book));
  CHECK_FALSE(ro::net::ClaimAchievementReward(*link, book, kRunning, true).has_value());
  CHECK_FALSE(ro::net::ClaimAchievementReward(*link, book, kDone, false).has_value());
  REQUIRE(ro::net::ClaimAchievementReward(*link, book, kDone, true).has_value());
  CHECK(book.ClaimPending(kDone));
  // En vol, elle ne repart pas.
  CHECK_FALSE(ro::net::ClaimAchievementReward(*link, book, kDone, true).has_value());

  const std::vector<std::uint8_t> sent =
      server.Receive(ro::proto::AchievementRewardRequest::kWireSize);
  REQUIRE(sent.size() == ro::proto::AchievementRewardRequest::kWireSize);
  CHECK((sent[0] | (sent[1] << 8)) == ro::proto::AchievementRewardRequest::kOpcode);
  const std::uint32_t sent_id =
      static_cast<std::uint32_t>(sent[2]) | (static_cast<std::uint32_t>(sent[3]) << 8) |
      (static_cast<std::uint32_t>(sent[4]) << 16) | (static_cast<std::uint32_t>(sent[5]) << 24);
  CHECK(sent_id == kDone);
}
