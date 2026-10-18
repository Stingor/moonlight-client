#include "ro/render/effect_electric_arc.h"

#include <algorithm>
#include <cstddef>
#include <optional>

#include "ro/render/effect_launch.h"
#include "ro/render/effect_lightning_fan.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// CE QUE LE MOUVEMENT POSE.

// DE COMBIEN L'ANCRE SE TIENT AU-DESSUS DU PORTEUR.
//
// ⚠ LE CLIENT RETRANCHE DIX, et son ordonnée descend : c'est dix PLUS HAUT.
constexpr float kAnchorAboveOwner = 10.0F;

// LE STYLE QUI FAIT TOURNER LE CAP DANS LE SENS CROISSANT ; tout autre le fait
// décroître.
constexpr std::int16_t kIncreasingSpinStyle = 1;
// DE COMBIEN LE CAP TOURNE PAR TIC, et combien de tics chaque rang de piste
// attend avant de tourner : dix de plus par piste.
constexpr std::int16_t kSpinPerTick = 2;
constexpr std::int32_t kSpinDelayPerRank = 10;

// PASSÉ QUEL TIC L'ALPHA TOMBE, et de combien par tic.
constexpr std::int32_t kFadeAfterTick = 60;
constexpr std::int16_t kFadePerTick = 6;

// AU PREMIER TIC, ET SEULEMENT LÀ, l'historique s'écrit.
constexpr std::int32_t kHistoryTick = 1;

[[nodiscard]] std::int32_t DrawBelow(std::uint32_t& rng_state, std::int32_t exclusive_bound) {
  return static_cast<std::int32_t>(NextEffectNodeDraw(rng_state) %
                                   static_cast<std::uint32_t>(exclusive_bound));
}

// UN POINT DU REPÈRE DU CLIENT, relatif à l'ancre, posé dans le monde.
[[nodiscard]] glm::vec3 ToWorld(const EffectTrack& effect_track, glm::vec3 client) {
  return effect_track.anchor + EffectClientOffsetInWorld(client.x, client.y, client.z);
}

// LE CAP DU PLAN DU TUBE pour un segment : un quart de tour de plus que le cap
// qui va du bout lointain vers le proche.
[[nodiscard]] std::int32_t PlanDuSegment(const EffectTrack& effect_track, std::size_t segment,
                                         const std::array<glm::vec3, kEffectTrackSlots>& tremb) {
  const glm::vec3 near_point = EffectLightningFanJoint(effect_track, segment, tremb[segment]);
  const glm::vec3 far_point =
      EffectLightningFanJoint(effect_track, segment + 1, tremb[segment + 1]);
  return WithinFullTurn(HeadingOfVector(near_point.x - far_point.x, near_point.z - far_point.z) +
                        kEffectTrackFullTurnDegrees / 4);
}

// LE BORD D'UN SEGMENT : ses deux bouts sur la dernière face du tube, celle
// qu'il ferme à trois cent soixante degrés. C'est là que les nappes s'accrochent.
struct Edge {
  glm::vec3 near_point{0.0F, 0.0F, 0.0F};
  glm::vec3 far_point{0.0F, 0.0F, 0.0F};
};

[[nodiscard]] bool IsTrackDrawn(const EffectTrack& effect_track) {
  // LA GARDE DU CLIENT : vive, un alpha et un compteur positifs.
  return effect_track.active && effect_track.alpha > 0 && effect_track.ticks > 0;
}

[[nodiscard]] std::size_t LastDrawnSegment(const EffectTrack& effect_track) {
  const std::size_t segments = LightningFanSegmentsOf(effect_track);
  return std::min(kLightningFanLastDrawnSegment, segments < 2 ? 0 : segments - 2);
}

[[nodiscard]] EffectWorldQuad UnQuad(const std::array<glm::vec3, 4>& corners, std::int16_t alpha,
                                     std::array<std::uint8_t, 3> tint,
                                     const EffectOwnerTint& owner) {
  EffectParticleQuadRequest request;
  request.corners = corners;
  // ⚠ ABSOLUS : l'ancre de la piste est déjà dans le monde.
  request.absolute = true;
  request.alpha = static_cast<std::uint8_t>(alpha);
  request.color = tint;
  // LES MÊMES NOMBRES DE TEXTURE QUE L'ÉCLAIR DU 101 : le client passe
  // `0, 1.0, 0, 1.0` au tube et aux nappes comme à l'éclair.
  request.v_at_a_and_b = 1.0F;
  request.v_at_c_and_d = 0.0F;
  return ComposeEffectParticleQuad(request, glm::vec3{0.0F, 0.0F, 0.0F}, owner);
}

