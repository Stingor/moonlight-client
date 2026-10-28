#include "survivors/arena_music.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace moonclient {

std::string_view ArenaTrackAt(std::uint64_t presentation_ms,
                              std::string_view field_track) noexcept {
  if (presentation_ms >= kArenaBossFromMs) {
    return kArenaBossTrack;
  }
  if (presentation_ms >= kArenaGlastHeimFromMs) {
    return kArenaGlastHeimTrack;
  }
  return field_track;
}

ArenaMusicCue ArenaMusic::Advance(const ArenaMusicFacts& facts) noexcept {
  ArenaMusicCue cue;
  cue.volume = facts.paused ? facts.music_volume * kArenaPausedMusicShare : facts.music_volume;
  // LE MORCEAU DU JOUEUR A PRIS LA PLACE : la piste de l'arène ne joue plus,
  // et le retour à la main de la carte relance celle du moment.
  if (!facts.player_track.empty()) {
    cued_ = {};
    return cue;
  }
  const std::string_view wanted = ArenaTrackAt(facts.presentation_ms, facts.field_track);
  if (wanted != cued_) {
    cued_ = wanted;
    cue.start_track = wanted;
  }
  return cue;
}

std::optional<float> ArenaMusicVolume::Pose(float cue_volume) noexcept {
  if (posed_ == cue_volume) return std::nullopt;
  posed_ = cue_volume;
  return cue_volume;
}

}  // namespace moonclient
