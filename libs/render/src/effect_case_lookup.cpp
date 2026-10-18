#include "ro/core/sorted_table.h"
#include "ro/render/effect_case_table.h"

namespace ro::render {

const EffectCaseEntry* EffectCaseFor(std::uint32_t effect_id) {
  // La table est triée par identifiant : une recherche dichotomique suffit, et
  // elle est appelée une fois par effet qui naît.
  return core::RowWithKey(EffectCaseTable(), effect_id, &EffectCaseEntry::effect_id);
}

}  // namespace ro::render
