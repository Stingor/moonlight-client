#include "ro/net/chat_room.h"

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/proto/chat_author_packets.h"
#include "ro/proto/chat_room_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/chat_room.h"
#include "ro/sim/world.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/net_frames.h"

// LES SALONS DE DISCUSSION : la création, l'entrée chez un autre, la vie du
// salon, et la parole qui s'y range.

using ro::net::ApplyChatRoomFrame;
using ro::net::ApplyFrame;
using ro::net::ChatRoomSettingsFault;
using ro::net::CheckChatRoomSettings;
using ro::net::ClaimChatRoomLine;
using ro::net::FrameOutcome;
using ro::sim::CellPosition;
using ro::sim::ChatRoom;
using ro::sim::ChatRoomNewsKind;
using ro::sim::ChatRoomSettings;
using ro::sim::World;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::FrameOf;
using ro::test_support::IdleUnit;

namespace {

constexpr std::uint32_t kHostId = 150'001;
constexpr std::uint32_t kRoomId = 7;

[[nodiscard]] World WorldWithRoom() {
  World world;
  world.set_player_name("Moi");
  world.set_player_id(2'000'001);
  static_cast<void>(
      ApplyFrame(FrameOf(Encode(IdleUnit(kHostId, CellPosition{10, 10, 0}))), world, 0));
  ro::proto::ZcRoomNewentry board;
  board.owner = kHostId;
  board.id = kRoomId;
  board.limit = 5;
  board.users = 1;
  board.type = ro::sim::kRoomPrivate;
  board.title = "Chasse MVP";
  static_cast<void>(ApplyFrame(FrameOf(Encode(board)), world, 0));
  return world;
}

[[nodiscard]] std::vector<std::uint8_t> EnterRoom(std::vector<std::string> names) {
  ro::proto::ZcEnterRoom entered;
  entered.chatId = kRoomId;
  for (std::size_t rank = 0; rank < names.size(); ++rank) {
    entered.members.push_back(ro::proto::ZcEnterRoomSub{
        .flag = rank == 0 ? ro::sim::kChatRoomOwnerRole : 1U, .name = std::move(names[rank])});
  }
  return Encode(entered);
}

// L'écho de sa propre parole : le paquet n'a pas d'encodeur, l'opcode ne
// figurant pas dans sa structure.
[[nodiscard]] std::vector<std::uint8_t> PlayerChat(const std::string& text) {
  std::vector<std::uint8_t> bytes = {0x8e, 0x00, 0, 0};
  bytes.insert(bytes.end(), text.begin(), text.end());
  bytes.push_back(0);
  bytes[2] = static_cast<std::uint8_t>(bytes.size());
  return bytes;
}

}  // namespace

TEST_CASE("Salon : les réglages se refusent dans l'ordre du client d'origine", "[net][salon]") {
  ChatRoomSettings settings{.title = "", .password = "", .is_public = true, .limit = 20};
  CHECK(CheckChatRoomSettings(settings) == ChatRoomSettingsFault::kNoTitle);
  settings.title = "Chasse à l'ours";
  CHECK(CheckChatRoomSettings(settings) == ChatRoomSettingsFault::kTitleNotAscii);
  settings.title = std::string(ro::sim::kChatRoomTitleMostBytes + 1, 'a');
  CHECK(CheckChatRoomSettings(settings) == ChatRoomSettingsFault::kTitleTooLong);
  settings.title = std::string(ro::sim::kChatRoomTitleMostBytes, 'a');
  CHECK(CheckChatRoomSettings(settings) == ChatRoomSettingsFault::kNone);
  // Le mot de passe ne compte que pour un salon privé.
  settings.is_public = false;
  CHECK(CheckChatRoomSettings(settings) == ChatRoomSettingsFault::kNoPassword);
  settings.password = "abc";
  CHECK(CheckChatRoomSettings(settings) == ChatRoomSettingsFault::kPasswordTooShort);
  settings.password = "abcd";
  CHECK(CheckChatRoomSettings(settings) == ChatRoomSettingsFault::kNone);
  settings.password = "abcdefghi";
  CHECK(CheckChatRoomSettings(settings) == ChatRoomSettingsFault::kPasswordTooLong);
  // Un accent compte UN octet dans l'encodage du serveur, pas deux.
  settings.password = "éééééééé";
  CHECK(CheckChatRoomSettings(settings) == ChatRoomSettingsFault::kNone);
  settings.password = "abcd😀";
  CHECK(CheckChatRoomSettings(settings) == ChatRoomSettingsFault::kPasswordNotEncodable);
}

