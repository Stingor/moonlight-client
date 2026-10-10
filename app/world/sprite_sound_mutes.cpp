#include "world/sprite_sound_mutes.h"

#include <algorithm>

namespace moonclient {

std::string SoundFileKey(std::string_view path) {
  const std::size_t cut = path.find_last_of("\\/");
  std::string key(cut == std::string_view::npos ? path : path.substr(cut + 1));
  for (char& letter : key) {
    if (letter >= 'A' && letter <= 'Z') {
      letter = static_cast<char>(letter - 'A' + 'a');
    }
  }
  return key;
}

bool SilencesFile(const SpriteSoundMutes& mutes, std::string_view path) {
  if (mutes.files.empty()) {
    return false;
  }
  return std::ranges::find(mutes.files, SoundFileKey(path)) != mutes.files.end();
}

}  // namespace moonclient
