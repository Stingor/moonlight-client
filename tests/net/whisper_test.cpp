#include "ro/net/whisper.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/chat_target.h"
#include "ro/net/world_update.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/friend_list.h"
#include "ro/sim/guild.h"
#include "ro/sim/whisper.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"
#include "support/wire_bytes.h"

// LES CHUCHOTEMENTS : la relation qui choisit l'en-tête, l'écho composé de ce
// qu'on avait gardé, et l'appariement des accusés dans l'ordre.

using ro::net::ApplyWhisperFrame;
using ro::net::FrameOutcome;
using ro::net::WhisperCircle;
using ro::sim::WhisperOutbox;
using ro::sim::WhisperRelation;
using ro::sim::WhisperSide;
using ro::sim::World;
using ro::test_support::FrameOf;
using ro::test_support::PutU16;
using ro::test_support::PutU32;

namespace {

// Un chuchotement reçu, écrit comme le serveur l'écrit : ce paquet n'a pas
// d'encodeur, le client ne l'émet jamais.
[[nodiscard]] std::vector<std::uint8_t> Received(std::string_view sender, std::string_view message,
                                                 bool staff = false, std::uint32_t sender_id = 0) {
  std::vector<std::uint8_t> frame;
  PutU16(frame, ro::proto::WhisperNotice::kOpcode);
  PutU16(frame, 0);
  PutU32(frame, sender_id);
  for (std::size_t at = 0; at < ro::proto::WhisperNotice::kNameSize; ++at) {
    frame.push_back(at < sender.size() ? static_cast<std::uint8_t>(sender[at]) : 0);
  }
  frame.push_back(staff ? 1 : 0);
  for (const char character : message) {
    frame.push_back(static_cast<std::uint8_t>(character));
  }
  frame.push_back(0);
  const auto length = static_cast<std::uint16_t>(frame.size());
  frame[2] = static_cast<std::uint8_t>(length & 0xFF);
  frame[3] = static_cast<std::uint8_t>(length >> 8);
  return frame;
}

[[nodiscard]] std::vector<std::uint8_t> Ack(std::uint8_t result) {
  ro::proto::ZcAckWhisper answer;
  answer.result = result;
  return Encode(answer);
}

constexpr std::uint8_t kDelivered = 0;
constexpr std::uint8_t kOffline = 1;

[[nodiscard]] ro::net::ChatIntent Whisper(std::string_view to, std::string_view text) {
  return ro::net::ChatIntent{.target = ro::net::ChatTarget::kWhisper,
                             .recipient = std::string(to),
                             .text = std::string(text)};
}

}  // namespace

TEST_CASE("Chuchotement : la relation choisit, l'ami d'abord", "[net][whisper]") {
  ro::sim::FriendList friends;
  friends.Add(ro::sim::Friend{.account_id = 1, .char_id = 2, .name = "Alice"});
  ro::sim::GuildBook guild;
  guild.ReplaceMembers({ro::sim::GuildMember{.account_id = 3, .char_id = 4, .name = "Bob"},
                        ro::sim::GuildMember{.account_id = 1, .char_id = 2, .name = "Alice"}});
  const WhisperCircle circle{.friends = &friends, .guild = &guild};

  // Ami ET membre : l'ami l'emporte, comme dans le client.
  CHECK(ro::net::WhisperRelationOf("Alice", false, circle) == WhisperRelation::kFriend);
  CHECK(ro::net::WhisperRelationOf("Alice", true, circle) == WhisperRelation::kFriend);
  // Le staff passe devant la guilde.
  CHECK(ro::net::WhisperRelationOf("Bob", true, circle) == WhisperRelation::kStaff);
  CHECK(ro::net::WhisperRelationOf("Bob", false, circle) == WhisperRelation::kGuildMember);
  CHECK(ro::net::WhisperRelationOf("Carol", false, circle) == WhisperRelation::kStranger);
  // Sans cercle, tout le monde est un inconnu.
  CHECK(ro::net::WhisperRelationOf("Alice", false, WhisperCircle{}) == WhisperRelation::kStranger);
}

TEST_CASE("Chuchotement : une ligne reçue porte l'auteur et sa relation", "[net][whisper]") {
  World world;
  ro::sim::FriendList friends;
  friends.Add(ro::sim::Friend{.account_id = 1, .char_id = 2, .name = "Alice"});
  const WhisperCircle circle{.friends = &friends};

  CHECK(ApplyWhisperFrame(FrameOf(Received("Alice", "coucou")), world, circle, 500) ==
        FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 1);
  const ro::sim::ChatLine& line = world.chat_lines()[0];
  CHECK(line.kind == ro::sim::ChatKind::kWhisper);
  CHECK(line.whisper.side == WhisperSide::kReceived);
  CHECK(line.whisper.peer_name == "Alice");
  CHECK(line.whisper.relation == WhisperRelation::kFriend);
  // ⚠ LE TEXTE NE PORTE PAS L'EN-TÊTE : il se compose à l'affichage.
  CHECK(line.text == "coucou");
}

