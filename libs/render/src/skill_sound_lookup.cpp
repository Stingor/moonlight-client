#include <algorithm>

#include "ro/render/skill_sound_table.h"

namespace ro::render {

std::string_view SkillSound(std::uint32_t skill_id) {
  const std::span<const SkillSoundEntry> table = SkillSoundTable();
  const auto found = std::ranges::lower_bound(table, skill_id, {}, &SkillSoundEntry::skill_id);
  if (found == table.end() || found->skill_id != skill_id) {
    return {};
  }
  return found->sound;
}

}  // namespace ro::render
