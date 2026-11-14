#include "ro/net/session.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/packet_lengths.h"
#include "ro/proto/pincode.h"

#include "support/char_session.h"
#include "support/loopback_server.h"

using ro::net::CharacterPhase;
using ro::net::Connection;
using ro::net::LoginPhase;
using ro::net::MapPhase;
using ro::net::PhaseState;
using ro::net::ServerAddress;
using ro::test::LoopbackServer;
using ro::test_support::BareAccountId;
using ro::test_support::CharacterList;
using ro::test_support::Churn;
using ro::test_support::kBriefWaitMs;
using ro::test_support::kEnterRequestSize;
using ro::test_support::kLoopbackWireIp;
using ro::test_support::kRoundsPastTheList;
using ro::test_support::Settle;
using ro::test_support::SettleUntil;

namespace {

std::vector<std::uint8_t> AcceptedLogin() {
  ro::proto::AcAcceptLogin accepted;
  accepted.AID = 2'000'123;
  accepted.login_id1 = 111;
  accepted.login_id2 = 222;
  accepted.sex = 1;
  ro::proto::AcAcceptLoginSub first;
  first.name = "Moonlight";
  first.ip = kLoopbackWireIp;
  first.port = 6121;
  first.users = 42;
  ro::proto::AcAcceptLoginSub second;
  second.name = "Essai";
  second.ip = kLoopbackWireIp;
  second.port = 6122;
  accepted.char_servers = {first, second};
  return Encode(accepted);
}

// Les états du code PIN, tels que enum pincode_state les numérote dans
// src/char/char.hpp. Depuis PACKETVER 20180124, « accepté » vaut zéro.
constexpr std::uint16_t kPincodeAccepted = 0;
constexpr std::uint16_t kPincodeAsk = 1;
constexpr std::uint16_t kPincodeNew = 4;
constexpr std::uint16_t kPincodeWrong = 8;

// Deux graines dont les permutations diffèrent l'une de l'autre ET de
// l'identité. Il en existe qui ne brouillent rien ; les prendre ferait passer
// un test qui n'aurait rien vérifié.
constexpr std::uint32_t kSeedThatShuffles = 1234;
constexpr std::uint32_t kOtherSeedThatShuffles = 5678;

std::vector<std::uint8_t> PincodeState(std::uint16_t state, std::uint32_t seed) {
  ro::proto::HcSecondPasswdLogin announcement;
  announcement.seed = seed;
  announcement.result = state;
  return Encode(announcement);
}

// UNE LISTE À TROU : le personnage du milieu a été supprimé. Elle reste DENSE
// sur le fil — deux entrées —, et leurs emplacements sont 0 et 2.
std::vector<std::uint8_t> GappedCharacterList() {
  ro::proto::HcAcceptEnter list;
  list.total = 2;
  ro::proto::CharacterInfo first;
  first.GID = 150'000;
  first.name = "Alice";
  first.CharNum = 0;
  ro::proto::CharacterInfo third;
  third.GID = 150'002;
  third.name = "Carole";
  third.CharNum = 2;
  list.characters = {first, third};
  return Encode(list);
}

std::vector<std::uint8_t> EmptyCharacterList() {
  // Le compte qui vient d'être créé. Le serveur envoie bien la liste : elle ne
  // porte simplement personne.
  ro::proto::HcAcceptEnter list;
  list.total = 0;
  return Encode(list);
}

}  // namespace

TEST_CASE("Session : une adresse reçue se rend en notation pointée", "[net][session]") {
  // Le serveur écrit l'adresse en ordre réseau ; l'interpréter à l'envers
  // enverrait le client sur 1.0.0.127, une adresse qui existe.
  const ServerAddress address{kLoopbackWireIp, 6121};
  CHECK(address.host() == "127.0.0.1");
}

TEST_CASE("Session : une connexion acceptée rend les jetons et les serveurs", "[net][session]") {
  LoopbackServer server;
  REQUIRE(server.listening());

  auto phase = LoginPhase::Start("127.0.0.1", server.port(), "compte", "motdepasse");
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());

  // Ce que le client a envoyé se relit avec le décodeur généré : la demande est
  // donc bien formée, et pas seulement acceptée par un banc complaisant.
  const std::vector<std::uint8_t> request = server.Receive(ro::proto::CaLogin::kWireSize);
  REQUIRE(request.size() == ro::proto::CaLogin::kWireSize);
  ro::proto::CaLogin sent;
  REQUIRE(Decode(request, sent));
  CHECK(sent.username == "compte");
  CHECK(sent.password == "motdepasse");
  CHECK(sent.version == static_cast<std::uint32_t>(ro::proto::kPacketVersion));

  REQUIRE(server.Send(AcceptedLogin()));
  REQUIRE(Settle(*phase) == PhaseState::kDone);

  CHECK(phase->account().account_id == 2'000'123);
  CHECK(phase->account().login_id1 == 111);
  CHECK(phase->account().login_id2 == 222);
  CHECK(phase->account().sex == 1);
  REQUIRE(phase->servers().size() == 2);
  CHECK(phase->servers()[0].name == "Moonlight");
  CHECK(phase->servers()[0].address.host() == "127.0.0.1");
  CHECK(phase->servers()[0].address.port == 6121);
  CHECK(phase->servers()[0].users == 42);
  CHECK(phase->servers()[1].address.port == 6122);
}

