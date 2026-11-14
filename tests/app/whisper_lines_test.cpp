#include "chat/whisper_lines.h"

#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/i18n.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/whisper.h"
#include "ro/vfs/item_words.h"
#include "ro/vfs/whisper_words.h"

#include "chat/chat_mvp_link.h"

// LES QUATRE EN-TÊTES DU FIL, la forme de la conversation, et le pseudo
// cliquable dans chacun.

using moonclient::ComposeWhisperHead;
using moonclient::WhisperHead;
using moonclient::WhisperSurface;
using ro::sim::WhisperFacts;
using ro::sim::WhisperRelation;
using ro::sim::WhisperSide;

namespace {

[[nodiscard]] WhisperFacts Facts(WhisperSide side, WhisperRelation relation,
                                 std::string_view peer = "Bob") {
  return WhisperFacts{.side = side, .peer_name = std::string(peer), .relation = relation};
}

// Le pseudo que l'en-tête rend cliquable.
[[nodiscard]] std::string_view LinkedName(const WhisperHead& head) {
  return std::string_view(head.text).substr(head.name_offset, head.name_length);
}

}  // namespace

TEST_CASE("En-tête : une ligne reçue dit qui, selon la relation", "[app][whisper]") {
  const WhisperHead friend_head = ComposeWhisperHead(
      Facts(WhisperSide::kReceived, WhisperRelation::kFriend), WhisperSurface::kLog, "Moi");
  CHECK(friend_head.text == "[ Friend Bob ] : ");
  CHECK(LinkedName(friend_head) == "Bob");
  CHECK(ComposeWhisperHead(Facts(WhisperSide::kReceived, WhisperRelation::kGuildMember),
                           WhisperSurface::kLog, "Moi")
            .text == "[ Member Bob ] : ");
  CHECK(ComposeWhisperHead(Facts(WhisperSide::kReceived, WhisperRelation::kStaff),
                           WhisperSurface::kLog, "Moi")
            .text == "[ Bob ] : ");
  const WhisperHead stranger = ComposeWhisperHead(
      Facts(WhisperSide::kReceived, WhisperRelation::kStranger), WhisperSurface::kLog, "Moi");
  CHECK(stranger.text == "( From Bob ) : ");
  CHECK(LinkedName(stranger) == "Bob");
}

TEST_CASE("En-tête : l'écho de notre envoi, crochets pour les proches", "[app][whisper]") {
  const WhisperHead plain = ComposeWhisperHead(
      Facts(WhisperSide::kSent, WhisperRelation::kStranger), WhisperSurface::kLog, "Moi");
  CHECK(plain.text == "( To Bob ) : ");
  CHECK(LinkedName(plain) == "Bob");
  CHECK(ComposeWhisperHead(Facts(WhisperSide::kSent, WhisperRelation::kFriend),
                           WhisperSurface::kLog, "Moi")
            .text == "[ To Bob ] : ");
  CHECK(ComposeWhisperHead(Facts(WhisperSide::kSent, WhisperRelation::kGuildMember),
                           WhisperSurface::kLog, "Moi")
            .text == "[ To Bob ] : ");
}

TEST_CASE("En-tête : la conversation écrit qui parle, sans sens", "[app][whisper]") {
  const WhisperHead received =
      ComposeWhisperHead(Facts(WhisperSide::kReceived, WhisperRelation::kFriend),
                         WhisperSurface::kConversation, "Moi");
  CHECK(received.text == "Bob : ");
  CHECK(LinkedName(received) == "Bob");
  // ⚠ NOTRE NOM N'EST PAS UN LIEN.
  const WhisperHead sent = ComposeWhisperHead(Facts(WhisperSide::kSent, WhisperRelation::kFriend),
                                              WhisperSurface::kConversation, "Moi");
  CHECK(sent.text == "Moi : ");
  CHECK(sent.name_length == 0);
}

