// À qui le joueur parle : les sigils du jeu, et ce qu'ils mettent sur le fil.

#include "ro/net/chat_target.h"

#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/manual_packets.h"

using ro::net::ChatTarget;
using ro::net::ReadChatIntent;

TEST_CASE("Destination : les trois sigils du jeu", "[net][chat]") {
  // ⚠ CE SONT CEUX QUE LES JOUEURS ONT DANS LES DOIGTS. Le client ne montre pas
  // de sélecteur : la destination se lit dans ce qu'on tape.
  const std::string groupe("%on y va");
  CHECK(ReadChatIntent(groupe).target == ChatTarget::kParty);
  CHECK(ReadChatIntent(groupe).text == "on y va");

  const std::string guilde("$bonsoir tout le monde");
  CHECK(ReadChatIntent(guilde).target == ChatTarget::kGuild);
  CHECK(ReadChatIntent(guilde).text == "bonsoir tout le monde");

  const std::string allies("^a l'aide");
  CHECK(ReadChatIntent(allies).target == ChatTarget::kAlly);
  CHECK(ReadChatIntent(allies).text == "a l'aide");

  // L'espace qui suit le sigil se mange : « % salut » vise bien le groupe.
  const std::string espace("%   salut");
  CHECK(ReadChatIntent(espace).text == "salut");

  // Et tout le reste se dit à voix haute, sigil compris s'il n'en est pas un.
  const std::string haute("bonjour !");
  CHECK(ReadChatIntent(haute).target == ChatTarget::kPublic);
  CHECK(ReadChatIntent(haute).text == "bonjour !");
  // 🔴 ET LE CROISILLON EN FAIT PARTIE, CE QUI N'EST PAS UN OUBLI : il
  // appartient aux CHARCOMMANDES de rAthena — « #pseudo @commande » —, et ce
  // serveur-ci le leur donne (`charcommand_symbol: #`). C'est même la raison
  // pour laquelle l'alliance est sur « ^ » : le correctif de Moonlight réécrit
  // l'immédiat du client pour rendre le croisillon. En faire un sigil de canal
  // enverrait la ligne en chuchotement vers un canal nommé d'après le pseudo
  // visé, que le serveur ne trouverait pas — au lieu de la parole publique, où
  // `is_atcommand` l'intercepte. Un canal s'ARME au sélecteur.
  const std::string diese("#Bob @heal");
  CHECK(ReadChatIntent(diese).target == ChatTarget::kPublic);
  CHECK(ReadChatIntent(diese).text == "#Bob @heal");
}

TEST_CASE("Destination : un chuchotement sépare le nom du message", "[net][chat]") {
  const std::string court("/w Bob tu es la ?");
  const ro::net::ChatIntent lu = ReadChatIntent(court);
  CHECK(lu.target == ChatTarget::kWhisper);
  CHECK(lu.recipient == "Bob");
  CHECK(lu.text == "tu es la ?");

  const std::string long_name("/whisper Alice  bonjour");
  CHECK(ReadChatIntent(long_name).recipient == "Alice");
  CHECK(ReadChatIntent(long_name).text == "bonjour");

  // ⚠ UN NOM SANS MESSAGE N'ENVOIE RIEN. Un chuchotement vide ferait paraître
  // une fenêtre chez l'autre pour ne rien lui dire.
  const std::string nu("/w Bob");
  CHECK(ReadChatIntent(nu).target == ChatTarget::kWhisper);
  CHECK(ReadChatIntent(nu).text.empty());
}

TEST_CASE("Destination : un sigil seul ne dit rien", "[net][chat]") {
  // Ce qui n'a pas de texte ne part pas : SendChat le refuse.
  for (const char* alone : {"%", "$", "^", "%   ", ""}) {
    const std::string line(alone);
    CHECK(ReadChatIntent(line).text.empty());
  }
}

