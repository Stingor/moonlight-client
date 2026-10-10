#include <algorithm>
#include <cstdint>
#include <span>

#include "ro/render/skill_named_table.h"

namespace ro::render {

std::span<const SkillNamedEffectEntry> SkillNamedEffects(std::uint32_t skill_id,
                                                         std::uint8_t slot) {
  // Une PLAGE, et non une entrée : une action en joue jusqu'à sept pour un même
  // moment, et leur ordre dans la table est celui de sa fonction.
  //
  // La table range les slots d'une compétence dans l'ordre, si bien que ceux
  // d'un même slot sont CONTIGUS et se prennent en une seconde borne.
  const std::span<const SkillNamedEffectEntry> table = SkillNamedEffectTable();
  const auto for_skill = std::ranges::equal_range(table, static_cast<std::uint16_t>(skill_id), {},
                                                  &SkillNamedEffectEntry::skill_id);
  const auto found = std::ranges::equal_range(for_skill, slot, {}, &SkillNamedEffectEntry::slot);
  return {found.begin(), found.end()};
}

std::span<const StateNamedEffectEntry> StateNamedEffects(std::uint32_t state_id,
                                                         std::uint8_t slot) {
  // ⚠ UNE TABLE À PART, et non la même indexée autrement : un identifiant d'état
  // et un identifiant de compétence ne vivent pas dans le même espace, et les
  // réunir ferait répondre l'un pour l'autre.
  const std::span<const StateNamedEffectEntry> table = StateNamedEffectTable();
  const auto for_state = std::ranges::equal_range(table, static_cast<std::uint16_t>(state_id), {},
                                                  &StateNamedEffectEntry::state_id);
  const auto found = std::ranges::equal_range(for_state, slot, {}, &StateNamedEffectEntry::slot);
  return {found.begin(), found.end()};
}

}  // namespace ro::render
