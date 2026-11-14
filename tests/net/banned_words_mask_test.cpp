#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/battleground.h"
#include "ro/net/chat_room.h"
#include "ro/net/guild.h"
#include "ro/net/party.h"
#include "ro/net/world_notice.h"
#include "ro/net/world_update.h"
#include "ro/proto/battleground_packets.h"
#include "ro/proto/chat_room_packets.h"
#include "ro/proto/guild_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/social_packets.h"
#include "ro/sim/banned_words.h"
#include "ro/sim/chat_room.h"
#include "ro/sim/guild.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/party_roster.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

// LES MOTS INTERDITS, MASQUÉS À LA LECTURE DES TRAMES : chaque canal où le
// client d'origine les masque, et la liste vide qui ne change rien.
//
// ⚠ LA LISTE EST SYNTHÉTIQUE : le mot ne sert qu'à l'essai.

using ro::net::ApplyBattlefieldFrame;
using ro::net::ApplyGuildFrame;
using ro::net::ApplyPartyFrame;
using ro::net::ApplySpokenFrame;
using ro::net::ClaimChatRoomLine;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::BannedWords;
using ro::sim::World;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kSomeone = 2001;
constexpr std::uint32_t kMe = 150001;
constexpr std::uint32_t kNowMs = 1000;
constexpr std::uint32_t kRoomId = 7;

[[nodiscard]] BannedWords Banned() { return BannedWords(std::vector<std::string>{"brocoli"}); }

// L'écho de sa propre parole : le serveur l'écrit à la main, hors structure.
[[nodiscard]] Frame PlayerChat(const std::string& text) {
  constexpr std::uint16_t kPlayerChatOpcode = 0x008e;
  std::vector<std::uint8_t> bytes = {0x8e, 0x00, 0, 0};
  bytes.insert(bytes.end(), text.begin(), text.end());
  bytes.push_back(0);
  bytes[2] = static_cast<std::uint8_t>(bytes.size());
  return Frame{.opcode = kPlayerChatOpcode, .bytes = bytes};
}

// Un monde où l'hôte tient l'enseigne d'un salon : l'entrée la lit.
[[nodiscard]] World WorldWithRoom() {
  World world;
  world.set_player_name("Moi");
  static_cast<void>(ro::net::ApplyFrame(
      FrameOf(Encode(ro::test_support::IdleUnit(kSomeone, ro::sim::CellPosition{10, 10, 0}))),
      world, 0));
  ro::proto::ZcRoomNewentry board;
  board.owner = kSomeone;
  board.id = kRoomId;
  board.limit = 5;
  board.users = 1;
  board.type = ro::sim::kRoomPrivate;
  board.title = "Salon";
  static_cast<void>(ro::net::ApplyFrame(FrameOf(Encode(board)), world, 0));
  return world;
}

// Les cinq paroles, dans l'ordre : publique, écho, groupe, guilde, clan.
[[nodiscard]] std::vector<Frame> Spoken() {
  ro::proto::ZcNotifyChat said;
  said.GID = kSomeone;
  said.Message = "Bob : du BROCOLI";
  ro::proto::ZcNotifyChatParty party;
  party.AID = kSomeone;
  party.chatMsg = "Bob : brocoli au groupe";
  ro::proto::ZcGuildChat guild;
  guild.message = "Bob : brocoli en guilde";
  ro::proto::ZcNotifyClanChat clan;
  clan.Message = "Bob : brocoli au clan";
  return {FrameOf(Encode(said)), PlayerChat("Moi : un brocoli"), FrameOf(Encode(party)),
          FrameOf(Encode(guild)), FrameOf(Encode(clan))};
}

[[nodiscard]] std::vector<std::string> SpokenTexts(const BannedWords* banned) {
  World world;
  for (const Frame& frame : Spoken()) {
    REQUIRE(ApplySpokenFrame(frame, world, banned, kNowMs) == FrameOutcome::kApplied);
  }
  std::vector<std::string> texts;
  for (const ro::sim::ChatLine& line : world.chat_lines()) {
    texts.push_back(line.text);
  }
  return texts;
}

}  // namespace

