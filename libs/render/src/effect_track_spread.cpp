#include "ro/render/effect_track_spread.h"

#include <algorithm>
#include <cstdlib>

#include "ro/render/effect_drifting_mote.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

[[nodiscard]] std::int32_t DrawBelow(std::uint32_t& rng_state, std::int32_t exclusive_bound) {
  return static_cast<std::int32_t>(NextEffectNodeDraw(rng_state) %
                                   static_cast<std::uint32_t>(exclusive_bound));
}

}  // namespace

// LA POURSUITE D'ANGLES, et TROIS familles la demandent : un
// courant, une cible, et un pas vers elle.
//
// ⚠ LE NOMBRE DE PAIRES EST LA SEULE DIFFÉRENCE entre les appelants — huit
// pour la rafale, deux pour le grain, six pour les trois plans. Le reste est
// le même code, et le recopier l'aurait fait diverger à la première
// correction.
void ChaseEffectTrackAngles(EffectTrack& track, std::size_t count, std::uint32_t& rng_state,
                            float descent, float rise) {
  for (std::size_t pair = 0; pair < count; ++pair) {
    float& current = track.value[2 * pair];
    float& target = track.value[2 * pair + 1];
    if (target <= current) {
      current = std::max(current - descent, 0.0F);
      if (target >= current) {
        // ⚠ LA CIBLE NEUVE SE TIRE SOUS LE COURANT ET SE COMPTE DEPUIS LE
        // HAUT : trois cent cinquante-neuf moins un tirage. Un courant nul ne
        // tire rien, et la cible remonte alors tout en haut.
        const auto ceiling = static_cast<std::int32_t>(current);
        target = static_cast<float>(static_cast<std::int32_t>(kQuadBurstAngleCeiling) -
                                    (ceiling != 0 ? DrawBelow(rng_state, ceiling) : 0));
      }
      continue;
    }
    current = std::min(current + rise, kQuadBurstAngleCeiling);
    if (current > target) {
      const auto ceiling = static_cast<std::int32_t>(current);
      target = static_cast<float>(ceiling != 0 ? DrawBelow(rng_state, ceiling) : ceiling);
    }
  }
}

namespace {

// LA DISTANCE QUE LA MARQUE QUINZE MESURE pour savoir si elle s'est assez
// écartée : son abscisse, ou sa profondeur quand l'abscisse est trop courte.
[[nodiscard]] std::int32_t MeasuredOffset(const EffectQuadBurstOffset& offset) {
  const auto de_cote = std::abs(static_cast<std::int32_t>(offset.sideways));
  if (de_cote < kQuadBurstFadeTriggerDistance) {
    return std::abs(static_cast<std::int32_t>(offset.forward));
  }
  return de_cote;
}

void AdvanceParticle(EffectTrack& track, std::size_t particle, std::uint32_t& rng_state) {
  const EffectQuadBurstDrift& reglage = kQuadBurstDrifts[particle];
  EffectQuadBurstOffset offset = EffectQuadBurstOffsetOf(track, particle);
  // ⚠ CHAQUE PARTICULE LIT SES DEUX ANGLES DANS LES CASES 4p ET 4p+2 : ce sont
  // les COURANTS de deux paires voisines, et jamais leurs cibles.
  const float de_cote = SinOfDegrees(static_cast<std::int32_t>(track.value[4 * particle]));
  const float forward = SinOfDegrees(static_cast<std::int32_t>(track.value[4 * particle + 2]));
  const std::uint8_t mark = track.mark[0];

  if (mark == kQuadBurstMarkRising) {
    if (offset.depth_below > kQuadBurstRisingFloor) {
      offset.sideways += reglage.rising_sideways * de_cote * kQuadBurstFadingDrift;
      offset.forward += reglage.rising_forward * forward * kQuadBurstFadingDrift;
    }
    // ⚠ ELLE RENAÎT EN HAUT SANS ÉCART : le client remet les deux à zéro et la
    // repose au plancher. Lui laisser son écart la ferait dériver sans fin.
    if (offset.depth_below > 0.0F) {
      offset.sideways = 0.0F;
      offset.forward = 0.0F;
      offset.depth_below = kQuadBurstRisingFloor;
    }
    offset.depth_below += track.lift_key;
  } else if (mark == kQuadBurstMarkScattering) {
    // ⚠ CELLE-CI NE DÉRIVE PAS DU TOUT : elle monte, et se REPOSE ailleurs à
    // chaque tour. C'est la seule dont l'écart est tiré au sort en vol.
    if (offset.depth_below > 0.0F) {
      offset.forward = 0.0F;
      offset.sideways = static_cast<float>(DrawBelow(rng_state, kQuadBurstWideScatterSpread) -
                                           kQuadBurstWideScatterBias);
      offset.depth_below = kQuadBurstRisingFloor;
    }
    offset.depth_below += track.lift_key;
  } else if (mark == kQuadBurstMarkFading) {
    // ⚠ CELLE-CI NE MONTE NI NE DESCEND : sa hauteur ne bouge jamais, et c'est
    // sa MARQUE qui l'éteint. La faire monter la sortirait de son cadre.
    offset.sideways += reglage.drift_sideways * de_cote * kQuadBurstFadingDrift;
    offset.forward += reglage.drift_forward * forward * kQuadBurstFadingDrift;
    if (MeasuredOffset(offset) > kQuadBurstFadeTriggerDistance) {
      track.mark[particle + 1] =
          static_cast<std::uint8_t>(track.mark[particle + 1] + kQuadBurstFadeStep);
      if (track.mark[particle + 1] >= kQuadBurstFadeCeiling) {
        track.mark[particle + 1] = 0;
        offset.sideways = 0.0F;
        offset.forward = 0.0F;
      }
    }
  } else {
    if (offset.depth_below < 0.0F) {
      offset.sideways += reglage.drift_sideways * de_cote * kQuadBurstOrdinaryDrift;
      offset.forward += reglage.drift_forward * forward * kQuadBurstOrdinaryDrift;
      const float seuil =
          mark == kQuadBurstMarkGrey ? reglage.grey_rebirth_floor : kQuadBurstRebirthFloor;
      if (offset.depth_below < seuil) {
        // ⚠ SEULE LA MARQUE ONZE RENAÎT ÉCARTÉE : les autres repartent de
        // l'axe. Leur donner le même écart brouillerait la colonne que les
        // autres marques forment.
        if (mark == kQuadBurstMarkGrey) {
          offset.sideways = static_cast<float>(DrawBelow(rng_state, kQuadBurstGreyScatterSpread) -
                                               kQuadBurstGreyScatterBias);
          offset.forward = static_cast<float>(DrawBelow(rng_state, kQuadBurstGreyScatterSpread) -
                                              kQuadBurstGreyScatterBias);
        } else {
          offset.sideways = 0.0F;
          offset.forward = 0.0F;
        }
        offset.depth_below =
            static_cast<float>(DrawBelow(rng_state, kQuadBurstRebirthHeightSpread));
      }
    }
    offset.depth_below -= track.lift_key;
  }
  SetEffectQuadBurstOffset(track, particle, offset);
}

}  // namespace

void AdvanceQuadBurstTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                            std::int16_t camera_turn_degrees, std::int16_t camera_tilt_degrees,
                            std::uint32_t& generator) {
  // ⚠ UNE SEULE FOIS PAR TIC, ET NON PAR PISTE : le client pose les deux
  // angles avant sa boucle. Les poser par piste donnerait le même résultat
  // aujourd'hui et cesserait de le donner le jour où une piste les lirait.
  StoreEffectBillboardAngles(tracks, camera_turn_degrees, camera_tilt_degrees);

  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ChaseEffectTrackAngles(track, kQuadBurstAnglePairs, generator);
    for (std::size_t particle = 0; particle < kQuadBurstParticles; ++particle) {
      AdvanceParticle(track, particle, generator);
    }
  }
}

void AdvanceDriftingMoteTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                               std::int16_t camera_turn_degrees, std::int16_t camera_tilt_degrees,
                               std::uint32_t& generator) {
  // ⚠ LES DEUX ANGLES DE CAMERA SE RANGENT COMME CEUX DE LA RAFALE, une seule
  // fois par tic et non par piste.
  StoreEffectBillboardAngles(tracks, camera_turn_degrees, camera_tilt_degrees);

  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    // ⚠ TANT QUE LE COMPTEUR EST NEGATIF, LA PISTE NE FAIT RIEN : c'est le
    // retard que le batisseur a tire.
    if (track.ticks <= 0) {
      continue;
    }
    const std::int16_t step = track.ticks >= kDriftingMoteFadesAfterTick
                                  ? static_cast<std::int16_t>(-kDriftingMoteAlphaFall)
                                  : kDriftingMoteAlphaRise;
    track.alpha = static_cast<std::int16_t>(track.alpha + step);
    if (track.alpha < 0) {
      track.alpha = 0;
    }

    ChaseEffectTrackAngles(track, kDriftingMoteAnglePairs, generator);

    // ⚠ IL NE DERIVE QUE TANT QU'IL EST AU-DESSUS DU NOEUD ; passe dessous il
    // continue de monter tout droit. Le laisser deriver l'eloignerait sans
    // fin.
    if (track.tail[kDriftingMoteSlotOfDepthBelow] < 0.0F) {
      track.tail[kDriftingMoteSlotOfSideways] +=
          SinOfDegrees(static_cast<std::int32_t>(track.value[0])) * kDriftingMoteDrift;
      track.tail[kDriftingMoteSlotOfForward] +=
          SinOfDegrees(static_cast<std::int32_t>(track.value[2])) * kDriftingMoteDrift;
    }
    track.tail[kDriftingMoteSlotOfDepthBelow] -= track.lift_key;
  }
}

}  // namespace ro::render
