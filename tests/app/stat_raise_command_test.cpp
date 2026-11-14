// `/str+ 5` ET SES ONZE SŒURS : ce que la ligne tapée demande, et les octets
// qui partent au serveur.

#include "chat/stat_raise_command.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/actor.h"
#include "ro/sim/world.h"

#include "chat/native_slash_commands.h"
#include "support/loopback_server.h"
#include "support/net_frames.h"

using moonclient::LeadingInteger;
using moonclient::ReadStatRaiseCommand;
using moonclient::RunStatRaiseCommand;
using moonclient::StatRaiseCommand;
using moonclient::StatRaiseKind;
using ro::test::LoopbackServer;

namespace {

constexpr std::uint16_t kSpStr = 13;
constexpr std::uint16_t kSpDex = 17;
constexpr std::uint16_t kSpLuk = 18;
constexpr std::uint16_t kSpPow = 219;
constexpr std::uint16_t kSpCrt = 224;
constexpr std::uint32_t kPlayerId = 2'000'011;
// Les tailles sur le fil : 0x00BB en cinq octets, 0x0B24 en six.
constexpr std::size_t kBaseFrameBytes = 5;
constexpr std::size_t kTraitFrameBytes = 6;
// Ce qu'on attend d'un serveur qui ne doit RIEN recevoir.
constexpr int kSilenceMs = 200;

// L'accusé du serveur, accepté.
void AnswerStatRaise(ro::sim::World& world, std::uint16_t parameter) {
  ro::proto::ZcStatusChangeAck ack;
  ack.sp = parameter;
  ack.ok = 1;
  ack.value = 1;
  static_cast<void>(
      ro::net::ApplyFrame(ro::test_support::FrameOf(ro::proto::Encode(ack)), world, 0));
}

}  // namespace

TEST_CASE("Montée : la commande n'est plus tue par la table des commandes connues",
          "[app][chat][montee]") {
  // 🔴 LE DÉFAUT SIGNALÉ : « /dex+ 45 » était reconnu comme une commande du jeu
  // que ce client n'avait pas reprise, et se taisait. Sans nombre, le jeu ne la
  // reconnaît pas non plus : elle part comme une parole.
  CHECK_FALSE(moonclient::NativeKnowsSlashCommand("/dex+ 45"));
  CHECK_FALSE(moonclient::NativeKnowsSlashCommand("/dex+"));
  CHECK_FALSE(moonclient::NativeKnowsSlashCommand("/pow+"));
}

TEST_CASE("Montée : chaque commande et son numéro", "[app][chat][montee]") {
  const StatRaiseCommand dex = ReadStatRaiseCommand("/dex+ 45");
  CHECK(dex.kind == StatRaiseKind::kBase);
  CHECK(dex.parameter == kSpDex);
  CHECK(dex.amount == 45);
  CHECK(ReadStatRaiseCommand("/str+ 1").parameter == kSpStr);
  CHECK(ReadStatRaiseCommand("/agi+ 1").parameter == 14);
  CHECK(ReadStatRaiseCommand("/vit+ 1").parameter == 15);
  CHECK(ReadStatRaiseCommand("/int+ 1").parameter == 16);
  CHECK(ReadStatRaiseCommand("/luk+ 1").parameter == kSpLuk);
  const StatRaiseCommand pow = ReadStatRaiseCommand("/pow+ 3");
  CHECK(pow.kind == StatRaiseKind::kTrait);
  CHECK(pow.parameter == kSpPow);
  CHECK(pow.amount == 3);
  CHECK(ReadStatRaiseCommand("/sta+ 1").parameter == 220);
  CHECK(ReadStatRaiseCommand("/wis+ 1").parameter == 221);
  CHECK(ReadStatRaiseCommand("/spl+ 1").parameter == 222);
  CHECK(ReadStatRaiseCommand("/con+ 1").parameter == 223);
  CHECK(ReadStatRaiseCommand("/crt+ 1").parameter == kSpCrt);
}

TEST_CASE("Montée : la ligne se coupe aux blancs, sans la casse", "[app][chat][montee]") {
  CHECK(ReadStatRaiseCommand("/DEX+ 45").parameter == kSpDex);
  CHECK(ReadStatRaiseCommand("/dex+    45").amount == 45);
  CHECK(ReadStatRaiseCommand("/dex+\t45 et la suite").amount == 45);
  // Collé, ce n'est plus le même mot.
  CHECK(ReadStatRaiseCommand("/dex+45").kind == StatRaiseKind::kNone);
  // Sans nombre, la ligne n'est pas une commande.
  CHECK(ReadStatRaiseCommand("/dex+").kind == StatRaiseKind::kNone);
  CHECK(ReadStatRaiseCommand("/dex+   ").kind == StatRaiseKind::kNone);
  // « // » clôt la ligne : le nombre qui suit ne compte pas.
  CHECK(ReadStatRaiseCommand("/dex+ //45").kind == StatRaiseKind::kNone);
  CHECK(ReadStatRaiseCommand("/dexterity 45").kind == StatRaiseKind::kNone);
  CHECK(ReadStatRaiseCommand("dex+ 45").kind == StatRaiseKind::kNone);
}

