// LES DERNIERS CORRESPONDANTS DE LA BOX « PSEUDO » : les deux sens, l'ordre, la
// borne, et ce qui n'y entre pas.

#include "chat/recent_whispers.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/chat_line.h"
#include "ro/sim/whisper.h"
#include "ro/ui/chat_whisper_list.h"

namespace {

using moonclient::RecentWhispers;
using moonclient::WhisperPeer;
using ro::sim::WhisperSide;
using ro::ui::ChatWhisperWay;

constexpr std::string_view kMe = "Moi";
// Un identifiant de personnage connecté : non nul.
constexpr std::uint32_t kSomeone = 2000001;

[[nodiscard]] ro::sim::ChatLine Line(std::uint64_t seq, WhisperSide side, std::string_view peer,
                                     std::uint32_t sender_id = kSomeone) {
  ro::sim::ChatLine line;
  line.seq = seq;
  line.kind = ro::sim::ChatKind::kWhisper;
  line.whisper.side = side;
  line.whisper.peer_name = std::string(peer);
  line.whisper.sender_id = side == WhisperSide::kReceived ? sender_id : 0;
  return line;
}

// Les noms montrés, dans l'ordre.
[[nodiscard]] std::vector<std::string> NamesOf(RecentWhispers& recent) {
  std::vector<std::string> names;
  for (const ro::ui::ChatRecentWhisper& entry : recent.Views()) {
    names.emplace_back(entry.name);
  }
  return names;
}

}  // namespace

TEST_CASE("Récents : le plus récent en tête, un nom qui revient remonte", "[app][chat]") {
  RecentWhispers recent;
  recent.Note("Alice", ChatWhisperWay::kOutgoing);
  recent.Note("Bob", ChatWhisperWay::kIncoming);
  recent.Note("Alice", ChatWhisperWay::kIncoming);
  CHECK(NamesOf(recent) == std::vector<std::string>{"Alice", "Bob"});
}

TEST_CASE("Récents : le sens du dernier échange l'emporte", "[app][chat]") {
  RecentWhispers recent;
  recent.Note("Alice", ChatWhisperWay::kOutgoing);
  REQUIRE(recent.Views()[0].way == ChatWhisperWay::kOutgoing);
  recent.Note("Alice", ChatWhisperWay::kIncoming);
  REQUIRE(recent.Views().size() == 1);
  CHECK(recent.Views()[0].way == ChatWhisperWay::kIncoming);
  recent.Note("Alice", ChatWhisperWay::kOutgoing);
  CHECK(recent.Views()[0].way == ChatWhisperWay::kOutgoing);
}

TEST_CASE("Récents : la comparaison est exacte, casse comprise", "[app][chat]") {
  // Deux personnages peuvent ne différer que par la casse.
  RecentWhispers recent;
  recent.Note("alice", ChatWhisperWay::kOutgoing);
  recent.Note("Alice", ChatWhisperWay::kOutgoing);
  CHECK(recent.Views().size() == 2);
}

TEST_CASE("Récents : dix noms au plus, le plus ancien s'en va", "[app][chat]") {
  RecentWhispers recent;
  constexpr int kOneTooMany = ro::ui::kChatWhisperNamesKept + 1;
  for (int rank = 0; rank < kOneTooMany; ++rank) {
    recent.Note("Nom" + std::to_string(rank), ChatWhisperWay::kOutgoing);
  }
  const std::vector<std::string> names = NamesOf(recent);
  REQUIRE(names.size() == static_cast<std::size_t>(ro::ui::kChatWhisperNamesKept));
  CHECK(ro::ui::kChatWhisperNamesKept == 10);
  CHECK(names.front() == "Nom10");
  CHECK(names.back() == "Nom1");
}

