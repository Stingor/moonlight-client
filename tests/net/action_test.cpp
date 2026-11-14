#include "ro/net/action.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/manual_packets.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/wire_bytes.h"

using ro::net::Connection;
using ro::net::RequestAttack;
using ro::net::RequestPickup;
using ro::net::RequestSit;
using ro::proto::ActionRequest;
using ro::proto::PickupRequest;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::ReadU32;

TEST_CASE("Actions : frapper part en sept octets, la cible puis l'action", "[net][action]") {
  // Ce que le serveur reçoit, octet par octet : c'est la seule preuve que la
  // demande traverse le fil telle qu'il la lit.
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);

  REQUIRE(RequestAttack(link, 0x11223344, true).has_value());
  const std::vector<std::uint8_t> repeated = server.Receive(ActionRequest::kWireSize);
  REQUIRE(repeated.size() == ActionRequest::kWireSize);
  CHECK((repeated[0] | (repeated[1] << 8)) == ActionRequest::kOpcode);
  CHECK(ReadU32(repeated, 2) == 0x11223344);
  CHECK(repeated[6] == ActionRequest::kAttackRepeatedly);

  REQUIRE(RequestAttack(link, 7, false).has_value());
  const std::vector<std::uint8_t> once = server.Receive(ActionRequest::kWireSize);
  REQUIRE(once.size() == ActionRequest::kWireSize);
  CHECK(ReadU32(once, 2) == 7);
  CHECK(once[6] == ActionRequest::kAttackOnce);
}

TEST_CASE("Actions : ramasser nomme l'objet, et rien d'autre", "[net][action]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);

  REQUIRE(RequestPickup(link, 0xAABBCCDD).has_value());
  const std::vector<std::uint8_t> bytes = server.Receive(PickupRequest::kWireSize);
  REQUIRE(bytes.size() == PickupRequest::kWireSize);
  CHECK((bytes[0] | (bytes[1] << 8)) == PickupRequest::kOpcode);
  CHECK(ReadU32(bytes, 2) == 0xAABBCCDD);
}

TEST_CASE("Actions : s'asseoir et se relever ne visent personne", "[net][action]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);

  REQUIRE(RequestSit(link, true).has_value());
  const std::vector<std::uint8_t> sit = server.Receive(ActionRequest::kWireSize);
  REQUIRE(sit.size() == ActionRequest::kWireSize);
  CHECK(ReadU32(sit, 2) == 0);
  CHECK(sit[6] == ActionRequest::kSitDown);

  REQUIRE(RequestSit(link, false).has_value());
  const std::vector<std::uint8_t> stand = server.Receive(ActionRequest::kWireSize);
  REQUIRE(stand.size() == ActionRequest::kWireSize);
  CHECK(stand[6] == ActionRequest::kStandUp);
}
