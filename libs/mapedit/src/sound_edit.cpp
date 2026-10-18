// LES SONS D'AMBIANCE DU DOCUMENT : leurs retouches, rangées par rang, et les
// sons ajoutés, dans l'ordre du document.

#include <algorithm>
#include <cmath>
#include <utility>

#include "ro/mapedit/map_edit.h"

namespace ro::mapedit {

bool SoundEditTargets(const SoundEdit& edit, std::string_view wave_file,
                      const std::array<float, 3>& position) {
  if (edit.wave_file != wave_file) {
    return false;
  }
  for (std::size_t axis = 0; axis < position.size(); ++axis) {
    if (std::abs(edit.origin[axis] - position[axis]) > kOriginTolerance) {
      return false;
    }
  }
  return true;
}

void MapEdit::SetSound(std::size_t slot, AddedSound sound) {
  if (slot < sounds_.size()) {
    sounds_[slot] = std::move(sound);
  }
}

void MapEdit::RemoveSound(std::size_t slot) {
  if (slot < sounds_.size()) {
    sounds_.erase(sounds_.begin() + static_cast<std::ptrdiff_t>(slot));
  }
}

const SoundEdit* MapEdit::SoundEditAt(std::size_t sound_index) const {
  const auto found =
      std::ranges::lower_bound(sound_edits_, sound_index, {}, &SoundEdit::sound_index);
  return found != sound_edits_.end() && found->sound_index == sound_index ? &*found : nullptr;
}

void MapEdit::SetSoundEdit(SoundEdit edit) {
  const auto place =
      std::ranges::lower_bound(sound_edits_, edit.sound_index, {}, &SoundEdit::sound_index);
  if (place != sound_edits_.end() && place->sound_index == edit.sound_index) {
    *place = std::move(edit);
  } else {
    sound_edits_.insert(place, std::move(edit));
  }
}

void MapEdit::RemoveSoundEdit(std::size_t sound_index) {
  std::erase_if(sound_edits_,
                [sound_index](const SoundEdit& edit) { return edit.sound_index == sound_index; });
}

}  // namespace ro::mapedit