TEST_CASE("Mots interdits : la parole des joueurs se masque, canal par canal", "[net][banned]") {
  const BannedWords banned = Banned();
  const std::vector<std::string> expected{"Bob : du *******", "Moi : un *******",
                                          "Bob : ******* au groupe", "Bob : ******* en guilde",
                                          "Bob : ******* au clan"};
  CHECK(SpokenTexts(&banned) == expected);
}

TEST_CASE("Mots interdits : sans liste, ou avec une liste vide, la parole passe telle quelle",
          "[net][banned]") {
  const std::vector<std::string> plain{"Bob : du BROCOLI", "Moi : un brocoli",
                                       "Bob : brocoli au groupe", "Bob : brocoli en guilde",
                                       "Bob : brocoli au clan"};
  CHECK(SpokenTexts(nullptr) == plain);
  const BannedWords none;
  CHECK(SpokenTexts(&none) == plain);
}

TEST_CASE("Mots interdits : le chuchotement et l'annonce ne sont pas de la parole",
          "[net][banned]") {
  World world;
  const BannedWords banned = Banned();
  ro::proto::ZcBroadcast announcement;
  announcement.message = "brocoli";
  CHECK(ApplySpokenFrame(FrameOf(Encode(announcement)), world, &banned, kNowMs) ==
        FrameOutcome::kUnhandled);
}

TEST_CASE("Mots interdits : la parole d'un salon se masque", "[net][banned]") {
  World world = WorldWithRoom();
  ro::sim::ChatRoom room;
  ro::proto::ZcEnterRoom entered;
  entered.chatId = kRoomId;
  entered.members.push_back(
      ro::proto::ZcEnterRoomSub{.flag = ro::sim::kChatRoomOwnerRole, .name = "Bob"});
  entered.members.push_back(ro::proto::ZcEnterRoomSub{.flag = 1U, .name = "Moi"});
  REQUIRE(ro::net::ApplyChatRoomFrame(FrameOf(Encode(entered)), room, world) ==
          FrameOutcome::kApplied);
  const BannedWords banned = Banned();
  ro::proto::ZcNotifyChat said;
  said.GID = kSomeone;
  said.Message = "Bob : brocoli";
  CHECK(ClaimChatRoomLine(FrameOf(Encode(said)), room, world, &banned, kNowMs) ==
        FrameOutcome::kApplied);
  CHECK(ClaimChatRoomLine(FrameOf(Encode(said)), room, world, nullptr, kNowMs) ==
        FrameOutcome::kApplied);
  REQUIRE(room.lines().size() == 2);
  CHECK(room.lines()[0].text == "Bob : *******");
  CHECK(room.lines()[1].text == "Bob : brocoli");
}

TEST_CASE("Mots interdits : la parole du champ de bataille se masque", "[net][banned]") {
  World world;
  const BannedWords banned = Banned();
  ro::proto::BattlefieldChatNotice said;
  said.message = "Bob : brocoli";
  CHECK(ApplyBattlefieldFrame(FrameOf(ro::proto::Encode(said)), world, &banned, kNowMs) ==
        FrameOutcome::kApplied);
  CHECK(ApplyBattlefieldFrame(FrameOf(ro::proto::Encode(said)), world, nullptr, kNowMs) ==
        FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 2);
  CHECK(world.chat_lines()[0].text == "Bob : *******");
  CHECK(world.chat_lines()[1].text == "Bob : brocoli");
}

