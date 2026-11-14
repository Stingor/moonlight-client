#include "ro/net/connection.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"
#include "ro/net/socket.h"
#include "ro/proto/chat_author_packets.h"
#include "ro/proto/map_packets.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"

using ro::net::Connection;
using ro::net::LinkStatus;
using ro::test::Connect;
using ro::test::LoopbackServer;

namespace {

// 0x0064, la demande de connexion : 55 octets, longueur fixe.
constexpr std::uint16_t kFixedOpcode = 0x0064;
constexpr std::size_t kFixedSize = 55;

// 0x0069, l'acceptation de connexion : longueur portée par le paquet.
constexpr std::uint16_t kVariableOpcode = 0x0069;

std::vector<std::uint8_t> FixedFrame(std::uint8_t filler) {
  std::vector<std::uint8_t> bytes(kFixedSize, filler);
  bytes[0] = static_cast<std::uint8_t>(kFixedOpcode & 0xFF);
  bytes[1] = static_cast<std::uint8_t>(kFixedOpcode >> 8);
  return bytes;
}

std::vector<std::uint8_t> VariableFrame(std::uint16_t announced, std::uint8_t filler) {
  std::vector<std::uint8_t> bytes(announced, filler);
  bytes[0] = static_cast<std::uint8_t>(kVariableOpcode & 0xFF);
  bytes[1] = static_cast<std::uint8_t>(kVariableOpcode >> 8);
  bytes[2] = static_cast<std::uint8_t>(announced & 0xFF);
  bytes[3] = static_cast<std::uint8_t>(announced >> 8);
  return bytes;
}

// Interroge la liaison jusqu'à ce qu'elle rende quelque chose.
//
// Les octets traversent la pile du système : ils ne sont pas forcément
// arrivés au premier tour. Une attente fixe rendrait le test lent OU
// intermittent selon la machine ; une boucle bornée ne fait ni l'un ni l'autre.
Connection::Polled PollUntil(Connection& link, std::size_t expected_frames) {
  Connection::Polled total;
  for (int attempt = 0; attempt < 200; ++attempt) {
    Connection::Polled polled = link.Poll();
    for (auto& frame : polled.frames) {
      total.frames.push_back(std::move(frame));
    }
    total.status = polled.status;
    if (total.frames.size() >= expected_frames || polled.status != LinkStatus::kOpen) {
      return total;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return total;
}

}  // namespace

TEST_CASE("Liaison : une trame envoyée par le serveur ressort entière", "[net][connection]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);

  const std::vector<std::uint8_t> frame = FixedFrame(0x11);
  REQUIRE(server.Send(frame));

  const Connection::Polled polled = PollUntil(link, 1);
  CHECK(polled.status == LinkStatus::kOpen);
  REQUIRE(polled.frames.size() == 1);
  CHECK(polled.frames[0].opcode == kFixedOpcode);
  CHECK(polled.frames[0].bytes.size() == kFixedSize);
  CHECK(polled.frames[0].bytes[10] == 0x11);
}

TEST_CASE("Liaison : une trame coupée en deux envois se recolle", "[net][connection]") {
  // Le cas qu'aucune simulation de socket ne reproduit : deux écritures
  // séparées dans le temps, que rien n'oblige le système à livrer ensemble.
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);

  const std::vector<std::uint8_t> frame = FixedFrame(0x22);
  REQUIRE(server.Send(std::span(frame).first(20)));

  const Connection::Polled partial = link.Poll();
  CHECK(partial.status == LinkStatus::kOpen);
  CHECK(partial.frames.empty());

  REQUIRE(server.Send(std::span(frame).subspan(20)));
  const Connection::Polled complete = PollUntil(link, 1);
  REQUIRE(complete.frames.size() == 1);
  CHECK(complete.frames[0].bytes.size() == kFixedSize);
}

TEST_CASE("Liaison : deux trames dans un même envoi se séparent", "[net][connection]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);

  std::vector<std::uint8_t> block = FixedFrame(0x33);
  const std::vector<std::uint8_t> second = VariableFrame(30, 0x44);
  block.insert(block.end(), second.begin(), second.end());
  REQUIRE(server.Send(block));

  const Connection::Polled polled = PollUntil(link, 2);
  REQUIRE(polled.frames.size() == 2);
  CHECK(polled.frames[0].opcode == kFixedOpcode);
  CHECK(polled.frames[0].bytes.size() == kFixedSize);
  CHECK(polled.frames[1].opcode == kVariableOpcode);
  CHECK(polled.frames[1].bytes.size() == 30);
  // Chaque trame porte SON remplissage : elles ne se sont pas mélangées.
  CHECK(polled.frames[1].bytes[10] == 0x44);
}

TEST_CASE("Liaison : ce que le serveur envoie AVANT de fermer n'est pas perdu",
          "[net][connection]") {
  // C'est exactement la forme d'un refus de connexion : le serveur répond puis
  // ferme. Jeter les trames en même temps que la liaison ferait perdre le seul
  // message qui explique la fermeture.
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);

