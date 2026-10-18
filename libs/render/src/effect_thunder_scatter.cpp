#include "ro/render/effect_thunder_scatter.h"

#include "ro/render/effect_node.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LES DEUX PISTES qui portent les angles de caméra, que le carré teinté relit
// à des positions FIXES du nœud.
constexpr std::size_t kTrackThatTakesTheTurn = 0;
constexpr std::size_t kTrackThatTakesTheTilt = 1;

}  // namespace

void AdvanceThunderScatterTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                 glm::vec3 node_position, glm::vec3 aim_point,
                                 std::int16_t billboard_turn_degrees,
                                 std::int16_t billboard_tilt_degrees, std::uint32_t& generator) {
  tracks[kTrackThatTakesTheTurn].style = billboard_turn_degrees;
  tracks[kTrackThatTakesTheTilt].style = billboard_tilt_degrees;

  for (EffectTrack& track : tracks) {
    // L'allumage ; une piste éteinte ne compte plus.
    if (!track.active) {
      continue;
    }
    // ⚠ LES BRANCHES SE JUGENT SUR LE COMPTEUR D'AVANT :
    // c'est lui qui vaut zéro au tic du cap, et dix au premier tic qui pâlit.
    const std::int32_t before = track.ticks;
    ++track.ticks;
    if (track.ticks <= 0) {
      continue;
    }
    track.degrees = static_cast<std::int16_t>(
        WithinFullTurn(static_cast<std::int32_t>(track.degrees) + kThunderScatterSpinPerTick));

    if (before == 0) {
      const std::int32_t offset =
          static_cast<std::int32_t>(NextEffectNodeDraw(generator) %
                                    static_cast<std::uint32_t>(kThunderScatterHeadingDraws)) -
          kThunderScatterHeadingBias;
      const std::int32_t cap_client =
          WithinFullTurn(ClientHeadingToward(node_position, aim_point) + offset);
      track.azimuth_degrees = static_cast<std::int16_t>(WithinFullTurn(-cap_client));
    } else if (before < kThunderScatterFirstFadingTick) {
      track.alpha = static_cast<std::int16_t>(track.alpha + kThunderScatterAlphaRise);
    } else {
      track.alpha = static_cast<std::int16_t>(track.alpha - kThunderScatterAlphaFall);
      if (track.alpha < 0) {
        track.alpha = 0;
        track.active = false;
      }
    }

    // ⚠ LES TROIS BRANCHES REJOIGNENT LA MÊME SUITE, celle qui
    // éteint comprise : l'ancre avance même au tic où la piste meurt.
    track.anchor.y -= track.value[kThunderScatterSlotOfRise];
    const float speed = track.value[kThunderScatterSlotOfSpeed];
    track.anchor.x += speed * CosOfDegrees(track.azimuth_degrees);
    track.anchor.z += speed * SinOfDegrees(track.azimuth_degrees);
  }
}

}  // namespace ro::render