TEST_CASE("Salon : créé, il s'ouvre sur la demande, joueur seul et chef", "[net][salon]") {
  World world = WorldWithRoom();
  ChatRoom room;
  room.NoteCreateSent(
      ChatRoomSettings{.title = "Vente", .password = "1234", .is_public = false, .limit = 5}, 100);
  REQUIRE(room.creating());

  ro::proto::ZcAckCreateChatroom ack;
  ack.flag = ro::sim::kChatRoomCreated;
  CHECK(ApplyChatRoomFrame(FrameOf(Encode(ack)), room, world) == FrameOutcome::kApplied);
  CHECK_FALSE(room.creating());
  REQUIRE(room.in_room());
  CHECK(room.inside()->title == "Vente");
  CHECK_FALSE(room.inside()->is_public());
  CHECK(room.inside()->users == 1);
  CHECK(room.inside()->limit == 5);
  CHECK(room.inside()->password == "1234");
  CHECK(room.inside()->owned);
  REQUIRE(room.inside()->members.size() == 1);
  CHECK(room.inside()->members[0].name == "Moi");
  const auto news = room.TakeNews();
  REQUIRE(news.size() == 1);
  CHECK(news[0].kind == ChatRoomNewsKind::kCreated);
}

TEST_CASE("Salon : un refus de création garde la demande morte, sans salon", "[net][salon]") {
  World world = WorldWithRoom();
  ChatRoom room;
  room.NoteCreateSent(ChatRoomSettings{.title = "Vente"}, 0);
  ro::proto::ZcAckCreateChatroom ack;
  ack.flag = ro::sim::kChatRoomSameTitle;
  CHECK(ApplyChatRoomFrame(FrameOf(Encode(ack)), room, world) == FrameOutcome::kApplied);
  CHECK_FALSE(room.in_room());
  CHECK_FALSE(room.creating());
  const auto news = room.TakeNews();
  REQUIRE(news.size() == 1);
  CHECK(news[0].kind == ChatRoomNewsKind::kSameTitle);
}

TEST_CASE("Salon : une création sans réponse se relâche", "[net][salon]") {
  ChatRoom room;
  room.NoteCreateSent(ChatRoomSettings{.title = "Vente"}, 1000);
  room.ExpireSilentCreate(1000 + ro::sim::kChatRoomCreateSilenceMs);
  CHECK(room.creating());
  room.ExpireSilentCreate(1001 + ro::sim::kChatRoomCreateSilenceMs);
  CHECK_FALSE(room.creating());
  REQUIRE(room.TakeNews().size() == 1);
}

TEST_CASE("Salon : on entre grâce à l'enseigne, qui disparaît", "[net][salon]") {
  World world = WorldWithRoom();
  ChatRoom room;
  CHECK(ApplyChatRoomFrame(FrameOf(EnterRoom({"Hote", "Moi"})), room, world) ==
        FrameOutcome::kApplied);
  REQUIRE(room.in_room());
  CHECK(room.inside()->room_id == kRoomId);
  CHECK(room.inside()->title == "Chasse MVP");
  CHECK(room.inside()->limit == 5);
  CHECK(room.inside()->users == 2);
  CHECK_FALSE(room.inside()->is_public());
  CHECK_FALSE(room.inside()->owned);
  CHECK(room.inside()->members[0].owner);
  CHECK_FALSE(world.Find(kHostId)->signboard.has_value());
}

