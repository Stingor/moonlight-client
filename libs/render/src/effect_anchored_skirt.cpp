#include "ro/render/effect_anchored_skirt.h"

#include <algorithm>
#include <cstddef>

#include "ro/render/effect_scene.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_trail_palette.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// CE QUE LE MOUVEMENT POSE EN DUR.

// DE COMBIEN LA DÉRIVE S'ÉCARTE DE L'AZIMUT : quinze degrés en arrière.
constexpr std::int32_t kDriftBehindDegrees = -15;
// À partir de quel tic l'alpha tombe, de combien il monte et de combien il
// tombe, et son plafond.
constexpr std::int32_t kFadeFromTick = 10;
constexpr std::int16_t kAlphaRise = 25;
constexpr std::int16_t kAlphaFall = 10;
constexpr std::int16_t kAlphaCeiling = 250;
// CE QUE CHAQUE TOUR COÛTE à la gerbe, et le plancher où elle s'éteint.
constexpr std::int16_t kTiltPerRound = 20;
constexpr std::int16_t kTiltFloor = 40;
constexpr float kRadiusPerRound = 0.4F;
// PENDANT COMBIEN DE TICS la gerbe s'ouvre, et de combien de degrés de sinus
// par tic.
constexpr std::int32_t kOpeningTicks = 15;
constexpr std::int32_t kOpeningStepDegrees = 6;

// COMBIEN LE RUBAN SE RÉTRÉCIT À SES DEUX BOUTS.
//
// ⚠ LA TABLE EST SYMÉTRIQUE et ne couvre que SIX segments de chaque côté ; les
// neuf du milieu gardent leur pleine longueur. C'est ce qui donne à la gerbe
// ses pointes.
constexpr std::array<float, 6> kEndTaper = {0.4F, 0.5F, 0.6F, 0.7F, 0.8F, 0.9F};

// La coordonnée de texture au travers du ruban, comme pour la jupe : le bord
// intérieur porte le haut de l'image, l'extérieur le bas.
constexpr float kTextureAtInnerEdge = 1.0F;
constexpr float kTextureAtOuterEdge = 0.0F;

[[nodiscard]] float ParDuSegment(std::size_t segment) {
  const std::size_t from_end = kEffectTrackSlots - 1 - segment;
  const std::size_t edge = std::min(segment, from_end);
  return edge < kEndTaper.size() ? kEndTaper[edge] : 1.0F;
}

}  // namespace

glm::vec2 EffectAnchoredSkirtDrift(const std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  // ⚠ CELUI DE LA PREMIÈRE PISTE, quelle que soit celle qu'on dessine : le
  // client lit un déplacement ABSOLU depuis le nœud, pas depuis la piste.
  return {tracks.front().tail[kAnchoredSkirtTailOfDriftX],
          tracks.front().tail[kAnchoredSkirtTailOfDriftZ]};
}

