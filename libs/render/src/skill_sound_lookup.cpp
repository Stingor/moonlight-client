#include "ro/core/sorted_table.h"
#include "ro/render/skill_sound_table.h"

namespace ro::render {

std::string_view SkillSound(std::uint32_t skill_id) {
  const SkillSoundEntry* found =
      core::RowWithKey(SkillSoundTable(), skill_id, &SkillSoundEntry::skill_id);
  if (found == nullptr) {
    return {};
  }
  return found->sound;
}

}  // namespace ro::render
