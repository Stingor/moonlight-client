#pragma once

// LA LIAISON D'UN ESSAI À SON SERVEUR EN BOUCLE LOCALE : le client ouvre, le
// serveur accepte, et l'essai garde la connexion pour parler au serveur.

#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"

#include "support/loopback_server.h"

namespace ro::test {

// Une liaison établie sur la boucle locale, prête des deux côtés ; l'essai
// tombe si l'un des deux bouts refuse.
//
// Le socket client est non bloquant : la connexion peut rendre la main avant
// que le serveur ait accepté, et l'acceptation avant que le client ait fini sa
// poignée de main. Les deux se rejoignent en quelques microsecondes.
[[nodiscard]] inline net::Connection Connect(LoopbackServer& server) {
  auto link = net::Connection::Open("127.0.0.1", server.port());
  REQUIRE(link.has_value());
  REQUIRE(server.Accept());
  return std::move(*link);
}

}  // namespace ro::test
