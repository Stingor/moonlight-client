// LA RECHERCHE DE GROUPE, décodée — et ses demandes, sur le fil.
//
// 🔴 CE BANC PORTE CE QU'AUCUNE SESSION NE MONTRERAIT VITE :
//
//   · chaque réponse et chaque avis atteint le modèle ;
//   · la page suivante repart des critères de la dernière recherche, au rang
//     qui suit sa dernière annonce ;
//   · un verbe refuse sans rien envoyer : sans annonce, une annonce déjà
//     publiée, un lieu ET un métier à la fois.

#include "ro/net/party_booking.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/party_booking_packets.h"
#include "ro/sim/party_booking.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/net_frames.h"
#include "support/wire_bytes.h"

using ro::net::ApplyPartyBookingFrame;
using ro::net::FrameOutcome;
using ro::sim::PartyBooking;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::FrameOf;
using ro::test_support::ReadU32;

namespace {

constexpr std::int16_t kKnight = 11;
constexpr std::int16_t kLevel = 50;
constexpr ro::sim::PartyBookingJobs kKnightOnly = {kKnight, -1, -1, -1, -1, -1};

ro::proto::PartyBookingEntry Entry(std::uint32_t index, const char* leader) {
  ro::proto::PartyBookingEntry entry;
  entry.index = index;
  entry.leader_name = leader;
  entry.level = kLevel;
  entry.jobs = kKnightOnly;
  return entry;
}

}  // namespace

TEST_CASE("Recherche de groupe : chaque trame atteint le modèle", "[net][party_booking]") {
  PartyBooking booking;
  booking.SetOwnName("Moi");

  ro::proto::PartyBookingSearchAck page;
  page.more_results = true;
  page.entries = {Entry(3, "Alice"), Entry(5, "Bob")};
  REQUIRE(ApplyPartyBookingFrame(FrameOf(ro::proto::EncodePartyBookingSearchAck(page)), booking,
                                 true) == FrameOutcome::kApplied);
  REQUIRE(booking.results().size() == 2);
  CHECK(booking.results()[0].leader_name == "Alice");
  CHECK(booking.more_on_server());

  // Une annonce poussée, liste ouverte : elle entre.
  REQUIRE(ApplyPartyBookingFrame(
              FrameOf(ro::proto::EncodePartyBookingInsertNotice({.entry = Entry(8, "Carl")})),
              booking, true) == FrameOutcome::kApplied);
  CHECK(booking.results().size() == 3);

  REQUIRE(ApplyPartyBookingFrame(FrameOf(ro::proto::EncodePartyBookingUpdateNotice(
                                     {.index = 8, .jobs = {12, -1, -1, -1, -1, -1}})),
                                 booking, true) == FrameOutcome::kApplied);
  CHECK(booking.results()[2].jobs[0] == 12);

  REQUIRE(ApplyPartyBookingFrame(FrameOf(ro::proto::EncodePartyBookingDeleteNotice({.index = 3})),
                                 booking, true) == FrameOutcome::kApplied);
  CHECK(booking.results().size() == 2);

  REQUIRE(ApplyPartyBookingFrame(FrameOf(ro::proto::EncodePartyBookingRegisterAck({.result = 1})),
                                 booking, true) == FrameOutcome::kApplied);
  REQUIRE(ApplyPartyBookingFrame(FrameOf(ro::proto::EncodePartyBookingDeleteAck({.result = 2})),
                                 booking, true) == FrameOutcome::kApplied);
  CHECK(booking.TakeNotices() ==
        std::vector<ro::sim::PartyBookingNotice>{ro::sim::PartyBookingNotice::kRegisterRefused,
                                                 ro::sim::PartyBookingNotice::kDeleteRefused});

  // Une trame tronquée ne se lit pas ; une trame d'un autre domaine n'est pas la sienne.
  std::vector<std::uint8_t> broken = ro::proto::EncodePartyBookingDeleteNotice({.index = 3});
  broken.pop_back();
  CHECK(ApplyPartyBookingFrame(FrameOf(broken), booking, true) == FrameOutcome::kUndecodable);
  CHECK(ApplyPartyBookingFrame(FrameOf({0x35, 0x08}), booking, true) == FrameOutcome::kUnhandled);
}

