#include "character/equip_slot_map.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <set>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/equip_location.h"
#include "ro/sim/inventory.h"

using moonclient::EquipLocationOf;
using moonclient::ShortcutGestureOf;
using moonclient::ShortcutItemAct;
using moonclient::ShortcutItemGesture;
using moonclient::ShortcutTargetIn;
using moonclient::ShownInBag;
using moonclient::ShownOnDoll;
using moonclient::WearAimedAt;
using moonclient::WearAt;
using moonclient::WornPieceViews;
using moonclient::WornViews;
using ro::ui::EquipSlot;
using ro::ui::kEquipSlotCount;

namespace {

constexpr bool kOrdinary = false;
constexpr bool kCostume = true;

// Une pièce portée, dans sa case du sac, avec son apparence.
void Wear(ro::sim::Inventory& bag, std::uint16_t client_slot, std::uint32_t worn_at,
          std::uint16_t view_id) {
  bag.Put(ro::sim::InventoryItem{
      .client_slot = client_slot, .wearable_at = worn_at, .worn_at = worn_at, .view_id = view_id});
}

// Les trois coiffes de `WornViews`, dans son ordre : basse, milieu, haute.
constexpr std::size_t kLow = 0;
constexpr std::size_t kMid = 1;
constexpr std::size_t kTop = 2;

// Un anneau quelconque : seul compte qu'il soit le même d'un exemplaire à l'autre.
constexpr std::uint32_t kRingId = 2601;

}  // namespace

TEST_CASE("Mannequin : les deux accessoires ne s'échangent pas", "[app][fiche]") {
  // 🔴 LA SEULE LIGNE DE CE FICHIER QUI PUISSE ÊTRE FAUSSE SANS QUE RIEN NE LE
  // DISE. Les deux bits sont voisins, leurs noms se ressemblent, et l'écran les
  // étiquette en miroir de la façon dont le serveur les nomme. Un échange ne se
  // verrait que sur un personnage portant DEUX accessoires différents.
  CHECK(EquipLocationOf(EquipSlot::kAccessoryLeft, kOrdinary) == ro::sim::kEquipAccessoryLeft);
  CHECK(EquipLocationOf(EquipSlot::kAccessoryRight, kOrdinary) == ro::sim::kEquipAccessoryRight);
  CHECK(EquipLocationOf(EquipSlot::kAccessoryLeft, kOrdinary) !=
        EquipLocationOf(EquipSlot::kAccessoryRight, kOrdinary));
}

TEST_CASE("Mannequin : les trois coiffes tombent chacune sur son bit", "[app][fiche]") {
  // Elles sont voisines sur l'écran ET dans l'énumération : c'est l'autre paire
  // qu'un copier-coller intervertit.
  CHECK(EquipLocationOf(EquipSlot::kHeadTop, kOrdinary) == ro::sim::kEquipHeadTop);
  CHECK(EquipLocationOf(EquipSlot::kHeadMid, kOrdinary) == ro::sim::kEquipHeadMid);
  CHECK(EquipLocationOf(EquipSlot::kHeadLow, kOrdinary) == ro::sim::kEquipHeadLow);
}

TEST_CASE("Mannequin : chaque case ordinaire a SON emplacement", "[app][fiche]") {
  // ⚠ ONZE PLACES, ONZE BITS, AUCUN PARTAGÉ. Deux cases qui rendraient le même
  // emplacement montreraient la même pièce deux fois — et le joueur croirait en
  // porter deux.
  std::set<std::uint32_t> seen;
  for (std::size_t rank = 0; rank < kEquipSlotCount; ++rank) {
    const std::uint32_t location = EquipLocationOf(static_cast<EquipSlot>(rank), kOrdinary);
    CHECK(location != 0);
    seen.insert(location);
  }
  CHECK(seen.size() == kEquipSlotCount);
}

TEST_CASE("Mannequin : le costume a ses PROPRES bits, pas ceux de l'équipement", "[app][fiche]") {
  // 🔴 SANS CELA, L'ONGLET COSTUME MONTRERAIT LE VRAI CHAPEAU. Les deux se
  // portent en même temps sur la même tête, et rien à l'écran ne dirait lequel
  // on regarde.
  CHECK(EquipLocationOf(EquipSlot::kHeadTop, kCostume) == ro::sim::kEquipCostumeHeadTop);
  CHECK(EquipLocationOf(EquipSlot::kHeadMid, kCostume) == ro::sim::kEquipCostumeHeadMid);
  CHECK(EquipLocationOf(EquipSlot::kHeadLow, kCostume) == ro::sim::kEquipCostumeHeadLow);
  CHECK(EquipLocationOf(EquipSlot::kGarment, kCostume) == ro::sim::kEquipCostumeGarment);

  for (std::size_t rank = 0; rank < kEquipSlotCount; ++rank) {
    const std::uint32_t location = EquipLocationOf(static_cast<EquipSlot>(rank), kCostume);
    if (location != 0) {
      CHECK(ro::sim::IsCostumeLocation(location));
    }
  }
}

