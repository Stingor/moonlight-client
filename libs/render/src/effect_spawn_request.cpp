#include "ro/render/effect_spawn_request.h"

#include <limits>
#include <optional>

#include "ro/render/effect_arcing_light.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_spiral_emitter.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_walking_sower.h"

namespace ro::render {
namespace {

constexpr std::size_t kTrackThatTakesTheTurn = 0;
constexpr std::size_t kTrackThatTakesTheTilt = 1;
constexpr std::size_t kTrackThatSows = 0;

// LA PREMIÈRE VALEUR QU'UN ENTIER NON SIGNÉ DE TRENTE-DEUX BITS NE TIENT PLUS.
constexpr float kFirstUnsignedOverflow = 4294967296.0F;

}  // namespace

float StoredDesignatedActor(std::uint32_t actor_id) { return static_cast<float>(actor_id); }

std::uint32_t DesignatedActorId(float stored) {
  if (!(stored > 0.0F)) {
    return 0;
  }
  if (stored >= kFirstUnsignedOverflow) {
    return std::numeric_limits<std::uint32_t>::max();
  }
  return static_cast<std::uint32_t>(stored);
}

bool DesignatesPlayer(float stored, std::uint32_t player_id) {
  return static_cast<float>(player_id) == stored;
}

std::uint32_t DesignatedActorAfterStorage(std::uint32_t actor_id, std::uint32_t player_id) {
  const float stored = StoredDesignatedActor(actor_id);
  return DesignatesPlayer(stored, player_id) ? player_id : DesignatedActorId(stored);
}

bool EffectNodeShapeOnlySpawns(PooledNodeShape shape) {
  return shape == PooledNodeShape::kSpiralEmitter || shape == PooledNodeShape::kWalkingSower;
}

void CollectEffectSpawnRequests(const PooledEffectNode& pooled,
                                std::vector<EffectSpawnRequest>& out) {
  const std::int16_t tour = pooled.tracks[kTrackThatTakesTheTurn].style;
  const std::int16_t tilt = pooled.tracks[kTrackThatTakesTheTilt].style;
  switch (pooled.shape) {
    case PooledNodeShape::kArcingLight:
      // L'EFFET QUI SUIT LA LUEUR, sur l'acteur qu'elle désigne. ⚠ L'ÉCART QUE
      // LE CLIENT PASSE N'EST LU PAR PERSONNE : l'effet qui naît ne change que
      // l'acteur, et il reste nul ici.
      for (const EffectTrack& track : pooled.tracks) {
        if (ArcingLightSowsFollowUp(track, tour, tilt)) {
          out.push_back({kArcingLightFollowUpEffectId, EffectSpawnPlace::kOnDesignatedActor,
                         track.value[kArcingLightSlotOfDesignatedActor], glm::vec3(0.0F)});
        }
      }
      return;
    case PooledNodeShape::kSpiralEmitter:
      for (const EffectTrack& track : pooled.tracks) {
        if (const std::optional<std::uint32_t> enfant = SpiralEmitterChildOf(track, tour, tilt);
            enfant.has_value()) {
          out.push_back({*enfant, EffectSpawnPlace::kOnDesignatedActor,
                         track.value[kSpiralEmitterSlotOfDesignatedActor],
                         SpiralEmitterChildOffset(track)});
        }
      }
      return;
    case PooledNodeShape::kWalkingSower:
      if (const std::optional<std::uint32_t> enfant = WalkingSowerChildOf(pooled.tracks);
          enfant.has_value()) {
        out.push_back(
            {*enfant, EffectSpawnPlace::kAtPoint, 0.0F, pooled.tracks[kTrackThatSows].anchor});
      }
      return;
    default:
      // Les autres formes ne font naître aucun effet.
      return;
  }
}

}  // namespace ro::render
