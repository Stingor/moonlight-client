// LE JOURNAL DES BLOCAGES DE CHAT sur le fil : la demande telle qu'elle part,
// la réponse telle que le serveur l'écrit, et ce qu'une réponse abîmée donne.

#include "ro/net/chat_blocks.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"

#include "support/loopback_server.h"
#include "support/net_frames.h"
#include "support/wire_bytes.h"

using ro::net::ChatBlockKind;
using ro::net::ChatBlocksStatus;
using ro::test::LoopbackServer;
using ro::test_support::ReadU16;
using ro::test_support::ReadU32;

namespace {

constexpr std::uint32_t kTarget = 2000123;
constexpr std::uint32_t kMutedAt = 1760190000;
constexpr std::uint32_t kUnmutedAt = 1760193600;
constexpr std::int32_t kMuteMinutes = 60;

// Une réponse telle que le serveur l'écrit : la plus récente d'abord.
[[nodiscard]] std::vector<std::uint8_t> Reply(std::uint8_t status,
                                              std::vector<ro::proto::MoonlightChatBlock> entries) {
  ro::proto::ZcMoonlightChatBlocks packet;
  packet.aid = kTarget;
  packet.status = status;
  packet.entries = std::move(entries);
  return ro::proto::Encode(packet);
}

[[nodiscard]] std::vector<ro::proto::MoonlightChatBlock> UnmuteThenMute() {
  return {{.date = kUnmutedAt, .minutes = 0, .kind = 1, .author = "Garde"},
          {.date = kMutedAt, .minutes = kMuteMinutes, .kind = 0, .author = "Sentinelle"}};
}

}  // namespace

TEST_CASE("Blocages : la demande porte sa longueur puis l'AID", "[net][chat_blocks]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  auto link = ro::net::Connection::Open("127.0.0.1", server.port());
  REQUIRE(link.has_value());
  REQUIRE(server.Accept());
  REQUIRE(ro::net::RequestChatBlocks(*link, kTarget).has_value());
  constexpr std::size_t kSize = ro::proto::CzMoonlightReqChatBlocks::kWireSize;
  const std::vector<std::uint8_t> sent = server.Receive(kSize);
  REQUIRE(sent.size() == kSize);
  CHECK(ReadU16(sent, 0) == 0x0F3E);
  // ⚠ LA LONGUEUR EST LUE : à zéro, le serveur rejette sans un mot.
  CHECK(ReadU16(sent, 2) == kSize);
  CHECK(ReadU32(sent, 4) == kTarget);
}

TEST_CASE("Blocages : les entrées se lisent dans l'ordre du serveur", "[net][chat_blocks]") {
  const auto reply = ro::net::ReadChatBlocks(ro::test_support::FrameOf(Reply(0, UnmuteThenMute())));
  REQUIRE(reply.has_value());
  CHECK(reply->aid == kTarget);
  CHECK(reply->status == ChatBlocksStatus::kFound);
  REQUIRE(reply->blocks.size() == 2);
  CHECK(reply->blocks[0].kind == ChatBlockKind::kUnmute);
  CHECK(reply->blocks[0].date_s == kUnmutedAt);
  CHECK(reply->blocks[0].author == "Garde");
  CHECK(reply->blocks[1].kind == ChatBlockKind::kMute);
  CHECK(reply->blocks[1].minutes == kMuteMinutes);
  CHECK(reply->blocks[1].author == "Sentinelle");
}

TEST_CASE("Blocages : chaque statut se distingue d'une liste vide", "[net][chat_blocks]") {
  CHECK(ro::net::DecodeChatBlocks(Reply(0, {}))->status == ChatBlocksStatus::kFound);
  CHECK(ro::net::DecodeChatBlocks(Reply(1, {}))->status == ChatBlocksStatus::kMissing);
  CHECK(ro::net::DecodeChatBlocks(Reply(2, {}))->status == ChatBlocksStatus::kDenied);
  CHECK(ro::net::DecodeChatBlocks(Reply(3, {}))->status == ChatBlocksStatus::kLogOff);
  CHECK(ro::net::DecodeChatBlocks(Reply(4, {}))->status == ChatBlocksStatus::kReadFailed);
  // Un statut que ce client ne connaît pas ne promet rien.
  constexpr std::uint8_t kUnknownStatus = 9;
  CHECK(ro::net::DecodeChatBlocks(Reply(kUnknownStatus, {}))->status ==
        ChatBlocksStatus::kReadFailed);
  // Hors du statut « trouvé », des entrées éventuelles ne se lisent pas.
  CHECK(ro::net::DecodeChatBlocks(Reply(2, UnmuteThenMute()))->blocks.empty());
}

TEST_CASE("Blocages : une entrée d'un genre inconnu est sautée", "[net][chat_blocks]") {
  constexpr std::uint8_t kUnknownKind = 7;
  std::vector<ro::proto::MoonlightChatBlock> entries = UnmuteThenMute();
  entries[0].kind = kUnknownKind;
  const auto reply = ro::net::DecodeChatBlocks(Reply(0, entries));
  REQUIRE(reply.has_value());
  REQUIRE(reply->blocks.size() == 1);
  CHECK(reply->blocks[0].author == "Sentinelle");
}

TEST_CASE("Blocages : une trame qui ne se découpe pas n'est pas une réponse",
          "[net][chat_blocks]") {
  std::vector<std::uint8_t> bytes = Reply(0, UnmuteThenMute());
  bytes.pop_back();
  CHECK_FALSE(ro::net::DecodeChatBlocks(bytes).has_value());
  CHECK_FALSE(ro::net::ReadChatBlocks(ro::net::Frame{.opcode = 0x0F23, .bytes = {}}).has_value());
}