TEST_CASE("Mannequin : les sept autres places n'ont pas de costume", "[app][fiche]") {
  // ⚠ ZÉRO, ET NON LE BIT ORDINAIRE. Rendre l'équipement y ferait apparaître
  // l'arme et l'armure dans l'onglet d'apparat, comme si un costume les
  // reprenait.
  CHECK(EquipLocationOf(EquipSlot::kWeapon, kCostume) == 0);
  CHECK(EquipLocationOf(EquipSlot::kShield, kCostume) == 0);
  CHECK(EquipLocationOf(EquipSlot::kArmor, kCostume) == 0);
  CHECK(EquipLocationOf(EquipSlot::kShoes, kCostume) == 0);
  CHECK(EquipLocationOf(EquipSlot::kAccessoryLeft, kCostume) == 0);
  CHECK(EquipLocationOf(EquipSlot::kAccessoryRight, kCostume) == 0);
  CHECK(EquipLocationOf(EquipSlot::kAmmo, kCostume) == 0);
}

TEST_CASE("Mannequin : aucune place ne tombe sur un emplacement d'ombre", "[app][fiche]") {
  // L'équipement d'ombre partage les places des costumes chez le client
  // d'origine, où il en couvre un en silence. On ne le montre pas ; cet essai
  // dit que ce n'est pas un oubli mais une règle.
  for (std::size_t rank = 0; rank < kEquipSlotCount; ++rank) {
    const auto slot = static_cast<EquipSlot>(rank);
    CHECK_FALSE(ro::sim::IsShadowLocation(EquipLocationOf(slot, kOrdinary)));
    CHECK_FALSE(ro::sim::IsShadowLocation(EquipLocationOf(slot, kCostume)));
  }
}

TEST_CASE("Mannequin : Ctrl demande l'autre main, et seulement pour une arme", "[app][fiche]") {
  // Le geste du client : Ctrl pendant le double-clic ou le lâcher pose l'arme
  // dans la main gauche. Une arme ordinaire ne déclare QUE la droite -- la
  // gauche se demande, elle ne se lit pas sur l'objet.
  CHECK(WearAt(ro::sim::kEquipWeapon, true) == ro::sim::kEquipShield);
  CHECK(WearAt(ro::sim::kEquipWeapon, false) == ro::sim::kEquipWeapon);

  // ⚠ ET RIEN D'AUTRE NE BOUGE. Ctrl sur un chapeau ne doit pas le transformer
  // en bouclier : la variante ne vaut que pour ce qui se porte en main droite et
  // nulle part ailleurs.
  CHECK(WearAt(ro::sim::kEquipHeadTop, true) == ro::sim::kEquipHeadTop);
  CHECK(WearAt(ro::sim::kEquipArmor, true) == ro::sim::kEquipArmor);
  CHECK(WearAt(ro::sim::kEquipAccessoryLeft, true) == ro::sim::kEquipAccessoryLeft);

  // Chez un ambidextre, la dague déclare les deux mains : Ctrl la resserre sur
  // la gauche. Sans Ctrl, le serveur choisit.
  CHECK(WearAt(ro::sim::kEquipBothHands, true) == ro::sim::kEquipShield);
  CHECK(WearAt(ro::sim::kEquipBothHands, false) == ro::sim::kEquipBothHands);
}

