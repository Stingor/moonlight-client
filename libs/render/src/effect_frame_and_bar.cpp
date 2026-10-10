#include "ro/render/effect_frame_and_bar.h"

#include <algorithm>
#include <cstddef>

#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// Le mode que la voie A entend comme une ADDITION : ce que les huit sites
// poussent. Voir `EffectParticleBlend`.
constexpr std::int16_t kAddedBlend = 0;

// CE QUE LE MOUVEMENT POSE EN DUR.

// Le plafond de l'âge. Le client le pose pour que ses durées, qui vont
// jusqu'à trente mille tics, ne débordent pas l'entier de seize bits où il
// range leur reste.
constexpr std::int32_t kAgeCeiling = 29000;
// En combien de tics la hauteur atteint son plein, et sur quel quart de sinus.
constexpr std::int32_t kRiseDegrees = 90;
// De combien la barre avance par tic, en unités du monde.
constexpr float kBarStep = 0.5F;
// COMBIEN DE DEMI-CÔTÉS FAIT LE TOUR : huit, et c'est le client qui suppose le
// carré. Un cadre dont les deux demi-côtés différeraient replierait sa barre
// au mauvais endroit — mais aucun bâtisseur n'en pose.
constexpr float kPerimeterInHalfSides = 8.0F;

// LES TROIS NOMBRES DE LA COUPURE PAR LE PORTEUR : à partir de quel style elle
// s'applique, après combien de tics, et au-dessus de quelle durée restante.
constexpr float kStyleThatWatchesItsOwner = 20.0F;
constexpr std::int32_t kTicksBeforeWatching = 200;
constexpr std::int16_t kDurationWorthCutting = 2000;

// LES QUATRE PENTES DE RETOMBÉE, une par famille de styles. Les deux premières
// se comptent depuis la durée, les deux autres depuis un instant FIXE que le
// client écrit en dur — et qui ne coïncide avec la durée que pour un style.
constexpr std::int32_t kLateFadeStep = 5;
constexpr std::int32_t kLateFadeBias = 18;
constexpr std::int32_t kPlainFadeEnd = 2475;
constexpr std::int32_t kQuietFadeEnd = 2505;

[[nodiscard]] std::int32_t AngleDeRetombee(float style, std::int16_t duration, std::int32_t ticks) {
  if (style >= kStyleThatWatchesItsOwner) {
    return kLateFadeStep * (duration - ticks + kLateFadeBias);
  }
  if (style >= kFrameAndBarBlueStyle) {
    return duration - ticks + kRiseDegrees;
  }
  return (style == 0.0F ? kQuietFadeEnd : kPlainFadeEnd) - ticks;
}

// LA HAUTEUR D'UNE PISTE à son âge : un quart de sinus qui monte, un plateau,
// puis un quart de sinus qui redescend.
[[nodiscard]] float HeightAtTick(const EffectTrack& effect_track) {
  if (effect_track.ticks <= effect_track.style) {
    if (effect_track.ticks >= kRiseDegrees) {
      return effect_track.lift_key;
    }
    return SinOfDegrees(effect_track.ticks) * effect_track.lift_key;
  }
  const std::int32_t angle = std::clamp(AngleDeRetombee(effect_track.value[kFrameAndBarSlotOfStyle],
                                                        effect_track.style, effect_track.ticks),
                                        0, kRiseDegrees);
  return SinOfDegrees(angle) * effect_track.lift_key;
}

// UN MUR DE CADRE : le mur partagé, et les deux figures de cette famille le
// posent TOUJOURS additif.
[[nodiscard]] EffectWorldQuad Wall(const glm::vec2& pres, const glm::vec2& far_point, float height,
                                   std::int16_t alpha, const std::array<std::uint8_t, 3>& tint,
                                   glm::vec3 node_origin, const EffectOwnerTint& owner) {
  return ComposeEffectWallQuad(pres, far_point, height, alpha, tint, kAddedBlend, node_origin,
                               owner);
}

}  // namespace

glm::vec2 EffectPerimeterPointOnSide(float half_side, std::size_t side, float travelled) {
  const float a = half_side;
  switch (side) {
    case 0:
      return {a, a - travelled};
    case 1:
      return {3.0F * a - travelled, -a};
    case 2:
      return {-a, travelled - 5.0F * a};
    default:
      return {travelled - 7.0F * a, a};
  }
}

std::size_t EffectPerimeterSideOf(float half_side, float travelled) {
  const float cote = half_side + half_side;
  if (travelled >= 3.0F * cote) {
    return 3;
  }
  if (travelled >= cote + cote) {
    return 2;
  }
  return travelled >= cote ? 1 : 0;
}

