// Une ligne qui nous nomme.
//
// Ce qui se vérifie ici, ce sont les EXCLUSIONS : un bandeau ou un son sur ce
// qu'on a dit soi-même, sur une ligne du serveur ou sur un coup de combat
// ferait du signal un bruit, et le joueur l'éteindrait. Puis le mot entier :
// « Anakin » ne cite pas « Ana ».

#include "ro/sim/chat_mention.h"

#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::sim::ChatKind;
using ro::sim::ChatLine;
using ro::sim::ChatType;
using ro::sim::IsOwnChatLine;
using ro::sim::MentionRules;
using ro::sim::MentionsMe;
using ro::sim::SplitMentionWords;
using ro::sim::TextMentions;
using ro::sim::WhisperSide;

namespace {

constexpr std::uint32_t kOwnId = 150000;
constexpr std::uint32_t kOtherId = 150001;
constexpr std::string_view kOwnName = "Ana";

[[nodiscard]] ChatLine Line(ChatKind kind, std::string text, std::uint32_t speaker_id = kOtherId) {
  ChatLine line;
  line.kind = kind;
  line.type = ro::sim::DefaultChatType(kind);
  line.speaker_id = speaker_id;
  line.text = std::move(text);
  return line;
}

[[nodiscard]] ChatLine Whisper(WhisperSide side, std::string text) {
  ChatLine line = Line(ChatKind::kWhisper, std::move(text), 0);
  line.whisper.side = side;
  line.whisper.peer_name = "Bob";
  return line;
}

[[nodiscard]] ChatLine Relayed(std::string text) {
  ChatLine line = Line(ChatKind::kPublic, std::move(text), 0);
  line.type = ChatType::kDiscord;
  return line;
}

// ⚠ LES MOTS-CLÉS SONT UNE VUE : le vecteur de l'appelant doit vivre autant
// que les règles.
[[nodiscard]] MentionRules Rules(std::span<const std::string> words = {}) {
  return MentionRules{.own_name = kOwnName, .own_id = kOwnId, .words = words};
}

}  // namespace

TEST_CASE("Mentions : le mot entier, la casse ignorée", "[sim][chat][mention]") {
  CHECK(TextMentions("salut Ana !", "Ana"));
  CHECK(TextMentions("salut ana", "Ana"));
  CHECK(TextMentions("ANA, viens", "ana"));
  CHECK(TextMentions("Ana", "Ana"));
  CHECK(TextMentions("(Ana)", "Ana"));
  CHECK_FALSE(TextMentions("Anakin arrive", "Ana"));
  CHECK_FALSE(TextMentions("Banana", "Ana"));
  CHECK_FALSE(TextMentions("Ana_bis", "Ana"));
  // Un octet non ASCII colle au mot : « Anaé » n'est pas « Ana ».
  CHECK_FALSE(TextMentions("Ana\xC3\xA9", "Ana"));
  CHECK_FALSE(TextMentions("salut", ""));
  CHECK(TextMentions("salut \xC3\x89va", "\xC3\x89va"));
}

TEST_CASE("Mentions : la casse hors ASCII se met de côté aussi", "[sim][chat][mention]") {
  // « ÉLODIE » cite « Élodie », et l'inverse.
  CHECK(TextMentions("salut \xC3\x89LODIE !", "\xC3\x89lodie"));
  CHECK(TextMentions("salut \xC3\xA9lodie", "\xC3\x89LODIE"));
  // « ẞ » majuscule replie en « ß », qui reste « ß » : « STRASSE » ne cite pas
  // « Straße ».
  CHECK(
      TextMentions("STRA\xE1\xBA\x9E"
                   "E",
                   "Stra\xC3\x9F"
                   "e"));
  CHECK_FALSE(TextMentions("STRASSE",
                           "Stra\xC3\x9F"
                           "e"));
  // Cyrillique : « НАТАША » cite « Наташа ».
  CHECK(TextMentions("\xD0\x9D\xD0\x90\xD0\xA2\xD0\x90\xD0\xA8\xD0\x90 !",
                     "\xD0\x9D\xD0\xB0\xD1\x82\xD0\xB0\xD1\x88\xD0\xB0"));
  // Un mot replié reste un mot entier : « ÉLODIEs » ne cite pas « Élodie ».
  CHECK_FALSE(TextMentions("\xC3\x89LODIEs", "\xC3\x89lodie"));
  CHECK_FALSE(TextMentions("\xC3\x89LODIE\xC3\x89", "\xC3\x89lodie"));
  // Un octet invalide ne fait pas tomber la comparaison : il se compare à lui.
  CHECK(TextMentions("salut \xE9va", "\xE9VA"));
  CHECK_FALSE(TextMentions("salut \xE9va", "\xC3\xA9va"));
}