TEST_CASE("Session : un refus rend son code, pas une erreur", "[net][session]") {
  // Un refus est une réponse valide du serveur : le confondre avec une panne
  // ferait afficher « connexion impossible » là où il faut dire « mot de passe
  // incorrect ».
  constexpr std::uint32_t kWrongPassword = 1;
  LoopbackServer server;
  REQUIRE(server.listening());

  auto phase = LoginPhase::Start("127.0.0.1", server.port(), "compte", "faux");
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());

  ro::proto::AcRefuseLogin refusal;
  refusal.error = kWrongPassword;
  REQUIRE(server.Send(Encode(refusal)));

  CHECK(Settle(*phase) == PhaseState::kRefused);
  CHECK(phase->refusal_code() == kWrongPassword);
}

TEST_CASE("Session : un serveur qui ferme sans répondre est un échec", "[net][session]") {
  // Une phase se termine toujours sur un paquet. Un silence suivi d'une
  // fermeture n'est pas un déroulement normal, et le prendre pour tel laisserait
  // la boucle attendre sans fin.
  LoopbackServer server;
  REQUIRE(server.listening());

  auto phase = LoginPhase::Start("127.0.0.1", server.port(), "compte", "motdepasse");
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());
  server.CloseClient();

  CHECK(Settle(*phase) == PhaseState::kFailed);
  CHECK_FALSE(phase->error().detail.empty());
}

TEST_CASE("Session : l'identifiant nu n'est pas pris pour une trame", "[net][session]") {
  // Le serveur de personnages répond à la demande d'entrée par quatre octets
  // SANS opcode. Les découper reviendrait à lire un opcode dans un numéro de
  // compte : 2 000 123 commence par 7b 80, soit l'opcode 0x807b, inconnu de la
  // table — la session serait perdue à son deuxième paquet.
  constexpr std::uint32_t kAccountId = 2'000'123;
  LoopbackServer server;
  REQUIRE(server.listening());

  LoginPhase::Account account;
  account.account_id = kAccountId;
  account.login_id1 = 111;
  account.login_id2 = 222;
  account.sex = 1;

  auto phase = CharacterPhase::Start(ServerAddress{kLoopbackWireIp, server.port()}, account);
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());

  const std::vector<std::uint8_t> request = server.Receive(kEnterRequestSize);
  REQUIRE(request.size() == kEnterRequestSize);
  CHECK(request[0] == 0x65);
  CHECK(request[1] == 0x00);
  CHECK(request[16] == 1);

  REQUIRE(server.Send(BareAccountId(kAccountId)));
  REQUIRE(server.Send(CharacterList()));

  REQUIRE(SettleUntil(*phase, CharacterPhase::Step::kAwaitingSelection) == PhaseState::kWaiting);
  REQUIRE(phase->step() == CharacterPhase::Step::kAwaitingSelection);
  REQUIRE(phase->characters().size() == 2);
  CHECK(phase->characters()[0].name == "Alice");
  CHECK(phase->characters()[1].name == "Bob");
}

TEST_CASE("Session : le choix d'un personnage mène à l'adresse de zone", "[net][session]") {
  constexpr std::uint8_t kChosenSlot = 1;
  constexpr std::uint16_t kZonePort = 5121;
  LoopbackServer server;
  REQUIRE(server.listening());

  LoginPhase::Account account;
  account.account_id = 2'000'123;
  auto phase = CharacterPhase::Start(ServerAddress{kLoopbackWireIp, server.port()}, account);
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());
  REQUIRE(server.Receive(kEnterRequestSize).size() == kEnterRequestSize);
  REQUIRE(server.Send(BareAccountId(account.account_id)));
  REQUIRE(server.Send(CharacterList()));
  REQUIRE(SettleUntil(*phase, CharacterPhase::Step::kAwaitingSelection) == PhaseState::kWaiting);

  phase->Select(kChosenSlot);
  const std::vector<std::uint8_t> choice = server.Receive(ro::proto::ChSelectChar::kWireSize);
  ro::proto::ChSelectChar selected;
  REQUIRE(Decode(choice, selected));
  CHECK(selected.slot == kChosenSlot);

  ro::proto::HcNotifyZonesvr handoff;
  handoff.CID = 150'001;
  handoff.mapname = "prontera.gat";
  handoff.ip = kLoopbackWireIp;
  handoff.port = kZonePort;
  REQUIRE(server.Send(Encode(handoff)));

  REQUIRE(Settle(*phase) == PhaseState::kDone);
  CHECK(phase->handoff().character_id == 150'001);
  // Le serveur nomme la carte du nom de son fichier de marchabilité ; le reste
  // du client la désigne par le tronc commun de ses TROIS fichiers.
  CHECK(phase->handoff().map_name == "prontera");
  CHECK(phase->handoff().address.host() == "127.0.0.1");
  CHECK(phase->handoff().address.port == kZonePort);
}

