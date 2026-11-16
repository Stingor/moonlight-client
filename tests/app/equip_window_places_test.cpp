// OÙ LA FENÊTRE D'ÉQUIPEMENT DU JEU MONTRE CE QUE LE SERVEUR RANGE.
//
// 🔴 UN ÉCHANGE DE DEUX PLACES VOISINES EST MUET EN JEU tant que le personnage
// n'en porte qu'une : c'est ici qu'il se voit.

#include "character/equip_window_places.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/equip_location.h"
#include "ro/sim/inventory.h"

using moonclient::EquipWindowLocationOf;
using moonclient::EquipWindowPlacesFor;
using moonclient::EquipWindowTakeOffAllMask;
using moonclient::EquipWindowWornViews;
using moonclient::MergeSheetGestures;
using ro::ui::EquipWindowTab;

namespace {

void Wear(ro::sim::Inventory& bag, std::uint16_t client_slot, std::uint32_t worn_at,
          std::uint16_t view_id) {
  bag.Put(ro::sim::InventoryItem{
      .client_slot = client_slot, .wearable_at = worn_at, .worn_at = worn_at, .view_id = view_id});
}

}  // namespace

TEST_CASE("Fenêtre d'équipement : sur Général, la place n est le bit n", "[app][equip_window]") {
  CHECK(EquipWindowLocationOf(0, EquipWindowTab::kGeneral) == ro::sim::kEquipHeadLow);
  CHECK(EquipWindowLocationOf(1, EquipWindowTab::kGeneral) == ro::sim::kEquipWeapon);
  CHECK(EquipWindowLocationOf(3, EquipWindowTab::kGeneral) == ro::sim::kEquipAccessoryRight);
  CHECK(EquipWindowLocationOf(7, EquipWindowTab::kGeneral) == ro::sim::kEquipAccessoryLeft);
  CHECK(EquipWindowLocationOf(8, EquipWindowTab::kGeneral) == ro::sim::kEquipHeadTop);
  CHECK(EquipWindowLocationOf(9, EquipWindowTab::kGeneral) == ro::sim::kEquipHeadMid);
  CHECK(EquipWindowLocationOf(10, EquipWindowTab::kGeneral) == 0);
}

TEST_CASE("Fenêtre d'équipement : sur Costume, l'apparat et l'ombre doublent les places",
          "[app][equip_window]") {
  CHECK(EquipWindowLocationOf(8, EquipWindowTab::kCostume) == ro::sim::kEquipCostumeHeadTop);
  CHECK(EquipWindowLocationOf(0, EquipWindowTab::kCostume) == ro::sim::kEquipCostumeHeadLow);
  CHECK(EquipWindowLocationOf(2, EquipWindowTab::kCostume) == ro::sim::kEquipCostumeGarment);
  CHECK(EquipWindowLocationOf(4, EquipWindowTab::kCostume) == ro::sim::kEquipShadowArmor);
  CHECK((EquipWindowLocationOf(1, EquipWindowTab::kCostume) & ro::sim::kEquipShadowWeapon) != 0);
  // Les deux masques de « tout retirer » couvrent tout, sans se recouper.
  const std::uint32_t general = EquipWindowTakeOffAllMask(EquipWindowTab::kGeneral);
  const std::uint32_t costume = EquipWindowTakeOffAllMask(EquipWindowTab::kCostume);
  CHECK((general & costume) == 0);
  for (std::size_t place = 0; place < ro::ui::kEquipWindowPlaceCount; ++place) {
    CHECK((general & EquipWindowLocationOf(place, EquipWindowTab::kGeneral)) != 0);
    CHECK((costume & EquipWindowLocationOf(place, EquipWindowTab::kCostume)) != 0);
  }
  CHECK((general & ro::sim::kEquipAmmo) != 0);
}

TEST_CASE("Fenêtre d'équipement : une arme à deux mains s'entoure aux deux places",
          "[app][equip_window]") {
  const auto places = EquipWindowPlacesFor(ro::sim::kEquipBothHands);
  CHECK(places[1]);
  CHECK(places[5]);
  CHECK_FALSE(places[0]);
  // Un costume de tête s'entoure à la place qu'il doublera.
  CHECK(EquipWindowPlacesFor(ro::sim::kEquipCostumeHeadMid)[9]);
}

TEST_CASE("Fenêtre d'équipement : la silhouette porte les coiffes de l'onglet montré",
          "[app][equip_window]") {
  ro::sim::Inventory bag;
  Wear(bag, 2, ro::sim::kEquipHeadTop | ro::sim::kEquipHeadMid, 11);
  Wear(bag, 3, ro::sim::kEquipCostumeHeadLow, 22);
  Wear(bag, 4, ro::sim::kEquipGarment, 33);

  const moonclient::WornViews general = EquipWindowWornViews(bag, EquipWindowTab::kGeneral);
  // La coiffe sur deux places ne se dessine qu'à la plus haute.
  CHECK(general.headgears[2] == 11);
  CHECK(general.headgears[1] == 0);
  CHECK(general.headgears[0] == 0);
  CHECK(general.robe == 33);

  const moonclient::WornViews costume = EquipWindowWornViews(bag, EquipWindowTab::kCostume);
  CHECK(costume.headgears[0] == 22);
  CHECK(costume.headgears[2] == 0);
  CHECK(costume.robe == 0);
}

TEST_CASE("Fenêtre d'équipement : ses gestes rejoignent ceux de la fiche sans les écraser",
          "[app][equip_window]") {
  ro::ui::CharacterWindowAction sheet;
  ro::ui::CharacterWindowAction window;
  window.verb = ro::ui::CharacterVerb::kTakeOff;
  window.slot = 9;
  window.describe_id = 501;
  window.show_costumes = false;
  MergeSheetGestures(sheet, window);
  CHECK(sheet.verb == ro::ui::CharacterVerb::kTakeOff);
  CHECK(sheet.slot == 9);
  CHECK(sheet.describe_id == 501);
  CHECK(sheet.show_costumes == std::optional<bool>(false));

  ro::ui::CharacterWindowAction busy;
  busy.verb = ro::ui::CharacterVerb::kWear;
  busy.slot = 4;
  MergeSheetGestures(busy, window);
  CHECK(busy.verb == ro::ui::CharacterVerb::kWear);
  CHECK(busy.slot == 4);
}