TEST_CASE("Mentions : notre nom accentué, écrit en capitales par un autre",
          "[sim][chat][mention]") {
  const MentionRules rules{.own_name = "\xC3\x89lodie", .own_id = kOwnId, .words = {}};
  CHECK(MentionsMe(Line(ChatKind::kPublic, "Bob : \xC3\x89LODIE, viens"), rules));
  CHECK_FALSE(MentionsMe(Line(ChatKind::kPublic, "Bob : Elodie, viens"), rules));
}

TEST_CASE("Mentions : les mots-clés se tapent séparés par un point-virgule",
          "[sim][chat][mention]") {
  const std::vector<std::string> words = SplitMentionWords(" mvp ; Baphomet;x;;MVP; boss ");
  REQUIRE(words.size() == 3);
  CHECK(words[0] == "mvp");
  CHECK(words[1] == "Baphomet");
  CHECK(words[2] == "boss");
  // Le doublon se reconnaît aussi hors ASCII.
  const std::vector<std::string> accented = SplitMentionWords("\xC3\x89lodie;\xC3\x89LODIE");
  REQUIRE(accented.size() == 1);
  CHECK(accented[0] == "\xC3\x89lodie");
  CHECK(SplitMentionWords("").empty());
  CHECK(SplitMentionWords(" ; a ;").empty());
}

TEST_CASE("Mentions : notre nom dans la parole d'un autre", "[sim][chat][mention]") {
  const MentionRules rules = Rules();
  CHECK(MentionsMe(Line(ChatKind::kPublic, "Bob : salut ana"), rules));
  CHECK(MentionsMe(Line(ChatKind::kParty, "Bob : Ana, heal"), rules));
  CHECK(MentionsMe(Line(ChatKind::kGuild, "Bob : gg Ana"), rules));
  CHECK(MentionsMe(Line(ChatKind::kChannel, "[#trade] Bob : Ana ?"), rules));
  CHECK(MentionsMe(Line(ChatKind::kClan, "Bob : Ana"), rules));
  CHECK(MentionsMe(Line(ChatKind::kBattlefield, "Bob : Ana go"), rules));
  CHECK(MentionsMe(Line(ChatKind::kAnnouncement, "Ana a gagné le tournoi"), rules));
  CHECK(MentionsMe(Whisper(WhisperSide::kReceived, "tu es là Ana ?"), rules));
  CHECK(MentionsMe(Relayed("[#gonryun][Zed] coucou Ana"), rules));
  CHECK_FALSE(MentionsMe(Line(ChatKind::kPublic, "Bob : Anakin arrive"), rules));
}

TEST_CASE("Mentions : un mot-clé allume la ligne comme le nom", "[sim][chat][mention]") {
  const std::vector<std::string> words = {"baphomet", "mvp"};
  const MentionRules rules = Rules(words);
  CHECK(MentionsMe(Line(ChatKind::kPublic, "Bob : Baphomet est là"), rules));
  CHECK(MentionsMe(Line(ChatKind::kGuild, "Bob : MVP up"), rules));
  CHECK_FALSE(MentionsMe(Line(ChatKind::kPublic, "Bob : mvpcard"), rules));
}

TEST_CASE("Mentions : l'auteur ne se mentionne pas", "[sim][chat][mention]") {
  // Un pseudo pris pour mot-clé ne doit pas allumer chaque ligne de ce joueur,
  // et « Ana Bis » qui parle ne cite pas « Ana ».
  const std::vector<std::string> words = {"bob"};
  CHECK_FALSE(MentionsMe(Line(ChatKind::kPublic, "Bob : salut"), Rules(words)));
  CHECK_FALSE(MentionsMe(Line(ChatKind::kPublic, "Ana Bis : salut"), Rules()));
  CHECK_FALSE(MentionsMe(Relayed("[#gonryun][bob] salut"), Rules(words)));
}