TEST_CASE("Session : l'emplacement n'est pas le rang dans la liste", "[net][session]") {
  // 🔴 CE QUE LA PREMIÈRE SUPPRESSION RÉVÈLE. Le serveur envoie la liste DENSE
  // — deux entrées pour un compte qui occupe les emplacements 0 et 2 — et c'est
  // `CharNum` qui porte l'emplacement de chacun. Tant que le compte n'a pas de
  // trou, rang et emplacement coïncident, et la confusion ne se voit pas.
  //
  // ⚠ Et c'est l'EMPLACEMENT que le serveur résout (`char_num = slot`) : lui
  // envoyer un rang ferait entrer en jeu avec un AUTRE personnage — ici Alice
  // au lieu de Carole — ou refuser, quand le rang ne désigne personne.
  const std::vector<ro::proto::CharacterInfo> listed = {[] {
                                                          ro::proto::CharacterInfo who;
                                                          who.name = "Alice";
                                                          who.CharNum = 0;
                                                          return who;
                                                        }(),
                                                        [] {
                                                          ro::proto::CharacterInfo who;
                                                          who.name = "Carole";
                                                          who.CharNum = 2;
                                                          return who;
                                                        }()};

  CHECK(ro::net::IndexOfSlot(listed, 0) == 0);
  CHECK(ro::net::IndexOfSlot(listed, 2) == 1);
  // L'emplacement vide n'est PAS le rang 1, et il n'a personne.
  CHECK(ro::net::IndexOfSlot(listed, 1) == -1);
  CHECK(ro::net::IndexOfSlot(listed, 3) == -1);
  CHECK(ro::net::IndexOfSlot({}, 0) == -1);
}

TEST_CASE("Session : un compte à trou choisit par emplacement", "[net][session]") {
  // Le même fait, de bout en bout : l'emplacement 2 part sur le fil tel quel,
  // et il ne vaut pas le rang 1 que ce personnage occupe dans la liste.
  constexpr std::uint8_t kGappedSlot = 2;
  LoopbackServer server;
  REQUIRE(server.listening());

  LoginPhase::Account account;
  account.account_id = 2'000'123;
  auto phase = CharacterPhase::Start(ServerAddress{kLoopbackWireIp, server.port()}, account);
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());
  REQUIRE(server.Receive(kEnterRequestSize).size() == kEnterRequestSize);
  REQUIRE(server.Send(BareAccountId(account.account_id)));
  REQUIRE(server.Send(GappedCharacterList()));
  REQUIRE(SettleUntil(*phase, CharacterPhase::Step::kAwaitingSelection) == PhaseState::kWaiting);

  REQUIRE(phase->characters().size() == 2);
  const int index = ro::net::IndexOfSlot(phase->characters(), kGappedSlot);
  REQUIRE(index == 1);
  CHECK(phase->characters()[static_cast<std::size_t>(index)].name == "Carole");

  phase->Select(kGappedSlot);
  ro::proto::ChSelectChar selected;
  REQUIRE(Decode(server.Receive(ro::proto::ChSelectChar::kWireSize), selected));
  CHECK(selected.slot == kGappedSlot);
}

TEST_CASE("Session : le nombre d'emplacements vient d'un AUTRE paquet", "[net][session]") {
  // 🔴 LA LISTE NE DIT PAS COMBIEN DE PLACES EXISTENT, elle dit qui les occupe.
  // Un compte neuf a zéro personnage et plusieurs emplacements : un écran qui
  // compterait les personnages pour dessiner des places libres n'en montrerait
  // aucune, donc n'offrirait jamais de créer. Le nombre vient de
  // `HC_ACCEPT_ENTER2` (0x082d), que le serveur envoie JUSTE AVANT la liste.
  constexpr std::uint8_t kProducible = 9;
  constexpr std::uint8_t kTotal = 15;
  LoopbackServer server;
  REQUIRE(server.listening());

  LoginPhase::Account account;
  account.account_id = 2'000'123;
  auto phase = CharacterPhase::Start(ServerAddress{kLoopbackWireIp, server.port()}, account);
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());
  REQUIRE(server.Receive(kEnterRequestSize).size() == kEnterRequestSize);
  REQUIRE(server.Send(BareAccountId(account.account_id)));

  CHECK(phase->producible_slots() == 0);

  ro::proto::HcAcceptEnter2 room;
  room.normal = kProducible;
  room.producible = kProducible;
  room.total = kTotal;
  REQUIRE(server.Send(Encode(room)));
  REQUIRE(server.Send(EmptyCharacterList()));
  REQUIRE(SettleUntil(*phase, CharacterPhase::Step::kAwaitingSelection) == PhaseState::kWaiting);

  CHECK(phase->characters().empty());
  CHECK(phase->producible_slots() == kProducible);
  CHECK(phase->total_slots() == kTotal);
}

