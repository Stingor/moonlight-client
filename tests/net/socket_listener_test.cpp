// L'ÉCOUTE SUR LA BOUCLE LOCALE : un port choisi par le système, un accueil qui
// n'attend pas, et des octets qui passent dans les deux sens.

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/socket.h"

using ro::net::ReceiveState;
using ro::net::TcpListener;
using ro::net::TcpSocket;

namespace {

// Assez de tours pour qu'une connexion locale aboutisse, sans jamais dormir.
constexpr int kPollTurns = 200000;

[[nodiscard]] std::optional<TcpSocket> AcceptSoon(TcpListener& listener) {
  for (int turn = 0; turn < kPollTurns; ++turn) {
    auto accepted = listener.Accept();
    REQUIRE(accepted.has_value());
    if (accepted->has_value()) {
      return std::move(**accepted);
    }
  }
  return std::nullopt;
}

[[nodiscard]] std::size_t ReceiveSoon(TcpSocket& socket, std::span<std::uint8_t> into) {
  for (int turn = 0; turn < kPollTurns; ++turn) {
    const auto read = socket.Receive(into);
    REQUIRE(read.has_value());
    if (read->state == ReceiveState::kBytes && read->bytes > 0) {
      return read->bytes;
    }
  }
  return 0;
}

}  // namespace

TEST_CASE("Écoute locale : accueil sans attente, octets dans les deux sens", "[net][socket]") {
  auto listening = TcpListener::ListenLocal(0);
  REQUIRE(listening.has_value());
  TcpListener listener = std::move(*listening);
  REQUIRE(listener.port() != 0);

  // PERSONNE N'APPELLE ENCORE : l'accueil rend la main, vide.
  const auto nobody = listener.Accept();
  REQUIRE(nobody.has_value());
  CHECK_FALSE(nobody->has_value());

  auto connected = TcpSocket::Connect("127.0.0.1", listener.port());
  REQUIRE(connected.has_value());
  std::optional<TcpSocket> accepted = AcceptSoon(listener);
  REQUIRE(accepted.has_value());

  constexpr std::string_view kAsked = "1 state\n";
  const auto sent = connected->Send(std::span<const std::uint8_t>(
      reinterpret_cast<const std::uint8_t*>(kAsked.data()), kAsked.size()));
  REQUIRE(sent.has_value());
  CHECK(*sent == kAsked.size());

  std::array<std::uint8_t, 64> received{};
  const std::size_t bytes = ReceiveSoon(*accepted, received);
  CHECK(std::string_view(reinterpret_cast<const char*>(received.data()), bytes) == kAsked);
}
