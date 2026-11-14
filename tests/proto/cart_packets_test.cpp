#include "ro/proto/cart_packets.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::proto::ChangeCartRequest;
using ro::proto::DecodeSelectCartOffer;
using ro::proto::Encode;
using ro::proto::EncodeSelectCartOffer;
using ro::proto::SelectCartOffer;
using ro::proto::SelectCartRequest;

TEST_CASE("Charrette : le changement d'apparence part sur quatre octets", "[proto][cart]") {
  const auto frame = Encode(ChangeCartRequest{7});
  REQUIRE(frame.has_value());
  CHECK(*frame == std::vector<std::uint8_t>{0xAF, 0x01, 0x07, 0x00});
}

TEST_CASE("Charrette : le choix dans la liste du serveur rappelle son identité", "[proto][cart]") {
  const auto frame = Encode(SelectCartRequest{0x01020304, 11});
  REQUIRE(frame.has_value());
  CHECK(*frame == std::vector<std::uint8_t>{0x80, 0x09, 0x04, 0x03, 0x02, 0x01, 0x0B});
}

TEST_CASE("Charrette : la liste du serveur se lit jusqu'au bout de sa longueur", "[proto][cart]") {
  SelectCartOffer sent;
  sent.identity = 2'000'001;
  sent.cart_types = {10, 11, 12};
  const auto read = DecodeSelectCartOffer(EncodeSelectCartOffer(sent));
  REQUIRE(read.has_value());
  CHECK(read->identity == sent.identity);
  CHECK(read->cart_types == sent.cart_types);

  SECTION("une longueur qui ment est refusée") {
    std::vector<std::uint8_t> frame = EncodeSelectCartOffer(sent);
    frame.pop_back();
    CHECK_FALSE(DecodeSelectCartOffer(frame).has_value());
  }
  SECTION("une liste vide se lit vide") {
    const auto empty = DecodeSelectCartOffer(EncodeSelectCartOffer(SelectCartOffer{7, {}}));
    REQUIRE(empty.has_value());
    CHECK(empty->cart_types.empty());
  }
}