TEST_CASE("Salon : sans enseigne connue, l'entrée n'ouvre rien et la sortie est due",
          "[net][salon]") {
  World world;
  ChatRoom room;
  CHECK(ApplyChatRoomFrame(FrameOf(EnterRoom({"Hote", "Moi"})), room, world) ==
        FrameOutcome::kApplied);
  CHECK_FALSE(room.in_room());
  CHECK(room.TakeLeaveOwed());
  CHECK_FALSE(room.TakeLeaveOwed());
}

TEST_CASE("Salon : arrivées, départs, expulsion et sortie du joueur", "[net][salon]") {
  World world = WorldWithRoom();
  ChatRoom room;
  REQUIRE(ApplyChatRoomFrame(FrameOf(EnterRoom({"Hote", "Moi"})), room, world) ==
          FrameOutcome::kApplied);

  ro::proto::ZcMemberNewentry joined;
  joined.count = 3;
  joined.name = "Tiers";
  CHECK(ApplyChatRoomFrame(FrameOf(Encode(joined)), room, world) == FrameOutcome::kApplied);
  CHECK(room.inside()->users == 3);
  CHECK(room.inside()->members.size() == 3);

  ro::proto::ZcMemberExit left;
  left.count = 2;
  left.name = "Tiers";
  left.kicked = 1;
  CHECK(ApplyChatRoomFrame(FrameOf(Encode(left)), room, world) == FrameOutcome::kApplied);
  CHECK(room.inside()->members.size() == 2);
  const auto news = room.TakeNews();
  REQUIRE(news.size() == 2);
  CHECK(news[0].kind == ChatRoomNewsKind::kMemberJoined);
  CHECK(news[1].kind == ChatRoomNewsKind::kMemberExpelled);
  CHECK(news[1].name == "Tiers");

  // Le chef cède ses droits au joueur.
  ro::proto::ZcRoleChange role;
  role.flag = 0;
  role.name = "Moi";
  CHECK(ApplyChatRoomFrame(FrameOf(Encode(role)), room, world) == FrameOutcome::kApplied);
  CHECK(room.inside()->owned);

  left.name = "Moi";
  left.kicked = 0;
  CHECK(ApplyChatRoomFrame(FrameOf(Encode(left)), room, world) == FrameOutcome::kApplied);
  CHECK_FALSE(room.in_room());
  CHECK(room.TakeNews().empty());
}

TEST_CASE("Salon : une expulsion sans effet se dit, une expulsion faite non", "[net][salon]") {
  World world = WorldWithRoom();
  ChatRoom room;
  REQUIRE(ApplyChatRoomFrame(FrameOf(EnterRoom({"Hote", "Moi"})), room, world) ==
          FrameOutcome::kApplied);
  ro::proto::ZcMemberNewentry joined;
  joined.count = 3;
  joined.name = "Garde";
  REQUIRE(ApplyChatRoomFrame(FrameOf(Encode(joined)), room, world) == FrameOutcome::kApplied);
  joined.count = 4;
  joined.name = "Tiers";
  REQUIRE(ApplyChatRoomFrame(FrameOf(Encode(joined)), room, world) == FrameOutcome::kApplied);
  static_cast<void>(room.TakeNews());

  constexpr std::uint32_t kSentMs = 10'000;
  room.NoteExpelSent("Garde", kSentMs);
  room.NoteExpelSent("Tiers", kSentMs);
  // « Tiers » sort, expulsé : rien à dire de plus que son départ.
  ro::proto::ZcMemberExit left;
  left.count = 3;
  left.name = "Tiers";
  left.kicked = 1;
  REQUIRE(ApplyChatRoomFrame(FrameOf(Encode(left)), room, world) == FrameOutcome::kApplied);
  static_cast<void>(room.TakeNews());

  // Avant le délai, on attend encore.
  room.ExpireSilentExpels(kSentMs + ro::sim::kChatRoomExpelSilenceMs - 1);
  CHECK(room.TakeNews().empty());
  // Après, « Garde » est toujours là : le serveur l'a protégé, une fois.
  room.ExpireSilentExpels(kSentMs + ro::sim::kChatRoomExpelSilenceMs);
  const auto news = room.TakeNews();
  REQUIRE(news.size() == 1);
  CHECK(news[0].kind == ChatRoomNewsKind::kExpelRefused);
  CHECK(news[0].name == "Garde");
  room.ExpireSilentExpels(kSentMs + 2 * ro::sim::kChatRoomExpelSilenceMs);
  CHECK(room.TakeNews().empty());
}

