#include <cstdint>
#include <span>

#include "ro/core/sorted_table.h"
#include "ro/render/skill_named_table.h"

namespace ro::render {

std::span<const SkillNamedEffectEntry> SkillNamedEffects(std::uint32_t skill_id,
                                                         std::uint8_t slot) {
  // Une PLAGE, et non une entrée : une action en joue jusqu'à sept pour un même
  // moment, et leur ordre dans la table est celui de sa fonction.
  //
  // La table range les slots d'une compétence dans l'ordre, si bien que ceux
  // d'un même slot sont CONTIGUS et se prennent en une seconde borne.
  const std::span<const SkillNamedEffectEntry> for_skill =
      core::RowsWithKey(SkillNamedEffectTable(), static_cast<std::uint16_t>(skill_id),
                        &SkillNamedEffectEntry::skill_id);
  return core::RowsWithKey(for_skill, slot, &SkillNamedEffectEntry::slot);
}

std::span<const StateNamedEffectEntry> StateNamedEffects(std::uint32_t state_id,
                                                         std::uint8_t slot) {
  // ⚠ UNE TABLE À PART, et non la même indexée autrement : un identifiant d'état
  // et un identifiant de compétence ne vivent pas dans le même espace, et les
  // réunir ferait répondre l'un pour l'autre.
  const std::span<const StateNamedEffectEntry> for_state =
      core::RowsWithKey(StateNamedEffectTable(), static_cast<std::uint16_t>(state_id),
                        &StateNamedEffectEntry::state_id);
  return core::RowsWithKey(for_state, slot, &StateNamedEffectEntry::slot);
}

}  // namespace ro::render