TEST_CASE("Montée : le nombre se lit comme atoi et se tronque au champ", "[app][chat][montee]") {
  CHECK(LeadingInteger("45") == 45);
  CHECK(LeadingInteger("12x") == 12);
  CHECK(LeadingInteger("abc") == 0);
  CHECK(LeadingInteger("+7") == 7);
  CHECK(LeadingInteger("-1") == -1);
  CHECK(LeadingInteger("99999999999") == 2'147'483'647);
  CHECK(LeadingInteger("-99999999999") == -2'147'483'647 - 1);
  // Un mot qui n'est pas un nombre demande zéro, que le serveur refuse.
  CHECK(ReadStatRaiseCommand("/dex+ beaucoup").amount == 0);
  // Base : l'octet bas. Métier : les deux octets bas.
  CHECK(ReadStatRaiseCommand("/dex+ 300").amount == 44);
  CHECK(ReadStatRaiseCommand("/dex+ -1").amount == 255);
  CHECK(ReadStatRaiseCommand("/pow+ 300").amount == 300);
  CHECK(ReadStatRaiseCommand("/pow+ -1").amount == 65'535);
}

TEST_CASE("Montée : les octets qui partent, et une seule demande à la fois",
          "[app][chat][montee]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  auto link = ro::net::Connection::Open("127.0.0.1", server.port());
  REQUIRE(link.has_value());
  REQUIRE(server.Accept());
  ro::sim::World world;

  // `CZ_STATUS_CHANGE` : l'opcode, le numéro sur deux octets, la quantité sur un.
  REQUIRE(RunStatRaiseCommand(*link, world, "/dex+ 45"));
  CHECK(server.Receive(kBaseFrameBytes) == std::vector<std::uint8_t>{0xBB, 0x00, kSpDex, 0x00, 45});

  // 🔴 TANT QUE L'ACCUSÉ N'EST PAS LÀ, LA SUIVANTE SE TAIT — et la ligne est
  // prise quand même : elle ne part pas en parole.
  REQUIRE(RunStatRaiseCommand(*link, world, "/str+ 1"));
  CHECK(server.Receive(kBaseFrameBytes, kSilenceMs).empty());

  AnswerStatRaise(world, kSpDex);
  // `CZ_ADVANCED_STATUS_CHANGE` : la quantité sur deux octets.
  REQUIRE(RunStatRaiseCommand(*link, world, "/crt+ 300"));
  CHECK(server.Receive(kTraitFrameBytes) ==
        std::vector<std::uint8_t>{0x24, 0x0B, 0xE0, 0x00, 0x2C, 0x01});

  // L'entrée sur une carte lève aussi l'attente.
  world.EnterMap("prontera", 150, 150);
  REQUIRE(RunStatRaiseCommand(*link, world, "/luk+ 2"));
  CHECK(server.Receive(kBaseFrameBytes) == std::vector<std::uint8_t>{0xBB, 0x00, kSpLuk, 0x00, 2});
}

TEST_CASE("Montée : un personnage mort ne demande rien", "[app][chat][montee]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  auto link = ro::net::Connection::Open("127.0.0.1", server.port());
  REQUIRE(link.has_value());
  REQUIRE(server.Accept());
  ro::sim::World world;
  world.set_player_id(kPlayerId);
  ro::sim::Actor self;
  self.id = kPlayerId;
  self.posture = ro::sim::Posture::kDead;
  world.Spawn(self);

  REQUIRE(RunStatRaiseCommand(*link, world, "/dex+ 45"));
  CHECK(server.Receive(kBaseFrameBytes, kSilenceMs).empty());
  // ⚠ ET RIEN NE RESTE EN ATTENTE : relevé, il monte du premier coup.
  CHECK_FALSE(world.stat_raise_awaited());
}

TEST_CASE("Montée : hors ligne, la ligne est prise et ne demande rien", "[app][chat][montee]") {
  ro::net::Connection link;
  ro::sim::World world;
  CHECK(RunStatRaiseCommand(link, world, "/dex+ 45"));
  CHECK_FALSE(world.stat_raise_awaited());
  CHECK_FALSE(RunStatRaiseCommand(link, world, "/dex+"));
  CHECK_FALSE(RunStatRaiseCommand(link, world, "bonjour"));
}
