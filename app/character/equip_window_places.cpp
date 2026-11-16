#include "character/equip_window_places.h"

#include "ro/sim/equip_location.h"

namespace moonclient {
namespace {

using ro::ui::EquipWindowTab;
using ro::ui::kEquipWindowPlaceCount;

// LES BITS DE L'ONGLET COSTUME, PAR PLACE : le costume aux quatre places qui en
// ont un, l'ombre aux six autres. ⚠ L'ARME D'OMBRE PREND AUSSI LE BIT DU
// COSTUME AU SOL, comme dans le jeu — aucune pièce ne le porte.
constexpr std::array<std::uint32_t, kEquipWindowPlaceCount> kCostumeLocations = {
    ro::sim::kEquipCostumeHeadLow, ro::sim::kEquipShadowWeapon | ro::sim::kEquipCostumeFloor,
    ro::sim::kEquipCostumeGarment, ro::sim::kEquipShadowAccessoryRight,
    ro::sim::kEquipShadowArmor,    ro::sim::kEquipShadowShield,
    ro::sim::kEquipShadowShoes,    ro::sim::kEquipShadowAccessoryLeft,
    ro::sim::kEquipCostumeHeadTop, ro::sim::kEquipCostumeHeadMid,
};

// Les dix places de l'onglet Général : la place `n` est le bit `n`.
constexpr std::uint32_t kOrdinaryLocations = (1U << kEquipWindowPlaceCount) - 1U;

// Les places des coiffes et de la cape.
constexpr std::size_t kHeadLowPlace = 0;
constexpr std::size_t kGarmentPlace = 2;
constexpr std::size_t kHeadTopPlace = 8;
constexpr std::size_t kHeadMidPlace = 9;

}  // namespace

std::uint32_t EquipWindowLocationOf(std::size_t place, EquipWindowTab tab) {
  if (place >= kEquipWindowPlaceCount) {
    return 0;
  }
  return tab == EquipWindowTab::kCostume ? kCostumeLocations[place] : 1U << place;
}

const ro::sim::InventoryItem* EquipWindowPieceOn(const ro::sim::Inventory& bag,
                                                 std::uint32_t location) {
  if (location == 0) {
    return nullptr;
  }
  for (const ro::sim::InventoryItem& item : bag.items()) {
    if ((item.worn_at & location) != 0) {
      return &item;
    }
  }
  return nullptr;
}

std::array<bool, kEquipWindowPlaceCount> EquipWindowPlacesFor(std::uint32_t wearable_at) {
  std::array<bool, kEquipWindowPlaceCount> places = {};
  for (std::size_t place = 0; place < kEquipWindowPlaceCount; ++place) {
    places[place] = (wearable_at & (EquipWindowLocationOf(place, EquipWindowTab::kGeneral) |
                                    EquipWindowLocationOf(place, EquipWindowTab::kCostume))) != 0;
  }
  return places;
}

std::uint32_t EquipWindowTakeOffAllMask(EquipWindowTab tab) {
  if (tab == EquipWindowTab::kCostume) {
    return ro::sim::kEquipCostumeMask | ro::sim::kEquipShadowMask;
  }
  return kOrdinaryLocations | ro::sim::kEquipAmmo;
}

WornViews EquipWindowWornViews(const ro::sim::Inventory& bag, EquipWindowTab tab) {
  const ro::sim::InventoryItem* top =
      EquipWindowPieceOn(bag, EquipWindowLocationOf(kHeadTopPlace, tab));
  const ro::sim::InventoryItem* mid =
      EquipWindowPieceOn(bag, EquipWindowLocationOf(kHeadMidPlace, tab));
  const ro::sim::InventoryItem* low =
      EquipWindowPieceOn(bag, EquipWindowLocationOf(kHeadLowPlace, tab));
  const ro::sim::InventoryItem* robe =
      EquipWindowPieceOn(bag, EquipWindowLocationOf(kGarmentPlace, tab));
  const auto view_of = [](const ro::sim::InventoryItem* piece) -> std::uint16_t {
    return piece == nullptr ? 0 : piece->view_id;
  };
  WornViews worn;
  worn.headgears = {(low != top && low != mid) ? view_of(low) : std::uint16_t{0},
                    mid != top ? view_of(mid) : std::uint16_t{0}, view_of(top)};
  worn.robe = view_of(robe);
  return worn;
}

void MergeSheetGestures(ro::ui::CharacterWindowAction& into,
                        const ro::ui::CharacterWindowAction& from) {
  if (into.verb == ro::ui::CharacterVerb::kNothing &&
      from.verb != ro::ui::CharacterVerb::kNothing) {
    into.verb = from.verb;
    into.slot = from.slot;
    into.alternate = from.alternate;
    into.wear_aimed = from.wear_aimed;
    into.wear_costume = from.wear_costume;
  }
  if (into.describe_id == 0 && from.describe_id != 0) {
    into.describe_id = from.describe_id;
    into.describe_slot = from.describe_slot;
  }
  if (into.link_slot == 0) {
    into.link_slot = from.link_slot;
  }
  if (!into.show_equipment.has_value()) {
    into.show_equipment = from.show_equipment;
  }
  if (!into.show_costumes.has_value()) {
    into.show_costumes = from.show_costumes;
  }
  if (into.toggled_companion < 0) {
    into.toggled_companion = from.toggled_companion;
  }
}

}  // namespace moonclient
