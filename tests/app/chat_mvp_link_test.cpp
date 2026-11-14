// LE LIEN D'UN RESPAWN DE MVP, `<MVPL>` : balise, lecture, libellés, échéance
// et expéditeur.
//
// 🔴 LE LIBELLÉ EST LE DERNIER CHAMP, et c'est ce qui le laisse porter des « : ».
// Les heures du libellé sont celles du LECTEUR : l'essai les compose par la même
// conversion, et ne dépend donc pas du fuseau de la machine qui le joue.

#include "chat/chat_mvp_link.h"

#include <cstdint>
#include <limits>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/mvp_tracker.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/mvp_tracker.h"
#include "ro/sim/whisper.h"

using moonclient::BuildMvpLink;
using moonclient::kMvpLawlessLinkLifetimeS;
using moonclient::MvpClockText;
using moonclient::MvpLink;
using moonclient::MvpLinkLabel;
using moonclient::MvpLinkPassed;
using moonclient::MvpLinkRead;
using moonclient::MvpRoomSenderOf;
using moonclient::MvpSenderOf;
using moonclient::MvpWhisperSenderOf;
using moonclient::ParseMvpLink;

namespace {

constexpr std::uint32_t kBaphomet = 1039;
// Une heure de mort quelconque, en secondes UNIX.
constexpr std::int64_t kKillS = 1'790'000'000;
constexpr std::int64_t kMinuteS = 60;

[[nodiscard]] MvpLink Baphomet() {
  return MvpLink{.monster_id = kBaphomet,
                 .map = "prt_maze03",
                 .kill_time_s = kKillS,
                 .exact_respawn_s = 0,
                 .delay1_min = 120,
                 .delay2_min = 10,
                 .tomb_x = 101,
                 .tomb_y = 47,
                 .name = "Baphomet"};
}

[[nodiscard]] MvpLinkRead Read(const std::string& text, MvpLink* out = nullptr) {
  std::size_t after = 0;
  return ParseMvpLink(text, out, &after);
}

}  // namespace

TEST_CASE("Lien de MVP : la balise fait l'aller-retour", "[app][mvp]") {
  MvpLink link = Baphomet();
  link.name = "Baphomet: le vrai";
  const std::string wire = BuildMvpLink(link);
  CHECK(wire == "<MVPL>1039:prt_maze03:1790000000:0:120:10:101:47:Baphomet: le vrai</MVPL>");
  MvpLink read;
  std::size_t after = 0;
  std::string_view body;
  // ⚠ LE CORPS POINTE DANS LE TEXTE LU : il doit lui survivre.
  const std::string line = wire + " suite";
  REQUIRE(ParseMvpLink(line, &read, &after, &body) == MvpLinkRead::kRead);
  CHECK(after == wire.size());
  CHECK(body == "1039:prt_maze03:1790000000:0:120:10:101:47:Baphomet: le vrai");
  CHECK(read.monster_id == kBaphomet);
  CHECK(read.map == "prt_maze03");
  CHECK(read.kill_time_s == kKillS);
  CHECK(read.exact_respawn_s == 0);
  CHECK(read.delay1_min == 120);
  CHECK(read.delay2_min == 10);
  CHECK(read.tomb_x == 101);
  CHECK(read.tomb_y == 47);
  CHECK(read.name == "Baphomet: le vrai");
  CHECK(moonclient::FindMvpLink("a <MVPL>x", 0) == 2);
  CHECK(moonclient::FindMvpLink("rien", 0) == std::string_view::npos);
}

TEST_CASE("Lien de MVP : la tombe inconnue voyage en -1", "[app][mvp]") {
  MvpLink link = Baphomet();
  link.tomb_x = -1;
  link.tomb_y = -1;
  MvpLink read;
  REQUIRE(Read(BuildMvpLink(link), &read) == MvpLinkRead::kRead);
  CHECK(read.tomb_x == -1);
  CHECK(read.tomb_y == -1);
}