TEST_CASE("Mannequin : la case visée resserre la demande", "[app][fiche]") {
  constexpr std::uint32_t kCostumeHead =
      ro::sim::kEquipCostumeHeadTop | ro::sim::kEquipCostumeHeadMid | ro::sim::kEquipCostumeHeadLow;
  // Un costume de tête lâché sur la coiffe basse y va : renvoyer les trois bits
  // laisserait le serveur prendre la haute, la première libre.
  CHECK(WearAimedAt(kCostumeHead, false, EquipLocationOf(EquipSlot::kHeadLow, kCostume)) ==
        ro::sim::kEquipCostumeHeadLow);
  CHECK(WearAimedAt(kCostumeHead, false, EquipLocationOf(EquipSlot::kHeadMid, kCostume)) ==
        ro::sim::kEquipCostumeHeadMid);

  // L'accessoire va du côté visé.
  constexpr std::uint32_t kBothAccessories =
      ro::sim::kEquipAccessoryLeft | ro::sim::kEquipAccessoryRight;
  CHECK(WearAimedAt(kBothAccessories, false,
                    EquipLocationOf(EquipSlot::kAccessoryRight, kOrdinary)) ==
        ro::sim::kEquipAccessoryRight);

  // ⚠ UNE CASE OÙ LA PIÈCE NE VA PAS retombe sur la demande entière : le même
  // costume lâché sur la coiffe ORDINAIRE, une armure lâchée sur un chapeau.
  CHECK(WearAimedAt(kCostumeHead, false, EquipLocationOf(EquipSlot::kHeadLow, kOrdinary)) ==
        kCostumeHead);
  CHECK(WearAimedAt(ro::sim::kEquipArmor, false, ro::sim::kEquipHeadTop) == ro::sim::kEquipArmor);
  // Et sans case visée, Ctrl garde son sens.
  CHECK(WearAimedAt(ro::sim::kEquipWeapon, true, 0) == ro::sim::kEquipShield);
}

TEST_CASE("Mannequin : il montre tout ce qui se porte, sauf l'ombre", "[app][fiche]") {
  // 🔴 C'EST CE PRÉDICAT QUI DÉCIDE CE QUI QUITTE LE SAC. Rendre vrai pour une
  // place que la fiche ne dessine pas ferait disparaître la pièce des DEUX
  // fenêtres, et il n'y aurait plus de geste pour l'ôter.
  for (std::size_t rank = 0; rank < kEquipSlotCount; ++rank) {
    const auto slot = static_cast<EquipSlot>(rank);
    CHECK(ShownOnDoll(EquipLocationOf(slot, kOrdinary)));
    if (const std::uint32_t apparat = EquipLocationOf(slot, kCostume); apparat != 0) {
      CHECK(ShownOnDoll(apparat));
    }
  }

  // L'ombre RESTE dans le sac : aucune case ne la lit.
  constexpr std::uint32_t kShadowArmor = 0x010000;
  CHECK_FALSE(ShownOnDoll(kShadowArmor));
  CHECK_FALSE(ShownOnDoll(ro::sim::kEquipShadowMask));

  // Ce qui n'est porté nulle part n'est montré nulle part.
  CHECK_FALSE(ShownOnDoll(0));

  // ⚠ UN SEUL BIT MONTRÉ SUFFIT. Une arme à deux mains en porte deux, et une
  // pièce d'ombre posée à côté d'un bit ordinaire ne doit pas la retenir dans le
  // sac : ce serait la montrer deux fois.
  CHECK(ShownOnDoll(ro::sim::kEquipBothHands));
  CHECK(ShownOnDoll(ro::sim::kEquipArmor | kShadowArmor));
}

TEST_CASE("Mannequin : une arme à deux mains remplit les deux cases des mains", "[app][fiche]") {
  const std::uint32_t weapon_place = EquipLocationOf(EquipSlot::kWeapon, kOrdinary);
  const std::uint32_t shield_place = EquipLocationOf(EquipSlot::kShield, kOrdinary);
  constexpr std::uint16_t kClaymoreSlot = 7;

  ro::sim::Inventory two_hands;
  Wear(two_hands, kClaymoreSlot, ro::sim::kEquipBothHands, 0);
  REQUIRE(two_hands.WornAt(weapon_place) != nullptr);
  REQUIRE(two_hands.WornAt(shield_place) != nullptr);
  CHECK(two_hands.WornAt(weapon_place)->client_slot == kClaymoreSlot);
  CHECK(two_hands.WornAt(shield_place)->client_slot == kClaymoreSlot);

  // ⚠ UNE ARME À UNE MAIN LAISSE LA CASE DU BOUCLIER VIDE.
  ro::sim::Inventory one_hand;
  Wear(one_hand, kClaymoreSlot, ro::sim::kEquipWeapon, 0);
  CHECK(one_hand.WornAt(weapon_place) != nullptr);
  CHECK(one_hand.WornAt(shield_place) == nullptr);

  // ⚠ DEUX ARMES RESTENT DEUX PIÈCES, chacune à sa place.
  constexpr std::uint16_t kLeftDaggerSlot = 8;
  ro::sim::Inventory dual_wield;
  Wear(dual_wield, kClaymoreSlot, ro::sim::kEquipWeapon, 0);
  Wear(dual_wield, kLeftDaggerSlot, ro::sim::kEquipShield, 0);
  REQUIRE(dual_wield.WornAt(weapon_place) != nullptr);
  REQUIRE(dual_wield.WornAt(shield_place) != nullptr);
  CHECK(dual_wield.WornAt(weapon_place)->client_slot == kClaymoreSlot);
  CHECK(dual_wield.WornAt(shield_place)->client_slot == kLeftDaggerSlot);
}

