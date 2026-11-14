// LE CHAMP DE BATAILLE SUR LE FIL : la file d'inscription, le combat, et les
// verbes qu'on émet.

#include "ro/net/battleground.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/battleground_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/actor.h"
#include "ro/sim/battlefield.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/entry_queue.h"
#include "ro/sim/world.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/net_frames.h"
#include "support/wire_bytes.h"

using ro::net::ApplyBattlefieldFrame;
using ro::net::ApplyEntryQueueFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::EntryQueue;
using ro::sim::EntryQueueNoticeKind;
using ro::sim::EntryQueuePhase;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::FrameOf;
using ro::test_support::ReadU16;

namespace {

constexpr const char* kField = "Tierra Gorge";
constexpr const char* kMap = "bat_a01";
constexpr std::uint32_t kNowMs = 5'000;
constexpr std::uint32_t kSelf = 2'000'001;
constexpr std::uint32_t kMate = 2'000'002;
constexpr std::uint32_t kFoe = 2'000'003;
constexpr std::uint16_t kLionCamp = 7;
constexpr std::uint16_t kEagleCamp = 8;
constexpr std::uint16_t kKnightJob = 7;
constexpr std::int16_t kCellX = 120;
constexpr std::int16_t kCellY = 64;
constexpr std::int32_t kSixInQueue = 6;
constexpr std::int32_t kMateHealth = 4'200;
constexpr std::int32_t kMateMaxHealth = 9'000;

// Les réponses du serveur et les nôtres, telles qu'elles passent sur le fil.
constexpr std::uint8_t kWireAccepted = 1;
constexpr std::uint8_t kWireLevel = 6;
constexpr std::uint8_t kWireCancelSucceeded = 1;
constexpr std::uint8_t kWireLobbyAccept = 1;
constexpr std::uint8_t kWireLobbyDecline = 2;
constexpr std::uint16_t kWireGuildApply = 4;

// Un nom de 24 octets lu à `at`, coupé au premier zéro.
[[nodiscard]] std::string ReadName(const std::vector<std::uint8_t>& bytes, std::size_t at) {
  std::string name;
  for (std::size_t offset = at; offset < at + ro::proto::kBattlefieldNameBytes; ++offset) {
    if (bytes[offset] == 0) {
      break;
    }
    name.push_back(static_cast<char>(bytes[offset]));
  }
  return name;
}

[[nodiscard]] Frame ApplyAck(std::uint8_t code) {
  ro::proto::PacketBgqueueAck ack;
  ack.type = code;
  ack.bg_name = kField;
  return FrameOf(ro::proto::Encode(ack));
}

// Un joueur en vue, à qui l'on peut poser une vie ou une bulle.
void SpawnPlayer(ro::sim::World& world, std::uint32_t id) {
  ro::sim::Actor actor;
  actor.id = id;
  actor.kind = ro::sim::ActorKind::kPlayer;
  actor.position = ro::sim::CellPosition{kCellX, kCellY, 0};
  world.Spawn(actor);
}

}  // namespace

// ── LA FILE ─────────────────────────────────────────────────────────────────

TEST_CASE("Champ de bataille : une trame étrangère n'est pas à la file", "[net][battleground]") {
  constexpr std::uint16_t kUnrelatedOpcode = 0x0080;
  EntryQueue queue;
  const Frame frame{.opcode = kUnrelatedOpcode, .bytes = {}};
  CHECK(ApplyEntryQueueFrame(frame, queue, 0) == FrameOutcome::kUnhandled);
  ro::sim::World world;
  CHECK(ApplyBattlefieldFrame(frame, world, nullptr, 0) == FrameOutcome::kUnhandled);
}

TEST_CASE("Champ de bataille : l'inscription acceptée met en file, l'effectif suit",
          "[net][battleground]") {
  EntryQueue queue;
  CHECK(ApplyEntryQueueFrame(ApplyAck(kWireAccepted), queue, kNowMs) == FrameOutcome::kApplied);
  CHECK(queue.phase() == EntryQueuePhase::kQueued);
  CHECK(queue.field_name() == kField);

  ro::proto::PacketBgqueueNotifyEntry entry;
  entry.name = kField;
  entry.position = kSixInQueue;
  CHECK(ApplyEntryQueueFrame(FrameOf(ro::proto::Encode(entry)), queue, kNowMs) ==
        FrameOutcome::kApplied);
  CHECK(queue.count() == kSixInQueue);
}