TEST_CASE("Lien de MVP : une balise qui n'apprend rien ne se lit pas", "[app][mvp]") {
  CHECK(Read("pas de balise") == MvpLinkRead::kNoTag);
  // UN CHAMP MANQUE : sept deux-points seulement.
  CHECK(Read("<MVPL>1039:prt_maze03:1790000000:0:120:10:101:47</MVPL>") ==
        MvpLinkRead::kUnreadable);
  // Carte vide, libellé vide.
  CHECK(Read("<MVPL>1039::1790000000:0:120:10:101:47:Baphomet</MVPL>") == MvpLinkRead::kUnreadable);
  CHECK(Read("<MVPL>1039:prt_maze03:1790000000:0:120:10:101:47:</MVPL>") ==
        MvpLinkRead::kUnreadable);
  // NI MORT NI INSTANT.
  CHECK(Read("<MVPL>1039:prt_maze03:0:0:120:10:101:47:Baphomet</MVPL>") ==
        MvpLinkRead::kUnreadable);
  CHECK(Read("<MVPL>1039:prt_maze03:-5:0:120:10:101:47:Baphomet</MVPL>") ==
        MvpLinkRead::kUnreadable);
  // L'INSTANT SEUL suffit.
  CHECK(Read("<MVPL>1039:prt_maze03:0:1790000000:0:0:-1:-1:Baphomet</MVPL>") == MvpLinkRead::kRead);
  // PAS DE FERMANTE : rien ne se lit.
  const std::string unclosed = "<MVPL>1039:prt_maze03:1790000000";
  std::size_t after = 0;
  CHECK(ParseMvpLink(unclosed, nullptr, &after) == MvpLinkRead::kUnclosed);
  CHECK(after == unclosed.size());
}

TEST_CASE("Lien de MVP : les champs se lisent comme atoi", "[app][mvp]") {
  MvpLink read;
  REQUIRE(Read("<MVPL> +1039x:prt_maze03: 1790000000:0:120abc::7:-3:Baphomet</MVPL>", &read) ==
          MvpLinkRead::kRead);
  CHECK(read.monster_id == kBaphomet);
  CHECK(read.kill_time_s == kKillS);
  CHECK(read.delay1_min == 120);
  CHECK(read.delay2_min == 0);
  CHECK(read.tomb_x == 7);
  CHECK(read.tomb_y == -3);
}

TEST_CASE("Lien de MVP : un seul signe, et la saturation d'atoll et d'atoi", "[app][mvp]") {
  MvpLink read;
  // DEUX SIGNES : aucun chiffre lu, donc zéro.
  REQUIRE(Read("<MVPL>+-1039:prt_maze03:1790000000:0:+-5:-7:-+3:-3:Baphomet</MVPL>", &read) ==
          MvpLinkRead::kRead);
  CHECK(read.monster_id == 0);
  CHECK(read.delay1_min == 0);
  // -7, sur les seize bits du champ.
  CHECK(read.delay2_min == 65529);
  CHECK(read.tomb_x == 0);
  CHECK(read.tomb_y == -3);
  // UN MONSTRE AU-DELÀ DE 32 BITS sature à la borne d'atoi, et reste non nul.
  REQUIRE(Read("<MVPL>4294967296:prt_maze03:1790000000:0:0:0:-1:-1:X</MVPL>", &read) ==
          MvpLinkRead::kRead);
  CHECK(read.monster_id == static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()));
  // Un délai de vingt chiffres sature lui aussi avant sa conversion.
  REQUIRE(Read("<MVPL>1039:prt_maze03:1790000000:0:99999999999999999999:-99999999999999999999:"
               "-1:-1:X</MVPL>",
               &read) == MvpLinkRead::kRead);
  // Les seize bits de poids faible des bornes de 32 bits.
  CHECK(read.delay1_min == std::numeric_limits<std::uint16_t>::max());
  CHECK(read.delay2_min == 0);
}

