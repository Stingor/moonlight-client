// OÙ « VOIR L'ÉQUIPEMENT » RANGE CHAQUE PIÈCE.
//
// 🔴 AUCUNE DE CES RÈGLES NE SE VOIT QUAND ELLE EST FAUSSE : une pièce rangée à
// la mauvaise place laisse une ligne « — » et en remplit une autre, et l'œil
// lit une fiche plausible.

#include "items/view_equip_rows.h"

#include <array>
#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/equip_location.h"
#include "ro/sim/inventory.h"

using moonclient::EquipPlace;
using moonclient::EquipSection;
using moonclient::LocationOf;
using moonclient::PiecePlace;
using moonclient::PlaceOf;
using moonclient::RankAt;

namespace {

[[nodiscard]] ro::sim::InventoryItem Worn(std::uint16_t slot, std::uint32_t worn_at) {
  ro::sim::InventoryItem item;
  item.client_slot = slot;
  item.item_id = slot;
  item.worn_at = worn_at;
  return item;
}

}  // namespace

TEST_CASE("Voir l'équipement : une pièce à plusieurs places se range à la PREMIÈRE",
          "[app][view_equip]") {
  const PiecePlace two_hands = PlaceOf(ro::sim::kEquipBothHands);
  CHECK(two_hands.section == EquipSection::kEquipment);
  CHECK(two_hands.place == EquipPlace::kWeapon);
  CHECK(two_hands.two_handed);

  const PiecePlace top_and_mid = PlaceOf(ro::sim::kEquipHeadTop | ro::sim::kEquipHeadMid);
  CHECK(top_and_mid.place == EquipPlace::kHeadTop);
  CHECK_FALSE(top_and_mid.two_handed);
}

TEST_CASE("Voir l'équipement : les accessoires portent le nom que l'ÉCRAN leur donne",
          "[app][view_equip]") {
  // Le bit que le serveur nomme « droit » s'affiche « gauche », et l'inverse.
  CHECK(PlaceOf(ro::sim::kEquipAccessoryRight).place == EquipPlace::kAccessoryLeft);
  CHECK(PlaceOf(ro::sim::kEquipAccessoryLeft).place == EquipPlace::kAccessoryRight);
  CHECK(LocationOf(EquipPlace::kAccessoryLeft, EquipSection::kEquipment) ==
        ro::sim::kEquipAccessoryRight);
  CHECK(LocationOf(EquipPlace::kAccessoryRight, EquipSection::kEquipment) ==
        ro::sim::kEquipAccessoryLeft);
}

TEST_CASE("Voir l'équipement : costumes, munition et ombre ont leur section", "[app][view_equip]") {
  const PiecePlace costume = PlaceOf(ro::sim::kEquipCostumeHeadMid);
  CHECK(costume.section == EquipSection::kCostume);
  CHECK(costume.place == EquipPlace::kHeadMid);
  CHECK(PlaceOf(ro::sim::kEquipCostumeGarment).place == EquipPlace::kGarment);

  const PiecePlace ammo = PlaceOf(ro::sim::kEquipAmmo);
  CHECK(ammo.section == EquipSection::kOther);
  CHECK(ammo.ammo);
  CHECK_FALSE(ammo.place.has_value());

  // L'ombre ne se range PAS sur les places des costumes, où elle en couvrirait un.
  constexpr std::uint32_t kShadowArmor = 0x010000;
  const PiecePlace shadow = PlaceOf(kShadowArmor);
  CHECK(shadow.section == EquipSection::kOther);
  CHECK(shadow.shadow);
}

TEST_CASE("Voir l'équipement : un costume n'a de bit qu'à ses quatre places", "[app][view_equip]") {
  CHECK(LocationOf(EquipPlace::kHeadTop, EquipSection::kCostume) == ro::sim::kEquipCostumeHeadTop);
  CHECK(LocationOf(EquipPlace::kGarment, EquipSection::kCostume) == ro::sim::kEquipCostumeGarment);
  CHECK(LocationOf(EquipPlace::kWeapon, EquipSection::kCostume) == 0);
  CHECK(LocationOf(EquipPlace::kHeadTop, EquipSection::kOther) == 0);
}

TEST_CASE("Voir l'équipement : la colonne d'en face ne montre qu'UNE fois une pièce doublée",
          "[app][view_equip]") {
  // Le fil envoie l'arme à deux mains une fois par main.
  const std::array<ro::sim::InventoryItem, 3> theirs = {Worn(5, ro::sim::kEquipBothHands),
                                                        Worn(5, ro::sim::kEquipBothHands),
                                                        Worn(8, ro::sim::kEquipGarment)};
  CHECK(RankAt(theirs, EquipPlace::kWeapon, EquipSection::kEquipment) ==
        std::optional<std::size_t>{0});
  // Le bouclier reste VIDE : la fenêtre y écrit pourquoi.
  CHECK_FALSE(RankAt(theirs, EquipPlace::kShield, EquipSection::kEquipment).has_value());
  CHECK(RankAt(theirs, EquipPlace::kGarment, EquipSection::kEquipment) ==
        std::optional<std::size_t>{2});
  CHECK_FALSE(RankAt(theirs, EquipPlace::kGarment, EquipSection::kCostume).has_value());
}

TEST_CASE("Voir l'équipement : MA colonne se lit par emplacement, deux mains comprises",
          "[app][view_equip]") {
  ro::sim::Inventory mine;
  ro::sim::InventoryItem blade = Worn(4, ro::sim::kEquipBothHands);
  blade.amount = 1;
  mine.Put(blade);
  // Ma deux-mains répond à l'arme ET au bouclier.
  const ro::sim::InventoryItem* weapon =
      mine.WornAt(LocationOf(EquipPlace::kWeapon, EquipSection::kEquipment));
  const ro::sim::InventoryItem* shield =
      mine.WornAt(LocationOf(EquipPlace::kShield, EquipSection::kEquipment));
  REQUIRE(weapon != nullptr);
  REQUIRE(shield != nullptr);
  CHECK(weapon->client_slot == 4);
  CHECK(shield->client_slot == 4);
}