TEST_CASE("Récents : un chuchotement reçu d'un joueur entre, en sens entrant", "[app][chat]") {
  RecentWhispers recent;
  recent.Note("Alice", ChatWhisperWay::kOutgoing);
  const std::vector<ro::sim::ChatLine> chat = {Line(1, WhisperSide::kReceived, "Bob")};
  recent.FollowChat(chat, kMe);
  REQUIRE(recent.Views().size() == 2);
  CHECK(recent.Views()[0].name == "Bob");
  CHECK(recent.Views()[0].way == ChatWhisperWay::kIncoming);
}

TEST_CASE("Récents : la voix du serveur n'entre pas, quel que soit son nom", "[app][chat]") {
  // ⚠ UN IDENTIFIANT NUL : le serveur parle sous un nom de sa configuration,
  // qu'on ne peut pas reconnaître, mais jamais sous celui d'un connecté.
  RecentWhispers recent;
  ro::sim::ChatLine announce = Line(1, WhisperSide::kReceived, "Server", 0);
  ro::sim::ChatLine plain;
  plain.seq = 2;
  plain.kind = ro::sim::ChatKind::kServer;
  plain.text = "Bob vient de se connecter.";
  const std::vector<ro::sim::ChatLine> chat = {announce, plain};
  recent.FollowChat(chat, kMe);
  CHECK(recent.Views().empty());
}

TEST_CASE("Récents : ni soi-même, ni un envoi refusé ; l'écho d'un envoi compte", "[app][chat]") {
  RecentWhispers recent;
  const std::vector<ro::sim::ChatLine> chat = {Line(1, WhisperSide::kReceived, kMe),
                                               Line(2, WhisperSide::kUndelivered, "Absent"),
                                               Line(3, WhisperSide::kSent, "Carol")};
  recent.FollowChat(chat, kMe);
  REQUIRE(recent.Views().size() == 1);
  CHECK(recent.Views()[0].name == "Carol");
  CHECK(recent.Views()[0].way == ChatWhisperWay::kOutgoing);
}

TEST_CASE("Récents : une ligne déjà lue ne se relit pas", "[app][chat]") {
  // ⚠ SANS LA REPRISE AU RANG, chaque image ferait remonter le dernier qui a
  // chuchoté au-dessus de celui à qui l'on vient de répondre.
  RecentWhispers recent;
  std::vector<ro::sim::ChatLine> chat = {Line(1, WhisperSide::kReceived, "Bob")};
  recent.FollowChat(chat, kMe);
  recent.Note("Alice", ChatWhisperWay::kOutgoing);
  recent.FollowChat(chat, kMe);
  CHECK(NamesOf(recent) == std::vector<std::string>{"Alice", "Bob"});
  // Et ce qui précède le début de la partie n'est pas neuf.
  RecentWhispers fresh;
  fresh.StartSession(1);
  fresh.FollowChat(chat, kMe);
  CHECK(fresh.Views().empty());
}

TEST_CASE("Récents : la relecture garde le sens, sans doublon ni excès", "[app][chat]") {
  std::vector<WhisperPeer> saved = {WhisperPeer{.name = "Bob", .incoming = true},
                                    WhisperPeer{.name = "Alice"}, WhisperPeer{.name = "Bob"}};
  for (int rank = 0; rank < ro::ui::kChatWhisperNamesKept; ++rank) {
    saved.push_back(WhisperPeer{.name = "Nom" + std::to_string(rank)});
  }
  RecentWhispers recent;
  recent.Restore(saved);
  const std::vector<WhisperPeer> kept = recent.saved();
  REQUIRE(kept.size() == static_cast<std::size_t>(ro::ui::kChatWhisperNamesKept));
  CHECK(kept[0] == WhisperPeer{.name = "Bob", .incoming = true});
  CHECK(kept[1] == WhisperPeer{.name = "Alice"});
  CHECK(kept[2].name == "Nom0");
  CHECK(recent.Views()[0].way == ChatWhisperWay::kIncoming);
  recent.Forget();
  CHECK(recent.Views().empty());
}
