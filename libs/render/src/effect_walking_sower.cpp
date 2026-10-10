#include "ro/render/effect_walking_sower.h"

#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

constexpr std::size_t kTrackThatSows = 0;

}  // namespace

void AdvanceWalkingSowerTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                               std::int16_t client_heading_to_target) {
  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    if (track.ticks <= 0) {
      continue;
    }
    if (track.ticks == kWalkingSowerHeadingTick) {
      track.degrees = client_heading_to_target;
    }
    // ⚠ LE CLIENT AVANCE SA PROFONDEUR DU SINUS, dans un repère où la nôtre
    // est opposée : elle se retranche ici.
    const float step = track.value[kWalkingSowerSlotOfSpeed];
    track.anchor.x += step * CosOfDegrees(track.degrees);
    track.anchor.z -= step * SinOfDegrees(track.degrees);
  }
}

std::optional<std::uint32_t> WalkingSowerChildOf(
    const std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  const EffectTrack& track = tracks[kTrackThatSows];
  if (!track.active) {
    return std::nullopt;
  }
  const bool on_ring_period = track.ticks % kWalkingSowerRingPeriodTicks == 0;
  switch (track.mark[kWalkingSowerMarkOfByte]) {
    case kWalkingSowerByteOfRings:
      return on_ring_period ? std::optional<std::uint32_t>(kWalkingSowerChildOfRings)
                            : std::nullopt;
    case kWalkingSowerByteOfSparks:
      return kWalkingSowerChildOfSparks;
    case kWalkingSowerByteOfBlueRings:
      return on_ring_period ? std::optional<std::uint32_t>(kWalkingSowerChildOfBlueRings)
                            : std::nullopt;
    default:
      // Un autre octet ne fait rien naître : aucun site n'en passe.
      return std::nullopt;
  }
}

}  // namespace ro::render
