// LE FORGERON D'UN OBJET : l'identifiant de personnage que portent ses deux
// dernières cases, et rien pour un objet qui n'est pas forgé.

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/inventory.h"

using ro::sim::InventoryItem;
using ro::sim::ItemCreatorId;

TEST_CASE("Forgeron : mot bas puis mot haut", "[sim][description]") {
  constexpr std::uint32_t kForgeMark = 0xFF;
  constexpr std::uint32_t kSmith = 150001;
  InventoryItem forged;
  forged.item_id = 1201;
  forged.cards = {kForgeMark, 0, kSmith & 0xFFFFU, kSmith >> 16U};
  CHECK(ItemCreatorId(forged) == kSmith);

  // Des cartes, pas un forgeron.
  InventoryItem carded;
  carded.cards = {4001, 0, 4002, 4003};
  CHECK(ItemCreatorId(carded) == 0);
}
