#include "ro/sim/title_book.h"

#include <algorithm>

namespace ro::sim {

bool TitleBook::NoteRewarded(std::uint32_t achievement_id) {
  if (std::ranges::find(rewarded_, achievement_id) != rewarded_.end()) {
    return false;
  }
  rewarded_.push_back(achievement_id);
  return true;
}

}  // namespace ro::sim
