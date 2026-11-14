// LE MARCHÉ, sur le fil : la demande telle qu'elle part, la réponse telle
// qu'elle se décode, et le modèle qui la remplit trame à trame.

#include "ro/net/market.h"

#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/market.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/wire_bytes.h"

using ro::net::ApplyMarketFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::Market;
using ro::sim::MarketKind;
using ro::sim::MarketQuery;
using ro::sim::MarketStatus;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::ReadU16;
using ro::test_support::ReadU32;

namespace {

constexpr std::uint32_t kOwnerAid = 2000005;
constexpr std::uint32_t kStoreId = 77;
constexpr std::uint32_t kSwordId = 1201;
constexpr std::uint32_t kCardId = 4001;
// L'en-tête de la demande : opcode, longueur, puis 23 octets de champs.
constexpr std::size_t kRequestHead = 27;
constexpr std::size_t kIdBytes = 4;

Frame Framed(std::uint16_t opcode, std::vector<std::uint8_t> bytes) {
  return Frame{.opcode = opcode, .bytes = std::move(bytes)};
}

ro::proto::MoonlightMarketEntry Entry(std::uint32_t price) {
  ro::proto::MoonlightMarketEntry entry;
  entry.store_id = kStoreId;
  entry.owner_aid = kOwnerAid;
  entry.itemId = kSwordId;
  entry.amount = 3;
  entry.price = price;
  entry.refine = 7;
  entry.grade = 2;
  entry.identified = 1;
  entry.autotrade = 1;
  entry.cards = {kCardId, 0, 0, 0};
  entry.options[0] = {.index = 25, .value = 3, .param = 0};
  entry.map = "prontera";
  entry.x = 150;
  entry.y = 120;
  entry.title = "Pommes\rpas cher";
  return entry;
}

Frame ReplyFrame(std::uint16_t query_id, MarketStatus status, std::uint16_t part,
                 std::uint16_t part_count, std::vector<std::uint32_t> prices,
                 std::uint32_t store_id = 0) {
  ro::proto::ZcMoonlightMarket packet;
  packet.query_id = query_id;
  packet.status = static_cast<std::uint8_t>(status);
  packet.kind = 0;
  packet.part = part;
  packet.part_count = part_count;
  packet.total = static_cast<std::uint16_t>(prices.size());
  packet.store_id = store_id;
  packet.owner_aid = store_id != 0 ? kOwnerAid : 0;
  for (const std::uint32_t price : prices) {
    packet.entries.push_back(Entry(price));
  }
  return Framed(packet.kOpcode, ro::proto::Encode(packet));
}

}  // namespace

TEST_CASE("Marché : la demande porte ses filtres puis les identifiants", "[net][market]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  Market market;
  MarketQuery query;
  query.kind = MarketKind::kBuying;
  query.category = ro::sim::MarketCategory::kWeapon;
  query.refine_min = 4;
  query.refine_max = 9;
  query.flags = ro::sim::kMarketWithCards;
  query.price_min_zeny = 1000;
  query.price_max_zeny = 50000;
  query.item_ids = {kSwordId, kCardId};
  REQUIRE(ro::net::RequestMarket(link, market, query));

  const std::size_t length = kRequestHead + 2 * kIdBytes;
  const std::vector<std::uint8_t> sent = server.Receive(length);
  REQUIRE(sent.size() == length);
  CHECK(ReadU16(sent, 0) == 0x0F41);
  CHECK(ReadU16(sent, 2) == length);
  // Le numéro de la demande n'est jamais nul : le serveur le réserve au catalogue.
  CHECK(ReadU16(sent, 4) != 0);
  CHECK(sent[6] == 1);  // achats
  CHECK(sent[7] == 2);  // armes
  CHECK(sent[8] == 4);
  CHECK(sent[9] == 9);
  CHECK(sent[10] == ro::sim::kMarketWithCards);
  CHECK(ReadU32(sent, 11) == 1000);
  CHECK(ReadU32(sent, 15) == 50000);
  CHECK(ReadU32(sent, 19) == 0);  // toutes les échoppes
  CHECK(ReadU32(sent, 23) == 0);
  CHECK(ReadU32(sent, kRequestHead) == kSwordId);
  CHECK(ReadU32(sent, kRequestHead + kIdBytes) == kCardId);
  // Le modèle attend la réponse dès l'envoi.
  CHECK(market.search().waiting());
  CHECK(market.search().query_id() == ReadU16(sent, 4));
}

