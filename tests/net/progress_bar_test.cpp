// LA BARRE DE PROGRESSION D'UN SCRIPT : la jauge qu'elle pose, sa durée, la
// réponse CZ_PROGRESS — une seule par barre, à l'échéance ou à l'annulation —
// et la barre au-dessus d'un PNJ, qui n'en attend aucune.

#include "ro/net/progress_bar.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/progress_packets.h"
#include "ro/sim/progress_bar.h"
#include "ro/sim/world.h"

#include "support/loopback_server.h"
#include "support/net_frames.h"

using ro::net::ApplyProgressFrame;
using ro::net::FrameOutcome;
using ro::net::ReportProgressDone;
using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::ProgressReport;
using ro::sim::World;
using ro::test::LoopbackServer;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kSelf = 150'001;
constexpr std::uint32_t kKafra = 110'012;
constexpr std::uint32_t kDroppedApple = 2'000'456;
constexpr std::uint32_t kStartMs = 10'000;
constexpr std::uint32_t kFiveSeconds = 5;
constexpr std::uint32_t kFiveSecondsMs = 5'000;

[[nodiscard]] Actor Spawned(std::uint32_t id, ActorKind kind) {
  Actor actor;
  actor.id = id;
  actor.kind = kind;
  return actor;
}

[[nodiscard]] World Peopled() {
  World world;
  world.Spawn(Spawned(kSelf, ActorKind::kPlayer));
  world.set_player_id(kSelf);
  world.Spawn(Spawned(kKafra, ActorKind::kNonPlayer));
  world.Spawn(Spawned(kDroppedApple, ActorKind::kItem));
  return world;
}

[[nodiscard]] FrameOutcome Start(World& world, ProgressReport& report, std::uint32_t seconds,
                                 std::uint32_t now_ms) {
  return ApplyProgressFrame(FrameOf(ro::proto::EncodeProgressStart(
                                ro::proto::ProgressStart{.color = 0xFFFF00, .seconds = seconds})),
                            world, report, now_ms);
}

[[nodiscard]] FrameOutcome OverActor(World& world, ProgressReport& report, std::uint32_t id,
                                     std::uint32_t seconds, std::uint32_t now_ms) {
  ro::proto::ZcProgressActor packet;
  packet.GID = static_cast<std::int32_t>(id);
  packet.color = 0xFF0000;
  packet.time = seconds;
  return ApplyProgressFrame(FrameOf(ro::proto::EncodeProgressActor(packet)), world, report, now_ms);
}

[[nodiscard]] FrameOutcome Cancel(World& world, ProgressReport& report, std::uint32_t now_ms) {
  return ApplyProgressFrame(FrameOf(ro::proto::EncodeProgressCancel()), world, report, now_ms);
}

// Une liaison ouverte, et le compte des CZ_PROGRESS qui y partent.
struct Wire {
  LoopbackServer server;
  ro::net::Connection link;
  std::size_t answers = 0;

  Wire() : link(Open(server)) {
    link.set_send_observer([this](std::span<const std::uint8_t> bytes) {
      if (bytes.size() == ro::proto::ProgressDone::kWireSize &&
          bytes[0] == (ro::proto::ProgressDone::kOpcode & 0xFFU) &&
          bytes[1] == (ro::proto::ProgressDone::kOpcode >> 8U)) {
        ++answers;
      }
    });
  }

  static ro::net::Connection Open(LoopbackServer& server) {
    REQUIRE(server.listening());
    auto link = ro::net::Connection::Open("127.0.0.1", server.port());
    REQUIRE(link.has_value());
    REQUIRE(server.Accept());
    return std::move(*link);
  }
};

}  // namespace

