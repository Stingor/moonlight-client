// Les lignes d'attributs d'équipement. Voir l'en-tête.

#include "character/equip_attribute_lines.h"

namespace moonclient {

std::vector<std::string> ComposeEquipAttributeLines(const ro::sim::EquipAttributes& told,
                                                    const EquipAttributeNames& names) {
  const std::string_view prefix = EquipAttributesPrefix(told.kind);
  std::vector<std::string> lines;
  lines.reserve(told.bonuses.size());
  for (const ro::sim::EquipBonus& bonus : told.bonuses) {
    std::string text = EquipBonusText(bonus, names);
    if (text.empty()) {
      continue;
    }
    lines.push_back(std::string(prefix) + text);
  }
  return lines;
}

}  // namespace moonclient