// LE TUBE DE LA PISTE DE TÊTE, et les bords où les nappes s'accrocheront.
void PlaceTube(const EffectTrack& effect_track,
               const std::array<glm::vec3, kEffectTrackSlots>& tremb, std::uint32_t& rng_state,
               const EffectOwnerTint& owner, std::vector<EffectWorldQuad>& quads,
               std::array<Edge, kEffectTrackSlots>& edges) {
  const bool random_tint = effect_track.degrees == kElectricArcRandomColorSweep;
  for (std::size_t segment = kLightningFanFirstDrawnSegment;
       segment <= LastDrawnSegment(effect_track); ++segment) {
    const std::int32_t plan = PlanDuSegment(effect_track, segment, tremb);
    const std::int16_t alpha = static_cast<std::int16_t>(
        effect_track.alpha - kLightningFanAlphaPerSegment * static_cast<std::int16_t>(segment));
    glm::vec3 previous_near{0.0F, 0.0F, 0.0F};
    glm::vec3 previous_far{0.0F, 0.0F, 0.0F};
    for (std::size_t face = 0; face <= kLightningFanFaces; ++face) {
      const std::int32_t autour = kLightningFanFaceStepDegrees * static_cast<std::int32_t>(face);
      const glm::vec3 near_point = ToWorld(
          effect_track, ElectricArcRimPoint(effect_track, segment, tremb[segment], plan, autour));
      const glm::vec3 far_point =
          ToWorld(effect_track,
                  ElectricArcRimPoint(effect_track, segment + 1, tremb[segment + 1], plan, autour));
      // ⚠ UN SEGMENT ÉPUISÉ NE SE PEINT PAS, mais ses voisins si : le client
      // saute et ne sort pas de la boucle.
      if (face > 0 && alpha > 0) {
        std::array<std::uint8_t, 3> tint = {};
        if (random_tint) {
          // ⚠ LE BLEU EST TIRÉ AVANT LE VERT, et c'est l'ordre du client.
          const auto blue = static_cast<std::uint8_t>(
              DrawBelow(rng_state, kElectricArcRandomBlueSpread) + kElectricArcRandomBlueFloor);
          const auto vert =
              static_cast<std::uint8_t>(DrawBelow(rng_state, kElectricArcRandomGreenSpread));
          tint = {kElectricArcRed, vert, blue};
        } else {
          const auto rise = static_cast<std::uint8_t>(kElectricArcGradientPerSegment *
                                                      static_cast<std::uint8_t>(segment));
          tint = {kElectricArcRed, rise, rise};
        }
        quads.push_back(
            UnQuad({near_point, far_point, previous_far, previous_near}, alpha, tint, owner));
      }
      previous_near = near_point;
      previous_far = far_point;
    }
    // ⚠ LE BORD SE RANGE MÊME QUAND LE SEGMENT EST ÉPUISÉ : c'est
    // la dernière face calculée, pas la dernière peinte.
    edges[segment] = {previous_near, previous_far};
  }
}

// LES NAPPES D'UNE PISTE SUIVANTE : un quad par segment, de son bord à celui de
// la piste de tête.
void StretchSheets(const EffectTrack& effect_track, std::size_t rang,
                   const std::array<glm::vec3, kEffectTrackSlots>& tremb,
                   const std::array<Edge, kEffectTrackSlots>& edges, const EffectOwnerTint& owner,
                   std::vector<EffectWorldQuad>& quads) {
  // ⚠ UNE DIVISION ENTIÈRE, et c'est celle du client : cent dix sur trois font
  // trente-six.
  const auto alpha =
      static_cast<std::int16_t>(effect_track.alpha / kElectricArcSheetAlphaDivisor[rang]);
  // ⚠ LE CLIENT PEINT AUSSI UNE NAPPE D'ALPHA NUL, qui ne se voit pas : nous
  // ne la rendons pas.
  if (alpha <= 0) {
    return;
  }
  for (std::size_t segment = kLightningFanFirstDrawnSegment;
       segment <= LastDrawnSegment(effect_track); ++segment) {
    const std::int32_t plan = PlanDuSegment(effect_track, segment, tremb);
    const std::int32_t ferme = kEffectTrackFullTurnDegrees;
    const glm::vec3 near_point = ToWorld(
        effect_track, ElectricArcRimPoint(effect_track, segment, tremb[segment], plan, ferme));
    const glm::vec3 far_point =
        ToWorld(effect_track,
                ElectricArcRimPoint(effect_track, segment + 1, tremb[segment + 1], plan, ferme));
    // LE ROUGE PUR, en dur (0xFF, 0, 0).
    quads.push_back(
        UnQuad({near_point, far_point, edges[segment].far_point, edges[segment].near_point}, alpha,
               {kElectricArcRed, 0, 0}, owner));
  }
}

}  // namespace

