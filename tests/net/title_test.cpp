// LES TITRES, décodés — et la demande d'en changer, sur le fil.
//
// 🔴 CE BANC PORTE LES RÈGLES DU CLIENT D'ORIGINE, qu'aucune session ne montre
// vite :
//
//   · seul un succès RÉCLAMÉ donne son titre, et rien ne s'en retire ;
//   · la mise à jour d'un seul succès ne donne rien, même réclamé ;
//   · un changement refusé ne touche pas au titre porté ;
//   · la carte de nom d'un AUTRE ne dit rien de ce que NOUS portons.

#include "ro/net/title.h"

#include <cstdint>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/title_packets.h"
#include "ro/sim/bank.h"
#include "ro/sim/title_book.h"

#include "support/loopback_server.h"

using ro::net::ApplyTitleFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::TitleBook;
using ro::test::LoopbackServer;

namespace {

constexpr std::uint32_t kPlayer = 2000001;
constexpr std::uint32_t kStranger = 2000002;

[[nodiscard]] Frame ListFrame(std::vector<ro::proto::AchievementEntry> entries) {
  ro::proto::AchievementList list;
  list.entries = std::move(entries);
  return Frame{.opcode = ro::proto::AchievementList::kOpcode,
               .bytes = ro::proto::EncodeAchievementList(list)};
}

[[nodiscard]] ro::proto::AchievementEntry Entry(std::uint32_t id, bool completed, bool rewarded) {
  ro::proto::AchievementEntry entry;
  entry.achievement_id = id;
  entry.completed = completed;
  entry.rewarded = rewarded;
  return entry;
}

[[nodiscard]] Frame RewardAckFrame(std::uint8_t failed, std::uint32_t achievement_id) {
  ro::proto::PacketAchievementRewardAck packet;
  packet.failed = failed;
  packet.ach_id = achievement_id;
  return Frame{.opcode = ro::proto::PacketAchievementRewardAck::kOpcode,
               .bytes = ro::proto::Encode(packet)};
}

[[nodiscard]] Frame ChangeAckFrame(std::uint8_t result, std::int32_t title_id) {
  return Frame{.opcode = ro::proto::ChangeTitleAck::kOpcode,
               .bytes = ro::proto::EncodeChangeTitleAck(
                   ro::proto::ChangeTitleAck{.result = result, .title_id = title_id})};
}

[[nodiscard]] Frame NameCardFrame(std::uint32_t gid, std::int32_t title_id) {
  ro::proto::ZcAckReqnameall packet;
  packet.gid = static_cast<std::int32_t>(gid);
  packet.name = "Stingor";
  packet.title_id = title_id;
  return Frame{.opcode = ro::proto::ZcAckReqnameall::kOpcode, .bytes = ro::proto::Encode(packet)};
}

}  // namespace

TEST_CASE("Titres : seul un succès réclamé est retenu", "[net][titres]") {
  TitleBook book;
  // ⚠ LE SECOND EST OBTENU SANS ÊTRE RÉCLAMÉ : le serveur accepterait son
  // titre, mais le client ne le propose pas.
  const Frame frame = ListFrame(
      {Entry(900007, true, true), Entry(900008, true, false), Entry(128001, false, false)});
  CHECK(ApplyTitleFrame(frame, book, kPlayer) == FrameOutcome::kApplied);
  REQUIRE(book.rewarded_achievements().size() == 1);
  CHECK(book.rewarded_achievements()[0] == 900007);
}

TEST_CASE("Titres : une liste suivante ajoute sans rien retirer", "[net][titres]") {
  TitleBook book;
  static_cast<void>(ApplyTitleFrame(ListFrame({Entry(900007, true, true)}), book, kPlayer));
  // Une seconde liste qui ne le porte plus ne reprend pas ce qui a été donné,
  // et une liste qui le redit ne le double pas.
  static_cast<void>(ApplyTitleFrame(
      ListFrame({Entry(900008, true, true), Entry(900007, true, true)}), book, kPlayer));
  static_cast<void>(ApplyTitleFrame(ListFrame({}), book, kPlayer));
  CHECK(book.rewarded_achievements().size() == 2);
}

TEST_CASE("Titres : l'accusé d'une réclamation ne donne aucun titre", "[net][titres]") {
  // 🔴 ZÉRO EST UN REFUS chez le serveur, et une réclamation qui rapporte un
  // titre reçoit la liste complète, pas un accusé. Ce traducteur ne le lit pas.
  TitleBook book;
  CHECK(ApplyTitleFrame(RewardAckFrame(0, 900007), book, kPlayer) == FrameOutcome::kUnhandled);
  CHECK(ApplyTitleFrame(RewardAckFrame(1, 900007), book, kPlayer) == FrameOutcome::kUnhandled);
  CHECK(book.rewarded_achievements().empty());
}

TEST_CASE("Titres : la mise à jour d'un seul succès ne donne rien", "[net][titres]") {
  // 🔴 MÊME RÉCLAMÉ. Le titre ne se prend qu'à la liste complète — et le
  // serveur renvoie justement la liste complète après une récompense à titre.
  TitleBook book;
  ro::proto::PacketAchievementUpdate packet;
  packet.ach.ach_id = 900007;
  packet.ach.completed = 1;
  packet.ach.reward = 1;
  const Frame frame{.opcode = ro::proto::PacketAchievementUpdate::kOpcode,
                    .bytes = ro::proto::Encode(packet)};
  CHECK(ApplyTitleFrame(frame, book, kPlayer) == FrameOutcome::kUnhandled);
  CHECK(book.rewarded_achievements().empty());
}

TEST_CASE("Titres : un changement accepté se porte, un refus ne touche à rien", "[net][titres]") {
  TitleBook book;
  CHECK(ApplyTitleFrame(ChangeAckFrame(0, 1103), book, kPlayer) == FrameOutcome::kApplied);
  CHECK(book.equipped() == 1103);
  CHECK(ApplyTitleFrame(ChangeAckFrame(1, 1046), book, kPlayer) == FrameOutcome::kIgnored);
  CHECK(book.equipped() == 1103);
  // Zéro, accepté, retire le titre.
  CHECK(ApplyTitleFrame(ChangeAckFrame(0, 0), book, kPlayer) == FrameOutcome::kApplied);
  CHECK(book.equipped() == 0);
}

TEST_CASE("Titres : seule NOTRE carte de nom dit ce qu'on porte", "[net][titres]") {
  TitleBook book;
  CHECK(ApplyTitleFrame(NameCardFrame(kStranger, 1046), book, kPlayer) == FrameOutcome::kIgnored);
  CHECK(book.equipped() == 0);
  CHECK(ApplyTitleFrame(NameCardFrame(kPlayer, 1001), book, kPlayer) == FrameOutcome::kApplied);
  CHECK(book.equipped() == 1001);
}

TEST_CASE("Titres : la demande de changement part telle quelle", "[net][titres]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  auto link = ro::net::Connection::Open("127.0.0.1", server.port());
  REQUIRE(link.has_value());
  REQUIRE(server.Accept());

  REQUIRE(ro::net::RequestChangeTitle(*link, 1104).has_value());
  const std::vector<std::uint8_t> sent = server.Receive(ro::proto::ChangeTitleRequest::kWireSize);
  REQUIRE(sent.size() == ro::proto::ChangeTitleRequest::kWireSize);
  CHECK((sent[0] | (sent[1] << 8)) == ro::proto::ChangeTitleRequest::kOpcode);
  CHECK((sent[2] | (sent[3] << 8)) == 1104);
}
