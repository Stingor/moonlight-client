// L'ADOPTION SUR LE FIL : la demande reçue, les refus dits, et ce qu'on envoie.

#include "ro/net/adoption.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/sim/adoption.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/net_frames.h"
#include "support/wire_bytes.h"

using ro::net::ApplyAdoptionFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::AdoptionDesk;
using ro::sim::AdoptionRefusal;
using ro::test::Connect;
using ro::test_support::FrameOf;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::ReadU32;

namespace {

constexpr std::uint16_t kInvitationOpcode = 0x01F6;
constexpr std::uint16_t kRefusalOpcode = 0x0216;
constexpr std::size_t kNameBytes = 24;
constexpr std::uint32_t kParent = 2000010;
constexpr std::uint32_t kSpouse = 2000011;
constexpr std::uint32_t kChild = 2000012;

[[nodiscard]] Frame Invitation(const std::string& name) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, kInvitationOpcode);
  PutU32(bytes, kParent);
  PutU32(bytes, kSpouse);
  for (std::size_t index = 0; index < kNameBytes; ++index) {
    bytes.push_back(index < name.size() ? static_cast<std::uint8_t>(name[index]) : 0);
  }
  return FrameOf(std::move(bytes));
}

[[nodiscard]] Frame Refusal(std::uint32_t reason) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, kRefusalOpcode);
  PutU32(bytes, reason);
  return FrameOf(std::move(bytes));
}

}  // namespace

TEST_CASE("Adoption : la demande reçue porte les deux comptes et le nom", "[net][adoption]") {
  AdoptionDesk desk;
  // « Aurélie » en 1252.
  CHECK(ApplyAdoptionFrame(Invitation("Aur\xE9lie"), desk) == FrameOutcome::kApplied);
  REQUIRE(desk.invitation().has_value());
  CHECK(desk.invitation()->parent_account_id == kParent);
  CHECK(desk.invitation()->spouse_account_id == kSpouse);
  CHECK(desk.invitation()->parent_name == "Aurélie");

  Frame cut = Invitation("Bob");
  cut.bytes.pop_back();
  CHECK(ApplyAdoptionFrame(cut, desk) == FrameOutcome::kUndecodable);
  CHECK(ApplyAdoptionFrame(Frame{.opcode = 0x0001, .bytes = {}}, desk) == FrameOutcome::kUnhandled);
}

TEST_CASE("Adoption : les trois refus que le serveur dit, et pas d'autre", "[net][adoption]") {
  AdoptionDesk desk;
  CHECK(ApplyAdoptionFrame(Refusal(0), desk) == FrameOutcome::kApplied);
  CHECK(ApplyAdoptionFrame(Refusal(1), desk) == FrameOutcome::kApplied);
  CHECK(ApplyAdoptionFrame(Refusal(2), desk) == FrameOutcome::kApplied);
  CHECK(ApplyAdoptionFrame(Refusal(ro::sim::kAdoptionRefusalCount), desk) ==
        FrameOutcome::kIgnored);
  CHECK(desk.TakeRefusals() == std::vector<AdoptionRefusal>{AdoptionRefusal::kAlreadyParent,
                                                            AdoptionRefusal::kParentLevel,
                                                            AdoptionRefusal::kChildMarried});
}

TEST_CASE("Adoption : la demande et la réponse partent aux octets près", "[net][adoption]") {
  ro::test::LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);

  REQUIRE(ro::net::RequestAdoption(link, kChild).has_value());
  std::vector<std::uint8_t> sent = server.Receive(6);
  REQUIRE(sent.size() == 6);
  CHECK(sent[0] == 0xF9);
  CHECK(sent[1] == 0x01);
  CHECK(ReadU32(sent, 2) == kChild);

  const ro::sim::AdoptionInvitation invitation{
      .parent_account_id = kParent, .spouse_account_id = kSpouse, .parent_name = "Ana"};
  REQUIRE(ro::net::AnswerAdoption(link, invitation, true).has_value());
  sent = server.Receive(14);
  REQUIRE(sent.size() == 14);
  CHECK(sent[0] == 0xF7);
  CHECK(sent[1] == 0x01);
  CHECK(ReadU32(sent, 2) == kParent);
  CHECK(ReadU32(sent, 6) == kSpouse);
  CHECK(ReadU32(sent, 10) == 1);

  REQUIRE(ro::net::AnswerAdoption(link, invitation, false).has_value());
  sent = server.Receive(14);
  REQUIRE(sent.size() == 14);
  CHECK(ReadU32(sent, 10) == 0);
}