TEST_CASE("Champ de bataille : un refus d'inscription dit pourquoi", "[net][battleground]") {
  EntryQueue queue;
  queue.NoteApplied(kField, ro::sim::EntryQueueApplyKind::kSolo, kNowMs);
  CHECK(ApplyEntryQueueFrame(ApplyAck(kWireLevel), queue, kNowMs) == FrameOutcome::kApplied);
  CHECK(queue.phase() == EntryQueuePhase::kIdle);
  const auto notices = queue.TakeNotices();
  REQUIRE(notices.size() == 1);
  CHECK(notices.front().kind == EntryQueueNoticeKind::kLevel);
}

TEST_CASE("Champ de bataille : l'annulation réussie fait sortir de la file",
          "[net][battleground]") {
  EntryQueue queue;
  REQUIRE(ApplyEntryQueueFrame(ApplyAck(kWireAccepted), queue, kNowMs) == FrameOutcome::kApplied);
  ro::proto::PacketBgqueueNoticeDelete cancelled;
  cancelled.type = kWireCancelSucceeded;
  cancelled.bg_name = kField;
  CHECK(ApplyEntryQueueFrame(FrameOf(ro::proto::Encode(cancelled)), queue, kNowMs) ==
        FrameOutcome::kApplied);
  CHECK(queue.phase() == EntryQueuePhase::kIdle);
}

TEST_CASE("Champ de bataille : l'annulation répondue 14 fait aussi sortir, 0 non",
          "[net][battleground]") {
  constexpr std::uint8_t kWireCancelClosed = 14;
  constexpr std::uint8_t kWireCancelRefused = 0;
  ro::proto::PacketBgqueueNoticeDelete answer;
  answer.bg_name = kField;

  EntryQueue closed;
  REQUIRE(ApplyEntryQueueFrame(ApplyAck(kWireAccepted), closed, kNowMs) == FrameOutcome::kApplied);
  answer.type = kWireCancelClosed;
  REQUIRE(ApplyEntryQueueFrame(FrameOf(ro::proto::Encode(answer)), closed, kNowMs) ==
          FrameOutcome::kApplied);
  CHECK(closed.phase() == EntryQueuePhase::kIdle);

  EntryQueue refused;
  REQUIRE(ApplyEntryQueueFrame(ApplyAck(kWireAccepted), refused, kNowMs) == FrameOutcome::kApplied);
  answer.type = kWireCancelRefused;
  REQUIRE(ApplyEntryQueueFrame(FrameOf(ro::proto::Encode(answer)), refused, kNowMs) ==
          FrameOutcome::kApplied);
  CHECK(refused.phase() == EntryQueuePhase::kQueued);
}

TEST_CASE("Champ de bataille : l'invitation, l'entrée accordée et la remise à zéro",
          "[net][battleground]") {
  EntryQueue queue;
  REQUIRE(ApplyEntryQueueFrame(ApplyAck(kWireAccepted), queue, kNowMs) == FrameOutcome::kApplied);
  (void)queue.TakeSounds();

  ro::proto::PacketBgqueueBattlebegins invitation;
  invitation.bg_name = kField;
  invitation.game_name = kField;
  CHECK(ApplyEntryQueueFrame(FrameOf(ro::proto::Encode(invitation)), queue, kNowMs) ==
        FrameOutcome::kApplied);
  CHECK(queue.phase() == EntryQueuePhase::kInvited);
  CHECK(queue.lobby_name() == kField);
  CHECK(queue.invitation_ms_left(kNowMs) == ro::sim::kEntryQueueInvitationMs);

  queue.NoteInvitationAnswered(true, kNowMs);
  ro::proto::LobbyAdmissionAck admitted;
  admitted.result = kWireLobbyAccept;
  admitted.field_name = kMap;
  admitted.lobby_name = kMap;
  CHECK(ApplyEntryQueueFrame(FrameOf(ro::proto::Encode(admitted)), queue, kNowMs) ==
        FrameOutcome::kApplied);
  const auto notices = queue.TakeNotices();
  REQUIRE(notices.size() == 1);
  CHECK(notices.front().kind == EntryQueueNoticeKind::kAdmitted);
  CHECK(notices.front().field_name == kMap);

  CHECK(ApplyEntryQueueFrame(FrameOf(ro::proto::Encode(ro::proto::ZcEntryQueueInit{})), queue,
                             kNowMs) == FrameOutcome::kApplied);
  CHECK(queue.phase() == EntryQueuePhase::kIdle);
}

TEST_CASE("Champ de bataille : une trame de file mal taillée ne se lit pas",
          "[net][battleground]") {
  EntryQueue queue;
  Frame frame = ApplyAck(kWireAccepted);
  frame.bytes.pop_back();
  CHECK(ApplyEntryQueueFrame(frame, queue, kNowMs) == FrameOutcome::kUndecodable);
  CHECK(queue.phase() == EntryQueuePhase::kIdle);
}

// ── LE COMBAT ───────────────────────────────────────────────────────────────

