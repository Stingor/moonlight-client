// LA RECHERCHE D'ÉCHOPPES, décodée — et ses demandes, sur le fil.
//
// 🔴 CE BANC PORTE CE QU'AUCUNE SESSION NE MONTRERAIT VITE :
//
//   · une page se lit jusqu'au bout de sa longueur, objet complet compris ;
//   · les changements de carte ferment la recherche SANS être consommés ;
//   · le clic part avec le compte du marchand PUIS l'échoppe, et allume
//     l'enseigne dès l'envoi ;
//   · un verbe refuse sans rien envoyer hors d'une recherche ouverte.

#include "ro/net/store_search.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/store_search.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/wire_bytes.h"

using ro::net::ApplyStoreSearchFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::StoreSearch;
using ro::sim::StoreSearchEffect;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::ReadU32;

namespace {

constexpr std::uint32_t kOwnerAid = 2000005;
constexpr std::uint32_t kStoreId = 77;

Frame Framed(std::uint16_t opcode, std::vector<std::uint8_t> bytes) {
  return Frame{.opcode = opcode, .bytes = std::move(bytes)};
}

Frame OpeningFrame(std::uint16_t effect, std::uint8_t uses) {
  ro::proto::ZcOpenSearchStoreInfo packet;
  packet.effect = effect;
  packet.remainingUses = uses;
  return Framed(packet.kOpcode, ro::proto::Encode(packet));
}

Frame ResultsFrame(bool first_page, bool more, std::uint8_t uses, std::size_t count) {
  ro::proto::ZcSearchStoreInfoAck packet;
  packet.firstPage = first_page ? 1 : 0;
  packet.nextPage = more ? 1 : 0;
  packet.usesCount = uses;
  for (std::size_t rank = 0; rank < count; ++rank) {
    ro::proto::ZcSearchStoreInfoAckSub entry;
    entry.storeId = kStoreId;
    entry.AID = kOwnerAid;
    entry.shopName = "Pommes\rpas cher";
    entry.itemId = 1201;
    entry.itemType = 5;
    entry.price = 1500;
    entry.amount = 3;
    entry.slot.card = {4001, 0, 0, 0};
    entry.option_data[0] = {.index = 25, .value = 3, .param = 0};
    entry.refine = 7;
    entry.grade = 2;
    packet.items.push_back(entry);
  }
  return Framed(packet.kOpcode, ro::proto::Encode(packet));
}

}  // namespace

