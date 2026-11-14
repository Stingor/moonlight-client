// LA RECHERCHE D'ÉCHOPPES : les trois demandes écrites à la main, octet par
// octet.
//
// 🔴 LE PRIX MAXIMAL PRÉCÈDE LE MINIMAL, et les cartes suivent les objets dans
// une seule queue : l'essai fige cet ordre, que le serveur lit par décalages.

#include "ro/proto/store_search_packets.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::proto::Encode;
using ro::proto::StoreSearchCloseRequest;
using ro::proto::StoreSearchNextPageRequest;
using ro::proto::StoreSearchRequest;

TEST_CASE("Recherche d'échoppes : la demande porte sorte, prix, comptes et identifiants",
          "[proto][store_search]") {
  StoreSearchRequest request;
  request.search_type = 1;
  request.max_price_zeny = 0x01020304;
  request.min_price_zeny = 0x0A0B0C0D;
  request.item_ids = {501, 0x00010203};
  request.card_ids = {4001};
  const auto frame = Encode(request);
  REQUIRE(frame.has_value());

  const std::vector<std::uint8_t> expected = {
      0x35, 0x08, 27,   0x00,                          // opcode, longueur
      0x01,                                            // échoppes d'achat
      0x04, 0x03, 0x02, 0x01,                          // prix maximal
      0x0D, 0x0C, 0x0B, 0x0A,                          // prix minimal
      0x02, 0x01,                                      // deux objets, une carte
      0xF5, 0x01, 0x00, 0x00, 0x03, 0x02, 0x01, 0x00,  // les objets
      0xA1, 0x0F, 0x00, 0x00,                          // la carte
  };
  CHECK(*frame == expected);
}

TEST_CASE("Recherche d'échoppes : plus de 255 identifiants ne tiennent pas dans leur compte",
          "[proto][store_search]") {
  StoreSearchRequest request;
  request.item_ids.assign(256, 501);
  CHECK_FALSE(Encode(request).has_value());
  request.item_ids.assign(255, 501);
  CHECK(Encode(request).has_value());
}

TEST_CASE("Recherche d'échoppes : page suivante et fermeture ne portent que leur opcode",
          "[proto][store_search]") {
  const auto next = Encode(StoreSearchNextPageRequest{});
  REQUIRE(next.has_value());
  CHECK(*next == std::vector<std::uint8_t>{0x38, 0x08});
  const auto close = Encode(StoreSearchCloseRequest{});
  REQUIRE(close.has_value());
  CHECK(*close == std::vector<std::uint8_t>{0x3B, 0x08});
}