TEST_CASE("Envoi : le canal des alliés passe par le chuchotement", "[net][chat]") {
  // ⚠ IL N'Y A PAS DE PAQUET DE CANAL À L'ENVOI. `clif_parse_WisMessage` regarde
  // si le destinataire commence par un croisillon et route vers ce canal. Le
  // serveur ne voit jamais l'accent circonflexe que le joueur a tapé.
  ro::proto::WhisperRequest vers_allies;
  vers_allies.recipient = std::string(ro::proto::WhisperRequest::kAllyChannel);
  vers_allies.text = "a l'aide";
  const auto frame = ro::proto::Encode(vers_allies);
  REQUIRE(frame.has_value());
  // Le nom du canal occupe le champ du destinataire, en clair.
  const std::string prefix(reinterpret_cast<const char*>(frame->data()) + 4, 5);
  CHECK(prefix == "#ally");
}

TEST_CASE("Envoi : un destinataire trop long est refusé, pas tronqué", "[net][chat]") {
  // ⚠ LE NOM TIENT SUR VINGT-QUATRE OCTETS, ZÉRO COMPRIS. Un nom plus long
  // déborderait sur le texte : le serveur lirait un destinataire tronqué suivi
  // d'un message ampute, et personne ne saurait pourquoi.
  ro::proto::WhisperRequest too_long;
  too_long.recipient = std::string(ro::proto::WhisperRequest::kNameSize, 'a');
  too_long.text = "bonjour";
  CHECK_FALSE(ro::proto::Encode(too_long).has_value());

  // Un nom vide non plus : le serveur n'aurait personne à qui parler.
  ro::proto::WhisperRequest without;
  without.text = "bonjour";
  CHECK_FALSE(ro::proto::Encode(without).has_value());
}

TEST_CASE("Envoi : groupe et guilde portent le nom en tête", "[net][chat]") {
  // ⚠ MEME EXIGENCE QUE LE MESSAGE PUBLIC : le serveur compare ce nom à celui du
  // personnage connecté et REJETTE tout le message s'il diffère.
  ro::proto::PartyChatRequest groupe;
  groupe.speaker = "Alice";
  groupe.text = "on y va";
  const auto frame = ro::proto::Encode(groupe);
  REQUIRE(frame.has_value());
  const std::string body(reinterpret_cast<const char*>(frame->data()) + 4, frame->size() - 5);
  CHECK(body == "Alice : on y va");
}

// ── CE QUE LA BARRE A ARMÉ ────────────────────────────────
//
// 🔴 TROIS CHEMINS DISENT OÙ LA LIGNE VA, ET LEUR ORDRE EST TOUT : ce qu'on
// tape, puis la box Â« Pseudo Â», puis le mode. Les essais ci-dessous le fixent
// paire par paire, parce qu'un ordre s'inverse sans que rien ne casse.

TEST_CASE("Destination : le mode armé comble une ligne qui ne dit rien", "[net][chat]") {
  const ro::net::ChatIntent said =
      ReadChatIntent("on y va", ro::net::ChatArming{.mode = ChatTarget::kGuild});
  CHECK(said.target == ChatTarget::kGuild);
  CHECK(said.text == "on y va");
}

TEST_CASE("Destination : un sigil l'emporte sur le mode armé", "[net][chat]") {
  // ⚠ L'INVERSE RENDRAIT LES SIGILS INUTILISABLES dès qu'un mode est armé,
  // alors que ce sont eux que les joueurs ont dans les doigts.
  const std::string party = std::string(1, ro::net::kPartySigil) + "on arrive";
  const ro::net::ChatIntent said =
      ReadChatIntent(party, ro::net::ChatArming{.mode = ChatTarget::kGuild});
  CHECK(said.target == ChatTarget::kParty);
  CHECK(said.text == "on arrive");
}

TEST_CASE("Destination : la box « Pseudo » chuchote sans qu'on tape /w", "[net][chat]") {
  // C'est ainsi que le client chuchote : un nom écrit dans la box EST une
  // destination.
  const ro::net::ChatIntent said =
      ReadChatIntent("tu es la ?", ro::net::ChatArming{.whisper_to = "Bob"});
  CHECK(said.target == ChatTarget::kWhisper);
  CHECK(said.recipient == "Bob");
  CHECK(said.text == "tu es la ?");
}

