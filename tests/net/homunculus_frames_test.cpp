// LES TRAMES DE L'HOMONCULE : chacune change l'état attendu, et les accusés
// disent ce qu'il faut écrire ; les demandes partent telles que le serveur les
// lit.

#include "ro/net/homunculus_frames.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/config_packets.h"
#include "ro/proto/homunculus_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/homunculus.h"

#include "support/loopback_server.h"
#include "support/net_frames.h"

using ro::net::ApplyHomunculusFrame;
using ro::net::FrameOutcome;
using ro::net::HomunculusEcho;
using ro::net::HomunculusEchoKind;
using ro::sim::Homunculus;
using ro::test::LoopbackServer;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kHomunculusId = 110000;

[[nodiscard]] std::vector<std::uint8_t> State(std::uint8_t state, std::uint32_t data) {
  return ro::proto::Encode(
      ro::proto::ZcChangestateMer{.type = 0, .state = state, .gid = kHomunculusId, .data = data});
}

[[nodiscard]] FrameOutcome Apply(std::vector<std::uint8_t> bytes, Homunculus& homunculus,
                                 HomunculusEcho& echo) {
  return ApplyHomunculusFrame(FrameOf(std::move(bytes)), homunculus, echo);
}

[[nodiscard]] std::vector<std::uint8_t> AutofeedAck(std::uint32_t value) {
  return ro::proto::EncodeConfigAck(ro::proto::ConfigAck{
      .type = static_cast<std::uint32_t>(ro::proto::ConfigType::kHomunculusAutofeed),
      .value = value});
}

// Un homoncule accusé dont la fiche porte ces drapeaux.
void Bring(Homunculus& homunculus, std::uint8_t flags) {
  HomunculusEcho ignored;
  REQUIRE(Apply(State(0, 0), homunculus, ignored) == FrameOutcome::kApplied);
  ro::proto::HomunculusProperties wire;
  wire.flags = flags;
  wire.hunger = 50;
  REQUIRE(Apply(ro::proto::EncodeHomunculusProperties(wire), homunculus, ignored) ==
          FrameOutcome::kApplied);
}

}  // namespace

TEST_CASE("Homoncule : l'accusé le fait exister, l'intimité et la satiété suivent",
          "[net][homunculus]") {
  Homunculus homunculus;
  HomunculusEcho echo;
  REQUIRE(Apply(State(0, 0), homunculus, echo) == FrameOutcome::kApplied);
  CHECK(homunculus.present());
  CHECK(homunculus.actor_id() == kHomunculusId);
  REQUIRE(Apply(State(1, 912), homunculus, echo) == FrameOutcome::kApplied);
  CHECK(homunculus.sheet().intimacy == 912);
  REQUIRE(Apply(State(2, 37), homunculus, echo) == FrameOutcome::kApplied);
  CHECK(homunculus.sheet().hunger == 37);
  CHECK(Apply(State(3, 0), homunculus, echo) == FrameOutcome::kIgnored);
  CHECK(echo.kind == HomunculusEchoKind::kNone);
}

TEST_CASE("Homoncule : la fiche se recopie, et la faim fait écrire son alerte",
          "[net][homunculus]") {
  Homunculus homunculus;
  HomunculusEcho echo;
  ro::proto::HomunculusProperties wire;
  wire.name = "Lif";
  wire.flags = ro::proto::kHomunculusRenamedFlag | ro::proto::kHomunculusRestingFlag |
               ro::proto::kHomunculusAliveFlag;
  wire.level = 12;
  wire.hunger = 10;
  wire.hp = 300;
  wire.max_hp = 400;
  wire.exp = 5;
  wire.exp_next = 0;
  wire.skill_points = 3;
  REQUIRE(Apply(ro::proto::EncodeHomunculusProperties(wire), homunculus, echo) ==
          FrameOutcome::kApplied);
  CHECK(echo.kind == HomunculusEchoKind::kStarving);
  const ro::sim::HomunculusSheet& sheet = homunculus.sheet();
  CHECK(sheet.name == "Lif");
  CHECK(sheet.renamed);
  CHECK(sheet.resting);
  CHECK(sheet.alive);
  CHECK(sheet.level == 12);
  CHECK(sheet.hp == 300);
  CHECK(sheet.max_hp == 400);
  CHECK(sheet.exp_next == 0);
  CHECK(sheet.skill_points == 3);
}