TEST_CASE("Barre de progression : ZC_PROGRESS pose la jauge du joueur, en secondes",
          "[net][progress]") {
  World world = Peopled();
  ProgressReport report;
  REQUIRE(Start(world, report, kFiveSeconds, kStartMs) == FrameOutcome::kApplied);
  const Actor* self = world.player();
  REQUIRE(self->gauge.has_value());
  CHECK(self->gauge->started_ms == kStartMs);
  CHECK(self->gauge->duration_ms == kFiveSecondsMs);
  CHECK(report.owed());

  SECTION("une seconde barre repart de l'instant") {
    REQUIRE(Start(world, report, 2, kStartMs + 1'000) == FrameOutcome::kApplied);
    CHECK(world.player()->gauge->started_ms == kStartMs + 1'000);
    CHECK(world.player()->gauge->duration_ms == 2'000);
  }
}

TEST_CASE("Barre de progression : sans joueur dans le monde, ni jauge ni dette",
          "[net][progress]") {
  World world;
  ProgressReport report;
  CHECK(Start(world, report, kFiveSeconds, kStartMs) == FrameOutcome::kIgnored);
  CHECK_FALSE(report.owed());
}

TEST_CASE("Barre de progression : CZ_PROGRESS part une fois, à l'échéance", "[net][progress]") {
  Wire wire;
  World world = Peopled();
  ProgressReport report;
  REQUIRE(Start(world, report, kFiveSeconds, kStartMs) == FrameOutcome::kApplied);

  REQUIRE(ReportProgressDone(wire.link, world, report, kStartMs).has_value());
  REQUIRE(ReportProgressDone(wire.link, world, report, kStartMs + kFiveSecondsMs - 1).has_value());
  CHECK(wire.answers == 0);

  REQUIRE(ReportProgressDone(wire.link, world, report, kStartMs + kFiveSecondsMs).has_value());
  CHECK(wire.answers == 1);
  // Les images suivantes ne redisent rien : la dette est soldée.
  REQUIRE(ReportProgressDone(wire.link, world, report, kStartMs + kFiveSecondsMs + 1).has_value());
  REQUIRE(ReportProgressDone(wire.link, world, report, kStartMs + 60'000).has_value());
  CHECK(wire.answers == 1);
  CHECK(wire.server.Receive(ro::proto::ProgressDone::kWireSize) ==
        std::vector<std::uint8_t>{0xF1, 0x02});
}

TEST_CASE("Barre de progression : l'annulation du serveur répond tout de suite, une fois",
          "[net][progress]") {
  Wire wire;
  World world = Peopled();
  ProgressReport report;
  REQUIRE(Start(world, report, kFiveSeconds, kStartMs) == FrameOutcome::kApplied);

  REQUIRE(Cancel(world, report, kStartMs + 1'000) == FrameOutcome::kApplied);
  CHECK_FALSE(world.player()->gauge.has_value());
  REQUIRE(ReportProgressDone(wire.link, world, report, kStartMs + 1'000).has_value());
  CHECK(wire.answers == 1);
  // Ni l'échéance d'origine, ni une seconde annulation ne redisent rien.
  REQUIRE(Cancel(world, report, kStartMs + 2'000) == FrameOutcome::kApplied);
  REQUIRE(ReportProgressDone(wire.link, world, report, kStartMs + kFiveSecondsMs).has_value());
  CHECK(wire.answers == 1);
}

TEST_CASE("Barre de progression : la dette traverse un changement de carte", "[net][progress]") {
  Wire wire;
  World world = Peopled();
  ProgressReport report;
  REQUIRE(Start(world, report, kFiveSeconds, kStartMs) == FrameOutcome::kApplied);
  world.EnterMap("prontera", 150, 150);
  REQUIRE(world.player()->gauge.has_value());
  REQUIRE(ReportProgressDone(wire.link, world, report, kStartMs + kFiveSecondsMs).has_value());
  CHECK(wire.answers == 1);
}

TEST_CASE("Barre de progression : une annulation sans barre ne répond rien", "[net][progress]") {
  Wire wire;
  World world = Peopled();
  ProgressReport report;
  REQUIRE(Cancel(world, report, kStartMs) == FrameOutcome::kApplied);
  REQUIRE(ReportProgressDone(wire.link, world, report, kStartMs).has_value());
  CHECK(wire.answers == 0);
}

TEST_CASE("Barre de progression : ZC_PROGRESS_ACTOR pose la jauge d'un PNJ, sans dette",
          "[net][progress]") {
  Wire wire;
  World world = Peopled();
  ProgressReport report;
  REQUIRE(OverActor(world, report, kKafra, 3, kStartMs) == FrameOutcome::kApplied);
  const Actor* kafra = world.Find(kKafra);
  REQUIRE(kafra->gauge.has_value());
  CHECK(kafra->gauge->started_ms == kStartMs);
  CHECK(kafra->gauge->duration_ms == 3'000);
  CHECK_FALSE(report.owed());
  CHECK_FALSE(world.player()->gauge.has_value());
  REQUIRE(ReportProgressDone(wire.link, world, report, kStartMs + 3'000).has_value());
  CHECK(wire.answers == 0);

  // Ni un acteur absent, ni le joueur, ni un objet au sol.
  CHECK(OverActor(world, report, 999, 3, kStartMs) == FrameOutcome::kIgnored);
  CHECK(OverActor(world, report, kSelf, 3, kStartMs) == FrameOutcome::kIgnored);
  CHECK(OverActor(world, report, kDroppedApple, 3, kStartMs) == FrameOutcome::kIgnored);
}

TEST_CASE("Barre de progression : la jauge d'un PNJ survit à sa réannonce", "[net][progress]") {
  World world = Peopled();
  ProgressReport report;
  REQUIRE(OverActor(world, report, kKafra, 3, kStartMs) == FrameOutcome::kApplied);
  world.Spawn(Spawned(kKafra, ActorKind::kNonPlayer));
  REQUIRE(world.Find(kKafra)->gauge.has_value());
  CHECK(world.Find(kKafra)->gauge->duration_ms == 3'000);
}

TEST_CASE("Barre de progression : une trame d'une autre famille n'est pas lue", "[net][progress]") {
  World world = Peopled();
  ProgressReport report;
  ro::proto::ZcShowImage other;
  other.image = "kafra_01";
  CHECK(ApplyProgressFrame(FrameOf(Encode(other)), world, report, kStartMs) ==
        FrameOutcome::kUnhandled);
}
