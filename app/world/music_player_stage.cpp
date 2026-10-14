#include "world/music_player_stage.h"

#include "ro/core/log.h"

namespace moonclient {
namespace {

// Le rang du morceau `game_path`, ou aucun.
[[nodiscard]] int RowOf(const std::vector<MusicTrack>& tracks, std::string_view game_path) {
  if (game_path.empty()) {
    return ro::ui::kMusicPlayerNoRow;
  }
  for (std::size_t index = 0; index < tracks.size(); ++index) {
    if (SameMusic(tracks[index].game_path, game_path)) {
      return static_cast<int>(index);
    }
  }
  return ro::ui::kMusicPlayerNoRow;
}

}  // namespace

void MusicPlayerStage::List(const MapMusic& music) {
  listed_ = true;
  tracks_ = ListMusicTracks(music.client_root(), music.table());
  joined_maps_.clear();
  rows_.clear();
  joined_maps_.reserve(tracks_.size());
  for (const MusicTrack& track : tracks_) {
    std::string joined;
    for (const std::string& map : track.maps) {
      joined += joined.empty() ? map : ", " + map;
    }
    joined_maps_.push_back(std::move(joined));
  }
  // ⚠ APRÈS LES CHAÎNES, ET UNE FOIS LEUR LISTE COMPLÈTE : une ligne désigne
  // ses textes, et un `push_back` qui réalloue les déplacerait sous elle.
  rows_.reserve(tracks_.size());
  for (std::size_t index = 0; index < tracks_.size(); ++index) {
    rows_.push_back(
        ro::ui::MusicPlayerRow{.label = tracks_[index].label, .maps = joined_maps_[index]});
  }
  ro::core::LogInfo("musique : {} morceau(x) au lecteur", tracks_.size());
}

void MusicPlayerStage::Advance(MusicPlayerWindows& windows, MapMusic& music,
                               ro::audio::AudioEngine& audio, std::string_view map_name) {
  const ro::ui::MusicPlayerAsk asked = windows.answer;
  windows.answer = {};
  if (asked.play >= 0 && static_cast<std::size_t>(asked.play) < tracks_.size()) {
    music.Choose(audio, tracks_[static_cast<std::size_t>(asked.play)].game_path);
  }
  if (asked.follow_map) {
    music.FollowMapAgain();
  }

  windows.composed = windows.window.open;
  if (!windows.composed) {
    return;
  }
  if (!listed_) {
    List(music);
  }
  windows.view =
      ro::ui::MusicPlayerView{.rows = rows_,
                              .playing = RowOf(tracks_, audio.music_name()),
                              .map_row = RowOf(tracks_, music.table().TrackFor(map_name)),
                              .map_name = map_name,
                              .chosen = !music.chosen().empty(),
                              .audible = audio.is_open() && audio.music_audible()};
}

}  // namespace moonclient
