// La commande qui bascule « Montrer mon équipement ». Voir l'en-tête.

#include "chat/show_equipment_command.h"

#include <array>

#include "ro/core/case_fold.h"

namespace moonclient {
namespace {

// Le nom latin, le nom coréen, et ce même nom tapé sur un clavier latin.
constexpr std::array<std::string_view, 3> kShowEquipmentCommands = {
    "/eqopen",
    "/장비공개",
    "/wkdqlrhdro",
};

}  // namespace

bool IsShowEquipmentCommand(std::string_view line) {
  const std::string_view word = line.substr(0, line.find(' '));
  for (const std::string_view command : kShowEquipmentCommands) {
    if (ro::core::EqualsFolded(word, command)) {
      return true;
    }
  }
  return false;
}

}  // namespace moonclient