TEST_CASE("Lien de MVP : une heure hors du fil se borne à la lecture", "[app][mvp]") {
  MvpLink read;
  // 🔴 LA PLUS GRANDE HEURE QU'UN JOUEUR PUISSE ÉCRIRE, et au-delà : la borne
  // du fil, sur laquelle libellé et échéance ajoutent encore leurs délais.
  REQUIRE(Read("<MVPL>1038:prt_maze03:9223372036854775807:0:65535:65535:-1:-1:Osiris</MVPL>",
               &read) == MvpLinkRead::kRead);
  CHECK(read.kill_time_s == ro::net::kMvpLatestWireSeconds);
  CHECK_FALSE(MvpLinkLabel(read).empty());
  CHECK_FALSE(MvpLinkPassed(read, kKillS));
  REQUIRE(Read("<MVPL>1038:prt_maze03:0:99999999999999999999999:0:0:-1:-1:Osiris</MVPL>", &read) ==
          MvpLinkRead::kRead);
  CHECK(read.exact_respawn_s == ro::net::kMvpLatestWireSeconds);
  // UNE HEURE NÉGATIVE se lit inconnue : sans autre heure, rien à lire.
  CHECK(Read("<MVPL>1038:prt_maze03:-9223372036854775808:-1:60:10:-1:-1:Osiris</MVPL>") ==
        MvpLinkRead::kUnreadable);
}

TEST_CASE("Lien de MVP : un chevron dans le corps ne donne aucune balise", "[app][mvp]") {
  MvpLink link = Baphomet();
  link.name = "Bapho<met";
  CHECK(BuildMvpLink(link).empty());
  link.name = "Bapho>met";
  CHECK(BuildMvpLink(link).empty());
  link = Baphomet();
  link.map = "prt<maze";
  CHECK(BuildMvpLink(link).empty());
  link = Baphomet();
  link.map.clear();
  CHECK(BuildMvpLink(link).empty());
  link = Baphomet();
  link.name.clear();
  CHECK(BuildMvpLink(link).empty());
}

TEST_CASE("Lien de MVP : les quatre formes du libellé", "[app][mvp]") {
  MvpLink link = Baphomet();
  // Tombe inconnue : le libellé ne la cite pas.
  link.tomb_x = ro::sim::kMvpNoTomb;
  link.tomb_y = ro::sim::kMvpNoTomb;
  const std::string from = MvpClockText(kKillS + 120 * kMinuteS);
  const std::string to = MvpClockText(kKillS + 130 * kMinuteS);
  REQUIRE_FALSE(from.empty());
  REQUIRE(from.size() == 5);
  // UNE FENÊTRE : la mort plus la loi.
  CHECK(MvpLinkLabel(link) == "[MVP: Baphomet (prt_maze03) — retour " + from + "–" + to + "]");
  // UN INSTANT EXACT passe devant.
  link.exact_respawn_s = kKillS + 125 * kMinuteS;
  CHECK(MvpLinkLabel(link) ==
        "[MVP: Baphomet (prt_maze03) — retour à " + MvpClockText(link.exact_respawn_s) + "]");
  // LA MORT SEULE, sans loi.
  link.exact_respawn_s = 0;
  link.delay1_min = 0;
  CHECK(MvpLinkLabel(link) == "[MVP: Baphomet (prt_maze03) — mort à " + MvpClockText(kKillS) + "]");
  // RIEN DE DATÉ, et des noms manquants.
  link.kill_time_s = 0;
  link.name.clear();
  link.map.clear();
  CHECK(MvpLinkLabel(link) == "[MVP: ? (?)]");
  CHECK(MvpClockText(0).empty());
  CHECK(MvpClockText(-1).empty());
}

