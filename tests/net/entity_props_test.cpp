// L'INSPECTEUR DU STAFF sur le fil : la demande telle qu'elle part, la réponse
// telle que le serveur l'écrit, et ce qu'une réponse tronquée laisse.

#include "ro/net/entity_props.h"

#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"

#include "support/loopback_server.h"
#include "support/net_frames.h"
#include "support/wire_bytes.h"

using ro::net::EntityPropsStatus;
using ro::test::LoopbackServer;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::ReadU16;
using ro::test_support::ReadU32;

namespace {

constexpr std::uint32_t kTarget = 2000123;
constexpr std::uint8_t kLowByte = 0xFF;
constexpr unsigned kBitsPerByte = 8;

void PutText(std::vector<std::uint8_t>& bytes, std::string_view text) {
  bytes.push_back(static_cast<std::uint8_t>(text.size()));
  bytes.insert(bytes.end(), text.begin(), text.end());
}

// Une réponse telle que le serveur l'écrit, la longueur posée en dernier.
[[nodiscard]] std::vector<std::uint8_t> Reply(
    std::uint8_t status, std::uint8_t count,
    const std::vector<std::pair<std::string_view, std::string_view>>& pairs) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::ZcMoonlightEntityProps::kOpcode);
  PutU16(bytes, 0);
  PutU32(bytes, kTarget);
  bytes.push_back(status);
  bytes.push_back(count);
  for (const auto& [key, value] : pairs) {
    PutText(bytes, key);
    PutText(bytes, value);
  }
  bytes[2] = static_cast<std::uint8_t>(bytes.size() & kLowByte);
  bytes[3] = static_cast<std::uint8_t>(bytes.size() >> kBitsPerByte);
  return bytes;
}

}  // namespace

TEST_CASE("Inspecteur : la demande porte sa longueur puis le GID", "[net][entity_props]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  auto link = ro::net::Connection::Open("127.0.0.1", server.port());
  REQUIRE(link.has_value());
  REQUIRE(server.Accept());
  REQUIRE(ro::net::RequestEntityProps(*link, kTarget).has_value());
  constexpr std::size_t kSize = ro::proto::CzMoonlightReqEntityProps::kWireSize;
  const std::vector<std::uint8_t> sent = server.Receive(kSize);
  REQUIRE(sent.size() == kSize);
  CHECK(ReadU16(sent, 0) == 0x0F22);
  // ⚠ LA LONGUEUR EST LUE : à zéro, le serveur rejette sans un mot.
  CHECK(ReadU16(sent, 2) == kSize);
  CHECK(ReadU32(sent, 4) == kTarget);
}

TEST_CASE("Inspecteur : les paires se lisent dans l'ordre, les titres à valeur vide",
          "[net][entity_props]") {
  const auto reply = ro::net::ReadEntityProps(ro::test_support::FrameOf(Reply(
      0, 3, {{"Serveur", ""}, {"Type", "BL_PC (joueur)"}, {"Carte", "prontera (150, 180)"}})));
  REQUIRE(reply.has_value());
  CHECK(reply->gid == kTarget);
  CHECK(reply->status == EntityPropsStatus::kFound);
  REQUIRE(reply->props.size() == 3);
  CHECK(reply->props[0].key == "Serveur");
  CHECK(reply->props[0].value.empty());
  CHECK(reply->props[1].value == "BL_PC (joueur)");
  CHECK(reply->props[2].key == "Carte");
}

TEST_CASE("Inspecteur : le refus et l'entité introuvable ne portent rien", "[net][entity_props]") {
  const auto denied = ro::net::DecodeEntityProps(Reply(2, 0, {}));
  REQUIRE(denied.has_value());
  CHECK(denied->status == EntityPropsStatus::kDenied);
  CHECK(denied->props.empty());
  const auto missing = ro::net::DecodeEntityProps(Reply(1, 0, {}));
  REQUIRE(missing.has_value());
  CHECK(missing->status == EntityPropsStatus::kMissing);
  // Un statut que ce client ne connaît pas se lit comme une entité introuvable.
  constexpr std::uint8_t kUnknownStatus = 7;
  CHECK(ro::net::DecodeEntityProps(Reply(kUnknownStatus, 0, {}))->status ==
        EntityPropsStatus::kMissing);
}

TEST_CASE("Inspecteur : une trame tronquée garde ce qui y tient entier", "[net][entity_props]") {
  std::vector<std::uint8_t> bytes = Reply(0, 3, {{"Nom", "Alice"}, {"Niveau", "base 99"}});
  // La troisième paire est annoncée, mais n'a que sa clé, sans sa valeur.
  PutText(bytes, "Etat");
  auto reply = ro::net::DecodeEntityProps(bytes);
  REQUIRE(reply.has_value());
  REQUIRE(reply->props.size() == 3);
  CHECK(reply->props[2].key == "Etat");
  CHECK(reply->props[2].value.empty());
  // Une clé coupée elle-même n'est pas gardée.
  bytes.resize(bytes.size() - 2);
  reply = ro::net::DecodeEntityProps(bytes);
  REQUIRE(reply.has_value());
  CHECK(reply->props.size() == 2);
}

TEST_CASE("Inspecteur : un en-tête incomplet n'est pas une réponse", "[net][entity_props]") {
  std::vector<std::uint8_t> bytes = Reply(0, 0, {});
  bytes.resize(ro::net::kEntityPropsHeaderSize - 1);
  CHECK_FALSE(ro::net::DecodeEntityProps(bytes).has_value());
  CHECK_FALSE(ro::net::ReadEntityProps(ro::net::Frame{.opcode = 0x0F20, .bytes = {}}).has_value());
}