TEST_CASE("Chuchotement : une ligne reçue garde l'identifiant de son auteur", "[net][whisper]") {
  // ⚠ ZÉRO DIT « PAS UN PERSONNAGE CONNECTÉ » : c'est ainsi que la voix du
  // serveur se distingue d'un joueur, quel que soit le nom qu'elle prend.
  constexpr std::uint32_t kAliceId = 2000001;
  World world;
  REQUIRE(ApplyWhisperFrame(FrameOf(Received("Alice", "coucou", false, kAliceId)), world,
                            WhisperCircle{}, 500) == FrameOutcome::kApplied);
  REQUIRE(ApplyWhisperFrame(FrameOf(Received("Server", "annonce")), world, WhisperCircle{}, 600) ==
          FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 2);
  CHECK(world.chat_lines()[0].whisper.sender_id == kAliceId);
  CHECK(world.chat_lines()[1].whisper.sender_id == 0);
}

TEST_CASE("Chuchotement : l'accusé compose l'écho de ce qu'on avait envoyé", "[net][whisper]") {
  World world;
  WhisperOutbox outbox;
  const WhisperCircle circle{.outbox = &outbox};
  CHECK(ro::net::NoteWhisperSent(outbox, Whisper("Bob", "tu es la ?"), 100));

  CHECK(ApplyWhisperFrame(FrameOf(Ack(kDelivered)), world, circle, 150) == FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 1);
  const ro::sim::ChatLine& echo = world.chat_lines()[0];
  CHECK(echo.whisper.side == WhisperSide::kSent);
  CHECK(echo.whisper.peer_name == "Bob");
  CHECK(echo.text == "tu es la ?");
  CHECK(outbox.size() == 0);
}

TEST_CASE("Chuchotement : les accusés s'apparient dans l'ordre des envois", "[net][whisper]") {
  World world;
  WhisperOutbox outbox;
  const WhisperCircle circle{.outbox = &outbox};
  CHECK(ro::net::NoteWhisperSent(outbox, Whisper("Absent", "allo ?"), 100));
  CHECK(ro::net::NoteWhisperSent(outbox, Whisper("Bob", "salut"), 110));

  // Le premier accusé dit l'absence : il concerne le premier envoi.
  CHECK(ApplyWhisperFrame(FrameOf(Ack(kOffline)), world, circle, 120) == FrameOutcome::kApplied);
  CHECK(ApplyWhisperFrame(FrameOf(Ack(kDelivered)), world, circle, 130) == FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 2);
  CHECK(world.chat_lines()[0].whisper.side == WhisperSide::kUndelivered);
  CHECK(world.chat_lines()[0].whisper.peer_name == "Absent");
  // Le code reste : c'est lui qui choisira la phrase du client à l'affichage.
  CHECK(world.chat_lines()[0].whisper.refusal == ro::sim::WhisperRefusal::kOffline);
  // Sans texte : la phrase se choisit à l'affichage.
  CHECK(world.chat_lines()[0].text.empty());
  CHECK(world.chat_lines()[1].whisper.side == WhisperSide::kSent);
  CHECK(world.chat_lines()[1].whisper.peer_name == "Bob");
  CHECK(world.chat_lines()[1].text == "salut");
}

TEST_CASE("Chuchotement : un joueur muet ne note pas ce qu'il chuchote", "[net][whisper]") {
  // 🔴 SOUS BERSERK, LE SERVEUR JETTE LA LIGNE SANS ACCUSÉ. Notée, elle prenait
  // l'accusé du chuchotement suivant : l'écho et le refus passaient à un autre.
  WhisperOutbox outbox;
  CHECK_FALSE(ro::net::NoteWhisperSent(outbox, Whisper("Alice", "hi"), 0,
                                       ro::net::WhisperNoting{.muted = true}));
  CHECK(outbox.size() == 0);
  CHECK(ro::net::NoteWhisperSent(outbox, Whisper("Bob", "yo"), 10, ro::net::WhisperNoting{}));
  const std::optional<ro::sim::PendingWhisper> settled = outbox.Settle(20);
  REQUIRE(settled.has_value());
  CHECK(settled->recipient == "Bob");
}