TEST_CASE("Lien de MVP : une tombe connue se cite avant le crochet final", "[app][mvp]") {
  MvpLink link = Baphomet();
  const std::string tomb = ", tombe en 101,47]";
  // LES QUATRE FORMES la citent, chacune juste avant son crochet.
  const std::string from = MvpClockText(kKillS + 120 * kMinuteS);
  const std::string to = MvpClockText(kKillS + 130 * kMinuteS);
  CHECK(MvpLinkLabel(link) == "[MVP: Baphomet (prt_maze03) — retour " + from + "–" + to + tomb);
  link.exact_respawn_s = kKillS + 125 * kMinuteS;
  CHECK(MvpLinkLabel(link) ==
        "[MVP: Baphomet (prt_maze03) — retour à " + MvpClockText(link.exact_respawn_s) + tomb);
  link.exact_respawn_s = 0;
  link.delay1_min = 0;
  CHECK(MvpLinkLabel(link) ==
        "[MVP: Baphomet (prt_maze03) — mort à " + MvpClockText(kKillS) + tomb);
  link.kill_time_s = 0;
  CHECK(MvpLinkLabel(link) == "[MVP: Baphomet (prt_maze03)" + tomb);
  // LA CELLULE (0, 0) EXISTE : elle se cite.
  link.tomb_x = 0;
  link.tomb_y = 0;
  CHECK(MvpLinkLabel(link) == "[MVP: Baphomet (prt_maze03), tombe en 0,0]");
  // UNE SEULE COORDONNÉE INCONNUE suffit à taire la tombe.
  link.tomb_x = 150;
  link.tomb_y = ro::sim::kMvpNoTomb;
  CHECK(MvpLinkLabel(link) == "[MVP: Baphomet (prt_maze03)]");
  link.tomb_x = ro::sim::kMvpNoTomb;
  link.tomb_y = 180;
  CHECK(MvpLinkLabel(link) == "[MVP: Baphomet (prt_maze03)]");
}

TEST_CASE("Lien de MVP : l'échéance", "[app][mvp]") {
  MvpLink link = Baphomet();
  // UNE FENÊTRE : échue après sa FIN, pas après son début.
  const std::int64_t window_end_s = kKillS + 130 * kMinuteS;
  CHECK_FALSE(MvpLinkPassed(link, window_end_s));
  CHECK(MvpLinkPassed(link, window_end_s + 1));
  CHECK_FALSE(MvpLinkPassed(link, kKillS + 125 * kMinuteS));
  // UN INSTANT EXACT dépassé est dépensé.
  link.exact_respawn_s = kKillS + 10;
  CHECK_FALSE(MvpLinkPassed(link, kKillS + 10));
  CHECK(MvpLinkPassed(link, kKillS + 11));
  // « MORT À » SANS LOI : échu passé la plus longue loi du serveur.
  link.exact_respawn_s = 0;
  link.delay1_min = 0;
  CHECK(kMvpLawlessLinkLifetimeS == 25 * 60 * kMinuteS);
  CHECK_FALSE(MvpLinkPassed(link, kKillS + 24 * 60 * kMinuteS));
  CHECK_FALSE(MvpLinkPassed(link, kKillS + kMvpLawlessLinkLifetimeS - 1));
  CHECK_FALSE(MvpLinkPassed(link, kKillS + kMvpLawlessLinkLifetimeS));
  CHECK(MvpLinkPassed(link, kKillS + kMvpLawlessLinkLifetimeS + 1));
  // UNE LOI COURTE garde sa propre échéance, bien avant ces 25 h.
  link.delay1_min = 120;
  CHECK(MvpLinkPassed(link, kKillS + 131 * kMinuteS));
  // NI MORT NI INSTANT : rien ne dit quand il revient, jamais échu.
  link.kill_time_s = 0;
  link.delay1_min = 0;
  CHECK_FALSE(MvpLinkPassed(link, kKillS * 2));
}