TEST_CASE("Mentions : ce qu'on a dit soi-même ne nous mentionne jamais", "[sim][chat][mention]") {
  const MentionRules rules = Rules();
  // L'écho de sa propre voix.
  const ChatLine self = Line(ChatKind::kSelf, "Ana : je suis Ana", kOwnId);
  CHECK(IsOwnChatLine(self, kOwnName, kOwnId));
  CHECK_FALSE(MentionsMe(self, rules));
  // Un chuchotement envoyé, ou qui n'a pas été remis.
  const ChatLine sent = Whisper(WhisperSide::kSent, "c'est Ana");
  CHECK(IsOwnChatLine(sent, kOwnName, kOwnId));
  CHECK_FALSE(MentionsMe(sent, rules));
  const ChatLine undelivered = Whisper(WhisperSide::kUndelivered, "c'est Ana");
  CHECK(IsOwnChatLine(undelivered, kOwnName, kOwnId));
  CHECK_FALSE(MentionsMe(undelivered, rules));
  // Notre identifiant comme orateur.
  const ChatLine spoken = Line(ChatKind::kParty, "x : Ana", kOwnId);
  CHECK(IsOwnChatLine(spoken, kOwnName, kOwnId));
  CHECK_FALSE(MentionsMe(spoken, rules));
  // Notre nom en tête de ligne.
  const ChatLine headed = Line(ChatKind::kGuild, "Ana : moi Ana", 0);
  CHECK(IsOwnChatLine(headed, kOwnName, kOwnId));
  CHECK_FALSE(MentionsMe(headed, rules));
  // Notre pseudo comme auteur d'une ligne relayée de Discord.
  const ChatLine relayed = Relayed("[#gonryun][Ana] Ana ici");
  CHECK(IsOwnChatLine(relayed, kOwnName, kOwnId));
  CHECK_FALSE(MentionsMe(relayed, rules));
  // ⚠ Zéro n'est personne : un orateur inconnu n'est pas nous.
  CHECK_FALSE(IsOwnChatLine(Line(ChatKind::kPublic, "Bob : Ana", 0), kOwnName, 0));
  // Le crochet d'auteur ne vaut que pour Discord.
  CHECK_FALSE(IsOwnChatLine(Line(ChatKind::kPublic, "[#x][Ana] y"), kOwnName, kOwnId));
}

TEST_CASE("Mentions : les lignes qui ne parlent pas n'en déclenchent jamais",
          "[sim][chat][mention]") {
  const MentionRules rules = Rules();
  CHECK_FALSE(MentionsMe(Line(ChatKind::kServer, "Ana est connectée", 0), rules));
  CHECK_FALSE(MentionsMe(Line(ChatKind::kItem, "Ana a obtenu une potion", 0), rules));
  CHECK_FALSE(MentionsMe(Line(ChatKind::kBattle, "Ana subit 120 dégâts", 0), rules));
  CHECK_FALSE(MentionsMe(Line(ChatKind::kPartyBattle, "Ana subit 120 dégâts", 0), rules));
  CHECK_FALSE(MentionsMe(Line(ChatKind::kExp, "Ana gagne 300 exp", 0), rules));
  CHECK_FALSE(MentionsMe(Line(ChatKind::kCall, "Ana a refusé l'appel", 0), rules));
}

TEST_CASE("Mentions : une ligne de canal se lit derrière son étiquette", "[sim][chat][mention]") {
  // Le serveur n'y met pas d'orateur : c'est « Nom : » derrière « [#canal] »
  // qui dit qui parle.
  const std::vector<std::string> mvp = {"mvp"};
  const ChatLine own = Line(ChatKind::kChannel, "[#main] Ana : mvp", 0);
  CHECK(IsOwnChatLine(own, kOwnName, kOwnId));
  CHECK_FALSE(MentionsMe(own, Rules(mvp)));

  CHECK(MentionsMe(Line(ChatKind::kChannel, "[#main] Bob : Ana ?", 0), Rules()));

  const std::vector<std::string> bob = {"bob"};
  CHECK_FALSE(MentionsMe(Line(ChatKind::kChannel, "[#main] Bob : salut", 0), Rules(bob)));
  CHECK_FALSE(MentionsMe(Line(ChatKind::kChannel, "#main Bob : salut", 0), Rules(bob)));
}

TEST_CASE("Mentions : le pseudo d'une ligne relayée de Discord", "[sim][chat][mention]") {
  CHECK(ro::sim::DiscordRelayAuthorName("[#gonryun][Ana] hi") == "Ana");
  CHECK(ro::sim::DiscordRelayAuthorName("[Discord][X] y") == "X");
  CHECK(ro::sim::DiscordRelayAuthorName("[a][] c").empty());
  CHECK(ro::sim::DiscordRelayAuthorName("[a] [b] c").empty());
  CHECK(ro::sim::DiscordRelayAuthorName("Ana : hi").empty());
}