TEST_CASE("Chuchotement : un « #nom » que le serveur ne prend pas pour un canal se note",
          "[net][whisper]") {
  // 🔴 UN CANAL INCONNU, OU FERMÉ À L'ÉCRITURE, RETOMBE EN CHUCHOTEMENT, accusé
  // « absent ». Non noté, cet accusé prenait la place de celui de l'envoi
  // suivant.
  const std::vector<ro::sim::ChatChannel> channels = {
      {.name = "main", .can_write = true},
      {.name = "annonces", .can_write = false},
      {.name = "ally", .can_write = true, .guild_only = true}};
  const ro::net::WhisperNoting noting{.channels = channels};
  WhisperOutbox outbox;
  CHECK_FALSE(ro::net::NoteWhisperSent(outbox, Whisper("#main", "salut"), 0, noting));
  CHECK_FALSE(ro::net::NoteWhisperSent(outbox, Whisper("#MAIN", "salut"), 0, noting));
  CHECK_FALSE(ro::net::NoteWhisperSent(outbox, Whisper("#ally", "salut"), 0, noting));
  CHECK(outbox.size() == 0);
  CHECK(ro::net::NoteWhisperSent(outbox, Whisper("#inexistant", "salut"), 0, noting));
  CHECK(ro::net::NoteWhisperSent(outbox, Whisper("#annonces", "salut"), 0, noting));
  CHECK(outbox.size() == 2);
  // Sans liste annoncée, on ne sait pas : tout « #nom » reste un canal.
  WhisperOutbox unknown;
  CHECK_FALSE(ro::net::NoteWhisperSent(unknown, Whisper("#inexistant", "salut"), 0));
}

TEST_CASE("Chuchotement : ce qui coupe la parole", "[net][whisper]") {
  // Les états `NoChat` de la table de ce serveur, et des bonnes manières
  // négatives — `SC_NOCHAT` n'a pas d'icône, c'est le compte qui le trahit.
  using ro::sim::ActiveStatus;
  using ro::sim::CannotChat;
  // 🔴 BERSERK (EFST_BERSERK) LAISSE PARLER sur ce serveur : sa surcharge de
  // status.yml lève le mutisme de la table d'origine.
  constexpr ro::sim::StatusEffectId kEfstBerserk = 107;
  const std::vector<ActiveStatus> berserk = {{.effect = kEfstBerserk}};
  const std::vector<ActiveStatus> fever = {{.effect = ro::sim::kEfstSaturdayNightFever}};
  const std::vector<ActiveStatus> asleep = {{.effect = ro::sim::kEfstDeepSleep}};
  const std::vector<ActiveStatus> other = {{.effect = ro::sim::kEfstTrickDead}};
  CHECK_FALSE(CannotChat(berserk, 0));
  CHECK(CannotChat(fever, 0));
  CHECK(CannotChat(asleep, 0));
  CHECK_FALSE(CannotChat(other, 0));
  CHECK(CannotChat({}, -1));
  CHECK_FALSE(CannotChat({}, 0));
}

TEST_CASE("Chuchotement : ce que le serveur n'accuse pas ne se note pas", "[net][whisper]") {
  // ⚠ LES NOTER DÉCALERAIT TOUS LES ACCUSÉS SUIVANTS.
  WhisperOutbox outbox;
  ro::net::ChatIntent channel = Whisper("ally", "a l'aide");
  channel.target = ro::net::ChatTarget::kChannel;
  CHECK_FALSE(ro::net::NoteWhisperSent(outbox, channel, 0));
  // UN CANAL PAR « /w #nom », que la lecture range en chuchotement.
  const ro::net::ChatIntent typed = ro::net::ReadChatIntent("/w #main salut");
  REQUIRE(typed.target == ro::net::ChatTarget::kWhisper);
  CHECK_FALSE(ro::net::NoteWhisperSent(outbox, typed, 0));
  // Et par la case « Pseudo ».
  const ro::net::ChatIntent boxed =
      ro::net::ReadChatIntent("salut", ro::net::ChatArming{.whisper_to = "#trade"});
  CHECK_FALSE(ro::net::NoteWhisperSent(outbox, boxed, 0));
  CHECK_FALSE(ro::net::NoteWhisperSent(outbox, Whisper("npc:Guide", "go"), 0));
  // UNE COMMANDE : le serveur l'exécute et ne chuchote rien.
  CHECK_FALSE(ro::net::NoteWhisperSent(outbox, Whisper("Bob", "@where"), 0));
  CHECK_FALSE(ro::net::NoteWhisperSent(outbox, Whisper("Bob", "#Alice @go 0"), 0));
  // UN TEXTE TROP LONG : le serveur le refuse sans un mot.
  CHECK_FALSE(ro::net::NoteWhisperSent(outbox, Whisper("Bob", std::string(254, 'a')), 0));
  CHECK_FALSE(ro::net::NoteWhisperSent(outbox, Whisper("Bob", ""), 0));
  CHECK(outbox.size() == 0);
  // « NPC: » SEUL N'EST PAS UN PNJ : le serveur le traite en joueur, et l'accuse.
  CHECK(ro::net::NoteWhisperSent(outbox, Whisper("NPC:", "allo"), 0));
  CHECK(ro::net::NoteWhisperSent(outbox, Whisper("Bob", std::string(253, 'a')), 0));
  CHECK(outbox.size() == 2);
}

