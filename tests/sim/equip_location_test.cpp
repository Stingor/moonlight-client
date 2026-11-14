#include "ro/sim/equip_location.h"

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/inventory.h"

using ro::sim::Inventory;
using ro::sim::InventoryItem;

namespace {

// Une pièce portée, réduite à ce que la recherche par emplacement regarde.
[[nodiscard]] InventoryItem Worn(std::uint16_t slot, std::uint32_t location) {
  return InventoryItem{
      .client_slot = slot, .item_id = 1000U + slot, .amount = 1, .worn_at = location};
}

}  // namespace

TEST_CASE("Emplacements : les deux accessoires ne se confondent pas", "[sim][fiche]") {
  // 🔴 LE PIÈGE DE TOUT LE MANNEQUIN. Le serveur appelle `0x08` la droite et
  // `0x80` la gauche ; l'écran du client dit l'inverse sur les mêmes bits. Les
  // deux valeurs sont si proches qu'un échange ne se verrait que sur un
  // personnage qui porte DEUX accessoires différents — c'est-à-dire tard.
  Inventory bag;
  bag.Put(Worn(1, ro::sim::kEquipAccessoryRight));
  bag.Put(Worn(2, ro::sim::kEquipAccessoryLeft));

  const InventoryItem* right = bag.WornAt(ro::sim::kEquipAccessoryRight);
  const InventoryItem* left = bag.WornAt(ro::sim::kEquipAccessoryLeft);
  REQUIRE(right != nullptr);
  REQUIRE(left != nullptr);
  CHECK(right->client_slot == 1);
  CHECK(left->client_slot == 2);

  // Et les valeurs elles-mêmes, telles que `enum equip_pos` les pose : c'est
  // cette ligne qui tient la table contre une relecture qui voudrait « réparer »
  // le désaccord avec l'écran.
  CHECK(ro::sim::kEquipAccessoryRight == 0x000008);
  CHECK(ro::sim::kEquipAccessoryLeft == 0x000080);
}

TEST_CASE("Emplacements : un costume ne répond pas au bit de la coiffe", "[sim][fiche]") {
  // Une même tête porte les deux, et ce sont deux cases distinctes. Si le
  // costume répondait au bit ordinaire, l'onglet Équipement montrerait la pièce
  // d'apparat à la place du vrai chapeau.
  Inventory bag;
  bag.Put(Worn(1, ro::sim::kEquipHeadTop));
  bag.Put(Worn(2, ro::sim::kEquipCostumeHeadTop));

  const InventoryItem* hat = bag.WornAt(ro::sim::kEquipHeadTop);
  const InventoryItem* costume = bag.WornAt(ro::sim::kEquipCostumeHeadTop);
  REQUIRE(hat != nullptr);
  REQUIRE(costume != nullptr);
  CHECK(hat->client_slot == 1);
  CHECK(costume->client_slot == 2);

  CHECK(ro::sim::IsCostumeLocation(ro::sim::kEquipCostumeGarment));
  CHECK_FALSE(ro::sim::IsCostumeLocation(ro::sim::kEquipGarment));
}

TEST_CASE("Emplacements : une pièce d'ombre se reconnaît pour être écartée", "[sim][fiche]") {
  // Le client d'origine les fait tomber sur les places des costumes, où elles en
  // couvrent un sans rien dire. On ne les montre pas — encore faut-il savoir les
  // nommer, sinon on les y remettra.
  constexpr std::uint32_t kShadowArmor = 0x010000;
  constexpr std::uint32_t kShadowAccessoryLeft = 0x200000;

  CHECK(ro::sim::IsShadowLocation(kShadowArmor));
  CHECK(ro::sim::IsShadowLocation(kShadowAccessoryLeft));
  CHECK(ro::sim::IsShadowLocation(0x004000));
  CHECK_FALSE(ro::sim::IsShadowLocation(ro::sim::kEquipArmor));
  CHECK_FALSE(ro::sim::IsShadowLocation(ro::sim::kEquipCostumeGarment));
}

TEST_CASE("Emplacements : une arme à deux mains occupe AUSSI le bouclier", "[sim][fiche]") {
  // 🔴 C'est ce que le serveur envoie — `EQP_ARMS` — et c'est ce qui ferait
  // dessiner la même arme deux fois. La case du bouclier se vide par
  // `IsTwoHanded`, à l'endroit qui dessine.
  Inventory bag;
  bag.Put(Worn(1, ro::sim::kEquipBothHands));

  const InventoryItem* shield = bag.WornAt(ro::sim::kEquipShield);
  REQUIRE(shield != nullptr);
  CHECK(shield->client_slot == 1);
  CHECK(ro::sim::IsTwoHanded(shield->worn_at));

  // Un vrai port à deux armes : deux pièces, chacune d'un seul côté, et aucune
  // n'est à deux mains.
  Inventory dual;
  dual.Put(Worn(1, ro::sim::kEquipWeapon));
  dual.Put(Worn(2, ro::sim::kEquipShield));
  const InventoryItem* main_hand = dual.WornAt(ro::sim::kEquipWeapon);
  const InventoryItem* off_hand = dual.WornAt(ro::sim::kEquipShield);
  REQUIRE(main_hand != nullptr);
  REQUIRE(off_hand != nullptr);
  CHECK(main_hand->client_slot == 1);
  CHECK(off_hand->client_slot == 2);
  CHECK_FALSE(ro::sim::IsTwoHanded(main_hand->worn_at));
  CHECK_FALSE(ro::sim::IsTwoHanded(off_hand->worn_at));
}

TEST_CASE("Emplacements : il faut TOUS les bits demandés, pas un seul", "[sim][fiche]") {
  // Une épée d'une main ne doit pas répondre à « les deux mains » : sinon le
  // premier objet d'un port à deux armes passerait pour une claymore.
  Inventory bag;
  bag.Put(Worn(1, ro::sim::kEquipWeapon));
  CHECK(bag.WornAt(ro::sim::kEquipBothHands) == nullptr);
  CHECK(bag.WornAt(ro::sim::kEquipWeapon) != nullptr);
}

TEST_CASE("Emplacements : une case libre ne rend rien, et zéro non plus", "[sim][fiche]") {
  Inventory bag;
  bag.Put(Worn(1, ro::sim::kEquipArmor));
  CHECK(bag.WornAt(ro::sim::kEquipShoes) == nullptr);

  // Un objet qui n'est pas porté a `worn_at` nul ; demander zéro ne doit pas le
  // rendre, sans quoi toute case vide montrerait le premier objet du sac.
  bag.Put(InventoryItem{.client_slot = 2, .item_id = 501, .amount = 3});
  CHECK(bag.WornAt(0) == nullptr);
}