TEST_CASE("Sac : les munitions portées y restent", "[app][fiche]") {
  // 🔴 ELLES SE DÉPENSENT, et c'est dans le sac qu'on surveille leur nombre et
  // qu'on en change. Une armure portée, elle, n'est que sur le mannequin.
  CHECK(ShownOnDoll(ro::sim::kEquipAmmo));
  CHECK(ShownInBag(ro::sim::kEquipAmmo));
  CHECK_FALSE(ShownInBag(ro::sim::kEquipArmor));
  CHECK(ShownInBag(0));
  CHECK(ShownInBag(ro::sim::kEquipShadowMask));
}

TEST_CASE("Pièces portées : trois coiffes, chacune à sa place, et la cape",
          "[app][fiche][show_costumes]") {
  ro::sim::Inventory bag;
  Wear(bag, 2, ro::sim::kEquipHeadLow, 11);
  Wear(bag, 3, ro::sim::kEquipHeadMid, 22);
  Wear(bag, 4, ro::sim::kEquipHeadTop, 33);
  Wear(bag, 5, ro::sim::kEquipGarment, 44);
  const WornViews worn = WornPieceViews(bag);
  CHECK(worn.headgears[kLow] == 11);
  CHECK(worn.headgears[kMid] == 22);
  CHECK(worn.headgears[kTop] == 33);
  CHECK(worn.robe == 44);
}

TEST_CASE("Pièces portées : une coiffe sur plusieurs places ne se dessine qu'à la plus haute",
          "[app][fiche][show_costumes]") {
  SECTION("les trois places") {
    ro::sim::Inventory bag;
    Wear(bag, 2, ro::sim::kEquipHeadTop | ro::sim::kEquipHeadMid | ro::sim::kEquipHeadLow, 7);
    const WornViews worn = WornPieceViews(bag);
    CHECK(worn.headgears[kTop] == 7);
    CHECK(worn.headgears[kMid] == 0);
    CHECK(worn.headgears[kLow] == 0);
  }
  SECTION("le haut et le milieu, une coiffe basse à part") {
    ro::sim::Inventory bag;
    Wear(bag, 2, ro::sim::kEquipHeadTop | ro::sim::kEquipHeadMid, 7);
    Wear(bag, 3, ro::sim::kEquipHeadLow, 8);
    const WornViews worn = WornPieceViews(bag);
    CHECK(worn.headgears[kTop] == 7);
    CHECK(worn.headgears[kMid] == 0);
    CHECK(worn.headgears[kLow] == 8);
  }
  SECTION("le milieu et le bas, une coiffe haute à part") {
    ro::sim::Inventory bag;
    Wear(bag, 2, ro::sim::kEquipHeadTop, 6);
    Wear(bag, 3, ro::sim::kEquipHeadMid | ro::sim::kEquipHeadLow, 7);
    const WornViews worn = WornPieceViews(bag);
    CHECK(worn.headgears[kTop] == 6);
    CHECK(worn.headgears[kMid] == 7);
    CHECK(worn.headgears[kLow] == 0);
  }
}

TEST_CASE("Pièces portées : deux coiffes de même apparence restent deux",
          "[app][fiche][show_costumes]") {
  // ⚠ ON COMPARE LES PIÈCES, PAS LES APPARENCES : les confondre effacerait l'une
  // des deux, comme si elle couvrait la place de l'autre.
  ro::sim::Inventory bag;
  Wear(bag, 2, ro::sim::kEquipHeadTop, 9);
  Wear(bag, 3, ro::sim::kEquipHeadMid, 9);
  const WornViews worn = WornPieceViews(bag);
  CHECK(worn.headgears[kTop] == 9);
  CHECK(worn.headgears[kMid] == 9);
}