TEST_CASE("Recherche de groupe : recherche, page, publication, modification et retrait, sur le fil",
          "[net][party_booking]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  PartyBooking booking;
  booking.SetOwnName("Moi");

  // Un lieu ET un métier ne partent pas ; sans page annoncée, pas de suivante.
  CHECK_FALSE(ro::net::RequestPartyBookingSearch(
                  link, booking, ro::sim::PartyBookingCriteria{.map_code = 0x0101, .job = kKnight})
                  .has_value());
  CHECK_FALSE(ro::net::RequestPartyBookingNextPage(link, booking).has_value());
  // Sans annonce, ni modification ni retrait.
  CHECK_FALSE(ro::net::RequestPartyBookingUpdate(link, booking, kKnightOnly).has_value());
  CHECK_FALSE(ro::net::RequestPartyBookingDelete(link, booking).has_value());

  REQUIRE(ro::net::RequestPartyBookingSearch(link, booking,
                                             ro::sim::PartyBookingCriteria{.job = kKnight})
              .has_value());
  const std::vector<std::uint8_t> search = server.Receive(14);
  REQUIRE(search.size() == 14);
  CHECK(search[0] == 0x04);
  CHECK(search[6] == kKnight);
  CHECK(ReadU32(search, 8) == 0);
  CHECK(booking.searching());

  ro::proto::PartyBookingSearchAck page;
  page.more_results = true;
  page.entries = {Entry(3, "Alice"), Entry(6, "Bob")};
  REQUIRE(ApplyPartyBookingFrame(FrameOf(ro::proto::EncodePartyBookingSearchAck(page)), booking,
                                 true) == FrameOutcome::kApplied);
  REQUIRE(ro::net::RequestPartyBookingNextPage(link, booking).has_value());
  const std::vector<std::uint8_t> next = server.Receive(14);
  REQUIRE(next.size() == 14);
  CHECK(next[6] == kKnight);
  CHECK(ReadU32(next, 8) == 7);

  ro::sim::PartyBookingAd draft;
  draft.leader_name = "Moi";
  draft.level = kLevel;
  draft.map_code = 0x0101;
  draft.jobs = kKnightOnly;
  REQUIRE(ro::net::RequestPartyBookingRegister(link, booking, draft).has_value());
  const std::vector<std::uint8_t> registered = server.Receive(18);
  REQUIRE(registered.size() == 18);
  CHECK(registered[0] == 0x02);
  CHECK(registered[2] == kLevel);
  CHECK(registered[4] == 0x01);
  CHECK(registered[5] == 0x01);
  CHECK(registered[6] == kKnight);
  // Une seconde publication attend l'accusé de la première.
  CHECK_FALSE(ro::net::RequestPartyBookingRegister(link, booking, draft).has_value());

  REQUIRE(ApplyPartyBookingFrame(FrameOf(ro::proto::EncodePartyBookingRegisterAck({.result = 0})),
                                 booking, true) == FrameOutcome::kApplied);
  REQUIRE(booking.mine().has_value());
  REQUIRE(ro::net::RequestPartyBookingUpdate(link, booking, {12, -1, -1, -1, -1, -1}).has_value());
  const std::vector<std::uint8_t> update = server.Receive(14);
  REQUIRE(update.size() == 14);
  CHECK(update[0] == 0x08);
  CHECK(update[2] == 12);

  REQUIRE(ro::net::RequestPartyBookingDelete(link, booking).has_value());
  const std::vector<std::uint8_t> removal = server.Receive(2);
  REQUIRE(removal.size() == 2);
  CHECK(removal[0] == 0x06);
  CHECK(removal[1] == 0x08);
}