std::vector<EffectWorldQuad> ComposeAnchoredSkirtQuads(const EffectTrack& track, glm::vec2 drift,
                                                       std::size_t texture_count,
                                                       glm::vec3 anchor) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active || track.ticks <= 0 || track.degrees < 0) {
    return quads;
  }
  const std::int32_t step = track.degrees / kAnchoredSkirtSteps;
  if (step <= 0 || texture_count == 0) {
    return quads;
  }
  const EffectTrailVariant tint =
      EffectTrailVariantFor(static_cast<std::uint8_t>(track.tail[kAnchoredSkirtTailOfVariant]));
  const std::int16_t alpha = tint.forced_alpha != 0 ? tint.forced_alpha : track.alpha;
  if (alpha <= 0) {
    return quads;
  }
  // ⚠ L'IMAGE DÉFILE AVEC LE COMPTEUR, modulo le nombre d'images du nœud. C'est
  // la première famille du lot dont ce nombre entre dans la figure.
  const std::size_t image = static_cast<std::size_t>(track.ticks) % texture_count;

  const float layer = CosOfDegrees(track.tilt_degrees);
  const float dressee = SinOfDegrees(track.tilt_degrees);
  // ⚠ LA DÉRIVE S'ACCUMULE DANS LE REPÈRE DU CLIENT, et la profondeur se
  // retourne avec l'ordonnée : le passage au nôtre est un demi-tour autour de X.
  // N'en retourner qu'une ferait de la gerbe le miroir de la sienne.
  const float drift_z = -drift.y;
  glm::vec3 previous_inner{0.0F, 0.0F, 0.0F};
  glm::vec3 previous_outer{0.0F, 0.0F, 0.0F};
  bool has_previous = false;
  std::int32_t band = 0;
  for (std::int32_t travelled = 0; travelled <= track.degrees; travelled += step) {
    const bool referme =
        track.degrees == kEffectTrackFullTurnDegrees && band == kAnchoredSkirtSteps;
    const std::int32_t angle =
        referme ? track.azimuth_degrees : WithinFullTurn(track.azimuth_degrees + travelled);
    const float vers_x = CosOfDegrees(angle);
    const float vers_z = -SinOfDegrees(angle);
    const float length = track.value[static_cast<std::size_t>(band)];

    // ⚠ LE BORD INTÉRIEUR NE REÇOIT QUE LES TROIS QUARTS DE LA DÉRIVE, et il
    // est à hauteur NULLE. Lui donner la dérive entière refermerait la gerbe
    // sur elle-même.
    const glm::vec3 inner{track.radius_at_anchor * vers_x - kAnchoredSkirtInnerShare * drift.x,
                          0.0F,
                          track.radius_at_anchor * vers_z - kAnchoredSkirtInnerShare * drift_z};
    // ⚠ NOTRE REPÈRE MONTE, CELUI DU CLIENT DESCEND : ce qu'il pose en ordonnée
    // négative s'élève, et se pose ici en positive.
    const glm::vec3 outer{(track.radius_at_anchor + length * layer) * vers_x - drift.x,
                          length * dressee,
                          (track.radius_at_anchor + length * layer) * vers_z - drift_z};

    if (has_previous) {
      const float u_before = static_cast<float>(band - 1) / static_cast<float>(kAnchoredSkirtBands);
      const float u_after = static_cast<float>(band) / static_cast<float>(kAnchoredSkirtBands);
      EffectWorldQuad quad;
      quad.corners = {anchor + previous_inner, anchor + inner, anchor + outer,
                      anchor + previous_outer};
      quad.texture_uv = {
          glm::vec2{u_before, kTextureAtInnerEdge}, glm::vec2{u_after, kTextureAtInnerEdge},
          glm::vec2{u_after, kTextureAtOuterEdge}, glm::vec2{u_before, kTextureAtOuterEdge}};
      quad.tint = {tint.color[0], tint.color[1], tint.color[2], static_cast<std::uint8_t>(alpha)};
      quad.blend_source = kEffectNodeBlendSource;
      quad.blend_destination = tint.blend_destination;
      quad.texture = image;
      quads.push_back(quad);
    }

    previous_inner = inner;
    previous_outer = outer;
    has_previous = true;
    band = std::min(band + 1, kAnchoredSkirtSteps);
  }
  return quads;
}

void AdvanceAnchoredSkirtTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  for (EffectTrack& effect_track : tracks) {
    if (!effect_track.active) {
      continue;
    }
    ++effect_track.ticks;
    // ⚠ LE RETARD EST PAR PISTE : les quatre naissent à huit tics d'écart.
    if (effect_track.ticks <= 0) {
      continue;
    }

    // LA DÉRIVE : quinze degrés en arrière de l'azimut, d'une unité par tic.
    const std::int32_t cap = WithinFullTurn(effect_track.azimuth_degrees + kDriftBehindDegrees);
    effect_track.tail[kAnchoredSkirtTailOfDriftX] += CosOfDegrees(cap);
    effect_track.tail[kAnchoredSkirtTailOfDriftZ] += SinOfDegrees(cap);
    if (effect_track.azimuth_degrees >= kEffectTrackFullTurnDegrees) {
      effect_track.azimuth_degrees =
          static_cast<std::int16_t>(effect_track.azimuth_degrees - kEffectTrackFullTurnDegrees);
    }

    if (effect_track.ticks >= kFadeFromTick) {
      effect_track.alpha = static_cast<std::int16_t>(effect_track.alpha - kAlphaFall);
      if (effect_track.alpha < 0) {
        // ⚠ ELLE RECOMMENCE PLUS BAS au lieu de mourir : inclinaison moins
        // vingt, rayon moins quatre dixièmes, compteur à zéro.
        effect_track.tilt_degrees =
            static_cast<std::int16_t>(effect_track.tilt_degrees - kTiltPerRound);
        effect_track.alpha = 0;
        effect_track.ticks = 0;
        effect_track.radius_at_anchor -= kRadiusPerRound;
        if (effect_track.tilt_degrees <= kTiltFloor) {
          effect_track.active = false;
          effect_track.tilt_degrees = kTiltFloor;
        }
      }
    } else {
      effect_track.alpha = std::min<std::int16_t>(
          static_cast<std::int16_t>(effect_track.alpha + kAlphaRise), kAlphaCeiling);
    }

    // LES VINGT ET UNE LONGUEURS : la pleine longueur, rétrécie aux deux bouts,
    // et multipliée par l'ouverture tant qu'elle dure.
    const float ouverture = effect_track.ticks <= kOpeningTicks
                                ? SinOfDegrees(kOpeningStepDegrees * effect_track.ticks)
                                : 1.0F;
    for (std::size_t segment = 0; segment < kEffectTrackSlots; ++segment) {
      float length = ParDuSegment(segment) * effect_track.lift_key;
      if (effect_track.ticks <= kOpeningTicks) {
        length *= ouverture;
      }
      effect_track.value[segment] = std::max(length, 0.0F);
    }
  }
}

}  // namespace ro::render
