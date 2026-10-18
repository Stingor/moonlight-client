#include "ro/render/skill_unit_primer.h"

#include "ro/core/sorted_table.h"

namespace ro::render {
namespace {

// L'amorce de cette table pour cette unité à ce niveau, ou ZÉRO.
std::uint16_t PrimerIn(std::span<const SkillUnitPrimerEntry> table, std::uint32_t placed_unit_id,
                       std::uint8_t skill_level) {
  for (const SkillUnitPrimerEntry& entry :
       core::RowsWithKey(table, placed_unit_id, &SkillUnitPrimerEntry::unit_id)) {
    if (skill_level >= entry.level_min && skill_level <= entry.level_max) {
      return entry.effect_id;
    }
  }
  return 0;
}

}  // namespace

std::uint16_t SkillUnitPrimer(std::uint32_t placed_unit_id, std::uint8_t skill_level) {
  const std::uint16_t primer = PrimerIn(SkillUnitPrimerTable(), placed_unit_id, skill_level);
  return primer != 0 ? primer : PrimerIn(kClientAddedPrimers, placed_unit_id, skill_level);
}

std::uint16_t SkillUnitPrimerOnLook(std::uint32_t placed_unit_id, std::uint8_t skill_level,
                                    std::uint32_t unit_id) {
  return unit_id == kUsedTrapUnit ? SkillUnitPrimer(placed_unit_id, skill_level) : 0;
}

}  // namespace ro::render