TEST_CASE("Homoncule : PV et SP ne lisent que leurs bits bas, l'expérience tout",
          "[net][homunculus]") {
  Homunculus homunculus;
  HomunculusEcho echo;
  const auto changed = [](std::uint16_t type, std::uint64_t value) {
    return ro::proto::EncodeHomunculusParameter(
        ro::proto::HomunculusParameter{.type = type, .value = value});
  };
  REQUIRE(Apply(changed(ro::proto::kHomunculusParameterExp, 0x100000002ULL), homunculus, echo) ==
          FrameOutcome::kApplied);
  CHECK(homunculus.sheet().exp == 0x100000002LL);
  REQUIRE(Apply(changed(ro::proto::kHomunculusParameterHp, 0x100000064ULL), homunculus, echo) ==
          FrameOutcome::kApplied);
  CHECK(homunculus.sheet().hp == 0x64);
  REQUIRE(Apply(changed(ro::proto::kHomunculusParameterSp, 9), homunculus, echo) ==
          FrameOutcome::kApplied);
  CHECK(homunculus.sheet().sp == 9);
  CHECK(Apply(changed(2, 1), homunculus, echo) == FrameOutcome::kIgnored);
}

TEST_CASE("Homoncule : la liste des compétences remplace, la mise à jour corrige",
          "[net][homunculus]") {
  Homunculus homunculus;
  HomunculusEcho echo;
  ro::proto::ZcHoskillinfoList list;
  list.skills.push_back(ro::proto::ZcHoskillinfoListSub{.id = 8005,
                                                        .inf = 4,
                                                        .level = 1,
                                                        .sp = 10,
                                                        .range = 1,
                                                        .name = "HAMI_CASTLE",
                                                        .upgradable = 1});
  REQUIRE(Apply(ro::proto::Encode(list), homunculus, echo) == FrameOutcome::kApplied);
  REQUIRE(homunculus.skills().size() == 1);
  CHECK(homunculus.skills()[0].targeting == 4);
  CHECK(homunculus.skills()[0].upgradable);
  CHECK(homunculus.skills()[0].server_name == "HAMI_CASTLE");
  REQUIRE(Apply(ro::proto::Encode(ro::proto::ZcHoskillinfoUpdate{
                    .skill_id = 8005, .Level = 5, .SP = 30, .AttackRange = 1, .upgradable = false}),
                homunculus, echo) == FrameOutcome::kApplied);
  CHECK(homunculus.skills()[0].level == 5);
  CHECK(homunculus.skills()[0].sp_cost == 30);
  CHECK_FALSE(homunculus.skills()[0].upgradable);
}

TEST_CASE("Homoncule : l'accusé de nourrissage dit la nourriture", "[net][homunculus]") {
  Homunculus homunculus;
  HomunculusEcho echo;
  CHECK(Apply(ro::proto::Encode(ro::proto::ZcFeedMer{.result = 1, .itemId = 537}), homunculus,
              echo) == FrameOutcome::kIgnored);
  CHECK(echo.kind == HomunculusEchoKind::kFed);
  CHECK(echo.item_id == 537);
  CHECK(Apply(ro::proto::Encode(ro::proto::ZcFeedMer{.result = 0, .itemId = 537}), homunculus,
              echo) == FrameOutcome::kIgnored);
  CHECK(echo.kind == HomunculusEchoKind::kNotFed);
}

