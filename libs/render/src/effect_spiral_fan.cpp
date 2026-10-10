#include "ro/render/effect_spiral_fan.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// COMMENT L'ÉVENTAIL LIT SA PISTE. Les cases n'ont pas de sens en elles-mêmes —
// voir `effect_track.h` — et c'est ici que cette figure donne le sien aux
// siennes.
//
// ⚠ DIX CASES D'AFFILÉE PORTENT DIX ALPHAS, un par lame, et la case zéro n'en
// est pas : elle pousse tout l'éventail de côté. Prendre la case zéro pour la
// première lame décalerait le dégradé d'un cran et perdrait la lame la plus
// brillante.
constexpr std::size_t kSlotOfSideways = 0;
constexpr std::size_t kSlotOfFirstBlade = 1;
constexpr std::size_t kBladeCount = 10;
constexpr std::size_t kMarkOfStyle = 4;
constexpr std::size_t kMarkOfTexture = 0;

constexpr std::size_t kCornerCount = 4;

// L'AURA EST UN DIXIÈME PLUS GRANDE que la lame qu'elle entoure.
constexpr float kAuraScale = 1.1F;

// LE MÉLANGE ADDITIF, celui que les auras et les deux styles qui s'ouvrent
// demandent. Zéro n'est pas « pas de mode » — voir `effect_particle_quad.h`.
constexpr std::int16_t kAdditiveBlend = 0;
constexpr std::int16_t kOrdinaryBlend = 1;

// L'image entière, telle que le client la passe : un en v aux coins A et B,
// zéro aux coins C et D. Les coins reviennent de son repère par `S` entier, qui
// ne retourne rien : ses nombres valent tels quels, sans compensation.
constexpr float kTextureAtLeft = 0.0F;
constexpr float kTextureAtRight = 1.0F;
constexpr float kTextureAtNear = 1.0F;
constexpr float kTextureAtFar = 0.0F;

// ⚠ L'ORDRE DES COINS N'EST PAS CELUI DES ANGLES. Le client bâtit ses quatre
// points dans l'ordre des angles croissants, puis les soumet dans un autre :
// le deuxième, le premier, le quatrième, le troisième. Les soumettre dans
// l'ordre des angles croiserait les deux triangles en sablier.
constexpr std::array<std::size_t, kCornerCount> kSubmissionOrder = {1, 0, 3, 2};

// LES DEUX STYLES QUI S'OUVRENT À LA FIN, et qui ne posent pas leurs coins
// comme les autres.
constexpr std::uint8_t kFirstStyleThatOpens = 6;
constexpr std::uint8_t kLastStyleThatOpens = 7;

// LE STYLE QUI DOUBLE SA LAME LA PLUS SERRÉE, et celui qui peint en noir.
constexpr std::uint8_t kStyleThatDoubles = 3;
constexpr std::uint8_t kStyleThatPaintsBlack = 0;

// LE PREMIER DES TROIS STYLES QUI PORTENT UNE AURA. L'aura commence un style
// avant l'ouverture : celui qui ne s'ouvre pas en a une aussi, bleue.
constexpr std::uint8_t kFirstStyleWithAura = 5;

// LES TROIS STYLES QUI AVANCENT D'UNE UNITÉ par lame, là où les autres
// avancent de trois.
constexpr std::uint8_t kFirstStyleWithTightStep = 2;
constexpr std::uint8_t kSecondStyleWithTightStep = 4;
constexpr float kTightStep = 1.0F;
constexpr float kWideStep = 3.0F;

// LES DEUX ÉLÉVATIONS, et rien entre les deux.
constexpr float kLowLift = 30.0F;
constexpr float kHighLift = 40.0F;

// Les couleurs que les styles nomment. Le noir et le presque-noir sont des
// couleurs choisies, pas des absences : le client les écrit en toutes lettres.
constexpr std::array<std::uint8_t, 3> kBlack = {0, 0, 0};
constexpr std::array<std::uint8_t, 3> kNearBlack = {10, 10, 10};
constexpr std::array<std::uint8_t, 3> kWhite = {255, 255, 255};
constexpr std::array<std::uint8_t, 3> kRedAura = {255, 5, 5};
constexpr std::array<std::uint8_t, 3> kBlueAura = {5, 5, 255};

// LES QUATRE ANGLES, en deux jeux. Le carré régulier, et celui que les deux
// styles qui s'ouvrent resserrent de dix degrés vers l'horizontale.
constexpr std::array<std::int32_t, kCornerCount> kSquareCorners = {45, 135, 225, 315};
constexpr std::array<std::int32_t, kCornerCount> kFlattenedCorners = {35, 145, 215, 325};

[[nodiscard]] float RadiansOfDegrees(std::int32_t degrees) {
  return static_cast<float>(degrees) * std::numbers::pi_v<float> / 180.0F;
}

