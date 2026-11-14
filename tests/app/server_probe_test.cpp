// LA SONDE DU SERVEUR TEST : son verdict en fond, et la connexion TCP qu'elle
// tente sur la boucle locale.

#include "lobby/server_probe.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/socket.h"

using moonclient::lobby::ProbeInFlight;
using moonclient::lobby::ServerProbe;
using moonclient::lobby::TcpServerProbe;

namespace {

// Assez pour qu'un fil démarre et conclue sur n'importe quelle machine d'essai.
constexpr std::chrono::milliseconds kSettleWait{5000};
// Ce qu'on accorde à une sonde qui ne conclut jamais avant de la dire absente.
constexpr std::chrono::milliseconds kShortWait{20};

// Une sonde qui répond ce qu'on lui dit, et retient ce qu'on lui a demandé.
class FixedProbe final : public ServerProbe {
 public:
  explicit FixedProbe(bool answer) : answer_(answer) {}
  bool Answers(std::string_view host, std::uint16_t port) override {
    asked_host = std::string(host);
    asked_port = port;
    return answer_;
  }
  std::string asked_host;
  std::uint16_t asked_port = 0;

 private:
  bool answer_;
};

// Une sonde qui ne conclut qu'une fois relâchée : celle d'un hôte qui tarde.
class StalledProbe final : public ServerProbe {
 public:
  bool Answers(std::string_view /*host*/, std::uint16_t /*port*/) override {
    std::unique_lock wait(guard);
    released_signal.wait(wait, [this] { return released; });
    return true;
  }
  void Release() {
    const std::scoped_lock release(guard);
    released = true;
    released_signal.notify_all();
  }
  std::mutex guard;
  std::condition_variable released_signal;
  bool released = false;
};

}  // namespace

TEST_CASE("Sonde : le verdict en fond est celui de la sonde", "[app][probe]") {
  auto answering = std::make_shared<FixedProbe>(true);
  CHECK(ProbeInFlight::Launch(answering, "127.0.0.1", 6900).Detected(kSettleWait));
  CHECK(answering->asked_host == "127.0.0.1");
  CHECK(answering->asked_port == 6900);

  CHECK_FALSE(
      ProbeInFlight::Launch(std::make_shared<FixedProbe>(false), "h", 1).Detected(kSettleWait));
}

TEST_CASE("Sonde : une sonde qui n'a pas conclu vaut absent, sans retenir", "[app][probe]") {
  auto stalled = std::make_shared<StalledProbe>();
  const ProbeInFlight probe = ProbeInFlight::Launch(stalled, "h", 1);
  CHECK_FALSE(probe.Detected(kShortWait));
  // Relâchée, elle conclut : le fil détaché finit sans personne pour l'attendre.
  stalled->Release();
  CHECK(probe.Detected(kSettleWait));
}

TEST_CASE("Sonde : la connexion TCP aboutit sur un écouteur, échoue sur un port fermé",
          "[app][probe]") {
  auto listening = ro::net::TcpListener::ListenLocal(0);
  REQUIRE(listening.has_value());
  const std::uint16_t open_port = listening->port();
  TcpServerProbe probe;
  // ⚠ SANS ACCUEIL : la file d'attente du système suffit à établir la connexion,
  // et c'est tout ce que la sonde demande.
  CHECK(probe.Answers("127.0.0.1", open_port));

  // Le même port, écouteur fermé : plus personne.
  listening->Close();
  CHECK_FALSE(probe.Answers("127.0.0.1", open_port));
}