std::vector<EffectWorldQuad> ComposeFrameWallQuads(const EffectTrack& track, glm::vec3 node_origin,
                                                   const EffectOwnerTint& owner) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active || track.alpha <= 0) {
    return quads;
  }
  const float a = track.value[kFrameAndBarSlotOfHalfX];
  const float b = track.value[kFrameAndBarSlotOfHalfZ];
  const float height = track.value[kFrameAndBarSlotOfHeight];
  const std::array<std::uint8_t, 3> tint =
      track.value[kFrameAndBarSlotOfStyle] >= kFrameAndBarBlueStyle ? kFrameBlue : kFrameGreen;

  // LES QUATRE COINS DU CARRÉ, dans l'ordre où le client fait le tour.
  const std::array<glm::vec2, kFrameAndBarWalls> corners = {glm::vec2{a, b}, glm::vec2{a, -b},
                                                            glm::vec2{-a, -b}, glm::vec2{-a, b}};
  quads.reserve(kFrameAndBarWalls);
  for (std::size_t wall = 0; wall < kFrameAndBarWalls; ++wall) {
    quads.push_back(Wall(corners[wall], corners[(wall + 1) % kFrameAndBarWalls], height,
                         track.alpha, tint, node_origin, owner));
  }
  return quads;
}

std::vector<EffectWorldQuad> ComposePerimeterBarQuads(const EffectTrack& track,
                                                      glm::vec3 node_origin,
                                                      const EffectOwnerTint& owner) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active || track.alpha <= 0) {
    return quads;
  }
  const float a = track.value[kFrameAndBarSlotOfHalfX];
  const float height = track.value[kFrameAndBarSlotOfHeight];
  const float swept = track.radius_at_anchor;
  const float tip = swept + track.value[kFrameAndBarSlotOfBarLength];
  const std::array<std::uint8_t, 3> tint =
      track.value[kFrameAndBarSlotOfStyle] >= kFrameAndBarBlueStyle ? kBarBlue : kBarGreen;

  const std::size_t cote = EffectPerimeterSideOf(a, swept);
  const float fin_du_cote = static_cast<float>(cote + 1) * (a + a);
  const glm::vec2 pres = EffectPerimeterPointOnSide(a, cote, swept);

  if (tip <= fin_du_cote) {
    quads.push_back(Wall(pres, EffectPerimeterPointOnSide(a, cote, tip), height, track.alpha, tint,
                         node_origin, owner));
    return quads;
  }
  // ⚠ LA BARRE PASSE UN COIN : le client coupe ici et repart de l'autre côté.
  // Le second morceau n'est PAS coupé à son tour — une barre plus longue qu'un
  // côté dépasse donc le coin suivant en ligne droite.
  const glm::vec2 corner = EffectPerimeterPointOnSide(a, cote, fin_du_cote);
  const std::size_t next = (cote + 1) % kFrameAndBarWalls;
  // Le quatrième côté repasse par l'origine du tour, et l'abscisse avec lui.
  const float au_dela = cote == kFrameAndBarWalls - 1 ? tip - kPerimeterInHalfSides * a : tip;
  quads.push_back(Wall(pres, corner, height, track.alpha, tint, node_origin, owner));
  quads.push_back(Wall(corner, EffectPerimeterPointOnSide(a, next, au_dela), height, track.alpha,
                       tint, node_origin, owner));
  return quads;
}

void AdvanceFrameAndBarTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                              glm::vec3 node_position, glm::vec3 owner_position) {
  // ⚠ LE CLIENT NE COMPARE QUE DEUX COORDONNÉES : celle de l'est et celle du
  // nord. Une marque posée sur une pente ne s'interrompt donc pas quand son
  // porteur monte d'une marche.
  const bool bearer_gone =
      node_position.x != owner_position.x || node_position.z != owner_position.z;

  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& effect_track = tracks[rang];
    if (!effect_track.active) {
      continue;
    }
    effect_track.ticks = std::min(effect_track.ticks + 1, kAgeCeiling);
    if (effect_track.ticks <= 0) {
      continue;
    }
    // ⚠ SEULES LES DEUX DERNIÈRES PISTES FONT COURIR LEUR BARRE, et le tour
    // qu'elles parcourent se mesure sur LEUR demi-côté, pas sur celui du cadre.
    if (rang >= kFrameAndBarFrameTracks) {
      effect_track.radius_at_anchor += kBarStep;
      const float tour = kPerimeterInHalfSides * effect_track.value[kFrameAndBarSlotOfHalfX];
      if (effect_track.radius_at_anchor > tour) {
        effect_track.radius_at_anchor -= tour;
      }
    }
    if (effect_track.value[kFrameAndBarSlotOfStyle] >= kStyleThatWatchesItsOwner &&
        effect_track.ticks > kTicksBeforeWatching && bearer_gone &&
        effect_track.style > kDurationWorthCutting) {
      // LA MARQUE S'ARRÊTE : la durée tombe à l'âge courant, et la retombée
      // commence au tic suivant.
      effect_track.style = static_cast<std::int16_t>(effect_track.ticks);
    }
    effect_track.value[kFrameAndBarSlotOfHeight] = HeightAtTick(effect_track);
  }
}

}  // namespace ro::render
