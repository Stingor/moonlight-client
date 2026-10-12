// Voir l'en-tête.

#include "ro/sim/monster_talk.h"

#include <algorithm>

namespace ro::sim {
namespace {

constexpr std::string_view kSpeakerSeparator = " : ";

[[nodiscard]] char LowerAscii(char letter) noexcept {
  return letter >= 'A' && letter <= 'Z' ? static_cast<char>(letter - 'A' + 'a') : letter;
}

}  // namespace

std::string MonsterTalkSpecies(std::string_view resource_name) {
  std::string species(resource_name);
  std::ranges::transform(species, species.begin(), LowerAscii);
  return species;
}

std::string MonsterLineShown(std::string_view speaker_name, std::string_view line) {
  std::string shown;
  shown.reserve(speaker_name.size() + kSpeakerSeparator.size() + line.size());
  shown.append(speaker_name).append(kSpeakerSeparator).append(line);
  return shown;
}

bool StrikeMemories::Strike(std::uint32_t actor_id, bool at_rest, std::uint32_t target_id,
                            std::uint8_t action) {
  return watches_[actor_id].Strike(at_rest, target_id, action);
}

void StrikeMemories::Remember(std::uint32_t actor_id, std::uint32_t target_id) {
  watches_[actor_id].Remember(target_id);
}

}  // namespace ro::sim
