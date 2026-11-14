#include "ro/sim/talk_mute.h"

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/battleground.h"
#include "ro/net/world_notice.h"
#include "ro/net/world_update.h"
#include "ro/proto/battleground_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

// `/notalkmsg` ET `/notalkmsg2` : la ligne se tait, la bulle reste ; la seconde
// tait aussi la guilde et le clan ; ni sa propre parole ni le groupe ne bougent.

using ro::net::ApplyBattlefieldFrame;
using ro::net::ApplySpokenFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::TalkMute;
using ro::sim::World;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kSomeone = 2001;
constexpr std::uint32_t kNowMs = 1000;

// L'écho de sa propre parole : le serveur l'écrit à la main, hors structure.
[[nodiscard]] Frame PlayerChat(const std::string& text) {
  constexpr std::uint16_t kPlayerChatOpcode = 0x008e;
  std::vector<std::uint8_t> bytes = {0x8e, 0x00, 0, 0};
  bytes.insert(bytes.end(), text.begin(), text.end());
  bytes.push_back(0);
  bytes[2] = static_cast<std::uint8_t>(bytes.size());
  return Frame{.opcode = kPlayerChatOpcode, .bytes = bytes};
}

// Un monde où l'orateur est en vue : sa bulle peut se poser.
[[nodiscard]] World WorldWithSpeaker() {
  World world;
  world.set_player_name("Moi");
  static_cast<void>(ro::net::ApplyFrame(
      FrameOf(Encode(ro::test_support::IdleUnit(kSomeone, ro::sim::CellPosition{10, 10, 0}))),
      world, 0));
  return world;
}

// Les cinq paroles : publique, écho, groupe, guilde, clan. Rend les lignes.
[[nodiscard]] std::vector<std::string> LinesUnder(World& world, TalkMute mute) {
  ro::proto::ZcNotifyChat said;
  said.GID = kSomeone;
  said.Message = "Bob : salut";
  ro::proto::ZcNotifyChatParty party;
  party.AID = kSomeone;
  party.chatMsg = "Bob : au groupe";
  ro::proto::ZcGuildChat guild;
  guild.message = "Bob : en guilde";
  ro::proto::ZcNotifyClanChat clan;
  clan.Message = "Bob : au clan";
  for (const Frame& frame :
       {FrameOf(Encode(said)), PlayerChat("Moi : bonjour"), FrameOf(Encode(party)),
        FrameOf(Encode(guild)), FrameOf(Encode(clan))}) {
    REQUIRE(ApplySpokenFrame(frame, world, nullptr, kNowMs, mute) == FrameOutcome::kApplied);
  }
  std::vector<std::string> texts;
  for (const ro::sim::ChatLine& line : world.chat_lines()) {
    texts.push_back(line.text);
  }
  return texts;
}

}  // namespace

TEST_CASE("Parole tue : sans bascule, tout entre dans la conversation", "[net][talkmute]") {
  World world = WorldWithSpeaker();
  const std::vector<std::string> all{"Bob : salut", "Moi : bonjour", "Bob : au groupe",
                                     "Bob : en guilde", "Bob : au clan"};
  CHECK(LinesUnder(world, TalkMute{}) == all);
}

TEST_CASE("Parole tue : /notalkmsg tait la ligne publique, pas sa bulle", "[net][talkmute]") {
  World world = WorldWithSpeaker();
  const std::vector<std::string> kept{"Moi : bonjour", "Bob : au groupe", "Bob : en guilde",
                                      "Bob : au clan"};
  CHECK(LinesUnder(world, TalkMute{.talk_hidden = true}) == kept);
}

TEST_CASE("Parole tue : la bulle d'une parole publique tue reste posée", "[net][talkmute]") {
  World world = WorldWithSpeaker();
  ro::proto::ZcNotifyChat said;
  said.GID = kSomeone;
  said.Message = "Bob : salut";
  REQUIRE(ApplySpokenFrame(FrameOf(Encode(said)), world, nullptr, kNowMs,
                           TalkMute{.talk_hidden = true}) == FrameOutcome::kApplied);
  CHECK(world.chat_lines().empty());
  const ro::sim::Actor* bob = world.Find(kSomeone);
  REQUIRE(bob != nullptr);
  CHECK(bob->chat_text == "Bob : salut");
}

TEST_CASE("Parole tue : /notalkmsg2 tait aussi la guilde et le clan", "[net][talkmute]") {
  World world = WorldWithSpeaker();
  const std::vector<std::string> kept{"Moi : bonjour", "Bob : au groupe"};
  CHECK(LinesUnder(world, TalkMute{.talk_and_guild_hidden = true}) == kept);
}

TEST_CASE("Parole tue : le champ de bataille garde sa bulle", "[net][talkmute]") {
  World world = WorldWithSpeaker();
  ro::proto::BattlefieldChatNotice said;
  said.speaker_id = kSomeone;
  said.message = "Bob : en avant";
  CHECK(ApplyBattlefieldFrame(FrameOf(ro::proto::Encode(said)), world, nullptr, kNowMs,
                              TalkMute{.talk_hidden = true}) == FrameOutcome::kApplied);
  CHECK(world.chat_lines().empty());
  const ro::sim::Actor* bob = world.Find(kSomeone);
  REQUIRE(bob != nullptr);
  CHECK(bob->chat_text == "Bob : en avant");
}
