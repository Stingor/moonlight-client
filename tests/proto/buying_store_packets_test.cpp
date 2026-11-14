// LES PAQUETS DE L'ÉCHOPPE D'ACHAT QUE LE SERVEUR N'A PAS DÉCRITS.
//
// ⚠ LES DEMANDES SONT ÉCRITES À LA MAIN : leur encodeur recoupe la table du
// serveur, et ces essais vérifient la disposition octet par octet.

#include "ro/proto/buying_store_packets.h"

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/buying_store_checks.h"

#include "support/wire_bytes.h"

using ro::proto::BuyingStoreCloseRequest;
using ro::proto::BuyingStoreOpenLine;
using ro::proto::BuyingStoreOpenRequest;
using ro::proto::BuyingStoreTradeLine;
using ro::proto::BuyingStoreTradeRequest;
using ro::proto::BuyingStoreVisitRequest;
using ro::proto::Encode;
using ro::test_support::ReadU16;
using ro::test_support::ReadU32;

TEST_CASE("Échoppe d'achat : la préparation se lit sur trois octets", "[buying_store]") {
  const std::vector<std::uint8_t> frame = {0x10, 0x08, 0x05};
  const auto packet = ro::proto::DecodeBuyingStorePreparation(frame);
  REQUIRE(packet.has_value());
  CHECK(packet->slots == 5);

  CHECK_FALSE(ro::proto::DecodeBuyingStorePreparation(std::vector<std::uint8_t>{0x10, 0x08}));
  CHECK_FALSE(ro::proto::DecodeBuyingStorePreparation(std::vector<std::uint8_t>{0x11, 0x08, 0x05}));
}

