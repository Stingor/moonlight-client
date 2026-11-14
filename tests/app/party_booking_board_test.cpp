// LA RECHERCHE DE GROUPE côté jeu : la table des lieux, les avis du serveur dans
// la case « Recherche de groupe » de la conversation, la vue composée, et les
// intentions de la fenêtre qui partent sur le fil.

#include "party/party_booking_board.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/party_booking_packets.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/world.h"

#include "party/party_booking_places.h"
#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/net_frames.h"

using moonclient::PartyBookingBoard;
using moonclient::PartyBookingPlaces;
using moonclient::PartyBookingWindows;
using ro::test::Connect;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kNowMs = 10'000;
constexpr std::int16_t kKnight = 11;
constexpr std::int16_t kLevel = 50;
// Région 1 (Prontera), lieu 0 (Fields).
constexpr std::int16_t kPronteraFields = 0x0100;

constexpr std::string_view kPlacesYaml = R"(version: 1
regions:
  - name: All Maps
    offered: false
    places: [All Maps]
  - name: Prontera
    places: [Fields, Prontera Culvert F1]
  - 12
)";

PartyBookingPlaces Places() {
  auto parsed = PartyBookingPlaces::Parse(kPlacesYaml);
  REQUIRE(parsed.has_value());
  return std::move(*parsed);
}

ro::proto::PartyBookingEntry Entry(std::uint32_t index, const char* leader) {
  ro::proto::PartyBookingEntry entry;
  entry.index = index;
  entry.leader_name = leader;
  entry.level = kLevel;
  entry.map_code = kPronteraFields;
  entry.jobs = {kKnight, ro::sim::kPartyBookingRoleAttack, -1, -1, -1, -1};
  return entry;
}

std::vector<const ro::sim::ChatLine*> PartySearchLines(const ro::sim::World& world) {
  std::vector<const ro::sim::ChatLine*> lines;
  for (const ro::sim::ChatLine& line : world.chat_lines()) {
    if (line.type == ro::sim::ChatType::kPartySearch) {
      lines.push_back(&line);
    }
  }
  return lines;
}

}  // namespace

TEST_CASE("Recherche de groupe : la table des lieux garde le rang de chaque entrée",
          "[app][party_booking]") {
  const PartyBookingPlaces places = Places();
  REQUIRE(places.regions().size() == 3);
  CHECK_FALSE(places.regions()[0].offered);
  CHECK(places.regions()[1].offered);
  CHECK(places.NameOf(1, 1) == "Prontera — Prontera Culvert F1");
  // L'entrée illisible reste à sa place, vide, et se dit.
  CHECK(places.regions()[2].places.empty());
  CHECK_FALSE(places.problems().empty());
  CHECK(places.NameOf(4, 0).empty());

  CHECK_FALSE(PartyBookingPlaces::Parse("version: 2\nregions: []\n").has_value());
  CHECK_FALSE(PartyBookingPlaces::Parse("version: 1\n").has_value());
}

TEST_CASE("Recherche de groupe : les avis s'écrivent dans la case 15, en cyan",
          "[app][party_booking]") {
  PartyBookingBoard board;
  ro::sim::World world;
  // La liste fermée : une annonce publiée se signale.
  CHECK(board.ApplyFrame(
            FrameOf(ro::proto::EncodePartyBookingInsertNotice({.entry = Entry(3, "Alice")})), world,
            kNowMs) == ro::net::FrameOutcome::kApplied);
  // Une recherche vide le dit aussi.
  CHECK(board.ApplyFrame(FrameOf(ro::proto::EncodePartyBookingSearchAck({})), world, kNowMs) ==
        ro::net::FrameOutcome::kApplied);
  const std::vector<const ro::sim::ChatLine*> lines = PartySearchLines(world);
  REQUIRE(lines.size() == 2);
  CHECK(lines[0]->text == "Une annonce de groupe vient d'être publiée.");
  CHECK(lines[0]->color == moonclient::kPartyBookingChatColor);
  CHECK(lines[0]->kind == ro::sim::ChatKind::kServer);
  CHECK(lines[1]->text == "Aucune annonce de groupe ne correspond à cette recherche.");
}

TEST_CASE("Recherche de groupe : la vue nomme lieux et métiers, et la fenêtre parle au serveur",
          "[app][party_booking]") {
  ro::test::LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  PartyBookingBoard board;
  board.SetPlaces(Places());
  ro::sim::World world;
  PartyBookingWindows windows;

  // Fermée, rien ne se compose ; l'icône en demande l'ouverture.
  moonclient::TogglePartyBooking(windows);
  board.Advance(windows, link, world, kNowMs);
  REQUIRE(windows.window.open);
  REQUIRE(windows.view != nullptr);
  // Seule la région proposée entre dans la vue, avec le code de son rang.
  REQUIRE(windows.view->regions.size() == 1);
  CHECK(windows.view->regions[0].name == "Prontera");
  REQUIRE(windows.view->regions[0].places.size() == 2);
  CHECK(windows.view->regions[0].places[1].map_code == 0x0101);
  CHECK_FALSE(windows.view->job_groups.empty());

  // La recherche demandée part ; sa page s'affiche, la liste ouverte.
  windows.answer.verb = ro::ui::PartyBookingVerb::kSearch;
  windows.answer.level = kLevel;
  windows.answer.job = kKnight;
  board.Advance(windows, link, world, kNowMs);
  const std::vector<std::uint8_t> sent =
      server.Receive(ro::proto::PartyBookingSearchRequest::kWireSize);
  REQUIRE(sent.size() == ro::proto::PartyBookingSearchRequest::kWireSize);
  CHECK(sent[0] == 0x04);
  CHECK(windows.view->searching);

  ro::proto::PartyBookingSearchAck page;
  page.entries = {Entry(3, "Alice")};
  REQUIRE(board.ApplyFrame(FrameOf(ro::proto::EncodePartyBookingSearchAck(page)), world, kNowMs) ==
          ro::net::FrameOutcome::kApplied);
  board.Advance(windows, link, world, kNowMs);
  REQUIRE(windows.view->rows.size() == 1);
  const ro::ui::PartyBookingRowView& row = windows.view->rows[0];
  CHECK(row.leader == "Alice");
  CHECK(row.levels == "50~65");
  CHECK(row.place == "Prontera — Fields");
  CHECK(row.jobs == "Knight, Attaque");

  // Liste ouverte, une annonce poussée y entre sans rien écrire.
  REQUIRE(board.ApplyFrame(
              FrameOf(ro::proto::EncodePartyBookingInsertNotice({.entry = Entry(4, "Bob")})), world,
              kNowMs) == ro::net::FrameOutcome::kApplied);
  board.Advance(windows, link, world, kNowMs);
  CHECK(windows.view->rows.size() == 2);
  CHECK(PartySearchLines(world).empty());

  // Chuchoter à l'auteur ne part pas au serveur : `app` le route.
  windows.answer.verb = ro::ui::PartyBookingVerb::kWhisperWindow;
  windows.answer.name = "Alice";
  board.Advance(windows, link, world, kNowMs);
  const auto whisper = board.TakeWhisper();
  REQUIRE(whisper.has_value());
  CHECK(whisper->name == "Alice");
  CHECK(whisper->own_window);
  CHECK_FALSE(board.TakeWhisper().has_value());

  // L'icône referme la fenêtre ouverte.
  moonclient::TogglePartyBooking(windows);
  CHECK_FALSE(windows.window.open);
}
