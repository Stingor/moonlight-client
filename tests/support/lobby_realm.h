#pragma once

// LE BANC DU LOBBY : trois serveurs de bouclage et les trames qu'ils jouent.
// Partagé par les essais de `LobbyFlow`, qui dépasseraient sinon le plafond de
// taille d'un fichier.

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "ro/proto/common_packets.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"

#include "lobby/backdrop_session.h"
#include "lobby/lobby_flow.h"
#include "support/loopback_server.h"

namespace lobby_test {

using moonclient::lobby::BackdropState;
using moonclient::lobby::LobbyFlow;
using moonclient::lobby::LobbyScreenKind;
using ro::test::LoopbackServer;

// 127.0.0.1 tel que le serveur l'écrit sur le fil : en ordre RÉSEAU, donc les
// octets se lisent du plus faible au plus fort après décodage.
constexpr std::uint32_t kLoopbackWireIp = 0x0100007F;

// La demande d'entrée sur le serveur de personnages : opcode, compte, deux
// jetons, un remplissage, le sexe.
constexpr std::size_t kEnterRequestSize = 17;
// Ses champs : l'opcode 0x0065 (octet faible), le compte, puis les deux jetons.
constexpr std::uint8_t kEnterOpcodeLow = 0x65;
constexpr std::size_t kEnterAccountOffset = 2;
constexpr std::size_t kEnterFirstTokenOffset = 6;
constexpr std::size_t kEnterSecondTokenOffset = 10;

constexpr std::uint32_t kAccountId = 2'000'123;

// De combien l'horloge du banc avance par tour. Ce n'est pas une cadence
// d'image : c'est juste de quoi distinguer deux tours.
constexpr std::uint32_t kTurnMs = 1;

// Assez de tours pour que les octets traversent la pile du système, pas assez
// pour qu'un essai bloqué suspende la suite.
constexpr int kSettleTurns = 60;

inline std::vector<std::uint8_t> BareAccountId(std::uint32_t account_id) {
  return {static_cast<std::uint8_t>(account_id & 0xFF),
          static_cast<std::uint8_t>((account_id >> 8) & 0xFF),
          static_cast<std::uint8_t>((account_id >> 16) & 0xFF),
          static_cast<std::uint8_t>((account_id >> 24) & 0xFF)};
}

inline std::vector<std::uint8_t> AcceptedLogin(std::uint16_t character_port) {
  ro::proto::AcAcceptLogin accepted;
  accepted.AID = kAccountId;
  accepted.login_id1 = 111;
  accepted.login_id2 = 222;
  accepted.sex = 1;
  ro::proto::AcAcceptLoginSub only;
  only.name = "Moonlight";
  only.ip = kLoopbackWireIp;
  only.port = character_port;
  accepted.char_servers = {only};
  return Encode(accepted);
}

inline std::vector<std::uint8_t> CharacterList() {
  ro::proto::HcAcceptEnter list;
  list.total = 1;
  ro::proto::CharacterInfo only;
  only.GID = 150'000;
  only.name = "Alice";
  only.CharNum = 0;
  list.characters = {only};
  return Encode(list);
}

// UNE LISTE À TROU : les emplacements 0 et 2, et rien au 1.
//
// 🔴 C'EST LE SEUL GENRE DE COMPTE QUI SÉPARE L'EMPLACEMENT DU RANG, et la
// création est ce qui le creuse. Sur une liste dense, indexer par l'un ou par
// l'autre donne le même personnage — donc tous les comptes d'essai passent.
inline std::vector<std::uint8_t> GappedCharacterList() {
  ro::proto::HcAcceptEnter list;
  list.total = 3;
  ro::proto::CharacterInfo first;
  first.GID = 150'000;
  first.name = "Alice";
  first.CharNum = 0;
  ro::proto::CharacterInfo third;
  third.GID = 150'002;
  third.name = "Carole";
  third.CharNum = 2;
  list.characters = {first, third};
  return Encode(list);
}

inline std::vector<std::uint8_t> EmptyCharacterList() {
  ro::proto::HcAcceptEnter list;
  list.total = 0;
  return Encode(list);
}

inline std::vector<std::uint8_t> ZoneHandoff(std::uint16_t zone_port, std::string_view map) {
  ro::proto::HcNotifyZonesvr handoff;
  handoff.CID = 150'000;
  handoff.mapname = std::string(map);
  handoff.ip = kLoopbackWireIp;
  handoff.port = zone_port;
  return Encode(handoff);
}

inline std::vector<std::uint8_t> MapAccepted() {
  ro::proto::ZcAcceptEnter accepted;
  accepted.startTime = 555'000;
  accepted.posDir = {0x0A, 0x1B, 0x2C};
  return Encode(accepted);
}

// LES TROIS SERVEURS D'UNE SESSION, sur trois ports de la boucle locale.
//
// ⚠ IL EN FAUT BIEN TROIS : le client ouvre une liaison par acte, et l'adresse
// des deux derniers est CHOISIE PAR LE SERVEUR — elle voyage dans ses réponses.
// Un banc à un seul port ne pourrait pas éprouver ce qui nous intéresse ici,
// qui est justement l'enchaînement.
struct Realm {
  LoopbackServer login;
  LoopbackServer characters;
  LoopbackServer zone;

