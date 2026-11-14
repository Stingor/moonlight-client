// « MONTRER MON ÉQUIPEMENT » ET « VOIR LES COSTUMES » SUR LE FIL : les demandes
// telles qu'elles partent, l'annonce et l'écho tels qu'ils arrivent.

#include "ro/net/player_config.h"

#include <cstdint>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/config_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/bank.h"
#include "ro/sim/equip_location.h"
#include "ro/sim/player_config.h"

#include "support/loopback_server.h"
#include "support/net_frames.h"
#include "support/wire_bytes.h"

using ro::net::ApplyConfigFrame;
using ro::net::FrameOutcome;
using ro::proto::ConfigAck;
using ro::proto::ConfigType;
using ro::sim::PlayerConfig;
using ro::test::LoopbackServer;
using ro::test_support::FrameOf;
using ro::test_support::ReadU16;
using ro::test_support::ReadU32;

namespace {

// L'annonce de l'entrée : l'opcode, puis UN octet.
[[nodiscard]] std::vector<std::uint8_t> Notice(std::uint8_t flag) {
  return {static_cast<std::uint8_t>(ro::proto::ZcConfigNotify::kOpcode & 0xFFU),
          static_cast<std::uint8_t>(ro::proto::ZcConfigNotify::kOpcode >> 8U), flag};
}

[[nodiscard]] std::vector<std::uint8_t> Echo(ConfigType type, std::uint32_t value) {
  return ro::proto::EncodeConfigAck(
      ConfigAck{.type = static_cast<std::uint32_t>(type), .value = value});
}

}  // namespace

TEST_CASE("Montrer mon équipement : rien n'est dit avant l'annonce", "[net][show_equip]") {
  // Rien, et non « caché » : c'est au serveur de le dire.
  const PlayerConfig config;
  CHECK_FALSE(config.equipment_shown().has_value());
}

TEST_CASE("Montrer mon équipement : l'annonce de l'entrée tient en un octet", "[net][show_equip]") {
  PlayerConfig config;
  REQUIRE(ApplyConfigFrame(FrameOf(Notice(1)), config) == FrameOutcome::kApplied);
  CHECK(config.equipment_shown() == true);
  REQUIRE(ApplyConfigFrame(FrameOf(Notice(0)), config) == FrameOutcome::kApplied);
  CHECK(config.equipment_shown() == false);
}

TEST_CASE("Montrer mon équipement : l'écho ramène toute valeur non nulle à vrai",
          "[net][show_equip]") {
  PlayerConfig config;
  REQUIRE(ApplyConfigFrame(FrameOf(Echo(ConfigType::kShowEquipment, 1)), config) ==
          FrameOutcome::kApplied);
  CHECK(config.equipment_shown() == true);
  REQUIRE(ApplyConfigFrame(FrameOf(Echo(ConfigType::kShowEquipment, 0)), config) ==
          FrameOutcome::kApplied);
  CHECK(config.equipment_shown() == false);
}

TEST_CASE("Montrer mon équipement : chaque annonce compte, même sans changement",
          "[net][show_equip]") {
  // C'est ce compte qui fait parler la conversation : la réponse à un clic qui
  // ne change rien doit s'y lire aussi.
  PlayerConfig config;
  CHECK(config.equipment_announcements() == 0);
  REQUIRE(ApplyConfigFrame(FrameOf(Notice(1)), config) == FrameOutcome::kApplied);
  REQUIRE(ApplyConfigFrame(FrameOf(Echo(ConfigType::kShowEquipment, 1)), config) ==
          FrameOutcome::kApplied);
  CHECK(config.equipment_announcements() == 2);
}

TEST_CASE("Voir les costumes : l'écho les dit masqués sur tout ce qui n'est pas zéro",
          "[net][show_costumes]") {
  PlayerConfig config;
  CHECK_FALSE(config.costumes_shown().has_value());
  REQUIRE(ApplyConfigFrame(FrameOf(Echo(ConfigType::kHideCostumes, 1)), config) ==
          FrameOutcome::kApplied);
  CHECK(config.costumes_shown() == false);
  REQUIRE(ApplyConfigFrame(FrameOf(Echo(ConfigType::kHideCostumes, 0)), config) ==
          FrameOutcome::kApplied);
  CHECK(config.costumes_shown() == true);
  // Le masque des places d'apparat, tel que la demande le fait partir : un
  // serveur qui le renverrait tel quel dirait encore « masqués ».
  REQUIRE(ApplyConfigFrame(FrameOf(Echo(ConfigType::kHideCostumes, ro::sim::kEquipCostumeMask)),
                           config) == FrameOutcome::kApplied);
  CHECK(config.costumes_shown() == false);
}

TEST_CASE("Voir les costumes : l'écho ne touche pas à l'équipement", "[net][show_costumes]") {
  // 🔴 LES COSTUMES ARRIVENT À CHAQUE CHANGEMENT DE CARTE : les prendre pour
  // notre équipement le basculerait sans clic, et écrirait une annonce dans la
  // conversation à chaque voyage.
  PlayerConfig config;
  REQUIRE(ApplyConfigFrame(FrameOf(Echo(ConfigType::kHideCostumes, 1)), config) ==
          FrameOutcome::kApplied);
  CHECK_FALSE(config.equipment_shown().has_value());
  CHECK(config.equipment_announcements() == 0);
}