TEST_CASE("En-tête : un refus nomme qui on n'a pas atteint", "[app][whisper]") {
  CHECK(ComposeWhisperHead(Facts(WhisperSide::kUndelivered, WhisperRelation::kStranger),
                           WhisperSurface::kLog, "Moi")
            .text == "(Bob) ");
  // Sans envoi retrouvé, rien à nommer.
  CHECK(ComposeWhisperHead(Facts(WhisperSide::kUndelivered, WhisperRelation::kStranger, ""),
                           WhisperSurface::kLog, "Moi")
            .text.empty());
}

TEST_CASE("En-tête : ce qu'on se chuchote ne se clique pas", "[app][whisper]") {
  const WhisperHead head =
      ComposeWhisperHead(Facts(WhisperSide::kReceived, WhisperRelation::kStranger, "Moi"),
                         WhisperSurface::kLog, "Moi");
  CHECK(head.text == "( From Moi ) : ");
  CHECK(head.name_length == 0);
}

TEST_CASE("En-tête : le journal garde le chuchotement lisible", "[app][whisper]") {
  ro::sim::ChatLine line;
  line.kind = ro::sim::ChatKind::kWhisper;
  line.whisper = Facts(WhisperSide::kReceived, WhisperRelation::kStranger);
  line.text = "salut";
  CHECK(moonclient::WhisperLogText(line, nullptr) == "( From Bob ) : salut");
  ro::sim::ChatLine plain;
  plain.text = "Alice : bonjour";
  CHECK(moonclient::WhisperLogText(plain, nullptr) == "Alice : bonjour");
}

TEST_CASE("En-tête : le journal relu rend le sens et le pair", "[app][whisper]") {
  // Toutes les formes du fil, lues comme `WhisperLogText` les a écrites : le
  // pair d'un `<MVPL>` relu doit être celui qu'on aurait vu en direct.
  for (const WhisperSide side : {WhisperSide::kReceived, WhisperSide::kSent}) {
    for (const WhisperRelation relation : {WhisperRelation::kFriend, WhisperRelation::kGuildMember,
                                           WhisperRelation::kStaff, WhisperRelation::kStranger}) {
      ro::sim::ChatLine line;
      line.kind = ro::sim::ChatKind::kWhisper;
      line.whisper = Facts(side, relation, "Jean Pierre");
      line.text = "<MVPL>1039:gef_dun03:1:0:60:10:5:6:Baphomet</MVPL> : vu";
      const std::string logged = moonclient::WhisperLogText(line, nullptr);
      INFO(logged);
      const WhisperFacts read = moonclient::WhisperFactsOfLogText(logged);
      CHECK(read.side == side);
      CHECK(read.peer_name == "Jean Pierre");
    }
  }
  // UN REFUS n'a rien dit ; une parole ordinaire n'a pas d'en-tête.
  CHECK(moonclient::WhisperFactsOfLogText("(Bob) Ce chuchotement n'est pas parvenu.").side ==
        WhisperSide::kNone);
  CHECK(moonclient::WhisperFactsOfLogText("Alice : bonjour").side == WhisperSide::kNone);
  // Un en-tête sans nom ni fermeture n'en est pas un.
  CHECK(moonclient::WhisperFactsOfLogText("( From  ) : x").side == WhisperSide::kNone);
  CHECK(moonclient::WhisperFactsOfLogText("[ Friend Bob").side == WhisperSide::kNone);
}

