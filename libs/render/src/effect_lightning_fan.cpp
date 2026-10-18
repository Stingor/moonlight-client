#include "ro/render/effect_lightning_fan.h"

#include <algorithm>
#include <cstddef>

#include "ro/render/effect_track.h"

#include "effect_draw_below.h"
#include "effect_track_angles.h"

namespace ro::render {
namespace {

// CE QUE LE MOUVEMENT DU CAS 101 POSE.

// DE COMBIEN LE RAYON ENFLE par tic, et au-delà de quoi l'alpha se met à
// tomber.
constexpr float kRadiusGrowth = 0.4F;
constexpr float kRadiusBeforeFading = 15.0F;
constexpr std::int16_t kAlphaFall = 2;
// JUSQU'À QUEL TIC L'ALPHA MONTE, et de combien : cinq, plus un tirage sur
// quatre.
constexpr std::int32_t kAlphaRiseUntilTick = 25;
constexpr std::int32_t kAlphaRiseSpread = 4;
constexpr std::int16_t kAlphaRiseFloor = 5;

// L'ÉCRITURE DE L'HISTORIQUE : combien de tours elle fait, de combien la tête
// avance à chaque tour, et où elle bute.
constexpr std::int32_t kHistoryPasses = 21;
constexpr float kPhasePerPass = 9.0F;
constexpr float kPhaseCeiling = 180.0F;
// LA MOITIÉ DU DEMI-TOUR, dont la phase tire sa part de distance.
constexpr float kPhaseHalf = 90.0F;
// AU PREMIER TIC, ET SEULEMENT LÀ.
constexpr std::int32_t kHistoryTick = 1;

[[nodiscard]] float DrawJitter(std::uint32_t& rng_state) {
  return static_cast<float>(DrawBelow(rng_state, kLightningFanJitterSpread)) *
             kLightningFanJitterStep +
         kLightningFanJitterFloor;
}

}  // namespace

std::size_t LightningFanSegmentsOf(const EffectTrack& track) {
  const float count = track.tail[kLightningFanTailOfSegments];
  if (count <= 0.0F) {
    return 0;
  }
  return std::min(static_cast<std::size_t>(count), kEffectTrackSlots);
}

std::array<glm::vec3, kEffectTrackSlots> DrawLightningFanJitters(const EffectTrack& track,
                                                                 std::uint32_t& generator) {
  std::array<glm::vec3, kEffectTrackSlots> tremblements = {};
  const std::size_t segments = LightningFanSegmentsOf(track);
  for (std::size_t segment = 0; segment < segments; ++segment) {
    // ⚠ L'ORDRE EST : hauteur, abscisse, profondeur. Le client remplit ses trois
    // tableaux à rebours.
    tremblements[segment].y = DrawJitter(generator);
    tremblements[segment].x = DrawJitter(generator);
    tremblements[segment].z = DrawJitter(generator);
  }
  return tremblements;
}

void WriteLightningFanHistory(EffectTrack& track) {
  const std::size_t segments = LightningFanSegmentsOf(track);
  for (std::int32_t passe = 0; passe < kHistoryPasses; ++passe) {
    track.value[0] += kPhasePerPass;
    if (track.value[0] > kPhaseCeiling) {
      // ⚠ UNE FOIS LA TÊTE AU PLAFOND, LES TOURS RESTANTS NE DÉCALENT PLUS :
      // le client saute à la fin de sa boucle. Continuer à décaler viderait
      // l'historique qu'il vient d'écrire.
      track.value[0] = kPhaseCeiling;
      continue;
    }
    for (std::size_t rang = segments; rang > 0; --rang) {
      track.value[rang] = track.value[rang - 1];
      track.mark[rang] = track.mark[rang - 1];
    }
  }
}

glm::vec3 EffectLightningFanJoint(const EffectTrack& track, std::size_t segment, glm::vec3 jitter) {
  const float phase = track.value[segment];
  // ⚠ UNE PHASE NULLE NE PARCOURT RIEN : le client teste le signe avant de
  // diviser, et un segment sans phase reste sur l'ancre.
  const float parcourue = phase > 0.0F ? (phase / kPhaseHalf) * track.radius_at_anchor : 0.0F;
  const float bombement =
      -track.radius_at_anchor *
      SinOfDegrees(static_cast<std::int32_t>(static_cast<std::int16_t>(phase))) *
      track.tail[kLightningFanTailOfBow];
  return {CosOfDegrees(track.tilt_degrees) * parcourue + jitter.x,
          bombement + jitter.y + kLightningFanDrop,
          SinOfDegrees(track.tilt_degrees) * parcourue + jitter.z};
}

std::vector<EffectWorldQuad> ComposeLightningFanQuads(const EffectTrack& track,
                                                      std::uint32_t& generator,
                                                      const EffectOwnerTint& owner) {
  std::vector<EffectWorldQuad> quads;
  const std::size_t segments = LightningFanSegmentsOf(track);

  // ⚠ LE TREMBLEMENT SE TIRE AVANT TOUTE GARDE, et pour TOUS les segments. Le
  // client le fait en tête de son dessinateur, hors du test de piste vive : une
  // piste éteinte consomme donc ses tirages comme les autres. Les sauter
  // décalerait la suite du générateur pour les trois autres pistes du nœud.
  const std::array<glm::vec3, kEffectTrackSlots> tremblements =
      DrawLightningFanJitters(track, generator);

  if (!track.active || track.alpha <= 0 || track.ticks <= 0) {
    return quads;
  }
  const bool random_tint = track.degrees == kLightningFanRandomColorSweep;

  const std::size_t last =
      std::min(kLightningFanLastDrawnSegment, segments == 0 ? 0 : segments - 2);
  for (std::size_t segment = kLightningFanFirstDrawnSegment; segment <= last; ++segment) {
    const std::int16_t alpha = static_cast<std::int16_t>(
        track.alpha - kLightningFanAlphaPerSegment * static_cast<std::int16_t>(segment));
    if (alpha <= 0) {
      continue;
    }
    // LES JOINTS SONT RELATIFS À L'ANCRE, et dans le repère du client. L'ancre,
    // elle, est déjà dans le monde : elle s'ajoute APRÈS le retour au nôtre.
    const glm::vec3 near_point = EffectLightningFanJoint(track, segment, tremblements[segment]);
    const glm::vec3 far_point =
        EffectLightningFanJoint(track, segment + 1, tremblements[segment + 1]);
    const std::int32_t plan =
        WithinFullTurn(HeadingOfVector(near_point.x - far_point.x, near_point.z - far_point.z) +
                       kEffectTrackFullTurnDegrees / 4);

    glm::vec3 previous_near{0.0F, 0.0F, 0.0F};
    glm::vec3 previous_far{0.0F, 0.0F, 0.0F};
    for (std::size_t face = 0; face <= kLightningFanFaces; ++face) {
      const std::int32_t autour = kLightningFanFaceStepDegrees * static_cast<std::int32_t>(face);
      const float radial = track.lift_key * CosOfDegrees(autour);
      // ⚠ NOTRE REPÈRE MONTE, CELUI DU CLIENT DESCEND : la hauteur de l'anneau
      // et celle du joint changent de signe ENSEMBLE, au dernier moment — mais
      // pas celle de l'ancre, qui est déjà la nôtre. La nier avec eux renvoyait
      // l'éventail en miroir sous le plan de hauteur nulle, donc sous le sol de
      // toute carte qui n'est pas à ce niveau.
      // ⚠ ET LA PROFONDEUR SE RETOURNE AVEC ELLES : l'écart entier passe dans
      // notre repère. N'en retourner qu'une ferait de l'éventail le miroir du
      // sien, ouvert de l'autre côté.
      const glm::vec3 offset{radial * CosOfDegrees(plan), track.lift_key * SinOfDegrees(autour),
                             radial * SinOfDegrees(plan)};
      const glm::vec3 near_corner = track.anchor + EffectPointFromClient(near_point + offset);
      const glm::vec3 far_corner = track.anchor + EffectPointFromClient(far_point + offset);
      if (face > 0) {
        EffectParticleQuadRequest request;
        request.corners = {near_corner, far_corner, previous_far, previous_near};
        // ⚠ ABSOLUS : l'ancre de la piste est déjà dans le monde.
        request.absolute = true;
        request.alpha = static_cast<std::uint8_t>(alpha);
        if (random_tint) {
          // ⚠ DEUX TIRAGES DE PLUS PAR QUAD, et dans cet ordre : le vert, puis
          // le rouge. Et le rouge RECULE de cinquante-six, si bien qu'il
          // reboucle par le haut — il est donc clair, jamais sombre.
          const auto vert =
              static_cast<std::uint8_t>(DrawBelow(generator, kLightningFanRandomGreenSpread));
          const auto red = static_cast<std::uint8_t>(
              DrawBelow(generator, kLightningFanRandomRedSpread) + kLightningFanRandomRedOffset);
          request.color = {red, vert, kLightningFanBlue};
        } else {
          const auto rise = static_cast<std::uint8_t>(kLightningFanGradientPerSegment *
                                                      static_cast<std::uint8_t>(segment));
          request.color = {rise, rise, kLightningFanBlue};
        }
        request.v_at_a_and_b = 1.0F;
        request.v_at_c_and_d = 0.0F;
        quads.push_back(ComposeEffectParticleQuad(request, glm::vec3{0.0F, 0.0F, 0.0F}, owner));
      }
      previous_near = near_corner;
      previous_far = far_corner;
    }
  }
  return quads;
}

void AdvanceLightningFanTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                               std::uint32_t& generator) {
  for (EffectTrack& effect_track : tracks) {
    if (!effect_track.active) {
      continue;
    }
    ++effect_track.ticks;
    if (effect_track.ticks <= 0) {
      continue;
    }

    // ⚠ LE RAYON ENFLE TOUJOURS, ET C'EST LUI QUI TUE L'ÉCLAIR : passé quinze,
    // l'alpha perd deux par tic. Ce n'est donc pas le compteur qui l'éteint.
    effect_track.radius_at_anchor += kRadiusGrowth;
    if (effect_track.radius_at_anchor > kRadiusBeforeFading) {
      effect_track.alpha = static_cast<std::int16_t>(effect_track.alpha - kAlphaFall);
    }
    if (effect_track.ticks <= kAlphaRiseUntilTick) {
      effect_track.alpha = static_cast<std::int16_t>(
          effect_track.alpha + DrawBelow(generator, kAlphaRiseSpread) + kAlphaRiseFloor);
    }
    effect_track.alpha = std::max<std::int16_t>(effect_track.alpha, 0);

    if (effect_track.ticks != kHistoryTick) {
      continue;
    }
    // ⚠ L'HISTORIQUE S'ÉCRIT D'UN COUP, au premier tic : vingt et un tours de la
    // même boucle. L'éclair naît entier.
    WriteLightningFanHistory(effect_track);
  }
}

}  // namespace ro::render