TEST_CASE("Salon : en salon, la parole alentour s'y range", "[net][salon]") {
  World world = WorldWithRoom();
  ChatRoom room;
  ro::proto::ZcNotifyChat said;
  said.GID = kHostId;
  said.Message = "Hote : bonjour";
  // Hors salon, la trame n'est pas prise : le monde l'écrit.
  CHECK(ClaimChatRoomLine(FrameOf(Encode(said)), room, world, nullptr, 0) ==
        FrameOutcome::kUnhandled);

  REQUIRE(ApplyChatRoomFrame(FrameOf(EnterRoom({"Hote", "Moi"})), room, world) ==
          FrameOutcome::kApplied);
  CHECK(ClaimChatRoomLine(FrameOf(Encode(said)), room, world, nullptr, 10) ==
        FrameOutcome::kApplied);
  CHECK(ClaimChatRoomLine(FrameOf(PlayerChat("Moi : salut")), room, world, nullptr, 20) ==
        FrameOutcome::kApplied);
  REQUIRE(room.lines().size() == 2);
  CHECK(room.lines()[0].color == ro::net::kChatRoomSpeechColor);
  CHECK(room.lines()[1].kind == ro::sim::ChatKind::kSelf);
  CHECK(room.lines()[1].color == ro::net::kChatRoomOwnSpeechColor);
}

TEST_CASE("Salon : la parole d'un membre garde son auteur", "[net][salon]") {
  World world = WorldWithRoom();
  ChatRoom room;
  REQUIRE(ApplyChatRoomFrame(FrameOf(EnterRoom({"Hote", "Moi"})), room, world) ==
          FrameOutcome::kApplied);
  ro::proto::ZcNotifyChat said;
  said.GID = kHostId;
  said.Message = "Hote : bonjour";
  ro::proto::ChatAuthorNotice author;
  author.fixed.next_opcode = ro::proto::ZcNotifyChat::kOpcode;
  author.fixed.scope = static_cast<std::uint8_t>(ro::proto::ChatAuthorScope::kPublic);
  author.fixed.channel_type = ro::proto::kChatAuthorNoChannelType;
  author.fixed.header_len = static_cast<std::uint16_t>(std::string("Hote : ").size());
  author.fixed.name = "Hote";
  ro::net::Frame frame = FrameOf(Encode(said));
  frame.preamble = Encode(author);

  CHECK(ClaimChatRoomLine(frame, room, world, nullptr, 10) == FrameOutcome::kApplied);
  REQUIRE(room.lines().size() == 1);
  CHECK(room.lines()[0].text == "Hote : bonjour");
  CHECK(room.lines()[0].speaker_id == kHostId);
  REQUIRE(room.lines()[0].author.has_value());
  CHECK(room.lines()[0].author->name == "Hote");
  CHECK(room.lines()[0].text.substr(room.lines()[0].author->said_offset) == "bonjour");
}