TEST_CASE("Session : choisir avant d'avoir la liste ne fait rien", "[net][session]") {
  // L'emplacement d'un personnage qu'on n'a pas encore vu ne veut rien dire.
  // Envoyer quand même mettrait le serveur et le client en désaccord sur ce que
  // le client croit avoir choisi.
  LoopbackServer server;
  REQUIRE(server.listening());

  auto phase =
      CharacterPhase::Start(ServerAddress{kLoopbackWireIp, server.port()}, LoginPhase::Account());
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());
  REQUIRE(server.Receive(kEnterRequestSize).size() == kEnterRequestSize);

  phase->Select(0);
  CHECK(phase->step() == CharacterPhase::Step::kAwaitingAccountId);
  // Rien de plus que la demande d'entrée n'est parti.
  CHECK(server.Receive(1, 50).empty());
}

TEST_CASE("Session : l'entrée sur la carte va jusqu'au monde", "[net][session]") {
  // Le troisième acte. Le serveur de zone répond comme celui des personnages —
  // un identifiant de compte nu — puis accepte, et le client doit alors dire que
  // sa carte est chargée. Sans cet avis, le serveur n'envoie jamais le monde.
  constexpr std::uint32_t kClientTick = 987'654;
  LoopbackServer server;
  REQUIRE(server.listening());

  LoginPhase::Account account;
  account.account_id = 2'000'123;
  account.login_id1 = 111;
  account.sex = 1;
  CharacterPhase::Handoff handoff;
  handoff.character_id = 150'001;
  handoff.map_name = "prontera.gat";
  handoff.address = ServerAddress{kLoopbackWireIp, server.port()};

  auto phase = MapPhase::Start(handoff, account, kClientTick);
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());

  // La demande porte bien l'opcode 0x086a, et non le 0x0072 de la
  // documentation ancienne — qui est l'usage d'une compétence à cette version.
  const std::vector<std::uint8_t> request = server.Receive(ro::proto::EnterMapRequest::kWireSize);
  REQUIRE(request.size() == ro::proto::EnterMapRequest::kWireSize);
  CHECK(request[0] == 0x6A);
  CHECK(request[1] == 0x08);

  REQUIRE(server.Send(BareAccountId(account.account_id)));

  ro::proto::ZcAcceptEnter accepted;
  accepted.startTime = 555'000;
  accepted.posDir = {0x0A, 0x1B, 0x2C};
  REQUIRE(server.Send(Encode(accepted)));

  REQUIRE(Settle(*phase) == PhaseState::kDone);
  CHECK(phase->step() == MapPhase::Step::kInGame);
  CHECK(phase->spawn().start_tick == 555'000);
  CHECK(phase->spawn().packed_position[0] == 0x0A);
  CHECK(phase->spawn().packed_position[2] == 0x2C);

  // L'avis de carte chargée est parti tout seul.
  const std::vector<std::uint8_t> loaded = server.Receive(2);
  REQUIRE(loaded.size() == 2);
  CHECK(loaded[0] == 0x7D);
  CHECK(loaded[1] == 0x00);

  // La liaison est remise à l'appelant, toujours ouverte : le monde arrive
  // dessus.
  Connection link = phase->TakeLink();
  CHECK(link.is_open());
}