  [[nodiscard]] bool listening() const {
    return login.listening() && characters.listening() && zone.listening();
  }
};

// Fait tourner l'arbitre quelques tours. L'horloge avance avec lui : c'est elle
// que les bornes de vie du décor mesurent.
inline void Pump(LobbyFlow& flow, std::uint32_t& clock_ms, int turns = kSettleTurns) {
  for (int turn = 0; turn < turns; ++turn) {
    flow.Advance(clock_ms);
    clock_ms += kTurnMs;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

// Fait tourner l'arbitre jusqu'à ce que la condition tienne, et rend l'INSTANT
// où elle a basculé — zéro si elle n'a pas tenu.
//
// 🔴 CET INSTANT NE SE DEVINE PAS, et le croire est une erreur que ce banc a
// commise : une borne de vie se compte depuis la bascule, pas depuis l'horloge
// d'après. La mise en place coûte quelques dizaines de tours, et un essai qui
// daterait la borne de la fin verrait la session se rendre un tour trop tôt —
// puis accuserait le code.
template <typename Holds>
[[nodiscard]] std::uint32_t PumpUntil(LobbyFlow& flow, std::uint32_t& clock_ms, Holds holds) {
  for (int turn = 0; turn < kSettleTurns; ++turn) {
    flow.Advance(clock_ms);
    if (holds()) {
      return clock_ms;
    }
    clock_ms += kTurnMs;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return 0;
}

// Joue le serveur de connexion jusqu'à ce qu'il ait renvoyé sa liste.
[[nodiscard]] inline bool PlayLogin(Realm& realm, LobbyFlow& flow, std::uint32_t& clock_ms) {
  if (!realm.login.Accept()) {
    return false;
  }
  if (realm.login.Receive(ro::proto::CaLogin::kWireSize).size() != ro::proto::CaLogin::kWireSize) {
    return false;
  }
  if (!realm.login.Send(AcceptedLogin(realm.characters.port()))) {
    return false;
  }
  Pump(flow, clock_ms);
  return true;
}

// Joue le serveur de personnages jusqu'à la liste donnée.
[[nodiscard]] inline bool PlayCharacters(Realm& realm, LobbyFlow& flow, std::uint32_t& clock_ms,
                                         const std::vector<std::uint8_t>& list) {
  if (!realm.characters.Accept()) {
    return false;
  }
  if (realm.characters.Receive(kEnterRequestSize).size() != kEnterRequestSize) {
    return false;
  }
  if (!realm.characters.Send(BareAccountId(kAccountId)) || !realm.characters.Send(list)) {
    return false;
  }
  Pump(flow, clock_ms);
  return true;
}

// Joue la remise au serveur de zone, puis l'entrée sur la carte.
[[nodiscard]] inline bool PlayZone(Realm& realm, LobbyFlow& flow, std::uint32_t& clock_ms,
                                   std::string_view map) {
  if (realm.characters.Receive(ro::proto::ChSelectChar::kWireSize).size() !=
      ro::proto::ChSelectChar::kWireSize) {
    return false;
  }
  if (!realm.characters.Send(ZoneHandoff(realm.zone.port(), map))) {
    return false;
  }
  Pump(flow, clock_ms);
  if (!realm.zone.Accept()) {
    return false;
  }
  if (realm.zone.Receive(ro::proto::EnterMapRequest::kWireSize).size() !=
      ro::proto::EnterMapRequest::kWireSize) {
    return false;
  }
  // ⚠ LES DEUX D'UN SEUL TENANT : le serveur de zone fait précéder son accord
  // d'un identifiant NU, et c'est ce qui SUIT ces quatre octets qui dit s'ils
  // portent une trame. Les envoyer séparément marche aussi — la phase attend —
  // mais les envoyer ensemble éprouve le chemin ordinaire.
  std::vector<std::uint8_t> arrival = BareAccountId(kAccountId);
  const std::vector<std::uint8_t> accepted = MapAccepted();
  arrival.insert(arrival.end(), accepted.begin(), accepted.end());
  return realm.zone.Send(arrival);
}

// Monte un décor VIVANT de bout en bout, et rend l'instant de la bascule —
// zéro si le banc n'a pas suivi. Voir `PumpUntil` pour pourquoi cet instant se
// relève au lieu de se déduire.
[[nodiscard]] inline std::uint32_t RaiseBackdrop(Realm& realm, LobbyFlow& flow,
                                                 std::uint32_t& clock_ms) {
  flow.OpenBackdrop(clock_ms);
  Pump(flow, clock_ms);
  if (!PlayLogin(realm, flow, clock_ms)) {
    return 0;
  }
  if (!PlayCharacters(realm, flow, clock_ms, CharacterList())) {
    return 0;
  }
  // Le décor choisit l'emplacement zéro tout seul : il n'a qu'un personnage.
  if (!PlayZone(realm, flow, clock_ms, "prontera.gat")) {
    return 0;
  }
  return PumpUntil(flow, clock_ms,
                   [&flow] { return flow.backdrop().state() == BackdropState::kLive; });
}

}  // namespace lobby_test
