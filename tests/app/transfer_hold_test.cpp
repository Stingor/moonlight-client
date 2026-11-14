// LES TRANSFERTS GELÉS PENDANT LA PRÉPARATION D'UNE ÉCHOPPE : ce qui passe d'un
// contenant à l'autre est retenu, le reste passe.

#include "items/transfer_hold.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

using moonclient::HoldTransfers;
using ro::ui::CartVerb;
using ro::ui::ItemHolder;
using ro::ui::ItemVerb;
using ro::ui::StorageVerb;

namespace {

constexpr std::uint16_t kSlot = 7;

struct Asks {
  ro::ui::InventoryWindowAction inventory;
  ro::ui::CartWindowAction cart;
  ro::ui::StorageWindowAction storage;

  bool Hold() { return HoldTransfers(inventory, cart, storage); }
};

}  // namespace

TEST_CASE("Transferts gelés : rien à retenir sans demande", "[app][items]") {
  Asks asks;
  CHECK_FALSE(asks.Hold());
}

TEST_CASE("Transferts gelés : le sac garde ses objets, mais s'en sert", "[app][items]") {
  Asks asks;
  asks.inventory.verb = ItemVerb::kToCart;
  CHECK(asks.Hold());
  CHECK(asks.inventory.verb == ItemVerb::kNothing);

  asks.inventory.verb = ItemVerb::kToStorage;
  CHECK(asks.Hold());
  CHECK(asks.inventory.verb == ItemVerb::kNothing);

  asks.inventory.verb = ItemVerb::kUseOrWear;
  CHECK_FALSE(asks.Hold());
  CHECK(asks.inventory.verb == ItemVerb::kUseOrWear);

  asks.inventory.verb = ItemVerb::kDrop;
  CHECK_FALSE(asks.Hold());
  CHECK(asks.inventory.verb == ItemVerb::kDrop);
}

TEST_CASE("Transferts gelés : la charrette ne rend rien", "[app][items]") {
  Asks asks;
  asks.cart.verb = CartVerb::kToInventory;
  CHECK(asks.Hold());
  CHECK(asks.cart.verb == CartVerb::kNothing);

  asks.cart.verb = CartVerb::kToStorage;
  CHECK(asks.Hold());
  CHECK(asks.cart.verb == CartVerb::kNothing);
}

TEST_CASE("Transferts gelés : le coffre ne rend rien, mais se ferme", "[app][items]") {
  Asks asks;
  asks.storage.verb = StorageVerb::kToInventory;
  CHECK(asks.Hold());
  CHECK(asks.storage.verb == StorageVerb::kNothing);

  asks.storage.verb = StorageVerb::kToCart;
  CHECK(asks.Hold());
  CHECK(asks.storage.verb == StorageVerb::kNothing);

  asks.storage.verb = StorageVerb::kClose;
  CHECK_FALSE(asks.Hold());
  CHECK(asks.storage.verb == StorageVerb::kClose);
}

TEST_CASE("Transferts gelés : un glisser entre contenants est oublié", "[app][items]") {
  Asks asks;
  asks.cart.received = {
      .from = ItemHolder::kInventory, .to = ItemHolder::kCart, .client_slot = kSlot};
  asks.inventory.received = {
      .from = ItemHolder::kStorage, .to = ItemHolder::kInventory, .client_slot = kSlot};
  CHECK(asks.Hold());
  CHECK(asks.cart.received.client_slot == 0);
  CHECK(asks.inventory.received.client_slot == 0);
}

TEST_CASE("Glisser reçu : la première fenêtre qui parle, sac d'abord", "[app][items]") {
  constexpr std::uint16_t kOtherSlot = 9;
  Asks asks;
  CHECK(moonclient::ReceivedTransfer(asks.inventory, asks.cart, asks.storage).client_slot == 0);
  asks.storage.received = {
      .from = ItemHolder::kCart, .to = ItemHolder::kStorage, .client_slot = kOtherSlot};
  CHECK(moonclient::ReceivedTransfer(asks.inventory, asks.cart, asks.storage).client_slot ==
        kOtherSlot);
  asks.inventory.received = {
      .from = ItemHolder::kCart, .to = ItemHolder::kInventory, .client_slot = kSlot};
  CHECK(moonclient::ReceivedTransfer(asks.inventory, asks.cart, asks.storage).client_slot == kSlot);
}

TEST_CASE("Transferts gelés : un glisser venu du mannequin passe", "[app][items]") {
  Asks asks;
  asks.inventory.received = {
      .from = ItemHolder::kEquipment, .to = ItemHolder::kInventory, .client_slot = kSlot};
  CHECK_FALSE(asks.Hold());
  CHECK(asks.inventory.received.client_slot == kSlot);
}