TEST_CASE("Session : ce qui entoure l'accord d'entrée revient au jeu", "[net][session]") {
  // Le serveur de zone envoie la liste d'amis dans la foulée de son accord.
  // Arrivé dans la même lecture que lui, ce qui le suit doit ressortir au premier
  // relevé du jeu, dans l'ordre du fil.
  constexpr std::uint16_t kFriendsListOpcode = ro::proto::ZcFriendsList::kOpcode;
  constexpr std::size_t kFriendEntrySize = ro::proto::ZcFriendsListSub::kWireSize;
  constexpr std::size_t kListHeaderSize = ro::proto::ZcFriendsList::kWireSize;
  LoopbackServer server;
  REQUIRE(server.listening());

  LoginPhase::Account account;
  account.account_id = 2'000'321;
  CharacterPhase::Handoff handoff;
  handoff.address = ServerAddress{kLoopbackWireIp, server.port()};
  auto phase = MapPhase::Start(handoff, account, 0);
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());
  REQUIRE(server.Receive(ro::proto::EnterMapRequest::kWireSize).size() ==
          ro::proto::EnterMapRequest::kWireSize);

  // Une liste d'un ami, puis une liste vide : leurs longueurs les distinguent à
  // l'arrivée.
  const auto list_of = [](std::size_t entries) {
    const std::size_t length = kListHeaderSize + entries * kFriendEntrySize;
    std::vector<std::uint8_t> frame(length, 0);
    frame[0] = static_cast<std::uint8_t>(kFriendsListOpcode & 0xFF);
    frame[1] = static_cast<std::uint8_t>(kFriendsListOpcode >> 8);
    frame[2] = static_cast<std::uint8_t>(length & 0xFF);
    frame[3] = static_cast<std::uint8_t>(length >> 8);
    return frame;
  };
  std::vector<std::uint8_t> burst = BareAccountId(account.account_id);
  const std::vector<std::uint8_t> accept = Encode(ro::proto::ZcAcceptEnter());
  const std::vector<std::uint8_t> first = list_of(1);
  const std::vector<std::uint8_t> second = list_of(0);
  burst.insert(burst.end(), accept.begin(), accept.end());
  burst.insert(burst.end(), first.begin(), first.end());
  burst.insert(burst.end(), second.begin(), second.end());
  REQUIRE(server.Send(burst));

  REQUIRE(Settle(*phase) == PhaseState::kDone);
  Connection link = phase->TakeLink();
  const Connection::Polled polled = link.Poll();
  REQUIRE(polled.frames.size() == 2);
  CHECK(polled.frames[0].opcode == kFriendsListOpcode);
  CHECK(polled.frames[0].bytes.size() == first.size());
  CHECK(polled.frames[1].opcode == kFriendsListOpcode);
  CHECK(polled.frames[1].bytes.size() == second.size());
  // Rendues une fois, pas deux.
  CHECK(link.Poll().frames.empty());
}

TEST_CASE("Session : un identifiant qui commence comme le refus entre quand même",
          "[net][session]") {
  // Les 16 bits bas de ce compte valent 0x006A, l'opcode du refus de la zone :
  // ses quatre premiers octets reçus ressemblent à la tête d'un refus. C'est ce
  // qui les SUIT — l'acceptation — qui tranche.
  constexpr std::uint32_t kAccountLikeRefusal = 0x001F'006A;
  LoopbackServer server;
  REQUIRE(server.listening());

  LoginPhase::Account account;
  account.account_id = kAccountLikeRefusal;
  CharacterPhase::Handoff handoff;
  handoff.address = ServerAddress{kLoopbackWireIp, server.port()};
  auto phase = MapPhase::Start(handoff, account, 0);
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());
  REQUIRE(server.Receive(ro::proto::EnterMapRequest::kWireSize).size() ==
          ro::proto::EnterMapRequest::kWireSize);

  REQUIRE(server.Send(BareAccountId(kAccountLikeRefusal)));
  REQUIRE(server.Send(Encode(ro::proto::ZcAcceptEnter())));

  CHECK(Settle(*phase) == PhaseState::kDone);
  CHECK(phase->step() == MapPhase::Step::kInGame);
}

TEST_CASE("Session : le serveur de zone refuse avec l'ANCIENNE forme", "[net][session]") {
  // Le serveur de connexion refuse par 0x083e sur 26 octets ; celui de zone par
  // 0x006a sur 23, dont trois seulement portent quelque chose. Employer le
  // mauvais décodeur lirait un code au hasard.
  constexpr std::uint8_t kRejected = 3;
  LoopbackServer server;
  REQUIRE(server.listening());

  CharacterPhase::Handoff handoff;
  handoff.address = ServerAddress{kLoopbackWireIp, server.port()};
  auto phase = MapPhase::Start(handoff, LoginPhase::Account(), 0);
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());
  REQUIRE(server.Receive(ro::proto::EnterMapRequest::kWireSize).size() ==
          ro::proto::EnterMapRequest::kWireSize);

  std::vector<std::uint8_t> refusal(23, 0);
  refusal[0] = 0x6A;
  refusal[1] = 0x00;
  refusal[2] = kRejected;
  REQUIRE(server.Send(refusal));

  CHECK(Settle(*phase) == PhaseState::kRefused);
  CHECK(phase->refusal_code() == kRejected);
}