TEST_CASE("Lien de MVP : un « mort à » sans loi prend celle du catalogue du lecteur",
          "[app][mvp]") {
  constexpr std::uint32_t kMsPerMinute = 60'000;
  constexpr std::uint32_t kOsiris = 1038;
  MvpLink link = Baphomet();
  link.delay1_min = 0;
  link.delay2_min = 0;
  const std::int64_t catalogue_end_s = kKillS + 70 * kMinuteS;

  ro::sim::MvpTrackerBook book;
  // CATALOGUE NON REÇU : rien à prêter, les 25 h.
  CHECK_FALSE(MvpLinkPassed(link, catalogue_end_s + 1, &book));
  CHECK_FALSE(MvpLinkPassed(link, catalogue_end_s + 1));

  book.ReceiveCatalogue({
      ro::sim::MvpSlot{.slot_id = 1,
                       .mob_id = static_cast<std::uint16_t>(kBaphomet),
                       .delay1_ms = 60 * kMsPerMinute,
                       .delay2_ms = 10 * kMsPerMinute,
                       .map = "PRT_MAZE03"},
      ro::sim::MvpSlot{.slot_id = 2,
                       .mob_id = 0,
                       .delay1_ms = 30 * kMsPerMinute,
                       .delay2_ms = 0,
                       .map = "moc_pryd06"},
      ro::sim::MvpSlot{.slot_id = 3,
                       .mob_id = static_cast<std::uint16_t>(kOsiris),
                       .delay1_ms = 0,
                       .delay2_ms = 0,
                       .map = "moc_pryd04"},
  });
  // CRÉNEAU CONNU : échu à la fin de la fenêtre du catalogue, la carte
  // comparée sans la casse.
  CHECK_FALSE(MvpLinkPassed(link, catalogue_end_s, &book));
  CHECK(MvpLinkPassed(link, catalogue_end_s + 1, &book));
  // SANS CATALOGUE, la même heure n'échoit rien.
  CHECK_FALSE(MvpLinkPassed(link, catalogue_end_s + 1));

  // LA LOI DU LIEN PASSE AVANT : c'est celle que l'expéditeur a vue.
  link.delay1_min = 120;
  link.delay2_min = 10;
  CHECK_FALSE(MvpLinkPassed(link, catalogue_end_s + 1, &book));
  CHECK(MvpLinkPassed(link, kKillS + 131 * kMinuteS, &book));

  // UN MONSTRE TIRÉ : le créneau scripté de sa carte répond.
  link.delay1_min = 0;
  link.delay2_min = 0;
  link.monster_id = kOsiris;
  link.map = "moc_pryd06";
  CHECK_FALSE(MvpLinkPassed(link, kKillS + 30 * kMinuteS, &book));
  CHECK(MvpLinkPassed(link, kKillS + 30 * kMinuteS + 1, &book));

  // UN CRÉNEAU SANS RETOUR AU PLUS TÔT, ou inconnu : les 25 h.
  link.map = "moc_pryd04";
  CHECK_FALSE(MvpLinkPassed(link, kKillS + 24 * 60 * kMinuteS, &book));
  CHECK(MvpLinkPassed(link, kKillS + kMvpLawlessLinkLifetimeS + 1, &book));
  link.map = "gef_dun02";
  CHECK_FALSE(MvpLinkPassed(link, kKillS + 24 * 60 * kMinuteS, &book));
}

TEST_CASE("Lien de MVP : la loi du catalogue tronque chaque délai à part, comme le carnet",
          "[app][mvp]") {
  // Une seconde et demie, deux fois : 1 s + 1 s pour le carnet, et non 3 s.
  constexpr std::uint32_t kSecondAndHalfMs = 1'500;
  MvpLink link = Baphomet();
  link.delay1_min = 0;
  link.delay2_min = 0;
  ro::sim::MvpTrackerBook book;
  book.ReceiveCatalogue({ro::sim::MvpSlot{.slot_id = 1,
                                          .mob_id = static_cast<std::uint16_t>(kBaphomet),
                                          .delay1_ms = kSecondAndHalfMs,
                                          .delay2_ms = kSecondAndHalfMs,
                                          .map = "prt_maze03"}});
  CHECK_FALSE(MvpLinkPassed(link, kKillS + 2, &book));
  CHECK(MvpLinkPassed(link, kKillS + 3, &book));
}

TEST_CASE("Lien de MVP : l'expéditeur d'une ligne parlée", "[app][mvp]") {
  using ro::sim::ChatType;
  CHECK(MvpSenderOf(ChatType::kPublicChat, "Alice : <MVPL>…") == "Alice");
  CHECK(MvpSenderOf(ChatType::kPartyChat, "Alice : x") == "Alice");
  CHECK(MvpSenderOf(ChatType::kGuildChat, "Alice : x") == "Alice");
  CHECK(MvpSenderOf(ChatType::kClanChat, "Alice : x") == "Alice");
  // UN TYPE SANS LOCUTEUR : rien, même si la ligne a la forme.
  CHECK(MvpSenderOf(ChatType::kPublic, "Alice : x").empty());
  CHECK(MvpSenderOf(ChatType::kBroadcast, "Alice : x").empty());
  // LE CHUCHOTEMENT se résout ailleurs, par son sens.
  CHECK(MvpSenderOf(ChatType::kWhisper, "Alice : x").empty());
  // Sans « : », ou rien devant.
  CHECK(MvpSenderOf(ChatType::kPublicChat, "Alice dit bonjour").empty());
  CHECK(MvpSenderOf(ChatType::kPublicChat, " : x").empty());
  // 24 OCTETS DU FIL au plus : 24 passent, 25 non.
  const std::string twenty_four(24, 'a');
  CHECK(MvpSenderOf(ChatType::kPublicChat, twenty_four + " : x") == twenty_four);
  CHECK(MvpSenderOf(ChatType::kPublicChat, std::string(25, 'a') + " : x").empty());
}

