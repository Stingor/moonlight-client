// Le signalement d'un outil de triche, relu du côté serveur sur un vrai socket :
// le serveur ne répond rien à un paquet qu'il n'a pas su lire, et une
// disposition fausse ne se verrait jamais autrement.

#include "ro/net/cheat_report.h"

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"

using ro::net::Connection;
using ro::test::Connect;
using ro::test::LoopbackServer;

namespace {

// La disposition déclarée par le serveur, `PACKET_CZ_MOONLIGHT_CHEAT_REPORT` :
//   [packetType:2][packetLength:2][tool_name:32][detail:64]
constexpr std::uint16_t kCheatReportOpcode = 0x0F0A;
constexpr std::size_t kLengthAt = 2;
constexpr std::size_t kToolAt = 4;
constexpr std::size_t kDetailAt = 36;
constexpr std::size_t kWireSize = 100;

[[nodiscard]] std::uint16_t U16At(const std::vector<std::uint8_t>& bytes, std::size_t at) {
  return static_cast<std::uint16_t>(bytes[at] | (static_cast<std::uint16_t>(bytes[at + 1]) << 8));
}

[[nodiscard]] std::string FieldAt(const std::vector<std::uint8_t>& bytes, std::size_t at,
                                  std::size_t length) {
  std::string field(reinterpret_cast<const char*>(bytes.data() + at), length);
  return field.substr(0, field.find('\0'));
}

}  // namespace

TEST_CASE("le signalement a la disposition que le serveur lit", "[net][guard]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);

  ro::net::SendCheatReport(link, "Outil", "outil-x86_64.exe");

  const std::vector<std::uint8_t> sent = server.Receive(kWireSize);
  REQUIRE(sent.size() == kWireSize);
  CHECK(U16At(sent, 0) == kCheatReportOpcode);
  CHECK(U16At(sent, kLengthAt) == kWireSize);
  CHECK(FieldAt(sent, kToolAt, ro::net::kCheatToolBytes) == "Outil");
  CHECK(FieldAt(sent, kDetailAt, ro::net::kCheatDetailBytes) == "outil-x86_64.exe");
}

TEST_CASE("un texte trop long est tronqué sans déborder sur le champ suivant", "[net][guard]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);

  const std::string long_tool(ro::net::kCheatToolBytes + 10, 'T');
  ro::net::SendCheatReport(link, long_tool, "d");

  const std::vector<std::uint8_t> sent = server.Receive(kWireSize);
  REQUIRE(sent.size() == kWireSize);
  CHECK(FieldAt(sent, kToolAt, ro::net::kCheatToolBytes) ==
        std::string(ro::net::kCheatToolBytes, 'T'));
  CHECK(FieldAt(sent, kDetailAt, ro::net::kCheatDetailBytes) == "d");
}
