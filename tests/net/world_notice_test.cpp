#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_update.h"
#include "ro/proto/chat_author_packets.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"
#include "support/wire_bytes.h"

// CE QUE LE SERVEUR DIT, par opposition à ce qu'il fait au monde.
//
// La parole, les bulles au-dessus des têtes, les annonces, l'heure et le
// battement de cœur : aucun de ces paquets ne déplace un corps ni n'en change
// l'apparence. Ils vivent à part des essais de traduction pour la même raison
// que les essais de compétences : une seule famille par fichier se relit, un
// fourre-tout non.

using ro::net::ApplyFrame;
using ro::net::FrameOutcome;
using ro::sim::CellPosition;
using ro::sim::World;
using ro::test_support::FrameOf;
using ro::test_support::IdleUnit;
using ro::test_support::PutU16;
using ro::test_support::PutU32;

// Un acteur quelconque, un joueur quelconque, une couleur quelconque : seules
// comptent leur distinction et le fait qu'aucun ne soit zéro.
constexpr std::uint32_t kSomeActor = 2001;
constexpr std::uint32_t kThePlayer = 3001;
constexpr std::uint32_t kSomeColor = 0xFF8040;

TEST_CASE("Traduction : une annonce ne vient de personne", "[net][update]") {
  World world;
  ro::proto::ZcBroadcast announcement;
  announcement.message = "Le serveur redemarre dans dix minutes.";

  CHECK(ApplyFrame(FrameOf(Encode(announcement)), world, 700) == FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 1);
  CHECK(world.chat_lines()[0].speaker_id == 0);
  CHECK(world.chat_lines()[0].text == "Le serveur redemarre dans dix minutes.");
  CHECK(world.chat_lines()[0].said_ms == 700);
  // La vingt-sixième case, celle des annonces.
  CHECK(world.chat_lines()[0].type == ro::sim::ChatType::kBroadcast);
}

TEST_CASE("Traduction : l'étiquette de couleur n'est pas du texte", "[net][update]") {
  // `clif_broadcast` écrit « blue » ou « ssss » DEVANT le message, sans qu'aucun
  // champ ne le dise. Les garder ferait lire « blueLe serveur redemarre ».
  World world;

  ro::proto::ZcBroadcast blue;
  blue.message = "blueUne annonce bleue.";
  CHECK(ApplyFrame(FrameOf(Encode(blue)), world, 0) == FrameOutcome::kApplied);

  ro::proto::ZcBroadcast siege;
  siege.message = "ssssLes portes s'ouvrent.";
  CHECK(ApplyFrame(FrameOf(Encode(siege)), world, 0) == FrameOutcome::kApplied);

  // Et un message qui ne porte PAS d'étiquette traverse entier.
  ro::proto::ZcBroadcast plain;
  plain.message = "bluets en promotion.";
  CHECK(ApplyFrame(FrameOf(Encode(plain)), world, 0) == FrameOutcome::kApplied);

  REQUIRE(world.chat_lines().size() == 3);
  CHECK(world.chat_lines()[0].text == "Une annonce bleue.");
  CHECK(world.chat_lines()[1].text == "Les portes s'ouvrent.");
  // « bluets » commence par « blue », et se fait donc couper. Le client
  // officiel coupe pareil : quatre octets en tête, sans autre condition.
  CHECK(world.chat_lines()[2].text == "ts en promotion.");

  // L'annonce de siège a sa case ; les autres restent sous celle des annonces.
  CHECK(world.chat_lines()[0].type == ro::sim::ChatType::kBroadcast);
  CHECK(world.chat_lines()[1].type == ro::sim::ChatType::kSiegeInfo);
  CHECK(world.chat_lines()[2].type == ro::sim::ChatType::kBroadcast);
}

TEST_CASE("Traduction : une annonce en couleur passe par la conversation", "[net][update]") {
  World world;
  ro::proto::ZcBroadcast2 announcement;
  announcement.fontColor = 0x00FF'FF00;
  announcement.message = "Un joueur a fabrique un objet rare.";

  CHECK(ApplyFrame(FrameOf(Encode(announcement)), world, 42) == FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 1);
  CHECK(world.chat_lines()[0].speaker_id == 0);
  // Rien à retirer en tête : ici la couleur est un champ.
  CHECK(world.chat_lines()[0].text == "Un joueur a fabrique un objet rare.");
  CHECK(world.chat_lines()[0].color == 0xFFFF00);
}

namespace {

constexpr std::uint32_t kAnnouncementYellow = 0xFFFF00;
constexpr std::uint32_t kAnnouncementCyan = 0x00FFFF;

// La seule ligne qu'une annonce écrit.
ro::sim::ChatLine AnnouncedLine(std::string message) {
  World world;
  ro::proto::ZcBroadcast announcement;
  announcement.message = std::move(message);
  REQUIRE(ApplyFrame(FrameOf(Encode(announcement)), world, 0) == FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 1);
  return world.chat_lines()[0];
}

}  // namespace

