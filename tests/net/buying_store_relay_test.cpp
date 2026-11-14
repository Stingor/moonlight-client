// LA VENTE À UN ACHETEUR, VUE DU MONDE : le paquet qui la confirme retire
// l'objet du sac, dans la chaîne ordinaire des trames.
//
// 🔴 LE SERVEUR N'ENVOIE PAS LE RETRAIT ORDINAIRE pour cette vente : sans ce
// maillon, le sac garderait ce qu'on vient de vendre, et une seconde vente de
// la même pile partirait vers un refus.

#include <cstdint>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_update.h"
#include "ro/sim/inventory.h"
#include "ro/sim/world.h"

#include "support/wire_bytes.h"

using ro::test_support::PutU16;
using ro::test_support::PutU32;

namespace {

// « Tu as vendu » : case du sac, quantité, prix unitaire — dix octets.
ro::net::Frame SoldFrame(std::uint16_t client_slot, std::uint16_t amount, std::uint32_t price) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, 0x081C);
  PutU16(bytes, client_slot);
  PutU16(bytes, amount);
  PutU32(bytes, price);
  return ro::net::Frame{.opcode = 0x081C, .bytes = std::move(bytes)};
}

}  // namespace

TEST_CASE("Échoppe d'achat : la vente confirmée retire la pile du sac, par le monde",
          "[buying_store]") {
  ro::sim::World world;
  ro::sim::InventoryItem jellopy;
  jellopy.client_slot = 4;
  jellopy.item_id = 909;
  jellopy.amount = 5;
  world.inventory().Put(jellopy);

  CHECK(ro::net::ApplyFrame(SoldFrame(4, 2, 70), world, 0) == ro::net::FrameOutcome::kApplied);
  const ro::sim::InventoryItem* left = world.inventory().Find(4);
  REQUIRE(left != nullptr);
  CHECK(left->amount == 3);

  CHECK(ro::net::ApplyFrame(SoldFrame(4, 3, 70), world, 0) == ro::net::FrameOutcome::kApplied);
  CHECK(world.inventory().Find(4) == nullptr);
}
