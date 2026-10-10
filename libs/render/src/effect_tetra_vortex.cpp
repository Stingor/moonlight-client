#include "ro/render/effect_tetra_vortex.h"

#include <algorithm>
#include <cstddef>

#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LA MARQUE QUE LE MOUVEMENT AVANCE et que la figure lit comme l'image à peindre.
constexpr std::size_t kMarkOfFrame = 0;

// LES DEUX PISTES qui reçoivent les angles de panneau.
constexpr std::size_t kTrackThatTakesTheTurn = 0;
constexpr std::size_t kTrackThatTakesTheTilt = 1;

}  // namespace

void AdvanceTetraVortexTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                              std::int16_t billboard_turn_degrees,
                              std::int16_t billboard_tilt_degrees, std::int32_t frame_count) {
  tracks[kTrackThatTakesTheTurn].style = billboard_turn_degrees;
  tracks[kTrackThatTakesTheTilt].style = billboard_tilt_degrees;

  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    if (track.ticks % kTetraVortexTicksPerFrame == 0) {
      // ⚠ L'OCTET EST SIGNÉ, comme chez le client.
      const auto image = static_cast<std::int8_t>(track.mark[kMarkOfFrame] + 1);
      track.mark[kMarkOfFrame] = static_cast<std::uint8_t>(image);
      if (image >= frame_count) {
        track.mark[kMarkOfFrame] = static_cast<std::uint8_t>(frame_count - 1);
        track.alpha = static_cast<std::int16_t>(track.alpha - kTetraVortexFadePerStep);
      }
    }
    track.alpha = std::clamp<std::int16_t>(track.alpha, 0, kEffectTrackAlphaMax);
  }
}

}  // namespace ro::render