TEST_CASE("Salon : un changement de carte nous en sort", "[net][salon]") {
  World world = WorldWithRoom();
  ChatRoom room;
  REQUIRE(ApplyChatRoomFrame(FrameOf(EnterRoom({"Hote", "Moi"})), room, world) ==
          FrameOutcome::kApplied);
  ro::proto::ZcNpcackMapmove moved;
  moved.mapName = "prontera.gat";
  CHECK(ApplyChatRoomFrame(FrameOf(ro::proto::Encode(moved)), room, world) ==
        FrameOutcome::kUnhandled);
  CHECK_FALSE(room.in_room());
}

TEST_CASE("Salon : la fin de notre salon nous en sort, pas celle d'un autre", "[net][salon]") {
  World world = WorldWithRoom();
  ChatRoom room;
  REQUIRE(ApplyChatRoomFrame(FrameOf(EnterRoom({"Hote", "Moi"})), room, world) ==
          FrameOutcome::kApplied);
  ro::proto::ZcDestroyRoom gone;
  gone.chatId = kRoomId + 1;
  CHECK(ApplyChatRoomFrame(FrameOf(Encode(gone)), room, world) == FrameOutcome::kUnhandled);
  CHECK(room.in_room());
  gone.chatId = kRoomId;
  CHECK(ApplyChatRoomFrame(FrameOf(Encode(gone)), room, world) == FrameOutcome::kUnhandled);
  CHECK_FALSE(room.in_room());
}

TEST_CASE("Salon : une demande d'entrée attend sa réponse, une seconde au plus", "[net][salon]") {
  ChatRoom room;
  CHECK(room.EnterMayLeave(0));
  room.NoteEnterSent(5000);
  CHECK_FALSE(room.EnterMayLeave(5000 + ro::sim::kChatRoomEnterSpacingMs));
  CHECK(room.EnterMayLeave(5001 + ro::sim::kChatRoomEnterSpacingMs));
  // Un refus libère aussitôt.
  room.TellEnterRefused(ro::sim::kChatRoomRefusedPassword);
  CHECK(room.EnterMayLeave(5001));
}

TEST_CASE("Salon : les trois demandes sans structure", "[proto][salon]") {
  const auto role = ro::proto::Encode(ro::proto::ChatRoomRoleRequest{.role = 0, .name = "Tiers"});
  REQUIRE(role.has_value());
  CHECK(role->size() == ro::proto::ChatRoomRoleRequest::kWireSize);
  CHECK((*role)[0] == 0xE0);
  CHECK((*role)[6] == 'T');
  const auto expel = ro::proto::Encode(ro::proto::ChatRoomExpelRequest{.name = "Tiers"});
  REQUIRE(expel.has_value());
  CHECK(expel->size() == ro::proto::ChatRoomExpelRequest::kWireSize);
  CHECK((*expel)[2] == 'T');
  const auto exit = ro::proto::Encode(ro::proto::ChatRoomExitRequest{});
  REQUIRE(exit.has_value());
  CHECK(exit->size() == ro::proto::ChatRoomExitRequest::kWireSize);
  CHECK_FALSE(ro::proto::Encode(ro::proto::ChatRoomExpelRequest{}).has_value());
}

TEST_CASE("Salon : une acceptation tardive ouvre quand même le salon", "[net][salon]") {
  World world = WorldWithRoom();
  ChatRoom room;
  room.NoteCreateSent(ChatRoomSettings{.title = "Vente", .limit = 5}, 0);
  room.ExpireSilentCreate(ro::sim::kChatRoomCreateSilenceMs + 1);
  REQUIRE_FALSE(room.creating());
  static_cast<void>(room.TakeNews());
  ro::proto::ZcAckCreateChatroom ack;
  ack.flag = ro::sim::kChatRoomCreated;
  CHECK(ApplyChatRoomFrame(FrameOf(Encode(ack)), room, world) == FrameOutcome::kApplied);
  REQUIRE(room.in_room());
  CHECK(room.inside()->title == "Vente");
}