TEST_CASE("Session : une demande de code PIN suspend le choix", "[net][session]") {
  // Choisir un personnage avant d'avoir répondu ferait COUPER la liaison : le
  // serveur n'accepte qu'une poignée de paquets tant que le code manque.
  LoopbackServer server;
  REQUIRE(server.listening());

  auto phase =
      CharacterPhase::Start(ServerAddress{kLoopbackWireIp, server.port()}, LoginPhase::Account());
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());
  REQUIRE(server.Receive(kEnterRequestSize).size() == kEnterRequestSize);
  REQUIRE(server.Send(BareAccountId(0)));
  REQUIRE(server.Send(PincodeState(kPincodeAsk, 4242)));

  CHECK(SettleUntil(*phase, CharacterPhase::Step::kAwaitingPincode) == PhaseState::kWaiting);
  CHECK(phase->step() == CharacterPhase::Step::kAwaitingPincode);
  CHECK_FALSE(phase->pincode_refused());
}

TEST_CASE("Session : le code PIN ne part pas en clair", "[net][session]") {
  // Le serveur attend les ANTÉCÉDENTS des chiffres par la permutation qu'il a
  // tirée ; c'est proto::ObfuscatePincode qui les calcule, et son test prouve
  // que le serveur retrouve bien le code. Ici, on vérifie seulement que c'est
  // cette forme-là qui part, sur exactement quatre octets.
  LoopbackServer server;
  REQUIRE(server.listening());

  auto phase =
      CharacterPhase::Start(ServerAddress{kLoopbackWireIp, server.port()}, LoginPhase::Account());
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());
  REQUIRE(server.Receive(kEnterRequestSize).size() == kEnterRequestSize);
  REQUIRE(server.Send(BareAccountId(0)));
  REQUIRE(server.Send(PincodeState(kPincodeAsk, kSeedThatShuffles)));
  REQUIRE(SettleUntil(*phase, CharacterPhase::Step::kAwaitingPincode) == PhaseState::kWaiting);

  REQUIRE(phase->SubmitPincode("9876").has_value());

  const std::vector<std::uint8_t> sent = server.Receive(ro::proto::ChSecondPasswdAck::kWireSize);
  REQUIRE(sent.size() == ro::proto::ChSecondPasswdAck::kWireSize);
  ro::proto::ChSecondPasswdAck answer;
  REQUIRE(ro::proto::Decode(sent, answer));
  CHECK(answer.pin.size() == ro::proto::kPincodeLength);
  CHECK(answer.pin != "9876");
}

TEST_CASE("Session : un code accepté rend la main au choix", "[net][session]") {
  // Le serveur envoie la liste AVANT de parler du code. Repartir en attente de
  // liste après l'acceptation attendrait une trame qui est déjà passée.
  LoopbackServer server;
  REQUIRE(server.listening());

  auto phase =
      CharacterPhase::Start(ServerAddress{kLoopbackWireIp, server.port()}, LoginPhase::Account());
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());
  REQUIRE(server.Receive(kEnterRequestSize).size() == kEnterRequestSize);
  REQUIRE(server.Send(BareAccountId(0)));
  REQUIRE(server.Send(CharacterList()));
  REQUIRE(server.Send(PincodeState(kPincodeAsk, kSeedThatShuffles)));
  REQUIRE(SettleUntil(*phase, CharacterPhase::Step::kAwaitingPincode) == PhaseState::kWaiting);
  REQUIRE(phase->SubmitPincode("1234").has_value());
  REQUIRE(server.Receive(ro::proto::ChSecondPasswdAck::kWireSize).size() ==
          ro::proto::ChSecondPasswdAck::kWireSize);

  REQUIRE(server.Send(PincodeState(kPincodeAccepted, 0)));

  CHECK(SettleUntil(*phase, CharacterPhase::Step::kAwaitingSelection) == PhaseState::kWaiting);
  CHECK(phase->step() == CharacterPhase::Step::kAwaitingSelection);
  CHECK(phase->characters().size() == 2);
}

TEST_CASE("Session : un code refusé se redemande avec la graine NEUVE", "[net][session]") {
  // Le serveur tire une graine à chaque annonce, y compris pour dire « faux ».
  // Rejouer la précédente ferait refuser un code pourtant juste, indéfiniment.
  LoopbackServer server;
  REQUIRE(server.listening());

  auto phase =
      CharacterPhase::Start(ServerAddress{kLoopbackWireIp, server.port()}, LoginPhase::Account());
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());
  REQUIRE(server.Receive(kEnterRequestSize).size() == kEnterRequestSize);
  REQUIRE(server.Send(BareAccountId(0)));
  REQUIRE(server.Send(PincodeState(kPincodeAsk, kSeedThatShuffles)));
  REQUIRE(SettleUntil(*phase, CharacterPhase::Step::kAwaitingPincode) == PhaseState::kWaiting);
  REQUIRE(phase->SubmitPincode("9876").has_value());
  const std::vector<std::uint8_t> first = server.Receive(ro::proto::ChSecondPasswdAck::kWireSize);
  REQUIRE(first.size() == ro::proto::ChSecondPasswdAck::kWireSize);

  REQUIRE(server.Send(PincodeState(kPincodeWrong, kOtherSeedThatShuffles)));
  REQUIRE(SettleUntil(*phase, CharacterPhase::Step::kAwaitingPincode) == PhaseState::kWaiting);
  CHECK(phase->pincode_refused());

  REQUIRE(phase->SubmitPincode("9876").has_value());
  const std::vector<std::uint8_t> second = server.Receive(ro::proto::ChSecondPasswdAck::kWireSize);
  REQUIRE(second.size() == ro::proto::ChSecondPasswdAck::kWireSize);

  // Même code, deux graines : deux trames. C'est la preuve que la seconde
  // n'emploie pas la première graine.
  CHECK(first != second);
}

