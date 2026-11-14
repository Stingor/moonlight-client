// `/eqopen` et ses graphies coréennes : la ligne reconnue, et la demande qui
// en part.

#include "chat/show_equipment_command.h"

#include <optional>

#include <catch2/catch_test_macros.hpp>

using moonclient::EquipmentShownAsked;
using moonclient::IsShowEquipmentCommand;

TEST_CASE("Montrer mon équipement : les trois noms de la commande", "[app][chat][show_equipment]") {
  CHECK(IsShowEquipmentCommand("/eqopen"));
  CHECK(IsShowEquipmentCommand("/장비공개"));
  CHECK(IsShowEquipmentCommand("/wkdqlrhdro"));
  // Sans la casse latine, et le premier mot seul.
  CHECK(IsShowEquipmentCommand("/EqOpen"));
  CHECK(IsShowEquipmentCommand("/eqopen maintenant"));
}

TEST_CASE("Montrer mon équipement : un autre mot n'est pas la commande",
          "[app][chat][show_equipment]") {
  CHECK_FALSE(IsShowEquipmentCommand("/eqopen2"));
  CHECK_FALSE(IsShowEquipmentCommand("/eq"));
  CHECK_FALSE(IsShowEquipmentCommand("eqopen"));
  CHECK_FALSE(IsShowEquipmentCommand(""));
}

TEST_CASE("Montrer mon équipement : la commande demande l'inverse de l'état dit",
          "[app][chat][show_equipment]") {
  CHECK(EquipmentShownAsked(false));
  CHECK_FALSE(EquipmentShownAsked(true));
  // Rien de dit : l'état d'un personnage neuf, l'équipement caché.
  CHECK(EquipmentShownAsked(std::nullopt));
}