TEST_CASE("Champ de bataille : les camps se posent, le nôtre compris, sans acteur",
          "[net][battleground]") {
  ro::sim::World world;
  world.set_player_id(kSelf);
  ro::proto::BattlefieldCampNotice camp;
  camp.actor_id = kFoe;
  camp.camp = kEagleCamp;
  CHECK(ApplyBattlefieldFrame(FrameOf(ro::proto::Encode(camp)), world, nullptr, kNowMs) ==
        FrameOutcome::kApplied);
  camp.actor_id = kSelf;
  camp.camp = kLionCamp;
  CHECK(ApplyBattlefieldFrame(FrameOf(ro::proto::Encode(camp)), world, nullptr, kNowMs) ==
        FrameOutcome::kApplied);
  CHECK(world.battlefield().own_camp() == kLionCamp);
  CHECK(world.battlefield().IsEnemy(kFoe));
  CHECK(world.battlefield().own_side() == ro::sim::BattleSide::kLion);
}

TEST_CASE("Champ de bataille : le score se pose", "[net][battleground]") {
  ro::sim::World world;
  ro::proto::BattlefieldScoreNotice score;
  score.lion = 1;
  score.eagle = 2;
  CHECK(ApplyBattlefieldFrame(FrameOf(ro::proto::Encode(score)), world, nullptr, kNowMs) ==
        FrameOutcome::kApplied);
  CHECK(world.battlefield().score().lion == 1);
  CHECK(world.battlefield().score().eagle == 2);
}

TEST_CASE("Champ de bataille : les coéquipiers se posent et se retirent, nous exceptés",
          "[net][battleground]") {
  ro::sim::World world;
  world.set_player_id(kSelf);
  ro::proto::BattlefieldPositionNotice position;
  position.account_id = kMate;
  position.name = "Brunhild";
  position.job = kKnightJob;
  position.x = kCellX;
  position.y = kCellY;
  CHECK(ApplyBattlefieldFrame(FrameOf(ro::proto::Encode(position)), world, nullptr, kNowMs) ==
        FrameOutcome::kApplied);
  REQUIRE(world.battlefield().mates().count(kMate) == 1);
  CHECK(world.battlefield().mates().at(kMate).name == "Brunhild");

  position.account_id = kSelf;
  CHECK(ApplyBattlefieldFrame(FrameOf(ro::proto::Encode(position)), world, nullptr, kNowMs) ==
        FrameOutcome::kIgnored);
  CHECK(world.battlefield().mates().count(kSelf) == 0);

  ro::proto::BattlefieldPositionNotice gone;
  gone.account_id = kMate;
  gone.x = -1;
  gone.y = -1;
  CHECK(ApplyBattlefieldFrame(FrameOf(ro::proto::Encode(gone)), world, nullptr, kNowMs) ==
        FrameOutcome::kApplied);
  CHECK(world.battlefield().mates().empty());
}

TEST_CASE("Champ de bataille : la vie d'un coéquipier en vue se pose, hors vue elle s'ignore",
          "[net][battleground]") {
  ro::sim::World world;
  ro::proto::ZcBattlefieldNotifyHp hp;
  hp.AID = kMate;
  hp.hp = kMateHealth;
  hp.maxhp = kMateMaxHealth;
  CHECK(ApplyBattlefieldFrame(FrameOf(ro::proto::Encode(hp)), world, nullptr, kNowMs) ==
        FrameOutcome::kIgnored);

  SpawnPlayer(world, kMate);
  CHECK(ApplyBattlefieldFrame(FrameOf(ro::proto::Encode(hp)), world, nullptr, kNowMs) ==
        FrameOutcome::kApplied);
  const ro::sim::Actor* mate = world.Find(kMate);
  REQUIRE(mate != nullptr);
  CHECK(mate->health == kMateHealth);
  // La vie maximale connue est ce qui range la barre parmi celles du groupe.
  CHECK(mate->max_health == kMateMaxHealth);
}

TEST_CASE("Champ de bataille : la parole du serveur entre sans bulle, celle d'un joueur en a une",
          "[net][battleground]") {
  ro::sim::World world;
  SpawnPlayer(world, kMate);

  ro::proto::BattlefieldChatNotice server;
  server.speaker_id = 0;
  server.speaker_name = "Server";
  server.message = "Server : Lions win!";
  CHECK(ApplyBattlefieldFrame(FrameOf(ro::proto::Encode(server)), world, nullptr, kNowMs) ==
        FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 1);
  const ro::sim::ChatLine& announced = world.chat_lines().back();
  CHECK(announced.kind == ro::sim::ChatKind::kBattlefield);
  CHECK(announced.type == ro::sim::ChatType::kBattlefield);
  CHECK(announced.speaker_id == 0);
  CHECK(announced.text == "Server : Lions win!");
  CHECK(announced.color == ro::sim::kBattlefieldChatRgb);
  CHECK(world.Find(kMate)->chat_text.empty());

  ro::proto::BattlefieldChatNotice said;
  said.speaker_id = kMate;
  said.speaker_name = "Brunhild";
  // « é » en page de codes du jeu : il arrive en UTF-8 dans la conversation.
  said.message =
      "Brunhild : d\xE9"
      "fendez";
  CHECK(ApplyBattlefieldFrame(FrameOf(ro::proto::Encode(said)), world, nullptr, kNowMs) ==
        FrameOutcome::kApplied);
  CHECK(world.chat_lines().back().text == "Brunhild : défendez");
  CHECK(world.Find(kMate)->chat_text == "Brunhild : défendez");
}