TEST_CASE("Session : un code PIN mal formé ne part pas", "[net][session]") {
  // Le serveur coupe la liaison sans un mot sur un code qui n'a pas quatre
  // chiffres. Le refuser ici permet de le DIRE.
  LoopbackServer server;
  REQUIRE(server.listening());

  auto phase =
      CharacterPhase::Start(ServerAddress{kLoopbackWireIp, server.port()}, LoginPhase::Account());
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());
  REQUIRE(server.Receive(kEnterRequestSize).size() == kEnterRequestSize);
  REQUIRE(server.Send(BareAccountId(0)));
  REQUIRE(server.Send(PincodeState(kPincodeAsk, kSeedThatShuffles)));
  REQUIRE(SettleUntil(*phase, CharacterPhase::Step::kAwaitingPincode) == PhaseState::kWaiting);

  CHECK_FALSE(phase->SubmitPincode("12").has_value());
  CHECK(phase->step() == CharacterPhase::Step::kAwaitingPincode);
}

TEST_CASE("Session : un code PIN à créer est signalé, pas subi", "[net][session]") {
  // Créer ou changer un code est un AUTRE échange, que ce client ne mène pas.
  // Prétendre le contraire ferait attendre une liste qui ne viendra jamais.
  LoopbackServer server;
  REQUIRE(server.listening());

  auto phase =
      CharacterPhase::Start(ServerAddress{kLoopbackWireIp, server.port()}, LoginPhase::Account());
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());
  REQUIRE(server.Receive(kEnterRequestSize).size() == kEnterRequestSize);
  REQUIRE(server.Send(BareAccountId(0)));
  REQUIRE(server.Send(PincodeState(kPincodeNew, 1)));

  CHECK(Settle(*phase) == PhaseState::kFailed);
  CHECK(phase->error().detail.find("PIN") != std::string::npos);
}

TEST_CASE("Session : un compte SANS personnage arrive tout de même au choix", "[net][session]") {
  // C'EST LE CAS DU JOUEUR QUI VIENT DE S'INSCRIRE, et c'est celui qu'une liste
  // VIDE trahissait : l'annonce de code PIN qui suit la liste relisait le
  // NOMBRE de personnages pour savoir si la liste était arrivée, et une liste
  // vide se lit exactement comme une liste absente. La phase repartait attendre
  // ce qu'elle avait déjà reçu, jusqu'au délai — après quoi l'appelant
  // annonçait « le serveur n'a pas répondu » d'un serveur qui avait répondu.
  LoopbackServer server;
  REQUIRE(server.listening());

  auto phase =
      CharacterPhase::Start(ServerAddress{kLoopbackWireIp, server.port()}, LoginPhase::Account());
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());
  REQUIRE(server.Receive(kEnterRequestSize).size() == kEnterRequestSize);
  REQUIRE(server.Send(BareAccountId(0)));
  REQUIRE(server.Send(EmptyCharacterList()));
  REQUIRE(server.Send(PincodeState(kPincodeAccepted, kSeedThatShuffles)));

  Churn(*phase, kRoundsPastTheList);
  CHECK(phase->step() == CharacterPhase::Step::kAwaitingSelection);
  CHECK(phase->characters().empty());
}

TEST_CASE("Session : l'avis d'éviction coupe la phase, avec son motif", "[net][session]") {
  // 0x0081 est le paquet du compte banni, du « déjà connecté » et du renvoi
  // pour intégrité. Le serveur appelle `set_eof` juste après l'avoir envoyé :
  // l'ignorer laissait la phase attendre une réponse qui ne viendrait pas, puis
  // conclure à une fermeture muette.
  constexpr std::uint8_t kAlreadyLoggedIn = 2;
  LoopbackServer server;
  REQUIRE(server.listening());

  auto phase = LoginPhase::Start("127.0.0.1", server.port(), "compte", "motdepasse");
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());
  REQUIRE(server.Receive(ro::proto::CaLogin::kWireSize).size() == ro::proto::CaLogin::kWireSize);

  ro::proto::ScNotifyBan ban;
  ban.result = kAlreadyLoggedIn;
  REQUIRE(server.Send(Encode(ban)));

  CHECK(Settle(*phase) == PhaseState::kRefused);
  // ⚠ LE TEXTE ET NON LE CODE : deux paquets refusent cette phase, et leurs
  // codes se lisent dans des tables DIFFÉRENTES. Rendre le seul nombre
  // laisserait l'appelant choisir la mauvaise.
  CHECK_FALSE(phase->refusal_text().empty());
}