glm::vec3 ElectricArcRimPoint(const EffectTrack& track, std::size_t segment, glm::vec3 jitter,
                              std::int32_t plan_degrees, std::int32_t face_degrees) {
  const glm::vec3 joint = EffectLightningFanJoint(track, segment, jitter);
  // L'ANNEAU DU TUBE, dans le repère du client : son
  // rayon est la clé de la piste, sa hauteur le sinus de la face.
  const float radial = track.lift_key * CosOfDegrees(face_degrees);
  return {joint.x + radial * CosOfDegrees(plan_degrees),
          joint.y + track.lift_key * SinOfDegrees(face_degrees),
          joint.z + radial * SinOfDegrees(plan_degrees)};
}

std::vector<EffectWorldQuad> ComposeElectricArcQuads(
    const std::array<EffectTrack, kEffectTracksPerNode>& tracks, std::uint32_t& generator,
    const EffectOwnerTint& owner) {
  std::vector<EffectWorldQuad> quads;
  // LES BORDS DE LA PISTE DE TÊTE, tels qu'elle les laisse pour les autres.
  std::optional<std::array<Edge, kEffectTrackSlots>> edges;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    const EffectTrack& effect_track = tracks[rang];
    // ⚠ LE TREMBLEMENT SE TIRE AVANT TOUTE GARDE, piste par piste : une piste
    // éteinte consomme ses tirages comme les autres.
    const std::array<glm::vec3, kEffectTrackSlots> tremb =
        DrawLightningFanJitters(effect_track, generator);
    if (!IsTrackDrawn(effect_track)) {
      continue;
    }
    if (rang == kElectricArcLeadTrack) {
      edges.emplace();
      PlaceTube(effect_track, tremb, generator, owner, quads, *edges);
      continue;
    }
    // ⚠ SANS TÊTE, PAS DE NAPPE. Le client tendrait alors ses nappes vers
    // l'ORIGINE DE LA CARTE, où ses tableaux locaux sont à zéro ; ce cas ne se
    // présente pas avec le seul bâtisseur connu — la tête naît la plus opaque
    // et toutes s'éteignent au même pas —, et nous ne le reproduisons pas.
    if (edges.has_value()) {
      StretchSheets(effect_track, rang, tremb, *edges, owner, quads);
    }
  }
  return quads;
}

void AdvanceElectricArcTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                              glm::vec3 owner_position) {
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& effect_track = tracks[rang];
    if (!effect_track.active) {
      continue;
    }
    ++effect_track.ticks;
    // ⚠ L'ANCRE SUIT LE PORTEUR À CHAQUE TIC, compteur positif ou non : le
    // client la recopie AVANT de tester le retard.
    effect_track.anchor = owner_position + glm::vec3{0.0F, kAnchorAboveOwner, 0.0F};
    if (effect_track.ticks <= 0) {
      continue;
    }

    const bool rotated = effect_track.ticks > kSpinDelayPerRank * static_cast<std::int32_t>(rang);
    if (effect_track.style == kIncreasingSpinStyle) {
      if (rotated) {
        effect_track.tilt_degrees =
            static_cast<std::int16_t>(effect_track.tilt_degrees + kSpinPerTick);
      }
      // ⚠ UNE SEULE SOUSTRACTION, comme le client : le cap ne sort jamais du
      // tour de plus d'un pas.
      DropOneTurn(effect_track.tilt_degrees);
    } else {
      if (rotated) {
        effect_track.tilt_degrees =
            static_cast<std::int16_t>(effect_track.tilt_degrees - kSpinPerTick);
      }
      if (effect_track.tilt_degrees < 0) {
        effect_track.tilt_degrees =
            static_cast<std::int16_t>(effect_track.tilt_degrees + kEffectTrackFullTurnDegrees);
      }
    }

    if (effect_track.ticks > kFadeAfterTick) {
      effect_track.alpha = static_cast<std::int16_t>(effect_track.alpha - kFadePerTick);
    }
    effect_track.alpha = std::max<std::int16_t>(effect_track.alpha, 0);

    if (effect_track.ticks == kHistoryTick) {
      WriteLightningFanHistory(effect_track);
    }
  }
}

}  // namespace ro::render