  REQUIRE(server.Send(FixedFrame(0x55)));
  server.CloseClient();

  const Connection::Polled polled = PollUntil(link, 1);
  REQUIRE(polled.frames.size() == 1);
  CHECK(polled.frames[0].opcode == kFixedOpcode);
  CHECK(polled.status == LinkStatus::kClosedByPeer);
  CHECK_FALSE(link.is_open());
}

TEST_CASE("Liaison : un opcode inconnu ferme au lieu de deviner", "[net][connection]") {
  // Sans sa longueur, avancer d'un nombre arbitraire d'octets reviendrait à
  // chercher un opcode au milieu d'un corps de paquet, et à fabriquer des
  // trames à partir de données.
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);

  std::vector<std::uint8_t> bytes(100, 0x00);
  bytes[0] = 0xFF;
  bytes[1] = 0xFF;
  REQUIRE(server.Send(bytes));

  Connection::Polled polled;
  for (int attempt = 0; attempt < 200 && polled.status == LinkStatus::kOpen; ++attempt) {
    polled = link.Poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(polled.status == LinkStatus::kProtocolError);
  CHECK_FALSE(link.is_open());
  // L'erreur nomme l'opcode fautif : sans lui, le journal ne dit pas où
  // chercher.
  CHECK(link.error().detail.find("0xffff") != std::string::npos);
}

TEST_CASE("Liaison : ce que le client envoie arrive tel quel", "[net][connection]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);

  const std::vector<std::uint8_t> request = FixedFrame(0x66);
  link.Send(request);
  CHECK(link.pending_output() == 0);

  const std::vector<std::uint8_t> received = server.Receive(request.size());
  CHECK(received == request);
}

TEST_CASE("Liaison : un gros envoi arrive entier, dans l'ordre", "[net][connection]") {
  // Un envoi peut n'être accepté qu'en partie : c'est le contrat de send, et un
  // client qui l'ignore coupe une trame en deux dès que le tampon du système se
  // remplit. La session se désynchronise alors sans qu'aucune erreur ne soit
  // signalée. Ce que ce cas vérifie de bout en bout : pas un octet perdu, pas un
  // octet dupliqué, l'ordre conservé.
  //
  // Le banc rétrécit les deux tampons du tuyau pour PROVOQUER le refus. Il
  // l'obtient sur les systèmes POSIX ; Windows, lui, avale le tampon entier
  // quoi qu'on règle — mesuré : quatre mégaoctets acceptés en un seul appel
  // avec SO_SNDBUF épinglé à quatre kilooctets, relu à quatre kilooctets. Le
  // chemin de reprise est donc éprouvé par le préréglage Linux, et celui-ci
  // vérifie partout que le flux traverse intact.
  constexpr int kNarrowBuffer = 4 * 1024;
  constexpr std::size_t kFrameCount = 40000;
  LoopbackServer server(kNarrowBuffer);
  REQUIRE(server.listening());
  Connection link = Connect(server);
  // ⚠ Le support de test prend le socket en `std::uintptr_t` : c'est le
  // dénominateur commun des deux plateformes — un SOCKET Windows est un
  // `UINT_PTR`, un descripteur POSIX un `int` — et c'est ce qui lui évite de
  // dépendre de `ro::net`. La conversion se dit donc ici.
  REQUIRE(ro::test::LimitSendBuffer(static_cast<std::uintptr_t>(link.socket().handle()),
                                    kNarrowBuffer));

  std::vector<std::uint8_t> stream;
  stream.reserve(kFrameCount * kFixedSize);
  for (std::size_t index = 0; index < kFrameCount; ++index) {
    const std::vector<std::uint8_t> frame = FixedFrame(static_cast<std::uint8_t>(index));
    stream.insert(stream.end(), frame.begin(), frame.end());
  }
  link.Send(stream);

  // Le serveur lit pendant que la liaison réémet ce qui reste.
  std::vector<std::uint8_t> received;
  for (int attempt = 0; attempt < 5000 && received.size() < stream.size(); ++attempt) {
    static_cast<void>(link.Poll());
    const std::vector<std::uint8_t> chunk = server.Receive(stream.size() - received.size(), 5);
    received.insert(received.end(), chunk.begin(), chunk.end());
  }

  REQUIRE(received.size() == stream.size());
  CHECK(received == stream);
  CHECK(link.pending_output() == 0);
}

