// L'ÉTAGE DE L'INSTANCE contre un serveur en boucle locale : la fenêtre qui
// s'ouvre d'elle-même une fois, la porte du bouton de destruction, et la
// commande qui part — ou non.

#include "game/memorial_stage.h"

#include <cstdint>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/net/movement.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/memorial_packets.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/net_frames.h"

using moonclient::MemorialDestroyGateOf;
using moonclient::MemorialStage;
using moonclient::MemorialWindows;
using ro::net::FrameOutcome;
using ro::test::LoopbackServer;
using ro::test_support::FrameOf;
using ro::ui::MemorialDestroyGate;

namespace {

using Bytes = std::vector<std::uint8_t>;

constexpr std::int64_t kNow = 1'800'000'000;
constexpr int kSilenceMs = 150;
constexpr std::size_t kDestroyCommandSize = 6;

struct Bench {
  LoopbackServer server;
  ro::net::Connection link;
  MemorialStage stage;
  MemorialWindows windows;

  Bench() : link(Connect(server)) { stage.StartSession(windows); }

  static ro::net::Connection Connect(LoopbackServer& listening) {
    REQUIRE(listening.listening());
    return ro::test::Connect(listening);
  }

  void Apply(Bytes bytes) {
    REQUIRE(stage.ApplyFrame(FrameOf(std::move(bytes)), kNow) == FrameOutcome::kApplied);
  }

  void Open(const char* name = "Old Glast Heim") {
    Apply(ro::proto::Encode(ro::proto::InstanceStatusNotice{
        .name = name, .keep_limit_unix_s = static_cast<std::uint32_t>(kNow + 3600)}));
  }

  void Details(bool may_destroy, bool on_instance_map, bool destroyable = true) {
    ro::proto::ZcMoonlightInstanceInfo info;
    info.server_time = static_cast<std::uint32_t>(kNow);
    info.duration_limit = 7200;
    info.hardmode = 5;
    info.turbo = 1;
    info.mode = 2;
    info.destroyable = destroyable ? 1 : 0;
    info.may_destroy = may_destroy ? 1 : 0;
    info.on_instance_map = on_instance_map ? 1 : 0;
    info.owner_name = "Lune";
    Apply(ro::proto::Encode(info));
  }

  void Advance(std::int64_t now = kNow) { stage.Advance(windows, link, true, now); }

  [[nodiscard]] bool Silent() { return server.Receive(1, kSilenceMs).empty(); }
};

}  // namespace

TEST_CASE("Instance : la fenêtre s'ouvre à l'arrivée, pas à chaque rappel", "[app][instance]") {
  Bench bench;
  bench.Advance();
  CHECK_FALSE(bench.windows.window.open);
  bench.Open();
  bench.Advance();
  CHECK(bench.windows.window.open);
  CHECK(bench.windows.view.present);
  CHECK(bench.windows.view.seconds_left == 3600);
  // LE JOUEUR LA FERME, et le serveur redit l'instance à la carte suivante.
  bench.windows.window.open = false;
  bench.Open();
  bench.Advance();
  CHECK_FALSE(bench.windows.window.open);
  // Alt+B la rouvre.
  bench.stage.Toggle();
  bench.Advance();
  CHECK(bench.windows.window.open);
  // UNE INSTANCE QUI DISPARAÎT referme sa fenêtre, et le dit.
  bench.Apply(ro::proto::Encode(ro::proto::InstanceChangeNotice{.type = 3}));
  bench.Advance();
  CHECK_FALSE(bench.windows.window.open);
  CHECK(bench.stage.TakeNotices().size() == 1);
}

TEST_CASE("Instance : la vue porte la difficulté et le turbo", "[app][instance]") {
  Bench bench;
  bench.Open();
  bench.Details(/*may_destroy=*/true, /*on_instance_map=*/false);
  bench.Advance();
  CHECK(bench.windows.view.detailed);
  CHECK(bench.windows.view.hard_mode == 5);
  CHECK(bench.windows.view.turbo);
  CHECK(bench.windows.view.owner == ro::ui::MemorialOwnerView::kParty);
  CHECK(bench.windows.view.owner_name == "Lune");
  CHECK(bench.windows.view.duration_s == 7200U);
  CHECK(bench.windows.view.destroy == MemorialDestroyGate::kAllowed);
}

TEST_CASE("Instance : la porte du bouton de destruction", "[app][instance]") {
  Bench bench;
  CHECK(MemorialDestroyGateOf(bench.stage.dungeon(), kNow) == MemorialDestroyGate::kHidden);
  bench.Open();
  // SANS DÉTAIL, le serveur jugera.
  CHECK(MemorialDestroyGateOf(bench.stage.dungeon(), kNow) == MemorialDestroyGate::kUnknownRights);
  bench.Details(/*may_destroy=*/false, /*on_instance_map=*/false);
  CHECK(MemorialDestroyGateOf(bench.stage.dungeon(), kNow) == MemorialDestroyGate::kNotOwner);
  bench.Details(/*may_destroy=*/true, /*on_instance_map=*/true);
  CHECK(MemorialDestroyGateOf(bench.stage.dungeon(), kNow) == MemorialDestroyGate::kOnInstanceMap);
  bench.Details(/*may_destroy=*/true, /*on_instance_map=*/false, /*destroyable=*/false);
  CHECK(MemorialDestroyGateOf(bench.stage.dungeon(), kNow) == MemorialDestroyGate::kHidden);
}

TEST_CASE("Instance : la destruction confirmée part une fois", "[app][instance]") {
  Bench bench;
  bench.Open();
  bench.Details(/*may_destroy=*/true, /*on_instance_map=*/false);
  bench.Advance();
  bench.windows.answer.destroy = true;
  bench.Advance();
  const Bytes sent = bench.server.Receive(kDestroyCommandSize);
  CHECK(sent == ro::proto::EncodeInstanceDestroyCommand());
  CHECK(bench.windows.view.destroy == MemorialDestroyGate::kPending);
  // UN SECOND « OUI » PENDANT L'ATTENTE ne repart pas.
  bench.windows.answer.destroy = true;
  bench.Advance();
  CHECK(bench.Silent());
}

TEST_CASE("Instance : une destruction que le serveur refuserait ne part pas", "[app][instance]") {
  Bench bench;
  bench.Open();
  bench.Details(/*may_destroy=*/true, /*on_instance_map=*/false, /*destroyable=*/false);
  bench.Advance();
  bench.windows.answer.destroy = true;
  bench.Advance();
  CHECK(bench.Silent());
}

TEST_CASE("Instance : chaque déclaration de carte appelle « prêt à jouer »", "[app][instance]") {
  Bench bench;
  // Une autre trame ne fait rien partir.
  REQUIRE(ro::net::AnswerMapProperty(FrameOf({0x80, 0x00}), bench.link).has_value());
  CHECK(bench.Silent());
  // ZC_MAPPROPERTY_R2 : type 0, aucun drapeau.
  REQUIRE(ro::net::AnswerMapProperty(FrameOf({0x9B, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}),
                                     bench.link)
              .has_value());
  CHECK(bench.server.Receive(2) == Bytes{0x47, 0x04});
}