TEST_CASE("Session : l'éviction coupe aussi le serveur de personnages", "[net][session]") {
  // Le serveur de personnages émet le MÊME paquet, et c'est par lui qu'il dit
  // « quelqu'un d'autre s'est connecté avec cet identifiant ». L'écran de
  // sélection peut rester ouvert des minutes : c'est là qu'on se fait évincer.
  constexpr std::uint8_t kServerClosed = 1;
  LoopbackServer server;
  REQUIRE(server.listening());

  auto phase =
      CharacterPhase::Start(ServerAddress{kLoopbackWireIp, server.port()}, LoginPhase::Account());
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());
  REQUIRE(server.Receive(kEnterRequestSize).size() == kEnterRequestSize);
  REQUIRE(server.Send(BareAccountId(0)));
  REQUIRE(server.Send(CharacterList()));

  ro::proto::ScNotifyBan ban;
  ban.result = kServerClosed;
  REQUIRE(server.Send(Encode(ban)));

  CHECK(Settle(*phase) == PhaseState::kRefused);
  CHECK_FALSE(phase->refusal_text().empty());
}

TEST_CASE("Session : le battement tient la liaison de personnages", "[net][session]") {
  // Le serveur ferme une session dont il n'a rien reçu depuis `stall_time`
  // secondes — soixante par défaut — et l'écran de sélection peut rester ouvert
  // bien plus longtemps. Aucun paquet ne partait de cette liaison une fois la
  // liste reçue.
  constexpr std::uint32_t kAccountId = 2'000'123;
  LoopbackServer server;
  REQUIRE(server.listening());

  LoginPhase::Account account;
  account.account_id = kAccountId;
  auto phase = CharacterPhase::Start(ServerAddress{kLoopbackWireIp, server.port()}, account);
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());
  REQUIRE(server.Receive(kEnterRequestSize).size() == kEnterRequestSize);
  REQUIRE(server.Send(BareAccountId(kAccountId)));
  REQUIRE(server.Send(CharacterList()));
  REQUIRE(SettleUntil(*phase, CharacterPhase::Step::kAwaitingSelection) == PhaseState::kWaiting);

  // AVANT L'HEURE, RIEN NE PART. Battre à chaque image enverrait un paquet par
  // milliseconde, et c'est le serveur qui les compte.
  phase->Beat(0);
  phase->Beat(ro::net::kCharacterPingIntervalMs - 1);
  CHECK(server.Receive(ro::proto::Ping::kWireSize, kBriefWaitMs).empty());

  // 🔴 ET L'IDENTIFIANT DOIT ÊTRE LE BON : `chclif_parse_keepalive` REFUSE la
  // trame dont l'identifiant ne correspond pas au compte de la session, et un
  // refus de lecture ferme la liaison. Un battement mal rempli tuerait
  // exactement ce qu'il vient tenir en vie.
  phase->Beat(ro::net::kCharacterPingIntervalMs);
  const std::vector<std::uint8_t> beat = server.Receive(ro::proto::Ping::kWireSize);
  REQUIRE(beat.size() == ro::proto::Ping::kWireSize);
  ro::proto::Ping sent;
  REQUIRE(Decode(beat, sent));
  CHECK(sent.AID == kAccountId);
}

TEST_CASE("Session : la tête d'un flux de zone dit ce qu'il faut sauter", "[net][session]") {
  constexpr std::uint32_t kAccountLikeRefusal = 0x001F'006A;
  constexpr std::size_t kTooShortToTell = 5;
  std::vector<std::uint8_t> accepted = BareAccountId(kAccountLikeRefusal);
  const std::vector<std::uint8_t> accept = Encode(ro::proto::ZcAcceptEnter());
  accepted.insert(accepted.end(), accept.begin(), accept.end());

  // L'identifiant nu devant l'acceptation : quatre octets à sauter, même quand
  // il commence comme l'opcode du refus.
  CHECK(ro::net::ZoneStreamPreamble(accepted) == 4U);
  // Une trame d'emblée : rien à sauter.
  CHECK(ro::net::ZoneStreamPreamble(accept) == 0U);
  // Trop court pour trancher : on attend.
  CHECK_FALSE(ro::net::ZoneStreamPreamble(std::span(accepted).first(kTooShortToTell)).has_value());
}
