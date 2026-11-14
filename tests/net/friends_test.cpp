// LA LISTE D'AMIS, telle que le serveur la décrit, et ce qu'on lui demande.

#include "ro/net/friends.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/net/social_text.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/friend_list.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/net_frames.h"
#include "support/wire_bytes.h"

using ro::net::ApplyFriendFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::FriendList;
using ro::test::Connect;
using ro::test_support::FrameOf;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::ReadU32;

namespace {

void PutText(std::vector<std::uint8_t>& into, const std::string& text, std::size_t width) {
  for (std::size_t index = 0; index < width; ++index) {
    into.push_back(index < text.size() ? static_cast<std::uint8_t>(text[index]) : 0);
  }
}

FriendList Listed() {
  ro::proto::ZcFriendsList packet;
  ro::proto::ZcFriendsListSub alice;
  alice.AID = 1;
  alice.CID = 10;
  // « Aurélie » en 1252.
  alice.name = "Aur\xE9lie";
  ro::proto::ZcFriendsListSub bob;
  bob.AID = 2;
  bob.CID = 20;
  bob.name = "Bob";
  packet.friends = {alice, bob};
  FriendList friends;
  REQUIRE(ApplyFriendFrame(FrameOf(ro::proto::Encode(packet)), friends) == FrameOutcome::kApplied);
  return friends;
}

Frame AddResult(std::uint16_t result, std::uint32_t account_id, std::uint32_t char_id,
                const std::string& name) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, 0x0209);
  PutU16(bytes, result);
  PutU32(bytes, account_id);
  PutU32(bytes, char_id);
  PutText(bytes, name, 24);
  return FrameOf(std::move(bytes));
}

}  // namespace

TEST_CASE("Amis : la liste se pose, puis chacun se dit en ligne", "[net][friend]") {
  FriendList friends = Listed();
  REQUIRE(friends.friends().size() == 2);
  CHECK(friends.HasName("Aurélie"));
  CHECK_FALSE(friends.Find(1, 10)->online);

  ro::proto::ZcFriendsState state;
  state.AID = 1;
  state.CID = 10;
  state.offline = 0;
  state.name = "Aur\xE9lie";
  CHECK(ApplyFriendFrame(FrameOf(ro::proto::Encode(state)), friends) == FrameOutcome::kApplied);
  CHECK(friends.Find(1, 10)->online);
  state.offline = 1;
  CHECK(ApplyFriendFrame(FrameOf(ro::proto::Encode(state)), friends) == FrameOutcome::kApplied);
  CHECK_FALSE(friends.Find(1, 10)->online);
  state.CID = 99;
  CHECK(ApplyFriendFrame(FrameOf(ro::proto::Encode(state)), friends) == FrameOutcome::kIgnored);
}

TEST_CASE("Amis : une demande reçue attend sa réponse", "[net][friend]") {
  FriendList friends;
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, 0x0207);
  PutU32(bytes, 5);
  PutU32(bytes, 50);
  PutText(bytes, "Carole", 24);
  CHECK(ApplyFriendFrame(FrameOf(std::move(bytes)), friends) == FrameOutcome::kApplied);
  REQUIRE(friends.invitation().has_value());
  CHECK(friends.invitation()->account_id == 5);
  CHECK(friends.invitation()->char_id == 50);
  CHECK(friends.invitation()->name == "Carole");
}

TEST_CASE("Amis : un ajout accepté entre en ligne, un refus se dit", "[net][friend]") {
  FriendList friends = Listed();
  CHECK(ApplyFriendFrame(AddResult(0, 3, 30, "Dan"), friends) == FrameOutcome::kApplied);
  REQUIRE(friends.Find(3, 30) != nullptr);
  CHECK(friends.Find(3, 30)->online);
  CHECK(ApplyFriendFrame(AddResult(1, 4, 40, "Eve"), friends) == FrameOutcome::kApplied);
  CHECK(friends.Find(4, 40) == nullptr);
  CHECK(ApplyFriendFrame(AddResult(5, 4, 40, "Eve"), friends) == FrameOutcome::kIgnored);

  const std::vector<ro::sim::FriendNotice> notices = friends.TakeNotices();
  REQUIRE(notices.size() == 2);
  const ro::net::SocialLine made = ro::net::FriendNoticeLine(notices[0]);
  CHECK(made.text == "Vous êtes désormais ami avec (Dan).");
  CHECK(made.tone == ro::net::SocialTone::kSuccess);
  const ro::net::SocialLine declined = ro::net::FriendNoticeLine(notices[1]);
  CHECK(declined.text == "(Eve) ne souhaite pas devenir votre ami.");
  CHECK(declined.tone == ro::net::SocialTone::kRefusal);
}

TEST_CASE("Amis : un retrait retire le bon personnage", "[net][friend]") {
  FriendList friends = Listed();
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, 0x020A);
  PutU32(bytes, 2);
  PutU32(bytes, 20);
  CHECK(ApplyFriendFrame(FrameOf(bytes), friends) == FrameOutcome::kApplied);
  CHECK(friends.friends().size() == 1);
  CHECK(ApplyFriendFrame(FrameOf(bytes), friends) == FrameOutcome::kIgnored);
  bytes.pop_back();
  CHECK(ApplyFriendFrame(FrameOf(bytes), friends) == FrameOutcome::kUndecodable);
  CHECK(ApplyFriendFrame(Frame{.opcode = 0x0001, .bytes = {}}, friends) ==
        FrameOutcome::kUnhandled);
}

TEST_CASE("Amis : chaque demande part aux octets près", "[net][friend]") {
  ro::test::LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);

  REQUIRE(ro::net::RequestFriend(link, "Aurélie").has_value());
  std::vector<std::uint8_t> sent = server.Receive(26);
  REQUIRE(sent.size() == 26);
  CHECK(sent[0] == 0x02);
  CHECK(sent[1] == 0x02);
  CHECK(sent[5] == 0xE9);
  CHECK(sent[25] == 0);

  REQUIRE(ro::net::AnswerFriendRequest(link, 5, 50, false).has_value());
  sent = server.Receive(14);
  REQUIRE(sent.size() == 14);
  CHECK(sent[0] == 0x08);
  CHECK(sent[1] == 0x02);
  CHECK(ReadU32(sent, 2) == 5);
  CHECK(ReadU32(sent, 6) == 50);
  CHECK(ReadU32(sent, 10) == 0);

  REQUIRE(ro::net::RequestFriendRemoval(link, 2, 20).has_value());
  sent = server.Receive(10);
  REQUIRE(sent.size() == 10);
  CHECK(sent[0] == 0x03);
  CHECK(sent[1] == 0x02);
  CHECK(ReadU32(sent, 2) == 2);
  CHECK(ReadU32(sent, 6) == 20);
}
