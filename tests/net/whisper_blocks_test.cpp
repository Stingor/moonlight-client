// BLOQUER LES CHUCHOTEMENTS : les trois réponses du serveur, ce qu'elles
// changent à la liste et ce qu'elles écrivent, et les trois demandes.

#include "ro/net/whisper_blocks.h"

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/i18n.h"
#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/social_packets.h"
#include "ro/sim/whisper_blocks.h"
#include "ro/sim/world.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/net_frames.h"

using ro::core::Tr;
using ro::net::ApplyWhisperBlockFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::WhisperBlockList;
using ro::sim::WhisperBlockWay;
using ro::test::Connect;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kNowMs = 1000;
constexpr std::uint8_t kBlockType = 0;
constexpr std::uint8_t kAllowType = 1;
constexpr std::uint8_t kDone = 0;
constexpr std::uint8_t kFailed = 1;
constexpr std::uint8_t kListFull = 2;
constexpr std::uint32_t kAnswerColor = 0x00FFFF;
constexpr std::uint32_t kListColor = 0xFFFF00;

Frame PcAnswer(std::uint8_t type, std::uint8_t result) {
  ro::proto::ZcSettingWhisperPc packet;
  packet.type = type;
  packet.result = result;
  return FrameOf(ro::proto::Encode(packet));
}

Frame EveryoneAnswer(std::uint8_t type, std::uint8_t result) {
  ro::proto::ZcSettingWhisperState packet;
  packet.type = type;
  packet.result = result;
  return FrameOf(ro::proto::Encode(packet));
}

Frame Listed(const std::vector<std::string>& names) {
  ro::proto::ZcWhisperList packet;
  for (const std::string& name : names) {
    ro::proto::ZcWhisperListSub entry;
    entry.name = name;
    packet.names.push_back(entry);
  }
  return FrameOf(ro::proto::Encode(packet));
}

}  // namespace

TEST_CASE("Blocage : un blocage réussi change la liste sans rien écrire", "[net][whisper_block]") {
  ro::sim::World world;
  WhisperBlockList blocks;
  blocks.Expect("Aurélie", WhisperBlockWay::kBlock);
  CHECK(ApplyWhisperBlockFrame(PcAnswer(kBlockType, kDone), blocks, world, kNowMs) ==
        FrameOutcome::kApplied);
  CHECK(blocks.Blocks("Aurélie"));
  CHECK(world.chat_lines().empty());
}

TEST_CASE("Blocage : un refus se dit, nom en tête, et ne change rien", "[net][whisper_block]") {
  ro::sim::World world;
  WhisperBlockList blocks;
  blocks.Expect("Bob", WhisperBlockWay::kBlock);
  blocks.Expect("Carl", WhisperBlockWay::kBlock);
  CHECK(ApplyWhisperBlockFrame(PcAnswer(kBlockType, kFailed), blocks, world, kNowMs) ==
        FrameOutcome::kApplied);
  CHECK(ApplyWhisperBlockFrame(PcAnswer(kBlockType, kListFull), blocks, world, kNowMs) ==
        FrameOutcome::kApplied);
  CHECK(blocks.names().empty());
  REQUIRE(world.chat_lines().size() == 2);
  CHECK(world.chat_lines()[0].text.starts_with("Bob"));
  CHECK(world.chat_lines()[0].text.find(Tr("La liste de blocage est pleine.")) ==
        std::string::npos);
  CHECK(world.chat_lines()[1].text.starts_with("Carl"));
  CHECK(world.chat_lines()[1].color == kAnswerColor);
}

TEST_CASE("Blocage : autoriser fait sortir le nom et se dit", "[net][whisper_block]") {
  ro::sim::World world;
  WhisperBlockList blocks;
  blocks.Replace({"Bob"});
  blocks.Expect("Bob", WhisperBlockWay::kAllow);
  CHECK(ApplyWhisperBlockFrame(PcAnswer(kAllowType, kDone), blocks, world, kNowMs) ==
        FrameOutcome::kApplied);
  CHECK_FALSE(blocks.Blocks("Bob"));
  REQUIRE(world.chat_lines().size() == 1);
  CHECK(world.chat_lines().front().text == "Bob : chuchotements acceptés.");
}

TEST_CASE("Blocage : une réponse sans demande se tait", "[net][whisper_block]") {
  ro::sim::World world;
  WhisperBlockList blocks;
  CHECK(ApplyWhisperBlockFrame(PcAnswer(kAllowType, kFailed), blocks, world, kNowMs) ==
        FrameOutcome::kIgnored);
  CHECK(world.chat_lines().empty());
}

