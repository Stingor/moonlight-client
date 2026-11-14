#include "ro/proto/map_packets.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/packet_lengths.h"

using ro::proto::CzMoonlightJump;
using ro::proto::Decode;
using ro::proto::Encode;
using ro::proto::PacketIdleUnit;
using ro::proto::PacketLength;
using ro::proto::ZcAcceptEnter;
using ro::proto::ZcNotifyPlayerchat;
using ro::proto::ZcNotifyTime;
using ro::proto::ZcNpcackMapmove;

TEST_CASE("Jeu : l'unité au repos se relit entière", "[proto][map]") {
  // C'est le paquet qui met un personnage à l'écran. Une erreur d'un seul octet
  // dans sa disposition décale tout ce qui suit, et le personnage apparaît avec
  // le métier d'un autre.
  PacketIdleUnit unit;
  unit.objecttype = 0;
  unit.AID = 2'000'123;
  unit.GID = 150'001;
  unit.speed = 150;
  unit.job = 4054;
  unit.head = 12;
  unit.weapon = 1201;
  unit.clevel = 99;
  unit.maxHP = 12'345;
  unit.HP = 6'000;
  unit.PosDir = {0x1A, 0x2B, 0x3C};
  unit.name = "Alice";

  const std::vector<std::uint8_t> frame = Encode(unit);
  REQUIRE(frame.size() == PacketIdleUnit::kWireSize);
  CHECK(frame[0] == 0xFF);
  CHECK(frame[1] == 0x09);

  PacketIdleUnit reread;
  REQUIRE(Decode(frame, reread));
  CHECK(reread.AID == unit.AID);
  CHECK(reread.GID == unit.GID);
  CHECK(reread.job == unit.job);
  CHECK(reread.weapon == unit.weapon);
  CHECK(reread.clevel == unit.clevel);
  CHECK(reread.maxHP == unit.maxHP);
  CHECK(reread.HP == unit.HP);
  CHECK(reread.PosDir == unit.PosDir);
  CHECK(reread.name == "Alice");
}

TEST_CASE("Jeu : la taille des structures s'accorde avec la table", "[proto][map]") {
  // Deux sondes distinctes du même compilateur : l'une mesure la structure
  // entière, l'autre somme ses champs depuis leurs décalages.
  const auto jump = PacketLength(CzMoonlightJump::kOpcode);
  REQUIRE(jump.has_value());
  CHECK(static_cast<std::size_t>(*jump) == CzMoonlightJump::kWireSize);

  // L'unité au repos annonce sa longueur, bien qu'elle n'ait pas de queue : le
  // serveur y écrit sizeof de sa structure. kWireSize ne décrit donc que la
  // partie que la structure couvre, et la table dit « variable ».
  const auto idle = PacketLength(PacketIdleUnit::kOpcode);
  REQUIRE(idle.has_value());
  CHECK(*idle == ro::proto::kVariableLength);
  CHECK(PacketIdleUnit::kWireSize == 108);
}

TEST_CASE("Jeu : la table suit la structure là où le serveur se contredit", "[proto][map]") {
  // Ce n'est pas la table du serveur qui écrit sur le fil, c'est
  // clif_send(&packet, sizeof(packet)). Trois structures ont grandi d'une
  // version à l'autre sans que leur entrée de table suive : le serveur envoie
  // alors plus d'octets qu'il n'en déclare.
  //
  // Le client, troisième source, donne raison à la structure
  // pour les trois. Découper sur la longueur annoncée par la table perdrait le
  // flux au premier envoi — et 0x0add est l'objet qui tombe au sol, donc au
  // premier monstre tué.
  const auto dropped = PacketLength(0x0ADD);
  REQUIRE(dropped.has_value());
  CHECK(*dropped == 24);  // la table du serveur en annonce 22

  const auto feed_pet = PacketLength(0x01A3);
  REQUIRE(feed_pet.has_value());
  CHECK(*feed_pet == 7);  // la table du serveur en annonce 5

  const auto feed_mercenary = PacketLength(0x022F);
  REQUIRE(feed_mercenary.has_value());
  CHECK(*feed_mercenary == 7);  // la table du serveur en annonce 5
}

