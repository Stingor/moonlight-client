// L'ALLIANCE ET L'HOSTILITÉ DE GUILDE : la proposition reçue, sa réponse sur le
// fil, et les verdicts écrits dans la conversation.
//
// 🔴 CE BANC PORTE CE QU'AUCUNE SESSION NE MONTRERAIT VITE :
//
//   · une proposition reçue survit au départ de la guilde — le serveur attend
//     sa réponse et refuse toute autre proposition d'ici là ;
//   · une nouvelle proposition remplace l'ancienne ;
//   · un verdict inconnu n'écrit rien.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/net/guild.h"
#include "ro/proto/guild_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/guild.h"
#include "ro/sim/world.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/net_frames.h"
#include "support/wire_bytes.h"

using ro::net::ApplyGuildFrame;
using ro::net::ApplyGuildRelationAnswerFrame;
using ro::net::FrameOutcome;
using ro::sim::GuildBook;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::FrameOf;
using ro::test_support::ReadU16;
using ro::test_support::ReadU32;

namespace {

constexpr std::uint32_t kMe = 150001;
constexpr std::uint32_t kInviter = 2000042;
constexpr std::uint32_t kLaterInviter = 2000043;
constexpr std::uint32_t kNowMs = 42'000;
constexpr std::uint32_t kWhite = 0xFFFFFF;
constexpr std::uint32_t kRed = 0xFF0000;

// Les verdicts, tels que le serveur les numérote.
constexpr std::uint8_t kAllianceAlready = 0;
constexpr std::uint8_t kAlliancePending = 1;
constexpr std::uint8_t kAllianceConcluded = 2;
constexpr std::uint8_t kAllianceNotConcluded = 3;
constexpr std::uint8_t kAllianceOwnFull = 4;
constexpr std::uint8_t kAllianceMasterOnly = 5;
constexpr std::uint8_t kHostilityDeclared = 0;
constexpr std::uint8_t kHostilityOwnFull = 1;
constexpr std::uint8_t kHostilityAlready = 2;
constexpr std::uint8_t kHostilityDisabled = 3;
constexpr std::uint8_t kUnknownVerdict = 9;

[[nodiscard]] ro::net::Frame Offer(std::uint32_t inviter, const std::string& guild_name) {
  ro::proto::ZcReqAllyGuild packet;
  packet.inviterId = inviter;
  packet.inviterGuildName = guild_name;
  return FrameOf(ro::proto::Encode(packet));
}

[[nodiscard]] FrameOutcome AllianceVerdict(std::uint8_t flag, ro::sim::World& world) {
  ro::proto::ZcAckReqAllyGuild packet;
  packet.flag = flag;
  return ApplyGuildRelationAnswerFrame(FrameOf(ro::proto::Encode(packet)), world, kNowMs);
}

[[nodiscard]] FrameOutcome HostilityVerdict(std::uint8_t flag, ro::sim::World& world) {
  ro::proto::ZcAckReqHostileGuild packet;
  packet.flag = flag;
  return ApplyGuildRelationAnswerFrame(FrameOf(ro::proto::Encode(packet)), world, kNowMs);
}

}  // namespace

TEST_CASE("Alliance : la proposition reçue nomme le compte et la guilde", "[net][guild]") {
  GuildBook guild;
  REQUIRE(ApplyGuildFrame(Offer(kInviter, "Les Porings"), guild, kMe) == FrameOutcome::kApplied);
  REQUIRE(guild.alliance_offer().has_value());
  CHECK(guild.alliance_offer()->inviter_account_id == kInviter);
  CHECK(guild.alliance_offer()->guild_name == "Les Porings");
}

TEST_CASE("Alliance : la proposition survit au départ, une nouvelle la remplace", "[net][guild]") {
  GuildBook guild;
  REQUIRE(ApplyGuildFrame(Offer(kInviter, "Les Porings"), guild, kMe) == FrameOutcome::kApplied);
  guild.Leave();
  REQUIRE(guild.alliance_offer().has_value());
  REQUIRE(ApplyGuildFrame(Offer(kLaterInviter, "Les Drops"), guild, kMe) == FrameOutcome::kApplied);
  CHECK(guild.alliance_offer()->inviter_account_id == kLaterInviter);
  CHECK(guild.alliance_offer()->guild_name == "Les Drops");
  guild.ForgetAllianceOffer();
  CHECK_FALSE(guild.alliance_offer().has_value());
}