TEST_CASE("Blocage : tout bloquer allume l'interrupteur et se dit", "[net][whisper_block]") {
  ro::sim::World world;
  WhisperBlockList blocks;
  CHECK(ApplyWhisperBlockFrame(EveryoneAnswer(kBlockType, kDone), blocks, world, kNowMs) ==
        FrameOutcome::kApplied);
  CHECK(blocks.blocks_everyone());
  CHECK(ApplyWhisperBlockFrame(EveryoneAnswer(kAllowType, kDone), blocks, world, kNowMs) ==
        FrameOutcome::kApplied);
  CHECK_FALSE(blocks.blocks_everyone());
  REQUIRE(world.chat_lines().size() == 2);
  CHECK(world.chat_lines()[0].text ==
        Tr("Les chuchotements de tous les personnages sont bloqués."));
  CHECK(world.chat_lines()[1].text ==
        Tr("Les chuchotements de tous les personnages sont autorisés."));
}

TEST_CASE("Blocage : la liste redite remplace la nôtre et s'écrit", "[net][whisper_block]") {
  ro::sim::World world;
  WhisperBlockList blocks;
  blocks.Replace({"Ancien"});
  // « Aurélie » en 1252.
  CHECK(ApplyWhisperBlockFrame(Listed({"Aur\xE9lie", "Bob"}), blocks, world, kNowMs) ==
        FrameOutcome::kApplied);
  CHECK(blocks.Blocks("Aurélie"));
  CHECK(blocks.Blocks("Bob"));
  CHECK_FALSE(blocks.Blocks("Ancien"));
  REQUIRE(world.chat_lines().size() == 3);
  CHECK(world.chat_lines()[0].text == Tr("-Liste de blocage-"));
  CHECK(world.chat_lines()[1].text == "Aurélie");
  CHECK(world.chat_lines()[2].color == kListColor);

  ro::sim::World empty_world;
  CHECK(ApplyWhisperBlockFrame(Listed({}), blocks, empty_world, kNowMs) == FrameOutcome::kApplied);
  CHECK(blocks.names().empty());
  REQUIRE(empty_world.chat_lines().size() == 1);
  CHECK(empty_world.chat_lines().front().text == Tr("La liste de blocage est vide."));
}

TEST_CASE("Blocage : une autre trame ne le regarde pas", "[net][whisper_block]") {
  ro::sim::World world;
  WhisperBlockList blocks;
  ro::proto::ZcNotifyTime other;
  CHECK(ApplyWhisperBlockFrame(FrameOf(ro::proto::Encode(other)), blocks, world, kNowMs) ==
        FrameOutcome::kUnhandled);
}

TEST_CASE("Blocage : les demandes partent aux octets près", "[net][whisper_block]") {
  ro::test::LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  WhisperBlockList blocks;

  REQUIRE(
      ro::net::RequestWhisperBlock(link, blocks, "Aurélie", WhisperBlockWay::kAllow).has_value());
  std::vector<std::uint8_t> sent = server.Receive(27);
  REQUIRE(sent.size() == 27);
  CHECK(sent[0] == 0xCF);
  CHECK(sent[1] == 0x00);
  CHECK(sent[5] == 0xE9);
  CHECK(sent[25] == 0);
  CHECK(sent[26] == kAllowType);
  // LA DEMANDE EST NOTÉE, sous son nom en UTF-8.
  CHECK(blocks.Settle(WhisperBlockWay::kAllow, kDone).name == "Aurélie");

  REQUIRE(ro::net::RequestWhisperBlockEveryone(link, WhisperBlockWay::kBlock).has_value());
  sent = server.Receive(3);
  REQUIRE(sent.size() == 3);
  CHECK(sent[0] == 0xD0);
  CHECK(sent[2] == kBlockType);

  REQUIRE(ro::net::RequestWhisperBlockList(link).has_value());
  sent = server.Receive(2);
  REQUIRE(sent.size() == 2);
  CHECK(sent[0] == 0xD3);
  CHECK(sent[1] == 0x00);
}

TEST_CASE("Blocage : un nom qui ne partirait pas tel quel est refusé avant l'envoi",
          "[net][whisper_block]") {
  ro::test::LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  WhisperBlockList blocks;
  // Vingt-quatre lettres : le serveur en garde vingt-trois.
  const std::string too_long(ro::proto::WhisperBlockRequest::kNameSize, 'a');
  CHECK_FALSE(ro::net::RequestWhisperBlock(link, blocks, too_long, WhisperBlockWay::kBlock));
  CHECK_FALSE(ro::net::RequestWhisperBlock(link, blocks, "", WhisperBlockWay::kBlock));
  // Un caractère que le 1252 n'écrit pas.
  CHECK_FALSE(ro::net::RequestWhisperBlock(link, blocks, "龍", WhisperBlockWay::kBlock));
  CHECK(blocks.pending() == 0);
}
