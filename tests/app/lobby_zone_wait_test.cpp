#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/common_packets.h"

#include "lobby/lobby_flow.h"
#include "support/lobby_realm.h"

using namespace lobby_test;

namespace {

// Les cartes accessibles qu'annonce un serveur dont aucune carte n'est servie :
// une entrée, marquée indisponible.
std::vector<std::uint8_t> NoMapServed() {
  ro::proto::HcNotifyAccessibleMapname answer;
  ro::proto::HcNotifyAccessibleMapnameSub only;
  only.status = 1;
  only.map = "prontera.gat";
  answer.maps = {only};
  return Encode(answer);
}

}  // namespace

TEST_CASE("Lobby : un serveur qui démarre ramène à la tablée, puis laisse réessayer",
          "[app][lobby]") {
  Realm realm;
  REQUIRE(realm.listening());
  std::uint32_t clock_ms = 1000;

  LobbyFlow flow("127.0.0.1", realm.login.port());
  flow.Submit("compte", "motdepasse");
  Pump(flow, clock_ms);
  REQUIRE(PlayLogin(realm, flow, clock_ms));
  REQUIRE(PlayCharacters(realm, flow, clock_ms, CharacterList()));

  flow.ChooseCharacter(0);
  REQUIRE(realm.characters.Receive(ro::proto::ChSelectChar::kWireSize).size() ==
          ro::proto::ChSelectChar::kWireSize);
  // 🔴 LE SERVEUR PARLE : aucune carte n'est servie, il le dit par la liste des
  // cartes accessibles. Ne pas lire cette réponse laissait l'écran d'entrée en
  // jeu attendre une adresse de zone qui ne viendrait jamais.
  REQUIRE(realm.characters.Send(NoMapServed()));
  Pump(flow, clock_ms);

  CHECK(flow.screen() == LobbyScreenKind::kCharacters);
  CHECK(flow.zone_unavailable());

  // La liaison est gardée : redemander mène à la carte, sans reconnexion.
  flow.ChooseCharacter(0);
  CHECK_FALSE(flow.zone_unavailable());
  REQUIRE(PlayZone(realm, flow, clock_ms, "prontera.gat"));
  Pump(flow, clock_ms);
  CHECK(flow.screen() == LobbyScreenKind::kHandoff);
}