TEST_CASE("Alliance : la réponse nomme le compte qui propose, accepter puis refuser",
          "[net][guild]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  REQUIRE(ro::net::AnswerGuildAlliance(link, kInviter, true).has_value());
  REQUIRE(ro::net::AnswerGuildAlliance(link, kInviter, false).has_value());
  constexpr std::size_t kSize = ro::proto::GuildAllianceAnswer::kWireSize;
  const std::vector<std::uint8_t> sent = server.Receive(kSize * 2);
  REQUIRE(sent.size() == kSize * 2);
  CHECK(ReadU16(sent, 0) == 0x0172);
  CHECK(ReadU32(sent, 2) == kInviter);
  CHECK(ReadU32(sent, 6) == 1);
  CHECK(ReadU16(sent, kSize) == 0x0172);
  CHECK(ReadU32(sent, kSize + 2) == kInviter);
  CHECK(ReadU32(sent, kSize + 6) == 0);
}

TEST_CASE("Alliance : chaque verdict s'écrit, le succès en blanc, le refus en rouge",
          "[net][guild]") {
  ro::sim::World world;
  for (const std::uint8_t flag : {kAllianceAlready, kAlliancePending, kAllianceNotConcluded,
                                  kAllianceOwnFull, kAllianceMasterOnly}) {
    CHECK(AllianceVerdict(flag, world) == FrameOutcome::kApplied);
    REQUIRE_FALSE(world.chat_lines().empty());
    CHECK(world.chat_lines().back().color == kRed);
  }
  CHECK(AllianceVerdict(kAllianceConcluded, world) == FrameOutcome::kApplied);
  const ro::sim::ChatLine& concluded = world.chat_lines().back();
  CHECK(concluded.text == "L'alliance est conclue.");
  CHECK(concluded.color == kWhite);
  CHECK(concluded.kind == ro::sim::ChatKind::kServer);
  CHECK(concluded.said_ms == kNowMs);
  CHECK(world.chat_lines().size() == 6);
}

TEST_CASE("Hostilité : chaque verdict s'écrit, et un numéro inconnu se tait", "[net][guild]") {
  ro::sim::World world;
  CHECK(HostilityVerdict(kHostilityDeclared, world) == FrameOutcome::kApplied);
  CHECK(world.chat_lines().back().text == "La guilde est désormais ennemie.");
  CHECK(world.chat_lines().back().color == kWhite);
  for (const std::uint8_t flag : {kHostilityOwnFull, kHostilityAlready, kHostilityDisabled}) {
    CHECK(HostilityVerdict(flag, world) == FrameOutcome::kApplied);
    CHECK(world.chat_lines().back().color == kRed);
  }
  CHECK(world.chat_lines().size() == 4);
  CHECK(HostilityVerdict(kUnknownVerdict, world) == FrameOutcome::kIgnored);
  CHECK(AllianceVerdict(kUnknownVerdict, world) == FrameOutcome::kIgnored);
  CHECK(world.chat_lines().size() == 4);
}

TEST_CASE("Alliance : une trame étrangère n'est pas aux verdicts", "[net][guild]") {
  ro::sim::World world;
  const ro::net::Frame other{.opcode = 0x0001, .bytes = {0x01, 0x00}};
  CHECK(ApplyGuildRelationAnswerFrame(other, world, kNowMs) == FrameOutcome::kUnhandled);
  // ⚠ LA PROPOSITION REÇUE EST AU CARNET, pas aux verdicts.
  CHECK(ApplyGuildRelationAnswerFrame(Offer(kInviter, "Les Porings"), world, kNowMs) ==
        FrameOutcome::kUnhandled);
}