TEST_CASE("En-tête : l'écho relu d'un envoi n'a pas d'auteur connu", "[app][whisper]") {
  // 🔴 LE JOURNAL EST CELUI DE L'INSTALLATION : l'écho relu a pu être écrit par
  // un autre personnage que celui en jeu.
  for (const WhisperRelation relation : {WhisperRelation::kFriend, WhisperRelation::kGuildMember,
                                         WhisperRelation::kStaff, WhisperRelation::kStranger}) {
    ro::sim::ChatLine line;
    line.kind = ro::sim::ChatKind::kWhisper;
    line.text = "<MVPL>1039:gef_dun03:1:0:60:10:5:6:Baphomet</MVPL>";
    line.whisper = Facts(WhisperSide::kSent, relation, "Destinataire");
    const std::string sent = moonclient::WhisperLogText(line, nullptr);
    INFO(sent);
    REQUIRE(moonclient::WhisperFactsOfLogText(sent).side == WhisperSide::kSent);
    CHECK(moonclient::MvpPastWhisperSenderOf(moonclient::WhisperFactsOfLogText(sent)).empty());
    // REÇU, l'auteur reste le pair, relu comme en direct.
    line.whisper = Facts(WhisperSide::kReceived, relation, "Jean Pierre");
    const std::string received = moonclient::WhisperLogText(line, nullptr);
    CHECK(moonclient::MvpPastWhisperSenderOf(moonclient::WhisperFactsOfLogText(received)) ==
          "Jean Pierre");
  }
  // UN ENVOI NON REMIS non plus.
  CHECK(moonclient::MvpPastWhisperSenderOf(
            WhisperFacts{.side = WhisperSide::kUndelivered, .peer_name = "Bob"})
            .empty());
}

TEST_CASE("En-tête : un refus s'écrit avec la phrase du client", "[app][whisper]") {
  ro::sim::ChatLine refused;
  refused.kind = ro::sim::ChatKind::kWhisper;
  refused.whisper = Facts(WhisperSide::kUndelivered, WhisperRelation::kStranger);
  refused.whisper.refusal = ro::sim::WhisperRefusal::kOffline;
  ro::vfs::ItemWords words;
  ro::vfs::FillWhisperWords(words.whisper);
  // LA PHRASE QUE LE CLIENT ÉCRIT POUR CE CODE, à l'écran comme au journal.
  const std::string_view offline =
      ro::core::Tr("Ce nom de personnage n'existe pas ou le joueur est hors ligne.");
  CHECK(moonclient::WhisperSaid(refused, &words) == offline);
  CHECK(moonclient::WhisperLogText(refused, &words) == "(Bob) " + std::string(offline));
  refused.whisper.refusal = ro::sim::WhisperRefusal::kIgnoresAll;
  CHECK(moonclient::WhisperSaid(refused, &words) ==
        std::string_view(ro::core::Tr("n'a envie de parler à personne.")));
  // UN CODE INCONNU, ou des mots pas encore posés, se disent quand même.
  const std::string_view general = ro::core::Tr("Ce chuchotement n'est pas parvenu.");
  refused.whisper.refusal = static_cast<ro::sim::WhisperRefusal>(9);
  CHECK(moonclient::WhisperSaid(refused, &words) == general);
  refused.whisper.refusal = ro::sim::WhisperRefusal::kOffline;
  CHECK(moonclient::WhisperSaid(refused, nullptr) == general);
  const ro::vfs::ItemWords unfilled;
  CHECK(moonclient::WhisperSaid(refused, &unfilled) == general);
}

TEST_CASE("Bulle : ce que l'équipe chuchote porte l'en-tête du fil", "[app][whisper]") {
  ro::sim::ChatLine line;
  line.text = "Bonjour à tous.";
  line.whisper = Facts(WhisperSide::kReceived, WhisperRelation::kStaff, "GM-Luna");
  line.whisper.from_staff = true;
  CHECK(moonclient::StaffBalloonText(line, /*shown_apart=*/false) ==
        "[ GM-Luna ] : Bonjour à tous.");
  // UN AMI DE L'ÉQUIPE garde son en-tête d'ami dans le fil, et dans sa bulle.
  line.whisper.relation = WhisperRelation::kFriend;
  CHECK(moonclient::StaffBalloonText(line, false) == "[ Friend GM-Luna ] : Bonjour à tous.");
  // PRISE PAR UNE CONVERSATION 1:1, la ligne n'a pas d'en-tête de fil : la
  // bulle prend la forme de l'équipe, même pour un ami.
  CHECK(moonclient::StaffBalloonText(line, /*shown_apart=*/true) ==
        "[ GM-Luna ] : Bonjour à tous.");
}