// UN QUAD DE LAME, coins posés à plat puis tournés puis élevés.
[[nodiscard]] EffectWorldQuad LameOuAura(const std::array<std::int32_t, kCornerCount>& angles,
                                         float radius, float de_cote, float elevation,
                                         std::int16_t tour, std::int16_t tilt, std::uint8_t alpha,
                                         const std::array<std::uint8_t, 3>& color,
                                         std::int16_t blend, std::size_t texture,
                                         glm::vec3 node_origin) {
  std::array<glm::vec3, kCornerCount> corners;
  for (std::size_t corner = 0; corner < kCornerCount; ++corner) {
    const float theta = RadiansOfDegrees(angles[corner]);
    corners[corner] = {radius * std::cos(theta) + de_cote, 0.0F, radius * std::sin(theta)};
  }
  RotateEffectQuadCorners(corners, tour, tilt);

  EffectParticleQuadRequest request;
  for (std::size_t corner = 0; corner < kCornerCount; ++corner) {
    const glm::vec3 rotated = corners[kSubmissionOrder[corner]];
    // ⚠ L'ÉLÉVATION SE RETRANCHE APRÈS LA ROTATION, dans le repère descendant
    // du client, puis le tout revient au nôtre par `S` entier : la lame,
    // tournée par les angles de SA caméra, monte.
    request.corners[corner] = EffectPointFromClient({rotated.x, rotated.y - elevation, rotated.z});
  }
  request.absolute = false;
  request.alpha = alpha;
  request.color = color;
  request.blend_mode = blend;
  request.texture = texture;
  request.u_at_a_and_d = kTextureAtLeft;
  request.u_at_b_and_c = kTextureAtRight;
  request.v_at_a_and_b = kTextureAtNear;
  request.v_at_c_and_d = kTextureAtFar;
  return ComposeEffectParticleQuad(request, node_origin);
}

}  // namespace

EffectSpiralFanStyle EffectSpiralFanStyleFor(std::uint8_t style) {
  const bool ouvre = style >= kFirstStyleThatOpens && style <= kLastStyleThatOpens;
  const bool serre = style == kFirstStyleWithTightStep || style == kSecondStyleWithTightStep ||
                     style == kFirstStyleWithAura;

  EffectSpiralFanStyle look;
  look.corner_degrees = ouvre ? kFlattenedCorners : kSquareCorners;
  // Trois styles serrent leur éventail ; tous les autres l'étalent.
  look.radius_step_per_blade = serre ? kTightStep : kWideStep;
  // Quatre styles montent haut ; les autres restent bas.
  look.lift_above_anchor =
      (style == kFirstStyleWithTightStep || style == kSecondStyleWithTightStep || ouvre) ? kHighLift
                                                                                         : kLowLift;

  if (style == kStyleThatPaintsBlack) {
    look.color = kBlack;
  } else if (style == kStyleThatDoubles) {
    look.color = kNearBlack;
    look.doubles_innermost = true;
  } else {
    look.color = kWhite;
  }
  look.blend_mode = ouvre ? kAdditiveBlend : kOrdinaryBlend;

  // ⚠ L'AURA COMMENCE UN STYLE AVANT L'OUVERTURE. Trois styles la portent, pas
  // deux : celui qui ne s'ouvre pas en a une aussi, bleue, et l'oublier lui
  // retirerait son halo sans rien casser d'autre — le genre d'écart qu'une
  // capture ne montre qu'à côté du client.
  look.has_aura = style == kFirstStyleWithAura || ouvre;
  look.aura_color = style == kLastStyleThatOpens ? kRedAura : kBlueAura;
  return look;
}

std::vector<EffectWorldQuad> ComposeSpiralFanQuads(const EffectTrack& track,
                                                   std::int16_t billboard_turn_degrees,
                                                   std::int16_t billboard_tilt_degrees,
                                                   glm::vec3 node_origin) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active) {
    return quads;
  }
  const EffectSpiralFanStyle look = EffectSpiralFanStyleFor(track.mark[kMarkOfStyle]);
  const float de_cote = track.value[kSlotOfSideways];
  const std::size_t texture = track.mark[kMarkOfTexture];

  for (std::size_t lame = 0; lame < kBladeCount; ++lame) {
    // ⚠ LA LAME LA PLUS SERRÉE PORTE LA CASE LA PLUS HAUTE. Le client compte à
    // rebours : le carré le plus petit lit la dernière case, celle que le
    // mouvement pousse le plus haut. Lire les cases dans l'ordre du rayon
    // retournerait le dégradé.
    const std::size_t case_de_la_lame = kSlotOfFirstBlade + kBladeCount - 1 - lame;
    const float shard = track.value[case_de_la_lame];
    if (shard <= 0.0F) {
      continue;
    }
    const auto alpha =
        static_cast<std::uint8_t>(std::clamp(static_cast<std::int32_t>(shard), 0, 255));
    const float radius =
        track.radius_at_anchor + look.radius_step_per_blade * static_cast<float>(lame);

    // LE CŒUR PLUS DENSE D'UN SEUL STYLE : sa lame la plus serrée sort deux
    // fois, la première à demi-alpha et par-dessous.
    if (look.doubles_innermost && lame == 0) {
      quads.push_back(LameOuAura(look.corner_degrees, radius, de_cote, look.lift_above_anchor,
                                 billboard_turn_degrees, billboard_tilt_degrees,
                                 static_cast<std::uint8_t>(alpha / 2), look.color, look.blend_mode,
                                 texture, node_origin));
    }
    quads.push_back(LameOuAura(look.corner_degrees, radius, de_cote, look.lift_above_anchor,
                               billboard_turn_degrees, billboard_tilt_degrees, alpha, look.color,
                               look.blend_mode, texture, node_origin));
    if (look.has_aura) {
      quads.push_back(LameOuAura(look.corner_degrees, radius * kAuraScale, de_cote,
                                 look.lift_above_anchor, billboard_turn_degrees,
                                 billboard_tilt_degrees, alpha, look.aura_color, kAdditiveBlend,
                                 texture, node_origin));
    }
  }
  return quads;
}

}  // namespace ro::render