TEST_CASE("Homoncule : seul l'écho de l'auto-alimentation de l'homoncule est pris",
          "[net][homunculus]") {
  Homunculus homunculus;
  Bring(homunculus, ro::proto::kHomunculusAliveFlag);
  HomunculusEcho echo;
  const auto ack = [](ro::proto::ConfigType type, std::uint32_t value) {
    return ro::proto::EncodeConfigAck(
        ro::proto::ConfigAck{.type = static_cast<std::uint32_t>(type), .value = value});
  };
  REQUIRE(Apply(ack(ro::proto::ConfigType::kHomunculusAutofeed, 1), homunculus, echo) ==
          FrameOutcome::kApplied);
  CHECK(homunculus.auto_feed());
  CHECK(echo.kind == HomunculusEchoKind::kAutoFeedOn);
  REQUIRE(Apply(ack(ro::proto::ConfigType::kHomunculusAutofeed, 0), homunculus, echo) ==
          FrameOutcome::kApplied);
  CHECK_FALSE(homunculus.auto_feed());
  CHECK(echo.kind == HomunculusEchoKind::kAutoFeedOff);
  // Celui du pet est l'affaire d'un autre.
  CHECK(Apply(ack(ro::proto::ConfigType::kPetAutofeed, 1), homunculus, echo) ==
        FrameOutcome::kUnhandled);
  CHECK_FALSE(homunculus.auto_feed());
}

TEST_CASE("Homoncule : le réglage d'un homoncule qui n'est pas sorti se retient sans s'écrire",
          "[net][homunculus]") {
  const auto silent = [](Homunculus& homunculus) {
    HomunculusEcho echo;
    REQUIRE(Apply(AutofeedAck(1), homunculus, echo) == FrameOutcome::kApplied);
    CHECK(homunculus.auto_feed());
    CHECK(echo.kind == HomunculusEchoKind::kNone);
  };
  SECTION("au repos") {
    Homunculus homunculus;
    Bring(homunculus, ro::proto::kHomunculusRestingFlag | ro::proto::kHomunculusAliveFlag);
    silent(homunculus);
  }
  SECTION("sans PV") {
    Homunculus homunculus;
    Bring(homunculus, 0);
    silent(homunculus);
  }
  SECTION("encore inconnu") {
    Homunculus homunculus;
    silent(homunculus);
  }
}

TEST_CASE("Homoncule : une trame étrangère n'est pas traitée", "[net][homunculus]") {
  Homunculus homunculus;
  HomunculusEcho echo;
  CHECK(Apply({0x87, 0x00, 0, 0}, homunculus, echo) == FrameOutcome::kUnhandled);
}

TEST_CASE("Homoncule : nourrir, renommer et l'auto-alimentation partent sur le fil",
          "[net][homunculus]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  auto link = ro::net::Connection::Open("127.0.0.1", server.port());
  REQUIRE(link.has_value());
  REQUIRE(server.Accept());

  REQUIRE(ro::net::RequestHomunculusFeed(*link).has_value());
  CHECK(server.Receive(ro::proto::HomunculusCommandRequest::kWireSize) ==
        std::vector<std::uint8_t>{0x2D, 0x02, 0x00, 0x00, 0x01});
  REQUIRE(ro::net::RequestHomunculusDelete(*link).has_value());
  CHECK(server.Receive(ro::proto::HomunculusCommandRequest::kWireSize) ==
        std::vector<std::uint8_t>{0x2D, 0x02, 0x00, 0x00, 0x02});

  // ⚠ UN NOM PART DANS LE JEU DE CARACTÈRES DU JEU : « é » tient en un octet.
  REQUIRE(ro::net::RequestHomunculusRename(*link, "Lé").has_value());
  const std::vector<std::uint8_t> renamed =
      server.Receive(ro::proto::HomunculusRenameRequest::kWireSize);
  REQUIRE(renamed.size() == ro::proto::HomunculusRenameRequest::kWireSize);
  CHECK(renamed[2] == 'L');
  CHECK(renamed[3] == 0xE9);
  CHECK(renamed[4] == 0);

  REQUIRE(ro::net::RequestHomunculusAutoFeed(*link, true).has_value());
  const std::vector<std::uint8_t> config = server.Receive(ro::proto::ConfigRequest::kWireSize);
  REQUIRE(config.size() == ro::proto::ConfigRequest::kWireSize);
  CHECK(config[0] == 0xD8);
  CHECK(config[1] == 0x02);
  CHECK(config[2] == 3);
  CHECK(config[6] == 1);
}
