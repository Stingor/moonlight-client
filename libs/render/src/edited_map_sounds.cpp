#include "ro/render/edited_map_sounds.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "ro/render/edited_map_effects.h"
#include "ro/vfs/encoding.h"

namespace ro::render {
namespace {

// Le nom d'une source que le document ajoute : elle ne vient d'aucun RSW.
constexpr std::string_view kAddedSoundName = "map_edit";

}  // namespace

std::vector<formats::RswSound> ActiveMapSounds(std::span<const formats::RswSound> map_sounds,
                                               const mapedit::MapEdit& document, float night,
                                               std::optional<float> hour) {
  std::vector<formats::RswSound> sounds;
  sounds.reserve(map_sounds.size() + document.sounds().size());
  for (std::size_t rank = 0; rank < map_sounds.size(); ++rank) {
    formats::RswSound sound = map_sounds[rank];
    const mapedit::SoundEdit* edit = document.SoundEditAt(rank);
    if (edit != nullptr &&
        mapedit::SoundEditTargets(*edit, vfs::Cp949ToUtf8Lossy(sound.wave_name), sound.position)) {
      if (edit->hidden || (edit->night_only && !LitAtNight(edit->hours, night, hour))) {
        continue;
      }
      sound.position = edit->position.value_or(sound.position);
      sound.volume = edit->volume.value_or(sound.volume);
      sound.range = edit->range.value_or(sound.range);
      sound.cycle_seconds = edit->cycle_seconds.value_or(sound.cycle_seconds);
    }
    sounds.push_back(std::move(sound));
  }
  for (const mapedit::AddedSound& added : document.sounds()) {
    if (added.night_only && !LitAtNight(added.hours, night, hour)) {
      continue;
    }
    auto raw = vfs::Utf8ToCp949(added.wave_file);
    sounds.push_back(formats::RswSound{.name = std::string(kAddedSoundName),
                                       .wave_name = raw ? std::move(*raw) : added.wave_file,
                                       .position = added.position,
                                       .volume = added.volume,
                                       .width = added.width,
                                       .height = added.height,
                                       .range = added.range,
                                       .cycle_seconds = added.cycle_seconds});
  }
  return sounds;
}

bool SameMapSounds(std::span<const formats::RswSound> left,
                   std::span<const formats::RswSound> right) {
  return std::ranges::equal(
      left, right, [](const formats::RswSound& one, const formats::RswSound& other) {
        return one.name == other.name && one.wave_name == other.wave_name &&
               one.position == other.position && one.volume == other.volume &&
               one.width == other.width && one.height == other.height && one.range == other.range &&
               one.cycle_seconds == other.cycle_seconds;
      });
}

}  // namespace ro::render
