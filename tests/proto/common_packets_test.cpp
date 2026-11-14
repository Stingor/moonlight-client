#include "ro/proto/common_packets.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/packet_lengths.h"

using ro::proto::AcRefuseLogin;
using ro::proto::CaLogin;
using ro::proto::CharacterInfo;
using ro::proto::Decode;
using ro::proto::Encode;
using ro::proto::HcAcceptEnter;
using ro::proto::HcNotifyZonesvr;
using ro::proto::PacketLength;

TEST_CASE("Paquets : le refus mesuré sur le fil se décode", "[proto][packets]") {
  // Ces octets sont ceux qu'un serveur RÉEL a renvoyés à une demande de
  // connexion portant un compte inexistant. C'est le seul cas de ce fichier qui
  // ne soit pas fabriqué : il vérifie la disposition contre le monde, pas
  // contre le générateur qui l'a produite.
  const std::vector<std::uint8_t> frame = {0x3e, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  REQUIRE(frame.size() == AcRefuseLogin::kWireSize);

  AcRefuseLogin refusal;
  REQUIRE(Decode(frame, refusal));
  CHECK(refusal.error == 0);
  CHECK(refusal.unblock_time.empty());
}

TEST_CASE("Paquets : la taille déclarée est celle de la table", "[proto][packets]") {
  // Les deux fichiers générés sortent de la même exécution, mais de deux sondes
  // distinctes : l'une mesure la structure entière, l'autre somme ses champs.
  // Un désaccord donnerait un décodeur qui lit à côté sans le dire.
  const auto login = PacketLength(CaLogin::kOpcode);
  REQUIRE(login.has_value());
  CHECK(static_cast<std::size_t>(*login) == CaLogin::kWireSize);

  const auto refusal = PacketLength(AcRefuseLogin::kOpcode);
  REQUIRE(refusal.has_value());
  CHECK(static_cast<std::size_t>(*refusal) == AcRefuseLogin::kWireSize);

  // Un paquet à longueur variable : la table dit -1, la structure ne donne que
  // la taille de ce qui précède la queue.
  const auto enter = PacketLength(HcAcceptEnter::kOpcode);
  REQUIRE(enter.has_value());
  CHECK(*enter == ro::proto::kVariableLength);
}

TEST_CASE("Paquets : une demande de connexion s'encode puis se relit", "[proto][packets]") {
  CaLogin request;
  request.version = 20250716;
  request.username = "compte";
  request.password = "secret";
  request.clienttype = 0x0A;

  const std::vector<std::uint8_t> frame = Encode(request);
  REQUIRE(frame.size() == CaLogin::kWireSize);
  CHECK(frame[0] == 0x64);
  CHECK(frame[1] == 0x00);

  CaLogin reread;
  REQUIRE(Decode(frame, reread));
  CHECK(reread.version == request.version);
  CHECK(reread.username == request.username);
  CHECK(reread.password == request.password);
  CHECK(reread.clienttype == request.clienttype);
}

TEST_CASE("Paquets : un champ texte trop long est coupé au BORD du champ", "[proto][packets]") {
  // Ce qui compte n'est pas qu'il reste un octet libre, c'est que le champ ne
  // DÉBORDE pas sur le suivant : ici, le mot de passe à la suite du nom de
  // compte. Le champ est donc rempli jusqu'au bord, et s'arrête là.
  //
  // Le serveur, lui, tronque en lisant : il copie vingt-trois octets d'un champ
  // qui en fait vingt-quatre et termine la chaîne lui-même.
  // Largeur du champ dans PACKET_CA_LOGIN, telle que l'encodeur généré l'écrit.
  constexpr std::size_t kUsernameWidth = 24;

  CaLogin request;
  request.username = std::string(64, 'x');

  const std::vector<std::uint8_t> frame = Encode(request);
  REQUIRE(frame.size() == CaLogin::kWireSize);

  CaLogin reread;
  REQUIRE(Decode(frame, reread));
  CHECK(reread.username.size() == kUsernameWidth);
  CHECK(reread.password.empty());
}

TEST_CASE("Paquets : une trame de mauvaise longueur est refusée", "[proto][packets]") {
  std::vector<std::uint8_t> frame = Encode(CaLogin());
  frame.pop_back();

  CaLogin reread;
  CHECK_FALSE(Decode(frame, reread));
}

TEST_CASE("Paquets : une trame au mauvais opcode est refusée", "[proto][packets]") {
  // Sans ce contrôle, décoder une trame comme un autre paquet rendrait des
  // valeurs plausibles au lieu d'une erreur — le pire des symptômes.
  std::vector<std::uint8_t> frame = Encode(CaLogin());
  frame[0] = 0x65;

  CaLogin reread;
  CHECK_FALSE(Decode(frame, reread));
}

TEST_CASE("Paquets : une queue variable rend ses éléments un par un", "[proto][packets]") {
  HcAcceptEnter list;
  list.total = 2;
  list.extension = "ext";
  CharacterInfo first;
  first.GID = 1000;
  first.name = "Alice";
  first.level = 42;
  CharacterInfo second;
  second.GID = 2000;
  second.name = "Bob";
  second.level = 7;
  list.characters = {first, second};

  const std::vector<std::uint8_t> frame = Encode(list);
  REQUIRE(frame.size() == HcAcceptEnter::kWireSize + 2 * CharacterInfo::kWireSize);
  // La longueur annoncée est celle de la trame ENTIÈRE, queue comprise.
  CHECK(static_cast<std::size_t>(frame[2] | (frame[3] << 8)) == frame.size());

  HcAcceptEnter reread;
  REQUIRE(Decode(frame, reread));
  REQUIRE(reread.characters.size() == 2);
  CHECK(reread.characters[0].GID == 1000);
  CHECK(reread.characters[0].name == "Alice");
  CHECK(reread.characters[0].level == 42);
  CHECK(reread.characters[1].GID == 2000);
  CHECK(reread.characters[1].name == "Bob");
  CHECK(reread.total == 2);
  CHECK(reread.extension == "ext");
}

TEST_CASE("Paquets : une queue tronquée en plein élément est refusée", "[proto][packets]") {
  // La rendre amputée ferait lire un personnage dont la moitié des champs
  // viendrait du néant, et rien ne le signalerait.
  HcAcceptEnter list;
  list.characters = {CharacterInfo()};
  std::vector<std::uint8_t> frame = Encode(list);
  frame.pop_back();

  HcAcceptEnter reread;
  CHECK_FALSE(Decode(frame, reread));
}

TEST_CASE("Paquets : les champs larges gardent leur largeur mesurée", "[proto][packets]") {
  // Le serveur déclare ces champs int16 dans une branche et int64 dans une
  // autre ; c'est la seconde qui est active. Une lecture sur deux octets
  // décalerait TOUT le reste du personnage.
  CharacterInfo hero;
  hero.hp = 1'000'000'000LL;
  hero.maxhp = 2'000'000'000LL;
  hero.exp = 9'000'000'000LL;

  HcAcceptEnter list;
  list.characters = {hero};
  HcAcceptEnter reread;
  REQUIRE(Decode(Encode(list), reread));
  REQUIRE(reread.characters.size() == 1);
  CHECK(reread.characters[0].hp == hero.hp);
  CHECK(reread.characters[0].maxhp == hero.maxhp);
  CHECK(reread.characters[0].exp == hero.exp);
}

TEST_CASE("Paquets : l'adresse du serveur de zone se relit entière", "[proto][packets]") {
  // Le dernier paquet de la phase de personnage, et celui sans lequel on
  // n'entre jamais en jeu.
  HcNotifyZonesvr handoff;
  handoff.CID = 150'000;
  handoff.mapname = "prontera.gat";
  handoff.ip = 0x7F000001;
  handoff.port = 5121;

  HcNotifyZonesvr reread;
  REQUIRE(Decode(Encode(handoff), reread));
  CHECK(reread.CID == handoff.CID);
  CHECK(reread.mapname == "prontera.gat");
  CHECK(reread.ip == handoff.ip);
  CHECK(reread.port == 5121);
}
