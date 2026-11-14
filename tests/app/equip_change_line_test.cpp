// Les lignes « Équipement mis/retiré » : le nom de la pièce suivi de la phrase,
// vert à la pose et rouge au retrait, et celles du jeu d'équipement de
// rechange, comme le client d'origine.

#include "character/equip_change_line.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/character_ledger.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/equip_location.h"

#include "character/equip_attribute_lines.h"

using moonclient::EquipChangeLine;
using moonclient::EquipChangeLineOf;
using moonclient::EquipSwitchLines;
using ro::sim::ChatType;
using ro::sim::EquipSwitchEvent;
using ro::sim::EquipSwitchNews;
using ro::sim::EquipSwitchRefusal;

namespace {

constexpr std::uint32_t kArmor = 0x0010;
constexpr std::uint32_t kAmmo = ro::sim::kEquipAmmo;
constexpr std::uint32_t kRed = 0xFF0000;

}  // namespace

TEST_CASE("Équipement mis : le nom, puis la phrase, en vert", "[app][equip]") {
  const EquipChangeLine line = EquipChangeLineOf("+7 Knife [3]", true);
  CHECK(line.text == "+7 Knife [3] a été équipé.");
  CHECK(line.color == moonclient::kEquipAttributesOnColor);
}

TEST_CASE("Équipement retiré : le nom, puis la phrase, en rouge", "[app][equip]") {
  const EquipChangeLine line = EquipChangeLineOf("Arrow", false);
  CHECK(line.text == "Arrow a été retiré.");
  CHECK(line.color == moonclient::kEquipAttributesOffColor);
  CHECK(line.type == ChatType::kEquipmentOnOff);
}

TEST_CASE("Jeu de rechange : une pièce rangée ou sortie se nomme, en vert, case 7",
          "[app][equip]") {
  const std::vector<EquipChangeLine> placed = EquipSwitchLines(
      EquipSwitchNews{.event = EquipSwitchEvent::kPlaced, .location = kArmor}, "Coat");
  REQUIRE(placed.size() == 1);
  CHECK(placed[0].text == "Coat a été placé dans la fenêtre d'échange d'équipement.");
  CHECK(placed[0].color == moonclient::kEquipAttributesOnColor);
  CHECK(placed[0].type == ChatType::kEquipmentOnOff);

  const std::vector<EquipChangeLine> removed = EquipSwitchLines(
      EquipSwitchNews{.event = EquipSwitchEvent::kRemoved, .location = kArmor}, "Coat");
  REQUIRE(removed.size() == 1);
  CHECK(removed[0].text == "Coat a été retiré de la fenêtre d'échange d'équipement.");
  CHECK(removed[0].color == moonclient::kEquipAttributesOnColor);

  // Sans nom, rien à dire.
  CHECK(EquipSwitchLines(EquipSwitchNews{.event = EquipSwitchEvent::kPlaced}, "").empty());
}

TEST_CASE("Jeu de rechange : des munitions sorties disent aussi leur retrait, d'abord",
          "[app][equip]") {
  const std::vector<EquipChangeLine> lines = EquipSwitchLines(
      EquipSwitchNews{.event = EquipSwitchEvent::kRemoved, .location = kAmmo}, "Arrow");
  REQUIRE(lines.size() == 2);
  CHECK(lines[0].text == "Arrow a été retiré.");
  CHECK(lines[0].color == moonclient::kEquipAttributesOffColor);
  CHECK(lines[1].text == "Arrow a été retiré de la fenêtre d'échange d'équipement.");
}

TEST_CASE("Jeu de rechange : un refus se dit en rouge, dans la case générale", "[app][equip]") {
  const std::vector<EquipChangeLine> swap =
      EquipSwitchLines(EquipSwitchNews{.event = EquipSwitchEvent::kSwapRefused,
                                       .refusal = EquipSwitchRefusal::kCannotSwap},
                       "");
  REQUIRE(swap.size() == 1);
  CHECK(swap[0].text == "Impossible d'échanger l'équipement.");
  CHECK(swap[0].color == kRed);
  CHECK(swap[0].type == ChatType::kPublic);

  const std::vector<EquipChangeLine> place =
      EquipSwitchLines(EquipSwitchNews{.event = EquipSwitchEvent::kPlaceRefused,
                                       .refusal = EquipSwitchRefusal::kCannotRegister},
                       "Coat");
  REQUIRE(place.size() == 1);
  CHECK(place[0].text == "Impossible de ranger cet item dans la fenêtre d'échange.");

  // Un refus de sortie de munitions : le refus, puis le retrait.
  const std::vector<EquipChangeLine> ammo =
      EquipSwitchLines(EquipSwitchNews{.event = EquipSwitchEvent::kRemoveRefused,
                                       .location = kAmmo,
                                       .refusal = EquipSwitchRefusal::kNothingToReplace},
                       "Arrow");
  REQUIRE(ammo.size() == 2);
  CHECK(ammo[0].text == "Il n'y a aucun équipement à échanger.");
  CHECK(ammo[1].text == "Arrow a été retiré.");
}
