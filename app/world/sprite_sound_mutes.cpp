#include "world/sprite_sound_mutes.h"

#include <algorithm>

#include "ro/core/ascii.h"

namespace moonclient {

std::string SoundFileKey(std::string_view path) {
  const std::size_t cut = path.find_last_of("\\/");
  return ro::core::AsciiLower(cut == std::string_view::npos ? path : path.substr(cut + 1));
}

bool SilencesFile(const SpriteSoundMutes& mutes, std::string_view path) {
  if (mutes.files.empty()) {
    return false;
  }
  return std::ranges::find(mutes.files, SoundFileKey(path)) != mutes.files.end();
}

}  // namespace moonclient
