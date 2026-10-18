#include "ro/render/effect_rainstorm_cylinder.h"

#include "ro/render/effect_cycling_skirt.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

void AdvanceOne(EffectTrack& track, std::size_t rank, std::int32_t life_ticks,
                bool effects_reduced) {
  if (!track.active) {
    return;
  }
  ++track.ticks;
  if (track.ticks <= 0) {
    return;
  }

  TurnOnward(track.azimuth_degrees, kRainstormCylinderTurnPerTick);

  if (track.ticks < kRainstormCylinderRiseTicks) {
    track.alpha = static_cast<std::int16_t>(track.alpha + kRainstormCylinderAlphaRisePerTick);
    const std::int16_t ceiling =
        effects_reduced ? kRainstormCylinderReducedAlphaCeiling : kRainstormCylinderAlphaCeiling;
    if (track.alpha > ceiling) {
      track.alpha = ceiling;
    }
  }

  if (track.ticks > life_ticks - kRainstormCylinderFadeTicks) {
    track.alpha = static_cast<std::int16_t>(track.alpha - kRainstormCylinderAlphaFallPerTick);
    if (track.alpha < 0) {
      track.alpha = 0;
    }
  }

  // L'ONDE, sur les VINGT ET UNE cases : la boucle court jusqu'à la fin de la
  // table des sinus, par pas de neuf degrés.
  const float le_long_du_temps = SinOfDegrees(UndulationPhaseDegrees(rank, track.ticks));
  const float key = track.lift_key;
  for (std::size_t segment = 0; segment < kEffectTrackSlots; ++segment) {
    const auto along_ribbon =
        static_cast<std::int32_t>(segment) * kEffectTrackUndulationStepDegrees;
    track.value[segment] = key + (key * kRainstormCylinderUndulationAmplitude *
                                  SinOfDegrees(along_ribbon) * le_long_du_temps);
  }
}

}  // namespace

std::vector<EffectWorldQuad> ComposeRainstormCylinderQuads(const EffectTrack& track,
                                                           std::uint8_t variant,
                                                           std::size_t texture_count,
                                                           glm::vec3 anchor) {
  // LA GARDE DU COMPTEUR, que la jupe n'a pas.
  if (track.ticks <= 0) {
    return {};
  }
  // LA CLÉ NEUTRALISÉE : ce dessinateur ne la lit pas, et la jupe à qui on la
  // prête en tirerait un soulèvement.
  EffectTrack without_key = track;
  without_key.lift_key = 0.0F;
  // ⚠ NOTRE REPÈRE MONTE, CELUI DU CLIENT DESCEND : ses cinq unités en ordonnée
  // positive sont cinq unités SOUS le nœud.
  const glm::vec3 abaisse = anchor - glm::vec3{0.0F, kRainstormCylinderDropBelowNode, 0.0F};
  return ComposeCyclingSkirtQuads(without_key, variant, texture_count, abaisse);
}

void AdvanceRainstormCylinderTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                    std::int32_t life_ticks, bool effects_reduced) {
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    AdvanceOne(tracks[rang], rang, life_ticks, effects_reduced);
  }
}

}  // namespace ro::render