// ── LES VERBES ──────────────────────────────────────────────────────────────

TEST_CASE("Champ de bataille : l'inscription part en 0x08D7, type puis nom",
          "[net][battleground]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  REQUIRE(ro::net::RequestEntryQueueApply(link, ro::sim::EntryQueueApplyKind::kGuild, kField)
              .has_value());
  const std::vector<std::uint8_t> sent =
      server.Receive(ro::proto::PacketBgqueueRegister::kWireSize);
  REQUIRE(sent.size() == ro::proto::PacketBgqueueRegister::kWireSize);
  CHECK(ReadU16(sent, 0) == 0x08D7);
  CHECK(ReadU16(sent, 2) == kWireGuildApply);
  CHECK(ReadName(sent, 4) == kField);
}

TEST_CASE("Champ de bataille : l'annulation et l'effectif partent avec le nom seul",
          "[net][battleground]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  REQUIRE(ro::net::RequestEntryQueueCancel(link, kField).has_value());
  REQUIRE(ro::net::RequestEntryQueueRanking(link, kField).has_value());
  const std::size_t cancel_size = ro::proto::PacketBgqueueRevokeReq::kWireSize;
  const std::size_t ranking_size = ro::proto::PacketBgqueueCheckstate::kWireSize;
  const std::vector<std::uint8_t> sent = server.Receive(cancel_size + ranking_size);
  REQUIRE(sent.size() == cancel_size + ranking_size);
  CHECK(ReadU16(sent, 0) == 0x08DA);
  CHECK(ReadName(sent, 2) == kField);
  CHECK(ReadU16(sent, cancel_size) == 0x090A);
  CHECK(ReadName(sent, cancel_size + 2) == kField);
}

TEST_CASE("Champ de bataille : la réponse à l'invitation porte 1 ou 2, puis les deux noms",
          "[net][battleground]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  REQUIRE(ro::net::AnswerLobbyAdmission(link, true, kField, kField).has_value());
  REQUIRE(ro::net::AnswerLobbyAdmission(link, false, kField, kField).has_value());
  const std::size_t size = ro::proto::PacketBgqueueBattlebeginAck::kWireSize;
  const std::vector<std::uint8_t> sent = server.Receive(size * 2);
  REQUIRE(sent.size() == size * 2);
  CHECK(ReadU16(sent, 0) == 0x08E0);
  CHECK(sent[2] == kWireLobbyAccept);
  CHECK(ReadName(sent, 3) == kField);
  CHECK(ReadName(sent, 3 + ro::proto::kBattlefieldNameBytes) == kField);
  CHECK(sent[size + 2] == kWireLobbyDecline);
}

TEST_CASE("Champ de bataille : un nom vide ou trop long ne part pas", "[net][battleground]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  const std::string too_long(ro::proto::kBattlefieldNameBytes, 'k');
  const std::string longest(ro::proto::kBattlefieldNameBytes - 1, 'k');
  CHECK_FALSE(
      ro::net::RequestEntryQueueApply(link, ro::sim::EntryQueueApplyKind::kSolo, "").has_value());
  CHECK_FALSE(ro::net::RequestEntryQueueApply(link, ro::sim::EntryQueueApplyKind::kSolo, too_long)
                  .has_value());
  CHECK_FALSE(ro::net::RequestEntryQueueCancel(link, "").has_value());
  CHECK_FALSE(ro::net::RequestEntryQueueRanking(link, too_long).has_value());
  CHECK_FALSE(ro::net::AnswerLobbyAdmission(link, true, kField, "").has_value());
  // Le plus long nom qui garde son zéro passe.
  REQUIRE(ro::net::RequestEntryQueueCancel(link, longest).has_value());
  const std::vector<std::uint8_t> sent =
      server.Receive(ro::proto::PacketBgqueueRevokeReq::kWireSize);
  REQUIRE(sent.size() == ro::proto::PacketBgqueueRevokeReq::kWireSize);
  CHECK(ReadName(sent, 2) == longest);
}
