// CHANGER DE SERVEUR SUR L'ÉCRAN DE CONNEXION : la connexion suivante et le
// décor partent vers la nouvelle adresse, et seulement depuis le formulaire.

#include <chrono>
#include <cstdint>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include "lobby/backdrop_session.h"
#include "lobby/lobby_flow.h"
#include "support/loopback_server.h"

using moonclient::lobby::BackdropState;
using moonclient::lobby::LobbyFlow;
using moonclient::lobby::LobbyScreenKind;
using ro::test::LoopbackServer;

namespace {

constexpr std::uint32_t kTurnMs = 1;
// Assez de tours pour que la connexion traverse la pile du système.
constexpr int kSettleTurns = 60;

void Pump(LobbyFlow& flow, std::uint32_t& clock_ms) {
  for (int turn = 0; turn < kSettleTurns; ++turn) {
    flow.Advance(clock_ms);
    clock_ms += kTurnMs;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

}  // namespace

TEST_CASE("Serveur choisi : la connexion suivante part vers la nouvelle adresse",
          "[app][lobby][servers]") {
  LoopbackServer first;
  LoopbackServer second;
  REQUIRE(first.listening());
  REQUIRE(second.listening());
  std::uint32_t clock_ms = 1000;

  LobbyFlow flow("127.0.0.1", first.port());
  REQUIRE(flow.Retarget("127.0.0.1", second.port()));
  flow.Submit("compte", "motdepasse");
  Pump(flow, clock_ms);
  CHECK(second.Accept());
}

TEST_CASE("Serveur choisi : refusé pendant une connexion", "[app][lobby][servers]") {
  LoopbackServer first;
  REQUIRE(first.listening());

  LobbyFlow flow("127.0.0.1", first.port());
  flow.Submit("compte", "motdepasse");
  REQUIRE(flow.screen() == LobbyScreenKind::kConnecting);
  CHECK_FALSE(flow.Retarget("127.0.0.1", 1));
}

TEST_CASE("Serveur choisi : le décor rend sa session et se rouvre sur le nouveau serveur",
          "[app][lobby][servers]") {
  LoopbackServer first;
  LoopbackServer second;
  REQUIRE(first.listening());
  REQUIRE(second.listening());
  std::uint32_t clock_ms = 1000;

  LobbyFlow flow("127.0.0.1", first.port());
  flow.OpenBackdrop(clock_ms);
  Pump(flow, clock_ms);
  REQUIRE(first.Accept());
  REQUIRE(flow.backdrop().state() == BackdropState::kOpening);

  REQUIRE(flow.Retarget("127.0.0.1", second.port()));
  // L'ancienne session est rendue, et rien ne se rouvre seul.
  CHECK(first.ClientHungUp());
  CHECK(flow.backdrop().state() == BackdropState::kAsleep);

  flow.OpenBackdrop(clock_ms);
  Pump(flow, clock_ms);
  CHECK(second.Accept());
}
