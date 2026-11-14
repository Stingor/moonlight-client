// LE DÉBUT DU FLUX DE ZONE, tel que le serveur l'envoie : ce qui précède l'accord
// ne se juge comme un identifiant nu que tant qu'aucune trame n'a ouvert le flux.

#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/session.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"

#include "support/char_session.h"
#include "support/loopback_server.h"

using ro::net::CharacterPhase;
using ro::net::Connection;
using ro::net::LoginPhase;
using ro::net::MapPhase;
using ro::net::PhaseState;
using ro::net::ServerAddress;
using ro::test::LoopbackServer;
using ro::test_support::kLoopbackWireIp;
using ro::test_support::Settle;

TEST_CASE("Session : le refus de zone tient en trois octets et garde son motif", "[net][session]") {
  // « Connexion encore reconnue » : l'avis puis la fermeture, sans rien d'autre.
  // Trois octets ne suffisent pas à juger un identifiant nu, et la fermeture dit
  // qu'il n'en viendra pas plus.
  constexpr std::uint8_t kStillRecognized = 8;
  LoopbackServer server;
  REQUIRE(server.listening());

  CharacterPhase::Handoff handoff;
  handoff.address = ServerAddress{kLoopbackWireIp, server.port()};
  auto phase = MapPhase::Start(handoff, LoginPhase::Account(), 0);
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());
  REQUIRE(server.Receive(ro::proto::EnterMapRequest::kWireSize).size() ==
          ro::proto::EnterMapRequest::kWireSize);

  ro::proto::ScNotifyBan ban;
  ban.result = kStillRecognized;
  REQUIRE(server.Send(Encode(ban)));
  server.CloseClient();

  CHECK(Settle(*phase) == PhaseState::kRefused);
  CHECK_FALSE(phase->refusal_text().empty());
}

TEST_CASE("Session : une trame de quatre octets devant l'accord n'est pas un identifiant",
          "[net][session]") {
  // Le flux réel de la zone : son accusé encadré, puis, après l'aller-retour entre
  // serveurs, la taille du sac et l'accord dans la même rafale.
  constexpr std::uint16_t kAccountAckOpcode = 0x0283;
  constexpr std::uint16_t kBagSizeOpcode = 0x0B18;
  constexpr int kMostRounds = 2000;
  LoopbackServer server;
  REQUIRE(server.listening());

  CharacterPhase::Handoff handoff;
  handoff.address = ServerAddress{kLoopbackWireIp, server.port()};
  auto phase = MapPhase::Start(handoff, LoginPhase::Account(), 0);
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());
  REQUIRE(server.Receive(ro::proto::EnterMapRequest::kWireSize).size() ==
          ro::proto::EnterMapRequest::kWireSize);

  const std::vector<std::uint8_t> account_ack = {0x83, 0x02, 0x80, 0x84, 0x1E, 0x00};
  REQUIRE(server.Send(account_ack));
  for (int round = 0; round < kMostRounds && phase->step() == MapPhase::Step::kAwaitingAccountId;
       ++round) {
    static_cast<void>(phase->Advance());
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  REQUIRE(phase->step() == MapPhase::Step::kAwaitingAccept);

  std::vector<std::uint8_t> burst = {0x18, 0x0B, 0x2C, 0x00};
  const std::vector<std::uint8_t> accept = Encode(ro::proto::ZcAcceptEnter());
  burst.insert(burst.end(), accept.begin(), accept.end());
  REQUIRE(server.Send(burst));

  REQUIRE(Settle(*phase) == PhaseState::kDone);
  Connection link = phase->TakeLink();
  const Connection::Polled polled = link.Poll();
  REQUIRE(polled.frames.size() == 2);
  CHECK(polled.frames[0].opcode == kAccountAckOpcode);
  CHECK(polled.frames[1].opcode == kBagSizeOpcode);
}