TEST_CASE("Lien de MVP : une ligne du serveur n'a pas d'expéditeur", "[app][mvp]") {
  using ro::sim::ChatKind;
  using ro::sim::ChatType;
  // 🔴 LE SERVEUR ÉCRIT SOUS LA CASE « CHAT PUBLIC » : sa tête n'est pas un pseudo.
  CHECK(
      moonclient::MvpSenderOfLine(ChatKind::kServer, ChatType::kPublicChat, "Autolootid : <MVPL>…")
          .empty());
  // La parole d'un joueur, et notre propre écho, gardent le leur.
  CHECK(moonclient::MvpSenderOfLine(ChatKind::kPublic, ChatType::kPublicChat, "Alice : <MVPL>…") ==
        "Alice");
  CHECK(moonclient::MvpSenderOfLine(ChatKind::kSelf, ChatType::kPublicChat, "Moi : <MVPL>…") ==
        "Moi");
  // Et la règle des types tient toujours.
  CHECK(moonclient::MvpSenderOfLine(ChatKind::kPublic, ChatType::kPublic, "Alice : x").empty());
}

TEST_CASE("Lien de MVP : la borne de l'expéditeur se mesure sur le fil", "[app][mvp]") {
  using ro::sim::ChatType;
  // 🔴 UN PSEUDO ACCENTUÉ DE 23 CARACTÈRES : 46 octets en UTF-8, 23 sur le fil.
  std::string accented;
  for (int letter = 0; letter < 23; ++letter) {
    accented += "é";
  }
  REQUIRE(accented.size() > moonclient::kMvpSenderMostWireBytes);
  CHECK(MvpSenderOf(ChatType::kPublicChat, accented + " : x") == accented);
  // 25 caractères accentués : trop long sur le fil aussi.
  CHECK(MvpSenderOf(ChatType::kPublicChat, accented + "éé : x").empty());
}

TEST_CASE("Lien de MVP : l'expéditeur d'une ligne de salon", "[app][mvp]") {
  CHECK(MvpRoomSenderOf("Alice : <MVPL>…") == "Alice");
  const std::string forty(40, 'b');
  CHECK(MvpRoomSenderOf(forty + " : x") == forty);
  CHECK(MvpRoomSenderOf(std::string(41, 'b') + " : x").empty());
  CHECK(MvpRoomSenderOf("Alice dit bonjour").empty());
}

TEST_CASE("Lien de MVP : un chuchotement se résout par son sens", "[app][mvp]") {
  using ro::sim::WhisperSide;
  ro::sim::WhisperFacts whisper{.side = WhisperSide::kReceived, .peer_name = "Bob"};
  // REÇU : l'auteur est le pair.
  CHECK(MvpWhisperSenderOf(whisper, "Moi") == "Bob");
  // NOTRE ÉCHO : c'est nous, jamais le destinataire.
  whisper.side = WhisperSide::kSent;
  CHECK(MvpWhisperSenderOf(whisper, "Moi") == "Moi");
  whisper.side = WhisperSide::kUndelivered;
  CHECK(MvpWhisperSenderOf(whisper, "Moi") == "Moi");
  // Notre nom inconnu : personne.
  CHECK(MvpWhisperSenderOf(whisper, "").empty());
  // Pas un chuchotement : personne.
  whisper.side = WhisperSide::kNone;
  CHECK(MvpWhisperSenderOf(whisper, "Moi").empty());
}
