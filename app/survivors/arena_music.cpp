#include "survivors/arena_music.h"

#include <cstdint>
#include <string_view>

namespace moonclient {

std::string_view ArenaTrackAt(std::uint64_t presentation_ms) noexcept {
  if (presentation_ms >= kArenaBossFromMs) {
    return kArenaBossTrack;
  }
  if (presentation_ms >= kArenaGlastHeimFromMs) {
    return kArenaGlastHeimTrack;
  }
  return kArenaFieldTrack;
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
  const std::string_view wanted = ArenaTrackAt(facts.presentation_ms);
  if (wanted != cued_) {
    cued_ = wanted;
    cue.start_track = wanted;
  }
  return cue;
}

}  // namespace moonclient
