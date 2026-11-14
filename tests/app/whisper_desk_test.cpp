#include "chat/whisper_desk.h"

#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/chat_line.h"
#include "ro/sim/whisper.h"

// LES CONVERSATIONS 1:1 : ce que le réglage des amis ouvre, ce qu'une
// conversation prend au fil, son non-lu, et le plafond.

using moonclient::Conversation;
using moonclient::FriendSetup;
using moonclient::WhisperDesk;
using ro::sim::ChatLine;
using ro::sim::WhisperRelation;
using ro::sim::WhisperSide;

namespace {

constexpr std::string_view kMe = "Moi";

[[nodiscard]] ChatLine Line(std::uint64_t seq, WhisperSide side, std::string_view peer,
                            WhisperRelation relation = WhisperRelation::kStranger) {
  ChatLine line;
  line.seq = seq;
  line.kind = ro::sim::ChatKind::kWhisper;
  line.whisper =
      ro::sim::WhisperFacts{.side = side, .peer_name = std::string(peer), .relation = relation};
  line.text = "bonjour";
  return line;
}

}  // namespace

TEST_CASE("Conversation : le réglage des amis décide d'ouvrir", "[app][whisper]") {
  const FriendSetup friends_only{.open_from_friend = true};
  CHECK(moonclient::OpensConversation(friends_only, WhisperRelation::kFriend));
  CHECK_FALSE(moonclient::OpensConversation(friends_only, WhisperRelation::kStranger));
  // ⚠ UN MEMBRE DE LA GUILDE EST UN INCONNU pour cette question : le client ne
  // regarde que sa liste d'amis.
  CHECK_FALSE(moonclient::OpensConversation(friends_only, WhisperRelation::kGuildMember));
  const FriendSetup strangers{.open_from_stranger = true};
  CHECK(moonclient::OpensConversation(strangers, WhisperRelation::kGuildMember));
  CHECK_FALSE(moonclient::OpensConversation(strangers, WhisperRelation::kFriend));
  // PAR DÉFAUT, RIEN NE S'OUVRE, comme au client.
  CHECK_FALSE(moonclient::OpensConversation(FriendSetup{}, WhisperRelation::kFriend));
  CHECK_FALSE(moonclient::OpensConversation(FriendSetup{}, WhisperRelation::kStranger));
}

TEST_CASE("Conversation : par défaut, le fil garde tout", "[app][whisper]") {
  WhisperDesk desk;
  const std::vector<ChatLine> chat = {Line(1, WhisperSide::kReceived, "Bob")};
  const WhisperDesk::Absorbed absorbed = desk.Absorb(chat, FriendSetup{}, kMe);
  CHECK(absorbed.opened == 0);
  CHECK(desk.conversations().empty());
  CHECK_FALSE(desk.ShownApart(1));
}

TEST_CASE("Conversation : ouverte par la réception, elle prend la ligne", "[app][whisper]") {
  WhisperDesk desk;
  const std::vector<ChatLine> chat = {Line(1, WhisperSide::kReceived, "Bob"),
                                      Line(2, WhisperSide::kReceived, "Bob")};
  const WhisperDesk::Absorbed absorbed =
      desk.Absorb(chat, FriendSetup{.open_from_stranger = true}, kMe);
  // L'ALARME NE SONNE QU'À LA NAISSANCE, pas à chaque ligne.
  CHECK(absorbed.opened == 1);
  REQUIRE(desk.conversations().size() == 1);
  CHECK(desk.conversations()[0].peer == "Bob");
  CHECK(desk.conversations()[0].unread);
  // Elle ne vole pas le clavier : personne ne l'a demandée.
  CHECK_FALSE(desk.conversations()[0].raise);
  CHECK(desk.ShownApart(1));
  CHECK(desk.ShownApart(2));
}

TEST_CASE("Conversation : ouverte à la main, elle prend ce qui arrive ensuite", "[app][whisper]") {
  WhisperDesk desk;
  std::vector<ChatLine> chat = {Line(1, WhisperSide::kReceived, "Bob")};
  static_cast<void>(desk.Absorb(chat, FriendSetup{}, kMe));
  const Conversation* opened = desk.Open("Bob");
  REQUIRE(opened != nullptr);
  CHECK(opened->raise);
  chat.push_back(Line(2, WhisperSide::kSent, "Bob"));
  chat.push_back(Line(3, WhisperSide::kReceived, "Carol"));
  static_cast<void>(desk.Absorb(chat, FriendSetup{}, kMe));
  // ⚠ CE QUI PRÉCÉDAIT L'OUVERTURE RESTE AU FIL : la prise se fait à l'arrivée.
  CHECK_FALSE(desk.ShownApart(1));
  CHECK(desk.ShownApart(2));
  CHECK_FALSE(desk.ShownApart(3));
}

