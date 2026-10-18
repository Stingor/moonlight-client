// LES SONS D'AMBIANCE DANS L'ÉDITEUR : les sources de son du RSW, listées et
// retouchées — déplacées, masquées, leur volume, leur portée ou leur cycle
// changés, réservées à la nuit —, et les sons ajoutés, un `.wav` de
// l'installation posé où regarde la caméra. Voir docs/adr/0118.
//
// Une retouche désigne sa source par son rang, son fichier et sa position
// d'origine ; elle disparaît quand elle ne change plus rien. Chaque geste
// compte dans l'historique, un glissé au relâcher seulement.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/vfs/encoding.h"

#include "world/map_editor_reasons.h"
#include "world/map_editor_session.h"

namespace moonclient {
namespace {

// La place d'un nom de geste, zéro final compris.
constexpr std::size_t kLabelCapacity = 192;
using Label = std::array<char, kLabelCapacity>;
// Une copie se pose à côté de l'original : deux carreaux du sol.
constexpr float kDuplicateShift = 20.0F;
// En deçà, une position est tenue pour celle d'origine.
constexpr float kBackHome = ro::mapedit::kOriginTolerance;
// La plus petite portée et le plus court cycle que garde un son : le document
// refuse une portée ou un cycle nuls.
constexpr float kSmallestRange = 1.0F;
constexpr float kShortestCycleSeconds = 0.1F;

bool SamePlace(const std::array<float, 3>& left, const std::array<float, 3>& right) {
  for (std::size_t axis = 0; axis < left.size(); ++axis) {
    if (std::abs(left[axis] - right[axis]) > kBackHome) {
      return false;
    }
  }
  return true;
}

// Une grandeur retouchée, ou rien quand elle reste celle du RSW.
std::optional<float> Changed(float wanted, float original) {
  return wanted == original ? std::nullopt : std::optional<float>(wanted);
}

ro::ui::MapEditorSound ShownAdded(std::size_t slot, const ro::mapedit::AddedSound& sound) {
  return ro::ui::MapEditorSound{.key = {.added = true, .index = slot},
                                .file = sound.wave_file,
                                .origin = sound.position,
                                .position = sound.position,
                                .volume = sound.volume,
                                .width = sound.width,
                                .height = sound.height,
                                .range = sound.range,
                                .cycle_seconds = sound.cycle_seconds,
                                .edited = true,
                                .hidden = false,
                                .night_only = sound.night_only,
                                .hours = ShownHours(sound.hours),
                                .why = sound.why};
}

ro::mapedit::AddedSound DocumentAdded(const ro::ui::MapEditorSound& shown) {
  return ro::mapedit::AddedSound{
      .wave_file = shown.file,
      .position = shown.position,
      .volume =
          std::clamp(shown.volume, ro::mapedit::kSoundVolumeLeast, ro::mapedit::kSoundVolumeMost),
      .width = std::max(shown.width, 0),
      .height = std::max(shown.height, 0),
      .range = std::max(shown.range, kSmallestRange),
      .cycle_seconds = std::max(shown.cycle_seconds, kShortestCycleSeconds),
      .night_only = shown.night_only,
      .hours = DocumentHours(shown.hours),
      .why = shown.why};
}

}  // namespace

void MapEditorSession::FollowMapSounds(std::string_view map_name,
                                       const ro::render::MapScene& scene) {
  const std::vector<ro::formats::RswSound>& sounds = scene.rsw.sounds();
  if (map_name == sound_files_map_ && sound_files_.size() == sounds.size()) {
    return;
  }
  if (map_name != sound_files_map_) {
    selected_sound_.reset();
    sound_dots_.clear();
    sound_files_map_ = std::string(map_name);
  }
  sound_files_.clear();
  sound_files_.reserve(sounds.size());
  for (const ro::formats::RswSound& sound : sounds) {
    sound_files_.push_back(ro::vfs::Cp949ToUtf8Lossy(sound.wave_name));
  }
}

void MapEditorSession::ViewSounds(const ro::render::MapScene& scene,
                                  const ro::mapedit::MapEdit& document,
                                  ro::ui::MapEditorView& view) const {
  const std::vector<ro::formats::RswSound>& sounds = scene.rsw.sounds();
  view.sounds.reserve(sounds.size() + document.sounds().size());
  for (std::size_t index = 0; index < sounds.size(); ++index) {
    const ro::formats::RswSound& sound = sounds[index];
    ro::ui::MapEditorSound row{.key = {.added = false, .index = index},
                               .file = index < sound_files_.size()
                                           ? sound_files_[index]
                                           : ro::vfs::Cp949ToUtf8Lossy(sound.wave_name),
                               .origin = sound.position,
                               .position = sound.position,
                               .volume = sound.volume,
                               .width = sound.width,
                               .height = sound.height,
                               .range = sound.range,
                               .cycle_seconds = sound.cycle_seconds};
    const ro::mapedit::SoundEdit* retouch = document.SoundEditAt(index);
    if (retouch != nullptr && ro::mapedit::SoundEditTargets(*retouch, row.file, sound.position)) {
      row.edited = true;
      row.position = retouch->position.value_or(row.origin);
      row.volume = retouch->volume.value_or(row.volume);
      row.range = retouch->range.value_or(row.range);
      row.cycle_seconds = retouch->cycle_seconds.value_or(row.cycle_seconds);
      row.hidden = retouch->hidden;
      row.night_only = retouch->night_only;
      row.hours = ShownHours(retouch->hours);
      row.why = retouch->why;
    }
    view.sounds.push_back(std::move(row));
  }
  for (std::size_t slot = 0; slot < document.sounds().size(); ++slot) {
    view.sounds.push_back(ShownAdded(slot, document.sounds()[slot]));
  }
  if (selected_sound_.has_value() &&
      std::ranges::any_of(view.sounds, [this](const ro::ui::MapEditorSound& row) {
        return row.key == *selected_sound_;
      })) {
    view.selected_sound = selected_sound_;
  }
}

std::vector<ro::ui::MapEditorLine> MapEditorSession::SoundTrace() const {
  std::vector<ro::ui::MapEditorLine> marks;
  marks.reserve(sound_dots_.size());
  for (const SoundDot& dot : sound_dots_) {
    marks.push_back(ro::ui::MapEditorLine{.from = dot.at,
                                          .to = dot.at,
                                          .kind = selected_sound_ == dot.key
                                                      ? ro::ui::MapEditorLineKind::kSoundSelected
                                                      : ro::ui::MapEditorLineKind::kSound});
  }
  return marks;
}

void MapEditorSession::ChooseSound(std::optional<ro::ui::MapEditorSoundKey> key) {
  selected_sound_ = key;
  if (key.has_value()) {
    selection_.Clear();
    vertex_.reset();
    selected_light_.reset();
    selected_effect_.reset();
    selected_map_effect_.reset();
    selected_sprite_.reset();
  }
}

bool MapEditorSession::GrabSound(glm::vec2 pointer) {
  float nearest = kGizmoGrabPixels;
  std::optional<ro::ui::MapEditorSoundKey> picked;
  for (const SoundDot& dot : sound_dots_) {
    const float distance = glm::length(dot.at - pointer);
    if (distance <= nearest) {
      nearest = distance;
      picked = dot.key;
    }
  }
  if (!picked.has_value()) {
    return false;
  }
  ChooseSound(picked);
  return true;
}

bool MapEditorSession::GrabSoundHandle(glm::vec2 pointer, const ro::ui::MapEditorView& view) {
  if (!view.selected_sound.has_value()) {
    return false;
  }
  const auto shown =
      std::ranges::find(view.sounds, *view.selected_sound, &ro::ui::MapEditorSound::key);
  if (shown == view.sounds.end()) {
    return false;
  }
  const std::optional<std::size_t> grabbed = GrabbedHandle(handles_, pointer);
  if (!grabbed.has_value()) {
    return false;
  }
  const ro::ui::map_editor::Place start{.position = shown->position, .released = false};
  drag_ = Drag{.handle = *grabbed,
               .grabbed = handles_[*grabbed],
               .frame = gizmo_frame_,
               .from = pointer,
               .start = start,
               .last = start,
               .sound = view.selected_sound};
  return true;
}

MapEditorSession::Outcome MapEditorSession::ApplySound(
    const ro::ui::map_editor::SelectSound& select, const ro::render::MapScene& /*scene*/,
    MapEditStore& /*store*/) {
  ChooseSound(select.key);
  return {};
}

MapEditorSession::Outcome MapEditorSession::ApplySound(const ro::ui::map_editor::SetSound& set,
                                                       const ro::render::MapScene& scene,
                                                       MapEditStore& store) {
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  const ro::ui::MapEditorSound& shown = set.sound;
  ro::mapedit::MapEdit after = before;
  Label label{};
  if (shown.key.added) {
    if (shown.key.index >= before.sounds().size()) {
      return {};
    }
    after.SetSound(shown.key.index, DocumentAdded(shown));
    std::snprintf(label.data(), label.size(), ro::core::Tr("Régler le son ajouté %zu"),
                  shown.key.index);
  } else {
    const std::vector<ro::formats::RswSound>& sounds = scene.rsw.sounds();
    if (shown.key.index >= sounds.size()) {
      return {};
    }
    const ro::formats::RswSound& sound = sounds[shown.key.index];
    ro::mapedit::SoundEdit retouch{
        .sound_index = shown.key.index,
        .wave_file = shown.key.index < sound_files_.size()
                         ? sound_files_[shown.key.index]
                         : ro::vfs::Cp949ToUtf8Lossy(sound.wave_name),
        .origin = sound.position,
        .position = SamePlace(shown.position, sound.position)
                        ? std::nullopt
                        : std::optional<std::array<float, 3>>(shown.position),
        .hidden = shown.hidden,
        .volume = Changed(
            std::clamp(shown.volume, ro::mapedit::kSoundVolumeLeast, ro::mapedit::kSoundVolumeMost),
            sound.volume),
        .range = Changed(std::max(shown.range, kSmallestRange), sound.range),
        .cycle_seconds =
            Changed(std::max(shown.cycle_seconds, kShortestCycleSeconds), sound.cycle_seconds),
        .night_only = shown.night_only,
        .hours = DocumentHours(shown.hours),
        .why = shown.why};
    // Une raison seule ne garde pas la retouche : voir `WithReasons`.
    const bool changes_nothing = !retouch.position.has_value() && !retouch.hidden &&
                                 !retouch.volume.has_value() && !retouch.range.has_value() &&
                                 !retouch.cycle_seconds.has_value() && !retouch.night_only;
    if (changes_nothing) {
      after.RemoveSoundEdit(shown.key.index);
    } else {
      after.SetSoundEdit(std::move(retouch));
    }
    std::snprintf(label.data(), label.size(), ro::core::Tr("Retoucher le son %zu"),
                  shown.key.index);
  }
  if (!gesture_before_.has_value()) {
    gesture_before_ = before;
  }
  if (!(after == before)) {
    store.Set(map_name_, WithReasons(after));
  }
  if (set.released) {
    Record(label.data(), *gesture_before_, store.Document(map_name_));
    gesture_before_.reset();
  }
  return {};
}

MapEditorSession::Outcome MapEditorSession::ApplySound(const ro::ui::map_editor::MoveSound& move,
                                                       const ro::render::MapScene& scene,
                                                       MapEditStore& store) {
  ro::ui::MapEditorView view;
  ViewSounds(scene, store.Document(map_name_), view);
  const auto shown = std::ranges::find(view.sounds, move.key, &ro::ui::MapEditorSound::key);
  if (shown == view.sounds.end()) {
    return {};
  }
  ro::ui::map_editor::SetSound set{.sound = *shown, .released = move.released};
  set.sound.position = move.native;
  return ApplySound(set, scene, store);
}

MapEditorSession::Outcome MapEditorSession::ApplySound(
    const ro::ui::map_editor::RevertSound& revert, const ro::render::MapScene& /*scene*/,
    MapEditStore& store) {
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  ro::mapedit::MapEdit after = before;
  Label label{};
  if (revert.key.added) {
    // UN SON AJOUTÉ SE RETIRE : il n'a pas d'origine où revenir.
    if (revert.key.index >= before.sounds().size()) {
      return {};
    }
    after.RemoveSound(revert.key.index);
    if (selected_sound_ == revert.key) {
      selected_sound_.reset();
    }
    std::snprintf(label.data(), label.size(), ro::core::Tr("Retirer le son %zu"), revert.key.index);
  } else {
    if (before.SoundEditAt(revert.key.index) == nullptr) {
      return {};
    }
    after.RemoveSoundEdit(revert.key.index);
    std::snprintf(label.data(), label.size(), ro::core::Tr("Rétablir le son %zu"),
                  revert.key.index);
  }
  store.Set(map_name_, WithReasons(after));
  Record(label.data(), before, after);
  return {};
}

MapEditorSession::Outcome MapEditorSession::ApplySound(const ro::ui::map_editor::AddSound& add,
                                                       const ro::render::MapScene& /*scene*/,
                                                       MapEditStore& store) {
  if (add.file.empty()) {
    return {};
  }
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  ro::mapedit::MapEdit after = before;
  after.AddSound(ro::mapedit::AddedSound{.wave_file = add.file, .position = drop_point_});
  store.Set(map_name_, WithReasons(after));
  ChooseSound(ro::ui::MapEditorSoundKey{.added = true, .index = after.sounds().size() - 1});
  Record(ro::core::Tr("Ajouter un son"), before, after);
  return {};
}

MapEditorSession::Outcome MapEditorSession::ApplySound(
    const ro::ui::map_editor::ListenSound& listen, const ro::render::MapScene& /*scene*/,
    MapEditStore& /*store*/) {
  return {.listen = listen};
}

std::optional<MapEditorSession::Outcome> MapEditorSession::NudgeSound(
    const std::array<float, 3>& delta, const ro::render::MapScene& scene, MapEditStore& store) {
  const std::optional<std::array<float, 3>> position = SoundFocus(scene, store);
  if (!position.has_value()) {
    return std::nullopt;
  }
  std::array<float, 3> pushed = *position;
  for (std::size_t axis = 0; axis < pushed.size(); ++axis) {
    pushed[axis] += delta[axis];
  }
  return ApplySound(
      ro::ui::map_editor::MoveSound{.key = *selected_sound_, .native = pushed, .released = true},
      scene, store);
}

std::optional<MapEditorSession::Outcome> MapEditorSession::DuplicateSound(
    const ro::render::MapScene& scene, MapEditStore& store) {
  if (!selected_sound_.has_value()) {
    return std::nullopt;
  }
  ro::ui::MapEditorView view;
  ViewSounds(scene, store.Document(map_name_), view);
  const auto shown = std::ranges::find(view.sounds, *selected_sound_, &ro::ui::MapEditorSound::key);
  if (shown == view.sounds.end()) {
    return Outcome{};
  }
  // LA COPIE EST UN SON AJOUTÉ, même d'une source du RSW : elle garde son
  // fichier, ses réglages et sa nuit, à côté d'elle.
  ro::mapedit::AddedSound copy = DocumentAdded(*shown);
  copy.position[0] += kDuplicateShift;
  copy.why.clear();
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  ro::mapedit::MapEdit after = before;
  after.AddSound(std::move(copy));
  store.Set(map_name_, WithReasons(after));
  ChooseSound(ro::ui::MapEditorSoundKey{.added = true, .index = after.sounds().size() - 1});
  Record(ro::core::Tr("Ajouter un son"), before, after);
  return Outcome{};
}

std::optional<MapEditorSession::Outcome> MapEditorSession::DeleteSound(
    const ro::render::MapScene& scene, MapEditStore& store) {
  if (!selected_sound_.has_value()) {
    return std::nullopt;
  }
  // UNE SOURCE DU RSW ne se retire pas : elle se tait, ou reprend.
  if (selected_sound_->added) {
    return ApplySound(ro::ui::map_editor::RevertSound{*selected_sound_}, scene, store);
  }
  ro::ui::MapEditorView view;
  ViewSounds(scene, store.Document(map_name_), view);
  const auto shown = std::ranges::find(view.sounds, *selected_sound_, &ro::ui::MapEditorSound::key);
  if (shown == view.sounds.end()) {
    return Outcome{};
  }
  ro::ui::map_editor::SetSound set{.sound = *shown, .released = true};
  set.sound.hidden = !set.sound.hidden;
  return ApplySound(set, scene, store);
}

std::optional<std::array<float, 3>> MapEditorSession::SoundFocus(const ro::render::MapScene& scene,
                                                                 const MapEditStore& store) const {
  if (!selected_sound_.has_value()) {
    return std::nullopt;
  }
  ro::ui::MapEditorView view;
  ViewSounds(scene, store.Document(map_name_), view);
  const auto shown = std::ranges::find(view.sounds, *selected_sound_, &ro::ui::MapEditorSound::key);
  if (shown == view.sounds.end()) {
    return std::nullopt;
  }
  return shown->position;
}

}  // namespace moonclient
