// CE QU'UNE PIÈCE EST DEVENUE : le paquet que le générateur écarte, lu à la main.

#include "ro/proto/item_option_packets.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/packet_lengths.h"

using ro::proto::DecodeItemOptionChange;
using ro::proto::EncodeItemOptionChange;
using ro::proto::ItemOptionChange;

TEST_CASE("Changement d'une pièce : la taille est celle de la table des paquets",
          "[proto][laphine]") {
  CHECK(ro::proto::PacketLength(ItemOptionChange::kOpcode) ==
        static_cast<std::int16_t>(ItemOptionChange::kWireSize));
}

TEST_CASE("Changement d'une pièce : le raffinement vient APRÈS les options", "[proto][laphine]") {
  ItemOptionChange change;
  change.client_slot = 7;
  change.damaged = true;
  change.cards[3] = 4001;
  change.options[4] = {.index = 12, .value = -3, .param = 1};
  change.refine_level = 9;
  change.grade = 2;

  const std::vector<std::uint8_t> frame = EncodeItemOptionChange(change);
  REQUIRE(frame.size() == ItemOptionChange::kWireSize);
  CHECK(frame[0] == 0x43);
  CHECK(frame[1] == 0x0B);
  CHECK(frame[2] == 7);
  CHECK(frame[4] == 1);
  // L'avant-dernier octet porte le raffinement, le dernier le grade.
  CHECK(frame[ItemOptionChange::kWireSize - 2] == 9);
  CHECK(frame[ItemOptionChange::kWireSize - 1] == 2);

  const auto decoded = DecodeItemOptionChange(frame);
  REQUIRE(decoded.has_value());
  CHECK(decoded->client_slot == 7);
  CHECK(decoded->damaged);
  CHECK(decoded->cards[3] == 4001);
  CHECK(decoded->options[4].index == 12);
  CHECK(decoded->options[4].value == -3);
  CHECK(decoded->options[4].param == 1);
  CHECK(decoded->refine_level == 9);
  CHECK(decoded->grade == 2);
}

TEST_CASE("Changement d'une pièce : une autre taille se refuse", "[proto][laphine]") {
  std::vector<std::uint8_t> frame = EncodeItemOptionChange(ItemOptionChange{});
  frame.pop_back();
  CHECK_FALSE(DecodeItemOptionChange(frame).has_value());
}