TEST_CASE("Réglages : l'alimentation des compagnons reste non traitée ici", "[net][show_equip]") {
  // Les fiches des compagnons la lisent ; la prendre pour un des réglages
  // suivis en changerait un sans clic.
  PlayerConfig config;
  CHECK(ApplyConfigFrame(FrameOf(Echo(ConfigType::kPetAutofeed, 1)), config) ==
        FrameOutcome::kUnhandled);
  CHECK(ApplyConfigFrame(FrameOf(Echo(ConfigType::kHomunculusAutofeed, 1)), config) ==
        FrameOutcome::kUnhandled);
  CHECK_FALSE(config.equipment_shown().has_value());
  CHECK_FALSE(config.costumes_shown().has_value());
  CHECK_FALSE(config.callable().has_value());
  CHECK(config.equipment_announcements() == 0);
}

TEST_CASE("Appel d'urgence : le serveur tient « refusé », et chaque écho compte",
          "[net][show_equip]") {
  PlayerConfig config;
  CHECK_FALSE(config.callable().has_value());

  // 🔴 UN VEUT DIRE REFUSÉ : le serveur range ce réglage sous `disable_call`.
  REQUIRE(ApplyConfigFrame(FrameOf(Echo(ConfigType::kCall, 1)), config) == FrameOutcome::kApplied);
  REQUIRE(config.callable().has_value());
  CHECK_FALSE(*config.callable());
  CHECK(config.call_announcements() == 1);

  // Le même écho redit compte encore : il s'écrit à chaque connexion.
  REQUIRE(ApplyConfigFrame(FrameOf(Echo(ConfigType::kCall, 1)), config) == FrameOutcome::kApplied);
  CHECK(config.call_announcements() == 2);

  REQUIRE(ApplyConfigFrame(FrameOf(Echo(ConfigType::kCall, 0)), config) == FrameOutcome::kApplied);
  CHECK(*config.callable());
  CHECK(config.call_announcements() == 3);
  // Rien d'autre ne bouge.
  CHECK_FALSE(config.equipment_shown().has_value());
  CHECK(config.equipment_announcements() == 0);
}

TEST_CASE("Montrer mon équipement : un écho tronqué est indéchiffrable", "[net][show_equip]") {
  PlayerConfig config;
  std::vector<std::uint8_t> truncated = Echo(ConfigType::kShowEquipment, 1);
  truncated.pop_back();
  CHECK(ApplyConfigFrame(FrameOf(std::move(truncated)), config) == FrameOutcome::kUndecodable);
  CHECK_FALSE(config.equipment_shown().has_value());
}

TEST_CASE("Montrer mon équipement : la demande part en 0x02D8, réglage zéro", "[net][show_equip]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  auto link = ro::net::Connection::Open("127.0.0.1", server.port());
  REQUIRE(link.has_value());
  REQUIRE(server.Accept());
  REQUIRE(ro::net::RequestShowEquipment(*link, true).has_value());
  const std::vector<std::uint8_t> sent = server.Receive(ro::proto::ConfigRequest::kWireSize);
  REQUIRE(sent.size() == ro::proto::ConfigRequest::kWireSize);
  CHECK(ReadU16(sent, 0) == 0x02D8);
  CHECK(ReadU32(sent, 2) == 0);
  CHECK(ReadU32(sent, 6) == 1);
}

TEST_CASE("Voir les costumes : la demande part en réglage cinq, le sens inversé",
          "[net][show_costumes]") {
  // 🔴 MONTRER, C'EST ZÉRO ; MASQUER, LES QUATRE PLACES D'APPARAT. Une demande
  // envoyée dans le sens de la case masquerait les costumes qu'on veut voir.
  LoopbackServer server;
  REQUIRE(server.listening());
  auto link = ro::net::Connection::Open("127.0.0.1", server.port());
  REQUIRE(link.has_value());
  REQUIRE(server.Accept());

  REQUIRE(ro::net::RequestShowCostumes(*link, true).has_value());
  const std::vector<std::uint8_t> shown = server.Receive(ro::proto::ConfigRequest::kWireSize);
  REQUIRE(shown.size() == ro::proto::ConfigRequest::kWireSize);
  CHECK(ReadU16(shown, 0) == 0x02D8);
  CHECK(ReadU32(shown, 2) == 5);
  CHECK(ReadU32(shown, 6) == 0);

  REQUIRE(ro::net::RequestShowCostumes(*link, false).has_value());
  const std::vector<std::uint8_t> hidden = server.Receive(ro::proto::ConfigRequest::kWireSize);
  REQUIRE(hidden.size() == ro::proto::ConfigRequest::kWireSize);
  CHECK(ReadU32(hidden, 2) == 5);
  CHECK(ReadU32(hidden, 6) == 0x3C00);
}

TEST_CASE("Appel d'urgence : la demande part en réglage un, le sens inversé", "[net][show_equip]") {
  // 🔴 ACCEPTER, C'EST ZÉRO : le serveur range ce réglage sous `disable_call`.
  LoopbackServer server;
  REQUIRE(server.listening());
  auto link = ro::net::Connection::Open("127.0.0.1", server.port());
  REQUIRE(link.has_value());
  REQUIRE(server.Accept());

  REQUIRE(ro::net::RequestCallable(*link, true).has_value());
  const std::vector<std::uint8_t> accepted = server.Receive(ro::proto::ConfigRequest::kWireSize);
  REQUIRE(accepted.size() == ro::proto::ConfigRequest::kWireSize);
  CHECK(ReadU16(accepted, 0) == 0x02D8);
  CHECK(ReadU32(accepted, 2) == 1);
  CHECK(ReadU32(accepted, 6) == 0);

  REQUIRE(ro::net::RequestCallable(*link, false).has_value());
  const std::vector<std::uint8_t> refused = server.Receive(ro::proto::ConfigRequest::kWireSize);
  REQUIRE(refused.size() == ro::proto::ConfigRequest::kWireSize);
  CHECK(ReadU32(refused, 2) == 1);
  CHECK(ReadU32(refused, 6) == 1);
}