TEST_CASE("Marché : comparer part avec son drapeau et le marchand, sans queue", "[net][market]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  Market market;
  REQUIRE(ro::net::RequestMarket(link, market,
                                 ro::sim::MarketCompareQuery(MarketKind::kVending, kOwnerAid)));
  const std::vector<std::uint8_t> sent = server.Receive(kRequestHead);
  REQUIRE(sent.size() == kRequestHead);
  CHECK(ReadU16(sent, 2) == kRequestHead);
  CHECK(sent[10] == ro::sim::kMarketCompare);
  CHECK(ReadU32(sent, 19) == 0);
  CHECK(ReadU32(sent, 23) == kOwnerAid);
  // C'est une recherche : la réponse remplit la liste du Marché.
  CHECK(market.search().waiting());
  CHECK(market.search().owner_aid() == kOwnerAid);
}

TEST_CASE("Marché : trop d'identifiants, rien ne part", "[net][market]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  Market market;
  MarketQuery query;
  query.item_ids.assign(ro::sim::kMarketMostIds + 1, kSwordId);
  CHECK_FALSE(ro::net::RequestMarket(link, market, query));
  CHECK_FALSE(market.search().waiting());
}

TEST_CASE("Marché : une réponse en deux trames ne se montre qu'entière", "[net][market]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  Market market;
  REQUIRE(ro::net::RequestMarket(link, market, MarketQuery{}));
  const std::uint16_t query_id = market.search().query_id();

  CHECK(ApplyMarketFrame(ReplyFrame(query_id, MarketStatus::kOk, 0, 2, {300, 100}), market) ==
        FrameOutcome::kApplied);
  CHECK(market.search().waiting());
  CHECK(market.search().offers().empty());
  CHECK(ApplyMarketFrame(ReplyFrame(query_id, MarketStatus::kOk, 1, 2, {200}), market) ==
        FrameOutcome::kApplied);
  REQUIRE(market.search().complete());
  REQUIRE(market.search().offers().size() == 3);

  const ro::sim::MarketOffer& offer = market.search().offers()[0];
  CHECK(offer.store_id == kStoreId);
  CHECK(offer.owner_aid == kOwnerAid);
  CHECK(offer.price_zeny == 300);
  CHECK(offer.autotrade);
  CHECK(offer.item.item_id == kSwordId);
  CHECK(offer.item.amount == 3);
  CHECK(offer.item.refine_level == 7);
  CHECK(offer.item.grade == 2);
  CHECK(offer.item.cards[0] == kCardId);
  CHECK(offer.item.options[0].id == 25);
  CHECK(offer.item.options[0].value == 3);
  CHECK(offer.map_name == "prontera");
  CHECK(offer.cell_x == 150);
  CHECK(offer.cell_y == 120);
  CHECK(offer.shop_name == "Pommes\rpas cher");
}

TEST_CASE("Marché : une trame d'une demande périmée ne compte pas", "[net][market]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  Market market;
  REQUIRE(ro::net::RequestMarket(link, market, MarketQuery{}));
  const std::uint16_t first = market.search().query_id();
  REQUIRE(ro::net::RequestMarket(link, market, MarketQuery{}));
  CHECK(market.search().query_id() != first);
  CHECK(ApplyMarketFrame(ReplyFrame(first, MarketStatus::kOk, 0, 1, {100}), market) ==
        FrameOutcome::kApplied);
  CHECK(market.search().waiting());
  CHECK_FALSE(market.search().complete());
}

TEST_CASE("Marché : le contenu d'une échoppe va à part de la recherche", "[net][market]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  Market market;
  REQUIRE(ro::net::RequestMarket(
      link, market, ro::sim::MarketStoreQuery(MarketKind::kVending, kStoreId, kOwnerAid)));
  const std::vector<std::uint8_t> sent = server.Receive(kRequestHead);
  REQUIRE(sent.size() == kRequestHead);
  CHECK(ReadU32(sent, 19) == kStoreId);
  CHECK(ReadU32(sent, 23) == kOwnerAid);
  CHECK(market.store().waiting());
  CHECK_FALSE(market.search().waiting());
  CHECK(ApplyMarketFrame(
            ReplyFrame(market.store().query_id(), MarketStatus::kOk, 0, 1, {100, 200}, kStoreId),
            market) == FrameOutcome::kApplied);
  CHECK(market.store().offers().size() == 2);
  CHECK(market.search().offers().empty());
}