TEST_CASE("Échoppe d'achat : le refus porte le code et le poids projeté", "[buying_store]") {
  const std::vector<std::uint8_t> frame = {0x12, 0x08, 0x02, 0x00, 0x10, 0x27, 0x00, 0x00};
  const auto packet = ro::proto::DecodeBuyingStoreOpenFailure(frame);
  REQUIRE(packet.has_value());
  CHECK(packet->result == 2);
  CHECK(packet->weight == 10'000);
}

TEST_CASE("Échoppe d'achat : la fin porte son code", "[buying_store]") {
  const std::vector<std::uint8_t> frame = {0x1A, 0x08, 0x04, 0x00};
  const auto packet = ro::proto::DecodeBuyingStoreEnd(frame);
  REQUIRE(packet.has_value());
  CHECK(packet->result == 4);
}

TEST_CASE("Échoppe d'achat : une ligne vendue porte la case, la quantité et le prix unitaire",
          "[buying_store]") {
  const std::vector<std::uint8_t> frame = {0x1C, 0x08, 0x07, 0x00, 0x03,
                                           0x00, 0xE8, 0x03, 0x00, 0x00};
  const auto packet = ro::proto::DecodeBuyingStoreItemSold(frame);
  REQUIRE(packet.has_value());
  CHECK(packet->client_slot == 7);
  CHECK(packet->amount == 3);
  CHECK(packet->unit_price == 1000);

  std::vector<std::uint8_t> longer = frame;
  longer.push_back(0);
  CHECK_FALSE(ro::proto::DecodeBuyingStoreItemSold(longer));
}

TEST_CASE("Échoppe d'achat : la demande d'ouverture suit la disposition du serveur",
          "[buying_store]") {
  BuyingStoreOpenRequest request;
  request.zeny_limit = 500'000;
  request.open = true;
  request.title_on_wire = "Achete Jellopy";
  request.lines = {BuyingStoreOpenLine{.item_id = 909, .amount = 100, .price = 50},
                   BuyingStoreOpenLine{.item_id = 501, .amount = 2, .price = 99'990'000}};
  const auto frame = Encode(request);
  REQUIRE(frame.has_value());
  const std::vector<std::uint8_t>& bytes = *frame;
  REQUIRE(bytes.size() == 89 + 2 * 10);
  CHECK(ReadU16(bytes, 0) == 0x0811);
  CHECK(ReadU16(bytes, 2) == 109);
  CHECK(ReadU32(bytes, 4) == 500'000);
  CHECK(bytes[8] == 1);
  CHECK(std::string(bytes.begin() + 9, bytes.begin() + 23) == "Achete Jellopy");
  CHECK(bytes[23] == 0);
  CHECK(bytes[88] == 0);
  CHECK(ReadU32(bytes, 89) == 909);
  CHECK(ReadU16(bytes, 93) == 100);
  CHECK(ReadU32(bytes, 95) == 50);
  CHECK(ReadU32(bytes, 99) == 501);
  CHECK(ReadU16(bytes, 103) == 2);
  CHECK(ReadU32(bytes, 105) == 99'990'000);
}

TEST_CASE("Échoppe d'achat : la renonciation part sans ligne, ouverture à zéro", "[buying_store]") {
  BuyingStoreOpenRequest request;
  request.open = false;
  const auto frame = Encode(request);
  REQUIRE(frame.has_value());
  REQUIRE(frame->size() == 89);
  CHECK(ReadU16(*frame, 2) == 89);
  CHECK((*frame)[8] == 0);
}

TEST_CASE("Échoppe d'achat : un titre qui ne tient pas dans 79 octets est refusé",
          "[buying_store]") {
  BuyingStoreOpenRequest request;
  request.title_on_wire = std::string(79, 'x');
  CHECK(Encode(request).has_value());
  request.title_on_wire = std::string(80, 'x');
  CHECK_FALSE(Encode(request).has_value());
}

TEST_CASE("Échoppe d'achat : fermeture et demande de liste", "[buying_store]") {
  const auto close = Encode(BuyingStoreCloseRequest{});
  REQUIRE(close.has_value());
  CHECK(*close == std::vector<std::uint8_t>{0x15, 0x08});

  const auto visit = Encode(BuyingStoreVisitRequest{.buyer_account_id = 2'000'001});
  REQUIRE(visit.has_value());
  CHECK(*visit == std::vector<std::uint8_t>{0x17, 0x08, 0x81, 0x84, 0x1E, 0x00});
}

TEST_CASE("Échoppe d'achat : la vente a des lignes de huit octets", "[buying_store]") {
  BuyingStoreTradeRequest request;
  request.buyer_account_id = 2'000'001;
  request.store_id = 42;
  request.lines = {BuyingStoreTradeLine{.client_slot = 7, .item_id = 909, .amount = 30}};
  const auto frame = Encode(request);
  REQUIRE(frame.has_value());
  const std::vector<std::uint8_t>& bytes = *frame;
  REQUIRE(bytes.size() == 20);
  CHECK(ReadU16(bytes, 0) == 0x0819);
  CHECK(ReadU16(bytes, 2) == 20);
  CHECK(ReadU32(bytes, 4) == 2'000'001);
  CHECK(ReadU32(bytes, 8) == 42);
  CHECK(ReadU16(bytes, 12) == 7);
  CHECK(ReadU32(bytes, 14) == 909);
  CHECK(ReadU16(bytes, 18) == 30);
}

TEST_CASE("Échoppe d'achat : une vente au-delà de 0x800 octets n'est pas émise", "[buying_store]") {
  BuyingStoreTradeRequest request;
  // Le compte du modèle doit concorder avec la disposition réelle.
  request.lines.resize(ro::sim::kBuyingStoreMostSaleLines);
  const auto fits = Encode(request);
  REQUIRE(fits.has_value());
  CHECK(fits->size() <= BuyingStoreTradeRequest::kMostFrameBytes);

  request.lines.resize(ro::sim::kBuyingStoreMostSaleLines + 1);
  CHECK_FALSE(Encode(request).has_value());
}