TEST_CASE("Destination : la box passe devant le mode armé", "[net][chat]") {
  // ⚠ UN NOM EST PLUS PRÉCIS QU'UN MODE. Le joueur qui écrit un pseudo désigne
  // quelqu'un ; celui qui arme un mode désigne un groupe.
  const ro::net::ChatIntent said = ReadChatIntent(
      "bonjour", ro::net::ChatArming{.mode = ChatTarget::kGuild, .whisper_to = "Bob"});
  CHECK(said.target == ChatTarget::kWhisper);
  CHECK(said.recipient == "Bob");
}

TEST_CASE("Destination : un sigil passe devant la box", "[net][chat]") {
  // La box est armée en permanence — elle ne se vide pas à l'envoi — et il faut
  // bien pouvoir dire un mot à voix haute sans l'effacer d'abord.
  const std::string guild = std::string(1, ro::net::kGuildSigil) + "on y va";
  const ro::net::ChatIntent said = ReadChatIntent(guild, ro::net::ChatArming{.whisper_to = "Bob"});
  CHECK(said.target == ChatTarget::kGuild);
  CHECK(said.recipient.empty());
}

TEST_CASE("Destination : le chuchotement ne s'arme pas en MODE", "[net][chat]") {
  // ⚠ IL LUI FAUT UN DESTINATAIRE, et un mode n'en porte aucun : l'accepter
  // enverrait la ligne dans le vide. Elle retombe donc à voix haute.
  const ro::net::ChatIntent said =
      ReadChatIntent("bonjour", ro::net::ChatArming{.mode = ChatTarget::kWhisper});
  CHECK(said.target == ChatTarget::kPublic);
  CHECK(said.text == "bonjour");
}

TEST_CASE("Destination : sans rien d'armé, rien ne change", "[net][chat]") {
  // Le défaut est la parole à voix haute, et c'est ce que tous les appelants qui
  // n'arment rien obtiennent sans le demander.
  CHECK(ReadChatIntent("bonjour").target == ChatTarget::kPublic);
  CHECK(ReadChatIntent("bonjour", ro::net::ChatArming{}).target == ChatTarget::kPublic);
}

TEST_CASE("Destination : le clan s'arme et n'a pas de sigil", "[net][chat]") {
  // ⚠ IL N'EN A PAS, ET C'EST VOULU : le client ne lui en donne pas non plus.
  // Un caractère de plus à retenir pour une appartenance qu'on ne choisit même
  // pas serait un mauvais échange.
  const ro::net::ChatIntent said =
      ReadChatIntent("on y va", ro::net::ChatArming{.mode = ChatTarget::kClan});
  CHECK(said.target == ChatTarget::kClan);
  CHECK(said.text == "on y va");
}

TEST_CASE("Envoi : le clan a son paquet, qui n'est pas celui de la guilde", "[net][chat]") {
  // ⚠ DEUX PAQUETS LÀ OÙ UN SEUL SUFFIRAIT SI C'ÉTAIT LA MÊME CHOSE : on entre
  // dans un clan par sa CLASSE, et un personnage peut tenir les deux à la fois.
  const auto clan = ro::proto::Encode(
      ro::proto::ClanChatRequest{.speaker = "Moi", .text = "on se retrouve ou ?"});
  REQUIRE(clan.has_value());
  CHECK((*clan)[0] == 0x8D);
  CHECK((*clan)[1] == 0x09);
  // Le nom du personnage voyage EN TÊTE du texte : le serveur passe par
  // clif_process_message, qui le réclame et rejette tout le message sinon.
  const std::string wire(clan->begin() + 4, clan->end());
  CHECK(wire.starts_with("Moi"));
  CHECK(wire.find("on se retrouve ou ?") != std::string::npos);
}

// ── LE CHAMP DE BATAILLE ──────────────────────────────────

TEST_CASE("Destination : « & » en tête de ligne parle au champ de bataille", "[net][chat]") {
  const ro::net::ChatArming field{.on_battlefield = true};
  const ro::net::ChatIntent said = ReadChatIntent("&  on tient le drapeau", field);
  CHECK(said.target == ChatTarget::kBattlefield);
  CHECK(said.text == "on tient le drapeau");
  // ⚠ EN TÊTE SEULEMENT : une esperluette dans la phrase reste une esperluette.
  CHECK(ReadChatIntent("Bob & moi", field).target == ChatTarget::kPublic);
  CHECK(ReadChatIntent(" &salut", field).target == ChatTarget::kPublic);
  // Seule, elle vise le champ et ne dit rien.
  CHECK(ReadChatIntent("&", field).target == ChatTarget::kBattlefield);
  CHECK(ReadChatIntent("&", field).text.empty());
}