TEST_CASE("Marché : les refus, et l'ouverture par le serveur", "[net][market]") {
  constexpr std::uint8_t kRetiredNotInTown = 2;
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  Market market;
  // LE CATALOGUE : aucune demande, le numéro zéro.
  CHECK(ApplyMarketFrame(ReplyFrame(0, MarketStatus::kOpen, 0, 1, {}), market) ==
        FrameOutcome::kApplied);
  CHECK(market.open_seq() == 1);
  CHECK(market.refusal_seq() == 0);
  // LE STATUT 2 EST RÉSERVÉ : il ne se décode pas et ne dit aucun refus.
  ro::proto::ZcMoonlightMarket retired;
  retired.status = kRetiredNotInTown;
  retired.part_count = 1;
  CHECK(ApplyMarketFrame(Framed(retired.kOpcode, ro::proto::Encode(retired)), market) ==
        FrameOutcome::kUndecodable);
  CHECK(market.refusal_seq() == 0);
  // UNE DEMANDE REFUSÉE n'est plus attendue.
  REQUIRE(ro::net::RequestMarket(link, market, MarketQuery{}));
  CHECK(ApplyMarketFrame(ReplyFrame(market.search().query_id(), MarketStatus::kTooSoon, 0, 1, {}),
                         market) == FrameOutcome::kApplied);
  CHECK(market.refusal() == MarketStatus::kTooSoon);
  CHECK_FALSE(market.refusal_for_store());
  CHECK_FALSE(market.search().waiting());
  // UN STATUT INCONNU ne se décode pas.
  ro::proto::ZcMoonlightMarket strange;
  strange.status = 99;
  CHECK(ApplyMarketFrame(Framed(strange.kOpcode, ro::proto::Encode(strange)), market) ==
        FrameOutcome::kUndecodable);
  // UN AUTRE OPCODE n'est pas à lui.
  CHECK(ApplyMarketFrame(Framed(0x0F40, {}), market) == FrameOutcome::kUnhandled);
}

TEST_CASE("Marché : l'ouverture porte sa saisie, et l'octet réservé ne se lit pas",
          "[net][market]") {
  constexpr std::uint8_t kStrayReserved = 10;
  Market market;
  ro::proto::ZcMoonlightMarket open;
  open.status = static_cast<std::uint8_t>(MarketStatus::kOpen);
  open.part_count = 1;
  // Quelle que soit sa valeur, la saisie qui le suit reste en place.
  open.reserved = kStrayReserved;
  open.query = "4001";
  CHECK(ApplyMarketFrame(Framed(open.kOpcode, ro::proto::Encode(open)), market) ==
        FrameOutcome::kApplied);
  CHECK(market.open_seq() == 1);
  CHECK(market.open_query() == "4001");
}

TEST_CASE("Marché : une offre de PNJ porte sa monnaie, et une monnaie inconnue s'écarte",
          "[net][market]") {
  constexpr std::uint32_t kNpcBlock = 110000123;
  constexpr std::uint32_t kJellopy = 909;
  ro::proto::ZcMoonlightMarket packet;
  packet.status = static_cast<std::uint8_t>(MarketStatus::kOk);
  packet.kind = static_cast<std::uint8_t>(MarketKind::kNpc);
  packet.part_count = 1;
  ro::proto::MoonlightMarketEntry itemshop = Entry(3);
  itemshop.store_id = kNpcBlock;
  itemshop.owner_aid = 0;
  itemshop.amount = 0;
  itemshop.title = "Marchande";
  itemshop.currency = static_cast<std::uint8_t>(ro::sim::MarketCurrency::kItem);
  itemshop.currency_item = kJellopy;
  ro::proto::MoonlightMarketEntry points = Entry(50);
  points.currency = static_cast<std::uint8_t>(ro::sim::MarketCurrency::kPoints);
  points.currency_name = "#KAFRAPOINTS";
  ro::proto::MoonlightMarketEntry strange = Entry(1);
  strange.currency = 99;
  packet.entries = {itemshop, points, strange};
  const std::optional<ro::sim::MarketReplyPart> part =
      ro::net::DecodeMarketReply(ro::proto::Encode(packet));
  REQUIRE(part.has_value());
  CHECK(part->kind == MarketKind::kNpc);
  REQUIRE(part->offers.size() == 2);
  CHECK(part->offers[0].currency == ro::sim::MarketCurrency::kItem);
  CHECK(part->offers[0].currency_item == kJellopy);
  CHECK(part->offers[0].store_id == kNpcBlock);
  CHECK(part->offers[0].shop_name == "Marchande");
  CHECK(part->offers[0].item.amount == 0);
  CHECK(part->offers[1].currency == ro::sim::MarketCurrency::kPoints);
  CHECK(part->offers[1].currency_name == "#KAFRAPOINTS");
}
