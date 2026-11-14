// L'ÉCHOPPE DE VENTE : les trois demandes écrites à la main, octet par octet.
//
// 🔴 L'ABANDON EST L'OUVERTURE avec l'octet à zéro, un titre vide et aucune
// ligne : l'essai fige cette forme, puisque c'est elle qui garantit que le
// serveur s'arrête au titre vide après avoir levé la préparation.

#include "ro/proto/vending_packets.h"

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::proto::Encode;
using ro::proto::VendingCloseRequest;
using ro::proto::VendingListRequest;
using ro::proto::VendingOpenLine;
using ro::proto::VendingOpenRequest;

TEST_CASE("Échoppe : l'ouverture porte titre, octet d'ouverture et lignes", "[proto][vending]") {
  VendingOpenRequest request;
  request.title_wire = "Ab";
  request.open = true;
  request.lines = {VendingOpenLine{.cart_slot = 5, .amount = 3, .price_zeny = 0x01020304}};
  const auto frame = Encode(request);
  REQUIRE(frame.has_value());

  std::vector<std::uint8_t> expected = {0xB2, 0x01, 93, 0x00, 'A', 'b'};
  expected.resize(4 + VendingOpenRequest::kTitleBytes, 0);
  expected.push_back(0x01);
  expected.insert(expected.end(), {0x05, 0x00, 0x03, 0x00, 0x04, 0x03, 0x02, 0x01});
  CHECK(*frame == expected);
}

TEST_CASE("Échoppe : l'abandon part sans titre ni ligne, octet à zéro", "[proto][vending]") {
  VendingOpenRequest request;
  request.open = false;
  const auto frame = Encode(request);
  REQUIRE(frame.has_value());
  REQUIRE(frame->size() == VendingOpenRequest::kHeaderSize);
  CHECK((*frame)[0] == 0xB2);
  CHECK((*frame)[1] == 0x01);
  CHECK((*frame)[2] == VendingOpenRequest::kHeaderSize);
  CHECK((*frame)[3] == 0x00);
  CHECK((*frame)[4] == 0x00);
  CHECK((*frame)[VendingOpenRequest::kHeaderSize - 1] == VendingOpenRequest::kCancel);
}

TEST_CASE("Échoppe : un titre sans place pour son NUL est refusé", "[proto][vending]") {
  VendingOpenRequest request;
  request.title_wire = std::string(VendingOpenRequest::kTitleBytes - 1, 'x');
  const auto fits = Encode(request);
  REQUIRE(fits.has_value());
  CHECK((*fits)[4 + VendingOpenRequest::kTitleBytes - 1] == 0);

  request.title_wire.push_back('y');
  CHECK_FALSE(Encode(request).has_value());
}

TEST_CASE("Échoppe : fermeture et demande de liste", "[proto][vending]") {
  const auto close = Encode(VendingCloseRequest{});
  REQUIRE(close.has_value());
  CHECK(*close == std::vector<std::uint8_t>{0x2E, 0x01});

  const auto list = Encode(VendingListRequest{.vendor_aid = 0x0A0B0C0D});
  REQUIRE(list.has_value());
  CHECK(*list == std::vector<std::uint8_t>{0x30, 0x01, 0x0D, 0x0C, 0x0B, 0x0A});
}
