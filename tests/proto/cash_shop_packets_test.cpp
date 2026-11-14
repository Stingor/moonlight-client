// LES PAQUETS DE LA BOUTIQUE SANS STRUCTURE CÔTÉ SERVEUR, et la carte des
// items à effet de chapeau.
//
// ⚠ LES DEUX DEMANDES NE PORTENT QUE LEUR OPCODE : c'est le gestionnaire du
// serveur qui les identifie, et c'est ce que leur encodeur recoupe.

#include "ro/proto/cash_shop_packets.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/hat_effect_packets.h"

using ro::proto::CashShopCatalogueRequest;
using ro::proto::CashShopCloseNotice;
using ro::proto::DecodeHatEffectMap;
using ro::proto::Encode;
using ro::proto::EncodeHatEffectMap;
using ro::proto::HatEffectMap;
using ro::proto::HatEffectMapEntry;

TEST_CASE("Boutique : la demande de catalogue part sur deux octets", "[proto][cash_shop]") {
  const auto frame = Encode(CashShopCatalogueRequest{});
  REQUIRE(frame.has_value());
  CHECK(*frame == std::vector<std::uint8_t>{0xC9, 0x08});
}

TEST_CASE("Boutique : la fermeture part sur deux octets", "[proto][cash_shop]") {
  const auto frame = Encode(CashShopCloseNotice{});
  REQUIRE(frame.has_value());
  CHECK(*frame == std::vector<std::uint8_t>{0x4A, 0x08});
}

TEST_CASE("Effets de chapeau : la carte se lit jusqu'au bout de sa longueur",
          "[proto][cash_shop]") {
  HatEffectMap sent;
  sent.entries = {HatEffectMapEntry{.item_id = 20500, .ordinal = 12},
                  HatEffectMapEntry{.item_id = 400001, .ordinal = 257}};
  const std::vector<std::uint8_t> frame = EncodeHatEffectMap(sent);
  CHECK(frame.size() == HatEffectMap::kHeaderSize + 2 * HatEffectMap::kEntrySize);
  const auto read = DecodeHatEffectMap(frame);
  REQUIRE(read.has_value());
  REQUIRE(read->entries.size() == 2);
  CHECK(read->entries[0].item_id == 20500);
  CHECK(read->entries[0].ordinal == 12);
  CHECK(read->entries[1].item_id == 400001);
  CHECK(read->entries[1].ordinal == 257);

  SECTION("une queue qui ne se découpe pas en entrées entières est refusée") {
    std::vector<std::uint8_t> cut = frame;
    cut.pop_back();
    cut[2] = static_cast<std::uint8_t>(cut.size());
    CHECK_FALSE(DecodeHatEffectMap(cut).has_value());
  }
  SECTION("une longueur qui ment est refusée") {
    std::vector<std::uint8_t> lying = frame;
    lying[2] = static_cast<std::uint8_t>(lying[2] + 1);
    CHECK_FALSE(DecodeHatEffectMap(lying).has_value());
  }
}