TEST_CASE("Annonce : « blue » peint en cyan, le reste en jaune", "[net][notice]") {
  CHECK(AnnouncedLine("blueUne annonce bleue.").color == kAnnouncementCyan);
  CHECK(AnnouncedLine("ssssLes portes s'ouvrent.").color == kAnnouncementYellow);
  CHECK(AnnouncedLine("Une annonce simple.").color == kAnnouncementYellow);

  // L'étiquette se lit sans égard à la casse, et « 블루 » en CP949 vaut « blue ».
  const ro::sim::ChatLine shouted = AnnouncedLine("BLUEFort.");
  CHECK(shouted.text == "Fort.");
  CHECK(shouted.color == kAnnouncementCyan);
  const ro::sim::ChatLine korean = AnnouncedLine(
      "\xBA\xED\xB7\xE7"
      "Bleu.");
  CHECK(korean.text == "Bleu.");
  CHECK(korean.color == kAnnouncementCyan);
  CHECK(korean.type == ro::sim::ChatType::kBroadcast);
}

TEST_CASE("Annonce : un nom suffixé d'un « # » se replie", "[net][notice]") {
  // Ce qui précède le « # », puis tout depuis le premier « : ».
  CHECK(AnnouncedLine("Alice#1234: Bonjour").text == "Alice: Bonjour");
  // Sans « : », le « # » coupe la fin.
  CHECK(AnnouncedLine("Alice#1234").text == "Alice");
  // Un « # » en tête ne replie rien.
  CHECK(AnnouncedLine("#1 au classement : Alice").text == "#1 au classement : Alice");
  // Le premier « : » se cherche dans tout le texte, devant le « # » compris.
  CHECK(AnnouncedLine("a:b#c").text == "a:b:b#c");
  // ⚠ UNE ÉTIQUETTE PROTÈGE SON TEXTE : rien ne s'y replie.
  CHECK(AnnouncedLine("blueAlice#1234 : Bonjour").text == "Alice#1234 : Bonjour");
}

TEST_CASE("Annonce : « tool » écrit sa couleur en tête", "[net][notice]") {
  const ro::sim::ChatLine line = AnnouncedLine("toolFF8040Un objet rare.");
  CHECK(line.text == "Un objet rare.");
  CHECK(line.color == 0xFF8040);
  CHECK(line.type == ro::sim::ChatType::kBroadcast);
  CHECK(line.kind == ro::sim::ChatKind::kAnnouncement);
}

TEST_CASE("Annonce : « micc » lit la voix d'un joueur après son nom", "[net][notice]") {
  constexpr std::size_t kNameSize = 24;
  std::string name = "Alice";
  name.resize(kNameSize, '\0');
  const ro::sim::ChatLine line = AnnouncedLine("micc" + name + "00FF00Bonjour a tous.");
  CHECK(line.text == "Bonjour a tous.");
  CHECK(line.color == 0x00FF00);
  // La case générale, et non celle des annonces.
  CHECK(line.type == ro::sim::ChatType::kPublic);
  CHECK(line.speaker_id == 0);

  // ⚠ AU-DELÀ DE 361 OCTETS, l'annonce entière se tait ; une charge trop
  // courte pour porter nom et couleur aussi.
  constexpr std::size_t kTooLong = 362;
  World world;
  ro::proto::ZcBroadcast shout;
  shout.message = "micc" + name + "00FF00" + std::string(kTooLong, 'a');
  CHECK(ApplyFrame(FrameOf(Encode(shout)), world, 0) == FrameOutcome::kIgnored);
  ro::proto::ZcBroadcast cut;
  cut.message = "miccAlice";
  CHECK(ApplyFrame(FrameOf(Encode(cut)), world, 0) == FrameOutcome::kIgnored);
  CHECK(world.chat_lines().empty());
}

TEST_CASE("Annonce colorée : le nom suffixé d'un « # » s'y replie aussi", "[net][notice]") {
  World world;
  ro::proto::ZcBroadcast2 announcement;
  // L'octet haut n'est pas une couleur.
  announcement.fontColor = 0xAB00'FFFF;
  announcement.message = "Alice#1234: Bonjour";
  REQUIRE(ApplyFrame(FrameOf(Encode(announcement)), world, 0) == FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 1);
  CHECK(world.chat_lines()[0].text == "Alice: Bonjour");
  CHECK(world.chat_lines()[0].color == 0x00FFFF);
  CHECK(world.chat_lines()[0].type == ro::sim::ChatType::kBroadcast);
}