TEST_CASE("Mots interdits : la guilde masque ses noms et son avis", "[net][banned]") {
  const BannedWords banned = Banned();
  ro::sim::GuildBook guild;

  ro::proto::ZcGuildInfo info;
  info.GDID = 42;
  info.guildname = "Les Brocolis";
  info.masterName = "Brocoli";
  REQUIRE(ApplyGuildFrame(FrameOf(ro::proto::Encode(info)), guild, kMe, &banned) ==
          FrameOutcome::kApplied);
  REQUIRE(guild.summary().has_value());
  CHECK(guild.summary()->name == "Les *******s");
  CHECK(guild.belonging()->name == "Les *******s");
  // ⚠ LE NOM DU MAÎTRE NE SE MASQUE PAS.
  CHECK(guild.summary()->master_name == "Brocoli");

  ro::proto::ZcGuildNotice notice;
  notice.subject = "Brocoli";
  notice.notice = "Pas de brocoli ce soir";
  REQUIRE(ApplyGuildFrame(FrameOf(ro::proto::Encode(notice)), guild, kMe, &banned) ==
          FrameOutcome::kApplied);
  CHECK(guild.notice_subject() == "*******");
  CHECK(guild.notice_body() == "Pas de ******* ce soir");

  ro::proto::ZcMyguildBasicInfo relations;
  ro::proto::RelatedGuildInfo ally;
  ally.GDID = 77;
  ally.guildname = "Brocoli Club";
  relations.rgInfo = {ally};
  REQUIRE(ApplyGuildFrame(FrameOf(ro::proto::Encode(relations)), guild, kMe, &banned) ==
          FrameOutcome::kApplied);
  REQUIRE(guild.relations().size() == 1);
  CHECK(guild.relations()[0].name == "******* Club");

  ro::proto::ZcReqJoinGuild invited;
  invited.guild_id = 78;
  invited.guild_name = "Brocoli";
  REQUIRE(ApplyGuildFrame(FrameOf(ro::proto::Encode(invited)), guild, kMe, &banned) ==
          FrameOutcome::kApplied);
  CHECK(guild.invitation()->guild_name == "*******");

  ro::proto::ZcReqAllyGuild offered;
  offered.inviterId = 79;
  offered.inviterGuildName = "Brocoli";
  REQUIRE(ApplyGuildFrame(FrameOf(ro::proto::Encode(offered)), guild, kMe, &banned) ==
          FrameOutcome::kApplied);
  CHECK(guild.alliance_offer()->guild_name == "*******");
}

TEST_CASE("Mots interdits : sans liste, la guilde garde ses noms", "[net][banned]") {
  ro::sim::GuildBook guild;
  ro::proto::ZcGuildNotice notice;
  notice.subject = "Brocoli";
  notice.notice = "brocoli";
  REQUIRE(ApplyGuildFrame(FrameOf(ro::proto::Encode(notice)), guild, kMe) ==
          FrameOutcome::kApplied);
  CHECK(guild.notice_subject() == "Brocoli");
  const BannedWords none;
  REQUIRE(ApplyGuildFrame(FrameOf(ro::proto::Encode(notice)), guild, kMe, &none) ==
          FrameOutcome::kApplied);
  CHECK(guild.notice_body() == "brocoli");
}

TEST_CASE("Mots interdits : le nom du groupe se masque, celui des membres non", "[net][banned]") {
  const BannedWords banned = Banned();
  ro::sim::PartyRoster party;

  ro::proto::ZcGroupList list;
  list.partyName = "Brocolis";
  ro::proto::ZcGroupListSub mate;
  mate.AID = kSomeone;
  mate.playerName = "Brocoli";
  list.members = {mate};
  REQUIRE(ApplyPartyFrame(FrameOf(ro::proto::Encode(list)), party, kMe, kNowMs, &banned) ==
          FrameOutcome::kApplied);
  CHECK(party.name() == "*******s");
  CHECK(party.members()[0].name == "Brocoli");

  ro::proto::ZcAddMemberToGroup joined;
  joined.AID = kSomeone + 1;
  joined.partyName = "Le brocoli";
  joined.playerName = "Alice";
  REQUIRE(ApplyPartyFrame(FrameOf(ro::proto::Encode(joined)), party, kMe, kNowMs, &banned) ==
          FrameOutcome::kApplied);
  CHECK(party.name() == "Le *******");

  ro::proto::ZcPartyJoinReq invited;
  invited.GRID = 5;
  invited.groupName = "Brocoli";
  REQUIRE(ApplyPartyFrame(FrameOf(ro::proto::Encode(invited)), party, kMe, kNowMs, &banned) ==
          FrameOutcome::kApplied);
  CHECK(party.invitation()->party_name == "*******");

  // SANS LISTE, rien ne change.
  REQUIRE(ApplyPartyFrame(FrameOf(ro::proto::Encode(invited)), party, kMe, kNowMs) ==
          FrameOutcome::kApplied);
  CHECK(party.invitation()->party_name == "Brocoli");
}