TEST_CASE("Salon : les réglages changés et la parole du serveur", "[net][salon]") {
  World world = WorldWithRoom();
  ChatRoom room;
  REQUIRE(ApplyChatRoomFrame(FrameOf(EnterRoom({"Hote", "Moi"})), room, world) ==
          FrameOutcome::kApplied);
  ro::proto::ZcChangeChatroom changed;
  changed.ownerId = kHostId;
  changed.chatId = kRoomId;
  changed.limit = 9;
  changed.users = 2;
  changed.flag = ro::sim::kRoomPublic;
  changed.title = "Nouveau";
  CHECK(ApplyChatRoomFrame(FrameOf(Encode(changed)), room, world) == FrameOutcome::kApplied);
  CHECK(room.inside()->title == "Nouveau");
  CHECK(room.inside()->limit == 9);
  CHECK(room.inside()->is_public());
  // Ce que le serveur écrit au joueur seul va aussi au salon, sans orateur.
  CHECK(ClaimChatRoomLine(FrameOf(PlayerChat("Commande inconnue")), room, world, nullptr, 0) ==
        FrameOutcome::kApplied);
  REQUIRE(room.lines().size() == 1);
  CHECK(room.lines()[0].kind == ro::sim::ChatKind::kServer);
  CHECK(room.lines()[0].speaker_id == 0);
}

TEST_CASE("Salon : une partie neuve change le rang du salon suivant", "[net][salon]") {
  World world = WorldWithRoom();
  ChatRoom room;
  REQUIRE(ApplyChatRoomFrame(FrameOf(EnterRoom({"Hote", "Moi"})), room, world) ==
          FrameOutcome::kApplied);
  const std::uint32_t first = room.room_seq();
  room.Reset();
  CHECK_FALSE(room.in_room());
  World again = WorldWithRoom();
  REQUIRE(ApplyChatRoomFrame(FrameOf(EnterRoom({"Hote", "Moi"})), room, again) ==
          FrameOutcome::kApplied);
  CHECK(room.room_seq() != first);
}

TEST_CASE("Salon : les verbes refusent ce que le serveur refuserait", "[net][salon]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  World world = WorldWithRoom();
  ChatRoom room;
  const ChatRoomSettings settings{.title = "Vente", .is_public = true, .limit = 5};
  REQUIRE(ro::net::RequestCreateChatRoom(link, room, settings, 0).has_value());
  // 15 octets d'en-tête, puis le titre ; le type public, aucun mot de passe.
  const std::vector<std::uint8_t> sent = server.Receive(20);
  REQUIRE(sent.size() == 20);
  CHECK(sent[0] == 0xD5);
  CHECK(sent[6] == ro::sim::kRoomPublic);
  // Une création attend : la seconde ne part pas.
  CHECK_FALSE(ro::net::RequestCreateChatRoom(link, room, settings, 1).has_value());
  // Un titre accentué ne part pas non plus.
  ChatRoom other;
  CHECK_FALSE(ro::net::RequestCreateChatRoom(
                  link, other, ChatRoomSettings{.title = "Créé", .is_public = true, .limit = 5}, 0)
                  .has_value());

  // Sortir ferme aussitôt, et ne part qu'une fois.
  REQUIRE(ApplyChatRoomFrame(FrameOf(EnterRoom({"Hote", "Moi"})), other, world) ==
          FrameOutcome::kApplied);
  REQUIRE(ro::net::RequestLeaveChatRoom(link, other).has_value());
  CHECK(server.Receive(2) == std::vector<std::uint8_t>{0xE3, 0x00});
  CHECK_FALSE(other.in_room());
  CHECK_FALSE(ro::net::RequestLeaveChatRoom(link, other).has_value());
  // Hors salon, et sans les droits, ni expulsion ni cession.
  CHECK_FALSE(ro::net::RequestChatRoomExpel(link, other, "Tiers").has_value());
}
