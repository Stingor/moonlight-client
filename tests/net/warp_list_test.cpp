// LA LISTE DE DESTINATIONS, décodée — et la réponse, sur le fil.
//
// 🔴 LES TRAMES SONT BÂTIES PAR L'ENCODEUR ENGENDRÉ DE LA DISPOSITION DU
// SERVEUR, et la réponse se relit par son décodeur : ce banc croise notre
// lecture à la main avec la structure que le serveur compile, dans les deux
// sens.

#include "ro/net/warp_list.h"

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/packet_lengths.h"
#include "ro/proto/parsed_packets.h"
#include "ro/sim/warp_menu.h"
#include "ro/sim/world.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"

using ro::net::ApplyWarpListFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::WarpMenu;
using ro::sim::World;
using ro::test::Connect;
using ro::test::LoopbackServer;

namespace {

constexpr std::uint16_t kTeleport = 26;
constexpr std::uint16_t kWarpPortal = 27;

[[nodiscard]] Frame ListFrame(std::uint16_t skill_id, const std::vector<std::string>& maps) {
  ro::proto::ZcWarplist packet;
  packet.skillId = skill_id;
  for (const std::string& map : maps) {
    packet.maps.push_back(ro::proto::ZcWarplistSub{.map = map});
  }
  std::vector<std::uint8_t> bytes = ro::proto::Encode(packet);
  REQUIRE_FALSE(bytes.empty());
  return Frame{.opcode = ro::proto::ZcWarplist::kOpcode, .bytes = std::move(bytes)};
}

}  // namespace

TEST_CASE("Destinations : la longueur est variable, la réponse fait vingt octets",
          "[net][destinations]") {
  // LA TABLE DE LONGUEURS du serveur, pour les deux sens.
  const auto list_length = ro::proto::PacketLength(ro::proto::ZcWarplist::kOpcode);
  REQUIRE(list_length.has_value());
  CHECK(*list_length == -1);
  const auto answer_length = ro::proto::PacketLength(ro::proto::CzSelectWarppoint::kOpcode);
  REQUIRE(answer_length.has_value());
  CHECK(static_cast<std::size_t>(*answer_length) == ro::proto::CzSelectWarppoint::kWireSize);
  CHECK(ro::proto::CzSelectWarppoint::kWireSize == 20);
  // ET C'EST BIEN LA FONCTION QUI LIT LE CHOIX : le serveur lit la structure
  // entière, sans décalage déclaré.
  CHECK(ro::proto::MatchesServerLayout(ro::proto::CzSelectWarppoint::kOpcode,
                                       "clif_parse_UseSkillMap", {}));
  CHECK(ro::net::kWarpListHeaderSize == ro::proto::ZcWarplist::kWireSize);
  CHECK(ro::net::kWarpListEntrySize == ro::proto::ZcWarplistSub::kWireSize);
}

TEST_CASE("Destinations : la liste ouvre la question, dans l'ordre du serveur",
          "[net][destinations]") {
  World world;
  REQUIRE(ApplyWarpListFrame(ListFrame(kWarpPortal, {"prontera.gat", "geffen.gat"}), world) ==
          FrameOutcome::kApplied);
  const WarpMenu& menu = world.warp_menu();
  REQUIRE(menu.open());
  CHECK(menu.offer().skill_id == kWarpPortal);
  REQUIRE(menu.offer().destinations.size() == 2);
  CHECK(menu.offer().destinations[0] == "prontera.gat");
  CHECK(menu.offer().destinations[1] == "geffen.gat");
  CHECK(menu.offer_seq() == 1);
}

TEST_CASE("Destinations : un nom de seize lettres en perd une, une entrée vide ne compte pas",
          "[net][destinations]") {
  World world;
  REQUIRE(ApplyWarpListFrame(ListFrame(kTeleport, {"Random.gat", "", "abcdefghijklmnop"}), world) ==
          FrameOutcome::kApplied);
  const auto& destinations = world.warp_menu().offer().destinations;
  REQUIRE(destinations.size() == 2);
  CHECK(destinations[0] == "Random.gat");
  CHECK(destinations[1] == "abcdefghijklmno");
}

TEST_CASE("Destinations : des octets de trop après la dernière entrée sont ignorés",
          "[net][destinations]") {
  World world;
  Frame frame = ListFrame(kTeleport, {"Random.gat"});
  frame.bytes.push_back(0x41);
  frame.bytes.push_back(0x42);
  REQUIRE(ApplyWarpListFrame(frame, world) == FrameOutcome::kApplied);
  REQUIRE(world.warp_menu().offer().destinations.size() == 1);
  // Une trame trop courte pour son en-tête, elle, ne se lit pas.
  const Frame truncated{.opcode = ro::proto::ZcWarplist::kOpcode, .bytes = {0xBE, 0x0A, 0x04}};
  CHECK(ApplyWarpListFrame(truncated, world) == FrameOutcome::kUndecodable);
}

TEST_CASE("Destinations : une seconde liste remplace la première", "[net][destinations]") {
  World world;
  REQUIRE(ApplyWarpListFrame(ListFrame(kWarpPortal, {"prontera.gat"}), world) ==
          FrameOutcome::kApplied);
  REQUIRE(ApplyWarpListFrame(ListFrame(kWarpPortal, {"prontera.gat"}), world) ==
          FrameOutcome::kApplied);
  // DEUX LISTES IDENTIQUES SONT DEUX QUESTIONS.
  CHECK(world.warp_menu().offer_seq() == 2);
}

TEST_CASE("Destinations : une trame étrangère n'est pas à lui", "[net][destinations]") {
  World world;
  const Frame other{.opcode = ro::proto::CzSelectWarppoint::kOpcode, .bytes = {}};
  CHECK(ApplyWarpListFrame(other, world) == FrameOutcome::kUnhandled);
  CHECK_FALSE(world.warp_menu().open());
}

TEST_CASE("Destinations : un changement de carte emporte la question", "[sim][destinations]") {
  WarpMenu menu;
  menu.Offer(kWarpPortal, {"prontera.gat"}, 3);
  menu.CloseIfMapChanged(3);
  CHECK(menu.open());
  menu.CloseIfMapChanged(4);
  CHECK_FALSE(menu.open());
}

TEST_CASE("Destinations : le choix et l'annulation, sur le fil", "[net][destinations]") {
  LoopbackServer server;
  ro::net::Connection link = Connect(server);

  REQUIRE(ro::net::RequestWarpPoint(link, kWarpPortal, "prontera.gat").has_value());
  const std::vector<std::uint8_t> chosen = server.Receive(ro::proto::CzSelectWarppoint::kWireSize);
  REQUIRE(chosen.size() == ro::proto::CzSelectWarppoint::kWireSize);
  ro::proto::CzSelectWarppoint decoded;
  REQUIRE(ro::proto::Decode(chosen, decoded));
  CHECK(decoded.skill_id == kWarpPortal);
  CHECK(decoded.mapname == "prontera.gat");
  // Le reste du champ est fait de zéros.
  CHECK(chosen[4 + std::string("prontera.gat").size()] == 0);

  REQUIRE(ro::net::RequestWarpPoint(link, kTeleport, ro::net::kWarpCancelName).has_value());
  const std::vector<std::uint8_t> cancel = server.Receive(ro::proto::CzSelectWarppoint::kWireSize);
  REQUIRE(ro::proto::Decode(cancel, decoded));
  CHECK(decoded.skill_id == kTeleport);
  CHECK(decoded.mapname == "cancel");
}