TEST_CASE("Chuchotement : un envoi retiré du milieu ne décale pas les autres", "[net][whisper]") {
  World world;
  WhisperOutbox outbox;
  const WhisperCircle circle{.outbox = &outbox};
  outbox.Note("Alice", "un", 0);
  outbox.Note("Marchand", "deux", 1);
  outbox.Note("Bob", "trois", 2);
  CHECK(outbox.Forget("Marchand"));
  CHECK(ApplyWhisperFrame(FrameOf(Ack(kDelivered)), world, circle, 3) == FrameOutcome::kApplied);
  CHECK(ApplyWhisperFrame(FrameOf(Ack(kDelivered)), world, circle, 4) == FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 2);
  CHECK(world.chat_lines()[0].whisper.peer_name == "Alice");
  CHECK(world.chat_lines()[1].whisper.peer_name == "Bob");
  CHECK(world.chat_lines()[1].text == "trois");
}

TEST_CASE("Chuchotement : un envoi trop ancien n'attend plus son accusé", "[net][whisper]") {
  WhisperOutbox outbox;
  outbox.Note("Vieux", "oublie", 0);
  outbox.Note("Neuf", "attendu", ro::sim::kWhisperAnswerWaitMs);
  const std::optional<ro::sim::PendingWhisper> settled =
      outbox.Settle(ro::sim::kWhisperAnswerWaitMs + 1);
  REQUIRE(settled.has_value());
  CHECK(settled->recipient == "Neuf");
}

TEST_CASE("Chuchotement : l'échoppe hors ligne répond à la place de l'accusé", "[net][whisper]") {
  // Le serveur chuchote lui-même que le destinataire tient une échoppe : aucun
  // accusé ne viendra pour cet envoi, et il doit quitter la file.
  World world;
  WhisperOutbox outbox;
  const WhisperCircle circle{.outbox = &outbox};
  outbox.Note("Marchand", "tu vends ?", 0);
  outbox.Note("Bob", "salut", 1);
  constexpr std::string_view kReply =
      "Marchand is in autotrade mode and cannot receive whispered messages.";
  // ⚠ PEU IMPORTE L'AUTEUR : le nom du serveur est un réglage qu'on ignore.
  // C'est le nom CITÉ qui compte — il doit attendre un accusé.
  CHECK(
      ApplyWhisperFrame(FrameOf(Received("Farceur",
                                         "Absent is in autotrade mode and cannot receive whispered "
                                         "messages.")),
                        world, circle, 2) == FrameOutcome::kApplied);
  CHECK(outbox.size() == 2);
  REQUIRE(world.chat_lines().size() == 1);
  CHECK(world.chat_lines()[0].whisper.side == WhisperSide::kReceived);

  CHECK(ApplyWhisperFrame(FrameOf(Received("NImporteQui", kReply)), world, circle, 3) ==
        FrameOutcome::kApplied);
  REQUIRE(outbox.size() == 1);
  CHECK(outbox.pending().front().recipient == "Bob");
  // RANGÉE COMME CE QUE LE SERVEUR ÉCRIT : pas un chuchotement, donc ni
  // conversation ni alarme.
  REQUIRE(world.chat_lines().size() == 2);
  CHECK(world.chat_lines()[1].kind == ro::sim::ChatKind::kServer);
  CHECK(world.chat_lines()[1].whisper.side == WhisperSide::kNone);
}

TEST_CASE("Chuchotement : sans file, un succès ne dit rien", "[net][whisper]") {
  World world;
  CHECK(ApplyWhisperFrame(FrameOf(Ack(kDelivered)), world, WhisperCircle{}, 0) ==
        FrameOutcome::kIgnored);
  CHECK(world.chat_lines().empty());
}