TEST_CASE("Conversation : le refus reste au fil, et l'écho ouvre aussi", "[app][whisper]") {
  WhisperDesk desk;
  const std::vector<ChatLine> chat = {Line(1, WhisperSide::kUndelivered, "Absent"),
                                      Line(2, WhisperSide::kSent, "Bob")};
  const WhisperDesk::Absorbed absorbed =
      desk.Absorb(chat, FriendSetup{.open_from_stranger = true}, kMe);
  CHECK_FALSE(desk.ShownApart(1));
  CHECK(desk.Find("Absent") == nullptr);
  CHECK(absorbed.opened == 1);
  REQUIRE(desk.Find("Bob") != nullptr);
  // Ce qu'on a dit n'est pas un non-lu.
  CHECK_FALSE(desk.Find("Bob")->unread);
  CHECK(desk.ShownApart(2));
}

TEST_CASE("Conversation : regardée, son non-lu s'efface", "[app][whisper]") {
  WhisperDesk desk;
  const std::vector<ChatLine> chat = {Line(1, WhisperSide::kReceived, "Bob")};
  static_cast<void>(desk.Absorb(chat, FriendSetup{.open_from_stranger = true}, kMe));
  REQUIRE(desk.Find("Bob") != nullptr);
  CHECK(desk.Find("Bob")->unread);
  desk.Seen("Bob");
  CHECK_FALSE(desk.Find("Bob")->unread);
}

TEST_CASE("Conversation : au plafond, la moins active cède", "[app][whisper]") {
  WhisperDesk desk;
  for (std::size_t rank = 0; rank < moonclient::kMostConversations; ++rank) {
    static_cast<void>(desk.Open("Joueur" + std::to_string(rank)));
  }
  // La première ouverte reprend de l'activité : c'est la deuxième qui cédera.
  static_cast<void>(desk.Open("Joueur0"));
  static_cast<void>(desk.Open("Neuf"));
  CHECK(desk.conversations().size() == moonclient::kMostConversations);
  CHECK(desk.Find("Joueur0") != nullptr);
  CHECK(desk.Find("Joueur1") == nullptr);
  CHECK(desk.Find("Neuf") != nullptr);
}

TEST_CASE("Conversation : ce qu'on se chuchote n'ouvre rien", "[app][whisper]") {
  WhisperDesk desk;
  const std::vector<ChatLine> chat = {Line(1, WhisperSide::kReceived, kMe)};
  CHECK(desk.Absorb(chat, FriendSetup{.open_from_stranger = true}, kMe).opened == 0);
  CHECK(desk.conversations().empty());
}

TEST_CASE("Conversation : une partie neuve n'ouvre rien sur l'ancienne", "[app][whisper]") {
  WhisperDesk desk;
  static_cast<void>(desk.Open("Bob"));
  desk.Clear(1);
  CHECK(desk.conversations().empty());
  const std::vector<ChatLine> chat = {Line(1, WhisperSide::kReceived, "Bob")};
  CHECK(desk.Absorb(chat, FriendSetup{.open_from_stranger = true}, kMe).opened == 0);
}

TEST_CASE("Conversation : au plafond, un non-lu ou une phrase en cours ne cède pas",
          "[app][whisper]") {
  WhisperDesk desk;
  // Huit conversations ouvertes par la réception : toutes non lues.
  std::vector<ChatLine> chat;
  for (std::size_t rank = 0; rank < moonclient::kMostConversations; ++rank) {
    chat.push_back(Line(rank + 1, WhisperSide::kReceived, "Joueur" + std::to_string(rank)));
  }
  static_cast<void>(desk.Absorb(chat, FriendSetup{.open_from_stranger = true}, kMe));
  REQUIRE(desk.conversations().size() == moonclient::kMostConversations);

  // UNE NEUVIÈME NE S'OUVRE PAS, et sa ligne reste au fil.
  const std::uint64_t ninth = moonclient::kMostConversations + 1;
  chat.push_back(Line(ninth, WhisperSide::kReceived, "Neuf"));
  CHECK(desk.Absorb(chat, FriendSetup{.open_from_stranger = true}, kMe).opened == 0);
  CHECK(desk.Find("Neuf") == nullptr);
  CHECK_FALSE(desk.ShownApart(ninth));
  CHECK(desk.Open("Neuf") == nullptr);

  // Lue, la plus ancienne peut céder ; une phrase en cours la protège encore.
  desk.Seen("Joueur0");
  desk.NoteDraft("Joueur0", true);
  CHECK(desk.Open("Neuf") == nullptr);
  desk.NoteDraft("Joueur0", false);
  CHECK(desk.Open("Neuf") != nullptr);
  CHECK(desk.Find("Joueur0") == nullptr);
  CHECK(desk.Find("Joueur1") != nullptr);
}