TEST_CASE("Recherche d'échoppes : ouverture, pages, refus et position se décodent",
          "[net][store_search]") {
  StoreSearch search;
  REQUIRE(ApplyStoreSearchFrame(OpeningFrame(1, 5), search) == FrameOutcome::kApplied);
  REQUIRE(search.open());
  CHECK(search.effect() == StoreSearchEffect::kOpenRemotely);
  CHECK(search.remaining_uses() == 5);

  REQUIRE(ApplyStoreSearchFrame(ResultsFrame(true, true, 4, 2), search) == FrameOutcome::kApplied);
  REQUIRE(search.results().size() == 2);
  const ro::sim::StoreSearchResult& result = search.results()[1];
  CHECK(result.store_id == kStoreId);
  CHECK(result.owner_aid == kOwnerAid);
  CHECK(result.shop_name == "Pommes\rpas cher");
  CHECK(result.price_zeny == 1500);
  CHECK(result.item.item_id == 1201);
  CHECK(result.item.type == 5);
  CHECK(result.item.amount == 3);
  CHECK(result.item.identified);
  CHECK(result.item.cards[0] == 4001);
  CHECK(result.item.options[0].id == 25);
  CHECK(result.item.options[0].value == 3);
  CHECK(result.item.refine_level == 7);
  CHECK(result.item.grade == 2);
  CHECK(search.more_on_server());
  CHECK(search.remaining_uses() == 4);

  ro::proto::ZcSearchStoreInfoFailed failed;
  failed.reason = ro::sim::kStoreSearchTooSoon;
  REQUIRE(ApplyStoreSearchFrame(Framed(failed.kOpcode, ro::proto::Encode(failed)), search) ==
          FrameOutcome::kApplied);
  CHECK(search.failure() == ro::sim::kStoreSearchTooSoon);

  ro::proto::ZcSsilistItemClickAck position;
  position.x = 150;
  position.y = 180;
  REQUIRE(ApplyStoreSearchFrame(Framed(position.kOpcode, ro::proto::Encode(position)), search) ==
          FrameOutcome::kApplied);
  REQUIRE(search.marker().has_value());
  CHECK(search.marker()->y == 180);

  SECTION("une page coupée est indéchiffrable") {
    Frame cut = ResultsFrame(false, false, 4, 1);
    cut.bytes.pop_back();
    CHECK(ApplyStoreSearchFrame(cut, search) == FrameOutcome::kUndecodable);
  }
  SECTION("une position reçue après la fermeture ne marque rien") {
    search.Close();
    CHECK(ApplyStoreSearchFrame(Framed(position.kOpcode, ro::proto::Encode(position)), search) ==
          FrameOutcome::kApplied);
    CHECK_FALSE(search.marker().has_value());
  }
  SECTION("le changement de carte ferme la recherche sans être consommé") {
    const Frame map_move{.opcode = ro::proto::ZcNpcackMapmove::kOpcode, .bytes = {}};
    CHECK(ApplyStoreSearchFrame(map_move, search) == FrameOutcome::kUnhandled);
    CHECK_FALSE(search.open());
    CHECK_FALSE(search.marker().has_value());
  }
}

TEST_CASE("Recherche d'échoppes : recherche, page, clic et fermeture, sur le fil",
          "[net][store_search]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);

  StoreSearch search;
  ro::sim::StoreSearchPlan plan;
  plan.item_ids = {501};
  // Fermée, rien ne part.
  CHECK_FALSE(ro::net::RequestStoreSearch(link, search, plan).has_value());
  CHECK_FALSE(ro::net::RequestStoreSearchClose(link, search).has_value());

  REQUIRE(ApplyStoreSearchFrame(OpeningFrame(0, 3), search) == FrameOutcome::kApplied);
  // Un plan fautif ne part pas.
  ro::sim::StoreSearchPlan faulty;
  faulty.fault = ro::sim::StoreSearchFault::kUnknownItem;
  CHECK_FALSE(ro::net::RequestStoreSearch(link, search, faulty).has_value());
  // Sans page annoncée, la suivante ne se demande pas.
  CHECK_FALSE(ro::net::RequestStoreSearchNextPage(link, search).has_value());

  REQUIRE(ro::net::RequestStoreSearch(link, search, plan).has_value());
  const std::vector<std::uint8_t> sent = server.Receive(19);
  REQUIRE(sent.size() == 19);
  CHECK(sent[0] == 0x35);
  CHECK(sent[1] == 0x08);
  CHECK(sent[13] == 1);
  CHECK(ReadU32(sent, 15) == 501);

  REQUIRE(ApplyStoreSearchFrame(ResultsFrame(true, false, 2, 1), search) == FrameOutcome::kApplied);
  REQUIRE(ro::net::RequestStoreSearchClick(link, search, search.results()[0]).has_value());
  CHECK(search.spotlit_owner_aid() == kOwnerAid);
  const std::vector<std::uint8_t> click = server.Receive(14);
  REQUIRE(click.size() == 14);
  CHECK(click[0] == 0x3C);
  CHECK(ReadU32(click, 2) == kOwnerAid);
  CHECK(ReadU32(click, 6) == kStoreId);
  CHECK(ReadU32(click, 10) == 1201);

  REQUIRE(ro::net::RequestStoreSearchClose(link, search).has_value());
  CHECK_FALSE(search.open());
  const std::vector<std::uint8_t> close = server.Receive(2);
  REQUIRE(close.size() == 2);
  CHECK(close[0] == 0x3B);
  CHECK(close[1] == 0x08);
}