namespace {

// Une parole de guilde, et l'auteur qui annonce `next_opcode`.
std::vector<std::uint8_t> GuildLine() {
  ro::proto::ZcGuildChat said;
  said.message = "Bob : bonsoir";
  return Encode(said);
}

std::vector<std::uint8_t> AuthorBytes(std::uint16_t next_opcode) {
  ro::proto::ChatAuthorNotice author;
  author.fixed.next_opcode = next_opcode;
  author.fixed.scope = static_cast<std::uint8_t>(ro::proto::ChatAuthorScope::kGuild);
  author.fixed.channel_type = ro::proto::kChatAuthorNoChannelType;
  author.fixed.header_len = static_cast<std::uint16_t>(std::string("Bob : ").size());
  author.fixed.name = "Bob";
  return Encode(author);
}

}  // namespace

TEST_CASE("Liaison : l'auteur d'une parole s'attache à la trame qu'il annonce",
          "[net][connection]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);

  const std::vector<std::uint8_t> author = AuthorBytes(ro::proto::ZcGuildChat::kOpcode);
  std::vector<std::uint8_t> block = author;
  const std::vector<std::uint8_t> line = GuildLine();
  block.insert(block.end(), line.begin(), line.end());
  // Puis une seconde parole, qu'aucun auteur ne précède.
  block.insert(block.end(), line.begin(), line.end());
  REQUIRE(server.Send(block));

  const Connection::Polled polled = PollUntil(link, 2);
  // ⚠ L'AUTEUR N'EST PAS RENDU SEUL : deux trames, pas trois.
  REQUIRE(polled.frames.size() == 2);
  CHECK(polled.frames[0].opcode == ro::proto::ZcGuildChat::kOpcode);
  CHECK(polled.frames[0].preamble == author);
  CHECK(polled.frames[1].preamble.empty());
}

TEST_CASE("Liaison : l'auteur attend sa trame d'un tour à l'autre", "[net][connection]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);

  const std::vector<std::uint8_t> author = AuthorBytes(ro::proto::ZcGuildChat::kOpcode);
  std::size_t received = 0;
  link.set_receive_observer(
      [&received](std::span<const std::uint8_t> bytes) { received += bytes.size(); });
  REQUIRE(server.Send(author));
  // Jusqu'à ce que l'auteur soit arrivé ENTIER : rien n'est rendu, il attend.
  for (int attempt = 0; attempt < 200 && received < author.size(); ++attempt) {
    CHECK(link.Poll().frames.empty());
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  REQUIRE(received == author.size());
  CHECK(link.Poll().frames.empty());

  REQUIRE(server.Send(GuildLine()));
  const Connection::Polled second = PollUntil(link, 1);
  REQUIRE(second.frames.size() == 1);
  CHECK(second.frames[0].preamble == author);
}

TEST_CASE("Liaison : un auteur qui annonce une autre trame se perd", "[net][connection]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);

  std::vector<std::uint8_t> block = AuthorBytes(ro::proto::ZcNotifyChatParty::kOpcode);
  const std::vector<std::uint8_t> line = GuildLine();
  block.insert(block.end(), line.begin(), line.end());
  block.insert(block.end(), line.begin(), line.end());
  REQUIRE(server.Send(block));

  const Connection::Polled polled = PollUntil(link, 2);
  REQUIRE(polled.frames.size() == 2);
  CHECK(polled.frames[0].preamble.empty());
  // Perdu, et non reporté sur la suivante.
  CHECK(polled.frames[1].preamble.empty());
}

TEST_CASE("Liaison : une trame rendue garde son auteur", "[net][connection]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);

  const std::vector<std::uint8_t> author = AuthorBytes(ro::proto::ZcGuildChat::kOpcode);
  std::vector<std::uint8_t> block = author;
  const std::vector<std::uint8_t> line = GuildLine();
  block.insert(block.end(), line.begin(), line.end());
  REQUIRE(server.Send(block));
  Connection::Polled polled = PollUntil(link, 1);
  REQUIRE(polled.frames.size() == 1);

  link.Requeue(std::move(polled.frames));
  const Connection::Polled again = link.Poll();
  REQUIRE(again.frames.size() == 1);
  CHECK(again.frames[0].preamble == author);
}

TEST_CASE("Liaison : une connexion vers un port fermé échoue sans attendre", "[net][connection]") {
  // Le port est réservé puis rendu : rien n'y écoute, mais il a bien existé.
  std::uint16_t closed_port = 0;
  {
    LoopbackServer server;
    REQUIRE(server.listening());
    closed_port = server.port();
  }

  const auto link = Connection::Open("127.0.0.1", closed_port, 1000);
  CHECK_FALSE(link.has_value());
}

TEST_CASE("Liaison : un port fermé se dit refusé, et non hors délai", "[net][connection]") {
  std::uint16_t closed_port = 0;
  {
    LoopbackServer server;
    REQUIRE(server.listening());
    closed_port = server.port();
  }

  const auto link = Connection::Open("127.0.0.1", closed_port, ro::net::kConnectTimeoutMs);
  REQUIRE_FALSE(link.has_value());
  CHECK(ro::core::Describe(link.error()).find("refus") != std::string::npos);
}