TEST_CASE("Destination : hors d'une carte de champ, « & » n'est pas un sigil", "[net][chat]") {
  // La ligne garde son esperluette et suit la barre, comme toute ligne sans
  // sigil.
  const ro::net::ChatIntent heart = ReadChatIntent("&lt;3");
  CHECK(heart.target == ChatTarget::kPublic);
  CHECK(heart.text == "&lt;3");
  CHECK(ReadChatIntent("&lt;3", ro::net::ChatArming{.mode = ChatTarget::kGuild}).target ==
        ChatTarget::kGuild);
  // « /bc », lui, vise le champ partout : c'est à l'envoi que son refus s'explique.
  CHECK(ReadChatIntent("/bc on y va").target == ChatTarget::kBattlefield);
  // Et la ligne part toujours au serveur.
  CHECK(ro::net::IsSpokenLine("&lt;3"));
}

TEST_CASE("Destination : les mots de commande se lisent à la casse près", "[net][chat]") {
  const ro::net::ChatIntent shouted = ReadChatIntent("/BC regroupez-vous");
  CHECK(shouted.target == ChatTarget::kBattlefield);
  CHECK(shouted.text == "regroupez-vous");
  CHECK(ReadChatIntent("/Bc").target == ChatTarget::kBattlefield);
  CHECK(ReadChatIntent("/BCAST").target == ChatTarget::kPublic);

  const ro::net::ChatIntent whispered = ReadChatIntent("/W Alice bonjour");
  CHECK(whispered.target == ChatTarget::kWhisper);
  CHECK(whispered.recipient == "Alice");
  CHECK(whispered.text == "bonjour");
  CHECK(ReadChatIntent("/Whisper Alice bonjour").recipient == "Alice");

  CHECK(ro::net::IsSpokenLine("/BC on y va"));
  CHECK(ro::net::IsSpokenLine("/W Bob salut"));
}

TEST_CASE("Destination : « /bc » parle au champ sans basculer la barre", "[net][chat]") {
  const ro::net::ChatIntent said = ReadChatIntent("/bc   regroupez-vous");
  CHECK(said.target == ChatTarget::kBattlefield);
  CHECK(said.text == "regroupez-vous");
  // Le mot seul, ou suivi de blancs : le champ, sans rien à dire.
  for (const char* bare : {"/bc", "/bc ", "/bc    "}) {
    const ro::net::ChatIntent nothing = ReadChatIntent(bare);
    CHECK(nothing.target == ChatTarget::kBattlefield);
    CHECK(nothing.text.empty());
  }
  // Un mot qui ne fait que commencer pareil n'est pas la commande.
  CHECK(ReadChatIntent("/bcast").target == ChatTarget::kPublic);
}

TEST_CASE("Destination : le champ tapé passe devant la box et le mode armés", "[net][chat]") {
  const ro::net::ChatArming armed{
      .mode = ChatTarget::kGuild, .whisper_to = "Bob", .on_battlefield = true};
  CHECK(ReadChatIntent("&on y va", armed).target == ChatTarget::kBattlefield);
  CHECK(ReadChatIntent("/bc on y va", armed).target == ChatTarget::kBattlefield);
  CHECK(ReadChatIntent("&on y va", armed).recipient.empty());
  // Et les autres sigils passent devant le champ armé par /battlechat.
  const ro::net::ChatArming battle{.mode = ChatTarget::kBattlefield};
  CHECK(ReadChatIntent("on y va", battle).target == ChatTarget::kBattlefield);
  CHECK(ReadChatIntent("%on y va", battle).target == ChatTarget::kParty);
  CHECK(ReadChatIntent("$on y va", battle).target == ChatTarget::kGuild);
}