TEST_CASE("Traduction : l'heure du serveur cale l'horloge", "[net][update]") {
  World world;
  // Tant que rien n'a été mesuré, un instant serveur vaut l'instant local.
  CHECK(world.server_clock().LocalTimeOf(1'000, 1'500) == 1'500);

  ro::proto::ZcNotifyTime clock;
  clock.time = 1'000;
  CHECK(ApplyFrame(FrameOf(Encode(clock)), world, 1'200) == FrameOutcome::kApplied);

  // Décalage de 200 ms : le tic 900 du serveur s'est produit à 1100 chez nous.
  CHECK(world.server_clock().LocalTimeOf(900, 1'300) == 1'100);
}

TEST_CASE("Traduction : le battement de cœur est compris et sans effet", "[net][update]") {
  World world;
  ro::proto::ZcPingLive heartbeat;

  CHECK(ApplyFrame(FrameOf(Encode(heartbeat)), world, 0) == FrameOutcome::kIgnored);
  CHECK(world.chat_lines().empty());
}

TEST_CASE("Traduction : une émoticône se pose sur qui la montre", "[net][update]") {
  // Le paquet ne porte que le bloc et un numéro, et ce numéro EST l'action du
  // sprite des émoticônes.
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(150'001, CellPosition{1, 1, 0}))), world, 0) ==
          FrameOutcome::kApplied);

  // La vieille annonce, celle des monstres et des scripts : pas de lot.
  ro::proto::ZcEmotion shown;
  shown.GID = 150'001;
  shown.type = 12;
  CHECK(ApplyFrame(FrameOf(Encode(shown)), world, 7000) == FrameOutcome::kApplied);
  REQUIRE(world.Find(150'001)->emotion.has_value());
  CHECK(world.Find(150'001)->emotion->id == 12);
  CHECK(world.Find(150'001)->emotion->pack == ro::sim::kBaseEmotionPack);
  CHECK(world.Find(150'001)->emotion_since_ms == 7000);

  // Celle du client moderne, qui porte le lot avec le numéro.
  ro::proto::ZcPlayCashEmotion played;
  played.GID = 150'001;
  played.pack_id = 3;
  played.emotion_id = 44;
  CHECK(ApplyFrame(FrameOf(Encode(played)), world, 8000) == FrameOutcome::kApplied);
  CHECK(world.Find(150'001)->emotion->pack == 3);
  CHECK(world.Find(150'001)->emotion->id == 44);

  // Une émoticône d'un acteur qu'on n'a jamais vu n'en crée pas un.
  shown.GID = 999;
  CHECK(ApplyFrame(FrameOf(Encode(shown)), world, 7000) == FrameOutcome::kIgnored);
  CHECK(world.size() == 1);
}

TEST_CASE("Traduction : la parole d'autrui se pose, celle du serveur non", "[net][update]") {
  // Le serveur envoie DEUX paquets de parole : l'un nomme qui parle, l'autre
  // s'adresse au joueur seul et porte aussi bien l'écho de sa propre voix que
  // les réponses de ses commandes. C'est le préfixe du nom qui les sépare.
  World world;
  world.set_player_id(150'009);
  world.set_player_name("Alice");
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(150'001, CellPosition{1, 1, 0}))), world, 0) ==
          FrameOutcome::kApplied);

  ro::proto::ZcNotifyChat said;
  said.GID = 150'001;
  said.Message = "Bob : bonjour";
  CHECK(ApplyFrame(FrameOf(Encode(said)), world, 5000) == FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 1);
  CHECK(world.chat_lines()[0].speaker_id == 150'001);
  CHECK(world.Find(150'001)->chat_text == "Bob : bonjour");
}

namespace {

// Une trame écrite à la main : ces deux paquets-là n'ont pas d'encodeur, parce
// que le client ne les ÉMET jamais. L'essai fabrique donc les octets comme le
// serveur les écrit.
void PutText(std::vector<std::uint8_t>& into, std::string_view text, std::size_t width) {
  for (std::size_t at = 0; at < width; ++at) {
    into.push_back(at < text.size() ? static_cast<std::uint8_t>(text[at]) : 0);
  }
}

[[nodiscard]] std::vector<std::uint8_t> WhisperFrame(std::uint32_t sender_id,
                                                     std::string_view sender,
                                                     std::string_view message, bool staff) {
  std::vector<std::uint8_t> frame;
  PutU16(frame, ro::proto::WhisperNotice::kOpcode);
  PutU16(frame, 0);  // la longueur, recopiée une fois connue
  PutU32(frame, sender_id);
  PutText(frame, sender, ro::proto::WhisperNotice::kNameSize);
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

[[nodiscard]] std::vector<std::uint8_t> ColoredFrame(std::uint32_t speaker_id, std::uint32_t color,
                                                     std::string_view message) {
  std::vector<std::uint8_t> frame;
  PutU16(frame, ro::proto::ColoredNotice::kOpcode);
  PutU16(frame, 0);
  PutU32(frame, speaker_id);
  PutU32(frame, color);
  for (const char character : message) {
    frame.push_back(static_cast<std::uint8_t>(character));
  }
  frame.push_back(0);
  const auto length = static_cast<std::uint16_t>(frame.size());
  frame[2] = static_cast<std::uint8_t>(length & 0xFF);
  frame[3] = static_cast<std::uint8_t>(length >> 8);
  return frame;
}

}  // namespace

TEST_CASE("Traduction : un chuchotement porte le NOM de son auteur", "[net][notice]") {
  // ⚠ IL NOMME SON AUTEUR SANS DIRE OÙ IL EST. Un chuchotement traverse les
  // cartes : chercher un acteur pour lui serait chercher ce qui n'est pas là.
  World world;
  const auto frame = FrameOf(WhisperFrame(150'009, "Bob", "tu es la ?", false));

  CHECK(ApplyFrame(frame, world, 900) == FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 1);
  const ro::sim::ChatLine& line = world.chat_lines()[0];
  CHECK(line.kind == ro::sim::ChatKind::kWhisper);
  CHECK(line.whisper.peer_name == "Bob");
  CHECK(line.whisper.side == ro::sim::WhisperSide::kReceived);
  CHECK(line.text == "tu es la ?");
  // Pas d'identifiant : c'est le nom qui désigne.
  CHECK(line.speaker_id == 0);
}

TEST_CASE("Traduction : un chuchotement de staff reste un chuchotement", "[net][notice]") {
  // ⚠ LE CLIENT D'ORIGINE SE TROMPE ICI : dès que le drapeau de staff est levé,
  // il bascule la ligne en « annonce » et elle échappe au filtre des
  // chuchotements. On range le genre nous-mêmes, donc ce travers ne nous
  // atteint pas — et cet essai le garde.
  World world;
  const auto frame = FrameOf(WhisperFrame(0, "Stingor", "bonjour", true));

  CHECK(ApplyFrame(frame, world, 0) == FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 1);
  CHECK(world.chat_lines()[0].kind == ro::sim::ChatKind::kWhisper);
  CHECK(world.chat_lines()[0].whisper.peer_name == "Stingor");
  CHECK(world.chat_lines()[0].whisper.relation == ro::sim::WhisperRelation::kStaff);
}

TEST_CASE("Traduction : groupe, guilde et canal se distinguent", "[net][notice]") {
  // Le genre est ce qui décide de l'onglet et de la couleur ; le déduire de
  // l'orateur ne marcherait pas, deux d'entre eux n'en ayant aucun.
  World world;

  ro::proto::ZcNotifyChatParty groupe;
  groupe.AID = 150'001;
  groupe.chatMsg = "Alice : on y va";
  CHECK(ApplyFrame(FrameOf(Encode(groupe)), world, 0) == FrameOutcome::kApplied);

  ro::proto::ZcGuildChat guilde;
  guilde.message = "Bob : bonsoir";
  CHECK(ApplyFrame(FrameOf(Encode(guilde)), world, 0) == FrameOutcome::kApplied);

  CHECK(ApplyFrame(FrameOf(ColoredFrame(0, 0x00FF00, "[#ally] Carol : a l'aide")), world, 0) ==
        FrameOutcome::kApplied);

  REQUIRE(world.chat_lines().size() == 3);
  CHECK(world.chat_lines()[0].kind == ro::sim::ChatKind::kParty);
  CHECK(world.chat_lines()[0].speaker_id == 150'001);
  CHECK(world.chat_lines()[1].kind == ro::sim::ChatKind::kGuild);
  // ⚠ LA GUILDE NE NOMME PERSONNE : ni identifiant, ni champ de nom. L'auteur
  // est dans le texte, et nulle part ailleurs.
  CHECK(world.chat_lines()[1].speaker_id == 0);
  CHECK(world.chat_lines()[1].text == "Bob : bonsoir");
  CHECK(world.chat_lines()[2].kind == ro::sim::ChatKind::kChannel);
  // Les cases de filtre : groupe, guilde, et la case générale pour un canal.
  CHECK(world.chat_lines()[0].type == ro::sim::ChatType::kPartyChat);
  CHECK(world.chat_lines()[1].type == ro::sim::ChatType::kGuildChat);
  CHECK(world.chat_lines()[2].type == ro::sim::ChatType::kPublic);
}

TEST_CASE("Traduction : l'écho de ma parole de groupe a sa couleur", "[net][notice]") {
  World world;
  world.set_player_id(150'002);

  ro::proto::ZcNotifyChatParty other;
  other.AID = 150'001;
  other.chatMsg = "Alice : on y va";
  CHECK(ApplyFrame(FrameOf(Encode(other)), world, 0) == FrameOutcome::kApplied);
  ro::proto::ZcNotifyChatParty mine;
  mine.AID = 150'002;
  mine.chatMsg = "Moi : j'arrive";
  CHECK(ApplyFrame(FrameOf(Encode(mine)), world, 0) == FrameOutcome::kApplied);

  REQUIRE(world.chat_lines().size() == 2);
  // Zéro : la couleur du genre, le rose du groupe.
  CHECK(world.chat_lines()[0].color == 0);
  CHECK(world.chat_lines()[1].color == 0xFFC800);
  CHECK(world.chat_lines()[1].kind == ro::sim::ChatKind::kParty);
}

TEST_CASE("Traduction : une trame de chuchotement tronquée est refusée", "[net][notice]") {
  // Ces octets viennent du réseau : une longueur qui ment ne doit pas faire lire
  // au-delà de la trame.
  World world;
  std::vector<std::uint8_t> frame = WhisperFrame(1, "Bob", "salut", false);
  frame.resize(ro::proto::WhisperNotice::kHeaderSize - 1);
  CHECK(ApplyFrame(FrameOf(frame), world, 0) == FrameOutcome::kUndecodable);

  // Et une longueur annoncée qui ne correspond pas non plus.
  std::vector<std::uint8_t> menteuse = WhisperFrame(1, "Bob", "salut", false);
  menteuse[2] = 0xFF;
  CHECK(ApplyFrame(FrameOf(menteuse), world, 0) == FrameOutcome::kUndecodable);
  CHECK(world.chat_lines().empty());
}

TEST_CASE("Traduction : une parole colorée nomme son acteur", "[net][update]") {
  // ⚠ DEUX CHOSES PASSENT PAR CET OPCODE, et c'est l'IDENTIFIANT qui les
  // sépare : zéro pour un canal, sinon l'acteur qui parle. Les confondre rangeait
  // Â« la banque est fermée Â» dans l'onglet des canaux.
  World world;
  world.Spawn(ro::sim::Actor{.id = kSomeActor});

  CHECK(ApplyFrame(FrameOf(ColoredFrame(kSomeActor, kSomeColor, "Bonjour, voyageur.")), world,
                   400) == FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 1);
  CHECK(world.chat_lines()[0].kind == ro::sim::ChatKind::kPublic);
  // ⚠ LA CASE GÉNÉRALE, PAS « CHAT PUBLIC » : ce paquet ne porte pas la parole
  // d'un joueur.
  CHECK(world.chat_lines()[0].type == ro::sim::ChatType::kPublic);
  CHECK(world.chat_lines()[0].speaker_id == kSomeActor);
  // La couleur demandée voyage avec la ligne : c'est le seul paquet qui en porte
  // une pour une parole.
  CHECK(world.chat_lines()[0].color == kSomeColor);
}

TEST_CASE("Traduction : la bulle d'une parole colorée en prend la couleur", "[net][update]") {
  // ⚠ NI MASQUE NI BASCULE : le client d'origine pose cette parole telle quelle,
  // et sa bulle reçoit la couleur du paquet, octets compris dans l'ordre inverse.
  World world;
  world.Spawn(ro::sim::Actor{.id = kSomeActor});

  CHECK(ApplyFrame(FrameOf(ColoredFrame(kSomeActor, kSomeColor, "Bonjour, voyageur.")), world,
                   400) == FrameOutcome::kApplied);
  const ro::sim::Actor* speaker = world.Find(kSomeActor);
  REQUIRE(speaker != nullptr);
  CHECK(speaker->chat_text == "Bonjour, voyageur.");
  CHECK(speaker->chat_since_ms == 400);
  CHECK(speaker->chat_bgr == kSomeColor);

  // Une parole sans couleur rend la sienne au thème.
  world.ShowSpeech(kSomeActor, "Au revoir.", 500);
  CHECK(world.Find(kSomeActor)->chat_bgr == 0);
}

TEST_CASE("Traduction : la parole longue d'un PNJ arrive entière", "[net][update]") {
  // Le serveur l'envoie telle quelle à qui annonce `kUiCapLongNpcTalk`, bien
  // au-delà de son tampon de chat.
  World world;
  world.Spawn(ro::sim::Actor{.id = kSomeActor});
  const std::string said(1024, 'a');

  CHECK(ApplyFrame(FrameOf(ColoredFrame(kSomeActor, kSomeColor, said)), world, 400) ==
        FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 1);
  CHECK(world.chat_lines()[0].text == said);
  CHECK(world.Find(kSomeActor)->chat_text == said);
}

TEST_CASE("Traduction : la parole colorée d'un canal ne teint aucune bulle", "[net][update]") {
  // L'identifiant zéro ne désigne personne : la ligne seule.
  World world;
  world.Spawn(ro::sim::Actor{.id = kSomeActor});

  CHECK(ApplyFrame(FrameOf(ColoredFrame(0, kSomeColor, "[#ally] Carol : a l'aide")), world, 400) ==
        FrameOutcome::kApplied);
  CHECK(world.Find(kSomeActor)->chat_text.empty());
  CHECK(world.Find(kSomeActor)->chat_bgr == 0);
}

TEST_CASE("Traduction : ce que le serveur RÉPOND ne vient de personne", "[net][update]") {
  // ⚠ SA RÉPONSE PORTE L'IDENTIFIANT DU JOUEUR, et ce n'est pas lui qui parle :
  // une bulle Â« la banque est fermée Â» au-dessus de sa propre tête serait la
  // lecture inverse de ce qui se passe.
  World world;
  world.set_player_id(kThePlayer);
  world.Spawn(ro::sim::Actor{.id = kThePlayer});

  CHECK(ApplyFrame(FrameOf(ColoredFrame(kThePlayer, kSomeColor, "La banque est fermee.")), world,
                   400) == FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 1);
  CHECK(world.chat_lines()[0].kind == ro::sim::ChatKind::kServer);
  CHECK(world.chat_lines()[0].type == ro::sim::ChatType::kPublic);
  CHECK(world.chat_lines()[0].speaker_id == 0);
  // Ni bulle : le client d'origine ne cherche l'orateur que parmi les autres.
  CHECK(world.Find(kThePlayer)->chat_text.empty());
}

TEST_CASE("Traduction : la réponse du paquet de parole se range sous « Chat public »",
          "[net][update]") {
  // ⚠ LE GENRE DIT « SERVEUR » (pas de bulle, sa couleur), LA CASE « CHAT
  // PUBLIC » : c'est le paquet de la parole publique, et l'écho comme la
  // réponse s'y rangent.
  World world;
  world.set_player_id(kThePlayer);
  world.set_player_name("Alice");

  // Ce paquet n'a pas d'encodeur : ses octets s'écrivent à la main.
  constexpr std::uint16_t kPlayerChatOpcode = 0x008E;
  const auto said = [](std::string_view message) {
    std::vector<std::uint8_t> frame;
    PutU16(frame, kPlayerChatOpcode);
    PutU16(frame, 0);
    PutText(frame, message, message.size() + 1);
    const auto length = static_cast<std::uint16_t>(frame.size());
    frame[2] = static_cast<std::uint8_t>(length & 0xFF);
    frame[3] = static_cast<std::uint8_t>(length >> 8);
    return frame;
  };
  CHECK(ApplyFrame(FrameOf(said("Alice : bonjour")), world, 0) == FrameOutcome::kApplied);
  CHECK(ApplyFrame(FrameOf(said("Commande inconnue.")), world, 0) == FrameOutcome::kApplied);

  REQUIRE(world.chat_lines().size() == 2);
  CHECK(world.chat_lines()[0].kind == ro::sim::ChatKind::kSelf);
  CHECK(world.chat_lines()[1].kind == ro::sim::ChatKind::kServer);
  CHECK(world.chat_lines()[0].type == ro::sim::ChatType::kPublicChat);
  CHECK(world.chat_lines()[1].type == ro::sim::ChatType::kPublicChat);
}

TEST_CASE("Traduction : le clan n'est pas la guilde", "[net][update]") {
  // ⚠ UN PERSONNAGE PEUT TENIR LES DEUX À LA FOIS : on entre dans un clan par sa
  // CLASSE, sans en choisir les membres. Les mêler donnerait un onglet
  // Â« Guilde Â» qui montre les paroles d'un groupe qu'on n'a pas choisi.
  World world;
  ro::proto::ZcNotifyClanChat said;
  said.Message = "Chef : on se retrouve ou ?";

  CHECK(ApplyFrame(FrameOf(Encode(said)), world, 500) == FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 1);
  CHECK(world.chat_lines()[0].kind == ro::sim::ChatKind::kClan);
  // ⚠ SON CHAMP DE NOM EST VIDE, ET C'EST LE SERVEUR QUI LE VEUT : l'auteur est
  // dans le texte, comme pour la guilde.
  CHECK(world.chat_lines()[0].text == "Chef : on se retrouve ou ?");
  CHECK(world.chat_lines()[0].speaker_id == 0);
}

TEST_CASE("Traduction : un chuchotement qui échoue le dit", "[net][update]") {
  // ⚠ SANS CE PAQUET, CHUCHOTER À QUELQU'UN D'ABSENT EST MUET : le serveur
  // n'envoie rien d'autre, et le joueur attend une réponse qui ne viendra pas.
  World world;
  ro::proto::ZcAckWhisper answer;
  answer.result = 1;  // ACKWHISPER_TARGET_OFFLINE

  CHECK(ApplyFrame(FrameOf(Encode(answer)), world, 600) == FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 1);
  // ⚠ UN CHUCHOTEMENT, COMME CHEZ LE CLIENT : l'onglet qui montre les
  // chuchotements montre aussi celui qui n'est pas parvenu.
  CHECK(world.chat_lines()[0].kind == ro::sim::ChatKind::kWhisper);
  CHECK(world.chat_lines()[0].whisper.side == ro::sim::WhisperSide::kUndelivered);
  // Sans file d'envois, l'accusé ne retrouve personne.
  CHECK(world.chat_lines()[0].whisper.peer_name.empty());
}

TEST_CASE("Traduction : un chuchotement reçu ne se commente pas", "[net][update]") {
  // ⚠ LE SERVEUR ACCUSE CHAQUE CHUCHOTEMENT, réussi compris. L'annoncer
  // doublerait chaque ligne qu'on écrit, puisqu'on la voit déjà partir.
  World world;
  ro::proto::ZcAckWhisper answer;
  answer.result = 0;  // ACKWHISPER_SUCCESS

  CHECK(ApplyFrame(FrameOf(Encode(answer)), world, 600) == FrameOutcome::kIgnored);
  CHECK(world.chat_lines().empty());
}

// ── L'AUTEUR DE LA PAROLE (`ZC_MOONLIGHT_CHAT_AUTHOR 0x0F3A`) ───────────────

namespace {

constexpr std::uint32_t kAuthorAccount = 2'000'001;
constexpr std::uint32_t kAuthorChar = 150'077;
// « Nom : », le séparateur que le serveur écrit derrière le nom.
constexpr std::uint16_t kSeparatorBytes = 3;

// L'auteur d'une ligne « Nom : … » : la tête est le nom et son séparateur.
[[nodiscard]] ro::proto::ChatAuthorNotice AuthorOf(std::uint16_t next_opcode,
                                                   ro::proto::ChatAuthorScope scope,
                                                   std::string_view name) {
  ro::proto::ChatAuthorNotice notice;
  notice.fixed.next_opcode = next_opcode;
  notice.fixed.scope = static_cast<std::uint8_t>(scope);
  notice.fixed.channel_type = ro::proto::kChatAuthorNoChannelType;
  notice.fixed.account_id = kAuthorAccount;
  notice.fixed.char_id = kAuthorChar;
  notice.fixed.header_len = static_cast<std::uint16_t>(name.size() + kSeparatorBytes);
  notice.fixed.name_offset = 0;
  notice.fixed.name = std::string(name);
  return notice;
}

// La ligne, précédée de son auteur comme la liaison l'attache.
[[nodiscard]] ro::net::Frame Authored(std::vector<std::uint8_t> line,
                                      const ro::proto::ChatAuthorNotice& author) {
  ro::net::Frame frame = FrameOf(std::move(line));
  frame.preamble = Encode(author);
  return frame;
}

}  // namespace

TEST_CASE("Auteur : la parole publique nomme son auteur à part", "[net][notice]") {
  World world;
  ro::proto::ZcNotifyChat said;
  said.GID = kSomeActor;
  said.Message = "Bob : bonjour";
  const auto author =
      AuthorOf(ro::proto::ZcNotifyChat::kOpcode, ro::proto::ChatAuthorScope::kPublic, "Bob");

  CHECK(ApplyFrame(Authored(Encode(said), author), world, 0) == FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 1);
  const ro::sim::ChatLine& line = world.chat_lines()[0];
  // Le texte reste ENTIER ; l'orateur, celui de la trame.
  CHECK(line.text == "Bob : bonjour");
  CHECK(line.speaker_id == kSomeActor);
  REQUIRE(line.author.has_value());
  CHECK(line.author->name == "Bob");
  CHECK(line.author->account_id == kAuthorAccount);
  CHECK(line.author->char_id == kAuthorChar);
  CHECK(line.author->name_offset == 0);
  CHECK(line.author->name_length == 3);
  CHECK(line.text.substr(line.author->said_offset) == "bonjour");
}

TEST_CASE("Auteur : groupe, guilde et clan, sans bulle hors du groupe", "[net][notice]") {
  World world;
  ro::proto::ZcNotifyChatParty party;
  party.AID = kSomeActor;
  party.chatMsg = "Alice : on y va";
  CHECK(ApplyFrame(Authored(Encode(party), AuthorOf(ro::proto::ZcNotifyChatParty::kOpcode,
                                                    ro::proto::ChatAuthorScope::kParty, "Alice")),
                   world, 0) == FrameOutcome::kApplied);

  ro::proto::ZcGuildChat guild;
  guild.message = "Bob : bonsoir";
  CHECK(ApplyFrame(Authored(Encode(guild), AuthorOf(ro::proto::ZcGuildChat::kOpcode,
                                                    ro::proto::ChatAuthorScope::kGuild, "Bob")),
                   world, 0) == FrameOutcome::kApplied);

  ro::proto::ZcNotifyClanChat clan;
  clan.Message = "Chef : on se retrouve";
  CHECK(ApplyFrame(Authored(Encode(clan), AuthorOf(ro::proto::ZcNotifyClanChat::kOpcode,
                                                   ro::proto::ChatAuthorScope::kClan, "Chef")),
                   world, 0) == FrameOutcome::kApplied);

  REQUIRE(world.chat_lines().size() == 3);
  for (const ro::sim::ChatLine& line : world.chat_lines()) {
    REQUIRE(line.author.has_value());
    CHECK(line.text.substr(line.author->name_offset, line.author->name_length) ==
          line.author->name);
  }
  CHECK(world.chat_lines()[0].speaker_id == kSomeActor);
  // ⚠ L'AUTEUR CONNU NE FAIT PAS UN ORATEUR : la guilde et le clan ne mettent
  // pas de bulle au-dessus d'une tête.
  CHECK(world.chat_lines()[1].speaker_id == 0);
  CHECK(world.chat_lines()[2].speaker_id == 0);
  CHECK(world.chat_lines()[1].type == ro::sim::ChatType::kGuildChat);
  CHECK(world.chat_lines()[2].type == ro::sim::ChatType::kClanChat);
}

TEST_CASE("Auteur : le canal d'une alliance a sa case", "[net][notice]") {
  World world;
  // « [#ally] Carol : a l'aide » : l'alias, une espace, puis le nom.
  constexpr std::string_view kAlias = "[#ally]";
  auto ally =
      AuthorOf(ro::proto::ColoredNotice::kOpcode, ro::proto::ChatAuthorScope::kChannel, "Carol");
  ally.fixed.channel_type = ro::proto::kChatAuthorAllyChannelType;
  ally.fixed.name_offset = static_cast<std::uint16_t>(kAlias.size() + 1);
  ally.fixed.header_len =
      static_cast<std::uint16_t>(ally.fixed.name_offset + ally.fixed.name.size() + kSeparatorBytes);
  ally.channel = "ally";
  CHECK(ApplyFrame(Authored(ColoredFrame(0, kSomeColor, "[#ally] Carol : a l'aide"), ally), world,
                   0) == FrameOutcome::kApplied);

  // Un autre canal reste sous la case générale.
  auto open = ally;
  open.fixed.channel_type = 0;
  open.channel = "main";
  CHECK(ApplyFrame(Authored(ColoredFrame(0, kSomeColor, "[#main] Carol : salut"), open), world,
                   0) == FrameOutcome::kApplied);

  REQUIRE(world.chat_lines().size() == 2);
  const ro::sim::ChatLine& line = world.chat_lines()[0];
  CHECK(line.kind == ro::sim::ChatKind::kChannel);
  CHECK(line.type == ro::sim::ChatType::kAllianceChat);
  CHECK(line.speaker_id == 0);
  CHECK(line.color == kSomeColor);
  REQUIRE(line.author.has_value());
  CHECK(line.author->name_offset == 8);
  CHECK(line.text.substr(line.author->said_offset) == "a l'aide");
  CHECK(world.chat_lines()[1].type == ro::sim::ChatType::kPublic);
}

TEST_CASE("Auteur : un nom en 1252 se compte en UTF-8", "[net][notice]") {
  World world;
  ro::proto::ZcGuildChat guild;
  guild.message = "Jos\xE9 : caf\xE9";
  CHECK(ApplyFrame(Authored(Encode(guild), AuthorOf(ro::proto::ZcGuildChat::kOpcode,
                                                    ro::proto::ChatAuthorScope::kGuild, "Jos\xE9")),
                   world, 0) == FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 1);
  const ro::sim::ChatLine& line = world.chat_lines()[0];
  CHECK(line.text == "Jos\xC3\xA9 : caf\xC3\xA9");
  REQUIRE(line.author.has_value());
  CHECK(line.author->name == "Jos\xC3\xA9");
  CHECK(line.author->name_length == 5);
  CHECK(line.author->said_offset == 8);
}

TEST_CASE("Auteur : décrire une autre trame, ou mentir sur la tête, n'en donne pas",
          "[net][notice]") {
  World world;
  ro::proto::ZcGuildChat guild;
  guild.message = "Bob : bonsoir";

  // Un auteur qui annonce une parole de groupe ne vaut pas pour la guilde.
  const auto stray =
      AuthorOf(ro::proto::ZcNotifyChatParty::kOpcode, ro::proto::ChatAuthorScope::kParty, "Bob");
  CHECK(ApplyFrame(Authored(Encode(guild), stray), world, 0) == FrameOutcome::kApplied);

  // Une tête plus longue que la phrase.
  auto too_long =
      AuthorOf(ro::proto::ZcGuildChat::kOpcode, ro::proto::ChatAuthorScope::kGuild, "Bob");
  too_long.fixed.header_len = 200;
  CHECK(ApplyFrame(Authored(Encode(guild), too_long), world, 0) == FrameOutcome::kApplied);

  // Un nom qui n'est pas là où la tête le dit.
  auto elsewhere =
      AuthorOf(ro::proto::ZcGuildChat::kOpcode, ro::proto::ChatAuthorScope::kGuild, "Bob");
  elsewhere.fixed.name_offset = 1;
  CHECK(ApplyFrame(Authored(Encode(guild), elsewhere), world, 0) == FrameOutcome::kApplied);

  // Une tête nulle : le serveur n'a pas reconnu la forme de la phrase.
  auto unknown =
      AuthorOf(ro::proto::ZcGuildChat::kOpcode, ro::proto::ChatAuthorScope::kGuild, "Bob");
  unknown.fixed.header_len = 0;
  CHECK(ApplyFrame(Authored(Encode(guild), unknown), world, 0) == FrameOutcome::kApplied);

  REQUIRE(world.chat_lines().size() == 4);
  for (const ro::sim::ChatLine& line : world.chat_lines()) {
    // La ligne passe comme avant, entière, sans auteur.
    CHECK(line.text == "Bob : bonsoir");
    CHECK_FALSE(line.author.has_value());
  }
}

TEST_CASE("Auteur : sous un faux nom, le nom seul", "[net][notice]") {
  World world;
  ro::proto::ZcNotifyChat said;
  said.GID = kSomeActor;
  said.Message = "Masque : coucou";
  auto hidden =
      AuthorOf(ro::proto::ZcNotifyChat::kOpcode, ro::proto::ChatAuthorScope::kPublic, "Masque");
  hidden.fixed.account_id = 0;
  hidden.fixed.char_id = 0;
  CHECK(ApplyFrame(Authored(Encode(said), hidden), world, 0) == FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 1);
  REQUIRE(world.chat_lines()[0].author.has_value());
  CHECK(world.chat_lines()[0].author->name == "Masque");
  CHECK(world.chat_lines()[0].author->account_id == 0);
  CHECK(world.chat_lines()[0].author->char_id == 0);
}