TEST_CASE("Pièces portées : un costume n'en est pas une", "[app][fiche][show_costumes]") {
  // 🔴 C'EST TOUT L'OBJET DE CETTE VUE : costumes masqués, l'avatar montre ce
  // qui est porté DESSOUS. Un costume pris ici le remontrerait.
  ro::sim::Inventory bag;
  Wear(bag, 2, ro::sim::kEquipCostumeHeadTop, 90);
  Wear(bag, 3, ro::sim::kEquipCostumeGarment, 91);
  Wear(bag, 4, ro::sim::kEquipHeadTop, 5);
  const WornViews worn = WornPieceViews(bag);
  CHECK(worn.headgears[kTop] == 5);
  CHECK(worn.robe == 0);
}

TEST_CASE("Pièces portées : rien de porté, rien de dessiné", "[app][fiche][show_costumes]") {
  ro::sim::Inventory bag;
  // Une coiffe dans le sac, mais pas sur la tête.
  bag.Put(ro::sim::InventoryItem{
      .client_slot = 2, .wearable_at = ro::sim::kEquipHeadTop, .view_id = 12});
  const WornViews worn = WornPieceViews(bag);
  CHECK(worn.headgears == std::array<std::uint16_t, 3>{});
  CHECK(worn.robe == 0);
}

TEST_CASE("Barre : une pièce qui se porte s'équipe, elle ne s'utilise pas", "[app][barre]") {
  // 🔴 LE DÉFAUT QUE CET ESSAI GARDE : une demande d'usage pour une épée, que le
  // serveur refuse sans un mot. La touche semble alors ne rien faire.
  const ro::sim::InventoryItem sword{
      .client_slot = 2, .wearable_at = ro::sim::kEquipWeapon, .identified = true};
  const ShortcutItemGesture gesture = ShortcutGestureOf(sword);
  CHECK(gesture.act == ShortcutItemAct::kWear);
  CHECK(gesture.wear_at == ro::sim::kEquipWeapon);

  const ro::sim::InventoryItem potion{.client_slot = 3, .identified = true};
  CHECK(ShortcutGestureOf(potion).act == ShortcutItemAct::kUse);
}

TEST_CASE("Barre : une pièce portée ou non identifiée ne fait rien", "[app][barre]") {
  const ro::sim::InventoryItem worn{.client_slot = 2,
                                    .wearable_at = ro::sim::kEquipArmor,
                                    .worn_at = ro::sim::kEquipArmor,
                                    .identified = true};
  CHECK(ShortcutGestureOf(worn).act == ShortcutItemAct::kNothing);

  const ro::sim::InventoryItem unknown{.client_slot = 3, .wearable_at = ro::sim::kEquipArmor};
  CHECK(ShortcutGestureOf(unknown).act == ShortcutItemAct::kNothing);
}

TEST_CASE("Barre : les munitions se reposent, même portées", "[app][barre]") {
  const ro::sim::InventoryItem arrows{.client_slot = 4,
                                      .wearable_at = ro::sim::kEquipAmmo,
                                      .worn_at = ro::sim::kEquipAmmo,
                                      .identified = true};
  const ShortcutItemGesture gesture = ShortcutGestureOf(arrows);
  CHECK(gesture.act == ShortcutItemAct::kWear);
  CHECK(gesture.wear_at == ro::sim::kEquipAmmo);
}

TEST_CASE("Barre : l'exemplaire visé est celui qui n'est pas porté", "[app][barre]") {
  ro::sim::Inventory bag;
  bag.Put(ro::sim::InventoryItem{.client_slot = 2,
                                 .item_id = kRingId,
                                 .wearable_at = ro::sim::kEquipAccessoryLeft,
                                 .worn_at = ro::sim::kEquipAccessoryLeft,
                                 .identified = true});
  bag.Put(ro::sim::InventoryItem{.client_slot = 5,
                                 .item_id = kRingId,
                                 .wearable_at = ro::sim::kEquipAccessoryLeft,
                                 .identified = true});
  const ro::sim::InventoryItem* target = ShortcutTargetIn(bag, kRingId);
  REQUIRE(target != nullptr);
  CHECK(target->client_slot == 5);
}

TEST_CASE("Barre : seul exemplaire porté, c'est lui qu'on vise", "[app][barre]") {
  ro::sim::Inventory bag;
  bag.Put(ro::sim::InventoryItem{.client_slot = 2,
                                 .item_id = kRingId,
                                 .wearable_at = ro::sim::kEquipAccessoryLeft,
                                 .worn_at = ro::sim::kEquipAccessoryLeft,
                                 .identified = true});
  const ro::sim::InventoryItem* target = ShortcutTargetIn(bag, kRingId);
  REQUIRE(target != nullptr);
  CHECK(target->client_slot == 2);
  CHECK(ShortcutTargetIn(bag, kRingId + 1) == nullptr);
}