TEST_CASE("Envoi : les barres obliques qui adressent la phrase partent au serveur", "[net][chat]") {
  // Une barre oblique est une commande du client, sauf quand elle dit à qui
  // parler : le chuchotement et le champ de bataille.
  CHECK(ro::net::IsSpokenLine("bonjour"));
  CHECK(ro::net::IsSpokenLine("%on y va"));
  CHECK(ro::net::IsSpokenLine("/w Bob salut"));
  CHECK(ro::net::IsSpokenLine("/whisper Bob salut"));
  CHECK(ro::net::IsSpokenLine("/bc on y va"));
  CHECK_FALSE(ro::net::IsSpokenLine("/battlechat"));
  CHECK_FALSE(ro::net::IsSpokenLine("/sit"));
  CHECK_FALSE(ro::net::IsSpokenLine("/bcast"));
}

TEST_CASE("Destination : Ctrl+Entrée au groupe, Alt+Entrée à la guilde", "[net][chat]") {
  using ro::net::ChatArming;
  using ro::net::ChatKeys;
  const ChatKeys ctrl{.party = true};
  const ChatKeys alt{.guild = true};
  const ChatKeys both{.party = true, .guild = true};
  // Barre à voix haute : Ctrl vise le groupe — s'il y en a un.
  CHECK(ReadChatIntent("on y va", ChatArming{.keys = ctrl, .in_party = true}).target ==
        ChatTarget::kParty);
  CHECK(ReadChatIntent("on y va", ChatArming{.keys = ctrl}).target == ChatTarget::kPublic);
  CHECK(ReadChatIntent("on y va", ChatArming{.keys = alt}).target == ChatTarget::kGuild);
  CHECK(ReadChatIntent("on y va", ChatArming{.keys = alt, .in_party = true}).target ==
        ChatTarget::kGuild);
  // Les deux : le groupe s'il existe, sinon la guilde.
  CHECK(ReadChatIntent("on y va", ChatArming{.keys = both, .in_party = true}).target ==
        ChatTarget::kParty);
  CHECK(ReadChatIntent("on y va", ChatArming{.keys = both}).target == ChatTarget::kGuild);
  // Sans touche, rien ne change.
  CHECK(ReadChatIntent("on y va", ChatArming{.in_party = true}).target == ChatTarget::kPublic);
}

TEST_CASE("Destination : les touches retournent le mode armé", "[net][chat]") {
  using ro::net::ChatArming;
  using ro::net::ChatKeys;
  const ChatKeys ctrl{.party = true};
  const ChatKeys alt{.guild = true};
  // Armée sur le groupe, Ctrl revient à la voix haute ; Alt n'y fait rien.
  CHECK(ReadChatIntent("x", ChatArming{.mode = ChatTarget::kParty, .keys = ctrl, .in_party = true})
            .target == ChatTarget::kPublic);
  CHECK(ReadChatIntent("x", ChatArming{.mode = ChatTarget::kParty, .keys = alt, .in_party = true})
            .target == ChatTarget::kParty);
  // Armée sur la guilde, Alt revient à la voix haute.
  CHECK(ReadChatIntent("x", ChatArming{.mode = ChatTarget::kGuild, .keys = alt}).target ==
        ChatTarget::kPublic);
  CHECK(ReadChatIntent("x", ChatArming{.mode = ChatTarget::kGuild, .keys = ctrl}).target ==
        ChatTarget::kGuild);
  // Les autres modes les ignorent.
  CHECK(ReadChatIntent("x", ChatArming{.mode = ChatTarget::kClan, .keys = alt}).target ==
        ChatTarget::kClan);
}

TEST_CASE(
    "Destination : un sigil, la box « Pseudo » et un mot adressé passent devant les "
    "touches",
    "[net][chat]") {
  using ro::net::ChatArming;
  const ChatArming ctrl{.keys = {.party = true}, .in_party = true};
  CHECK(ReadChatIntent("$bonsoir", ctrl).target == ChatTarget::kGuild);
  CHECK(ReadChatIntent("/gc bonsoir", ctrl).target == ChatTarget::kGuild);
  CHECK(ReadChatIntent("/w Bob salut", ctrl).target == ChatTarget::kWhisper);
  const ChatArming whisper{.whisper_to = "Bob", .keys = {.guild = true}};
  CHECK(ReadChatIntent("salut", whisper).target == ChatTarget::kWhisper);
}