TEST_CASE("Jeu : un champ nommé packetLength peut être du CONTENU", "[proto][map]") {
  // Les opcodes personnalisés du serveur nomment `packetLength` un champ que
  // leur table déclare de taille FIXE, et que le serveur ne lit jamais comme
  // une longueur. C'est la table qui découpe le fil : la prendre pour un
  // en-tête ferait attendre une trame à rallonge qui n'arrive jamais.
  CzMoonlightJump jump;
  jump.packetLength = 0x1234;

  const std::vector<std::uint8_t> frame = Encode(jump);
  REQUIRE(frame.size() == CzMoonlightJump::kWireSize);
  CHECK(frame.size() == 4);

  CzMoonlightJump reread;
  REQUIRE(Decode(frame, reread));
  CHECK(reread.packetLength == 0x1234);
}

TEST_CASE("Jeu : un paquet sans opcode déclaré se décode quand même", "[proto][map]") {
  // Le serveur pose l'opcode de ce paquet depuis son code, par une constante
  // qu'aucune règle ne rattache à la structure. Le décodeur ne peut donc pas le
  // vérifier — l'appelant a déjà trié sur l'opcode reçu — mais la disposition,
  // elle, est connue.
  constexpr std::uint16_t kAnyOpcode = 0x008E;
  const std::string message = "bonjour le monde";

  std::vector<std::uint8_t> frame = {static_cast<std::uint8_t>(kAnyOpcode & 0xFF),
                                     static_cast<std::uint8_t>(kAnyOpcode >> 8), 0, 0};
  frame.insert(frame.end(), message.begin(), message.end());
  frame[2] = static_cast<std::uint8_t>(frame.size() & 0xFF);
  frame[3] = static_cast<std::uint8_t>(frame.size() >> 8);

  ZcNotifyPlayerchat chat;
  REQUIRE(Decode(frame, chat));
  CHECK(chat.Message == message);
}

TEST_CASE("Jeu : la chaîne d'entrée sur la carte est décrite", "[proto][map]") {
  // Ces quatre paquets sont tout ce qu'il faut pour entrer en jeu et voir
  // quelqu'un. Ils vivent dans src/map/packets.hpp, un fichier que le générateur
  // ne lisait pas : sans lui, l'acceptation d'entrée, le changement de carte et
  // l'heure du serveur n'existaient pas du tout.
  CHECK(ZcAcceptEnter::kOpcode == 0x02EB);
  CHECK(ZcNpcackMapmove::kOpcode == 0x0091);
  CHECK(ZcNotifyTime::kOpcode == 0x007F);
  CHECK(PacketIdleUnit::kOpcode == 0x09FF);

  // L'acceptation d'entrée porte la position de départ dans trois octets, où
  // deux coordonnées et une direction sont empaquetées. Les rendre tels quels
  // garde l'information ; les découper est l'affaire de la couche qui place le
  // personnage.
  ZcAcceptEnter accepted;
  accepted.startTime = 123'456;
  accepted.posDir = {0x12, 0x34, 0x56};
  accepted.font = 7;

  ZcAcceptEnter reread;
  REQUIRE(Decode(Encode(accepted), reread));
  CHECK(reread.startTime == accepted.startTime);
  CHECK(reread.posDir == accepted.posDir);
  CHECK(reread.font == accepted.font);
}

TEST_CASE("Jeu : une trame plus courte que sa structure est refusée", "[proto][map]") {
  std::vector<std::uint8_t> frame = Encode(PacketIdleUnit());
  frame.pop_back();

  PacketIdleUnit reread;
  CHECK_FALSE(Decode(frame, reread));
}

TEST_CASE("Jeu : le maître d'un monstre invoqué se relit, longueur comprise", "[proto][map]") {
  // Tel que le serveur l'écrit : opcode, longueur, monstre, maître — en petit-boutiste.
  const std::vector<std::uint8_t> frame = {0x37, 0x0f, 0x0c, 0x00, 0x41, 0x0d,
                                           0x03, 0x00, 0x15, 0x93, 0x1e, 0x00};
  ro::proto::ZcMoonlightUnitMaster told;
  REQUIRE(Decode(frame, told));
  CHECK(told.GID == 200'001);
  CHECK(told.master_id == 2'003'733);
  CHECK(ro::proto::ZcMoonlightUnitMaster::kWireSize == frame.size());
  CHECK(Encode(told) == frame);
  // 🔴 VARIABLE DANS LA TABLE : toute la plage 0x0F00 l'est chez le client d'origine,
  // et c'est la longueur écrite dans la trame qui la découpe.
  CHECK(PacketLength(ro::proto::ZcMoonlightUnitMaster::kOpcode) == -1);
}
