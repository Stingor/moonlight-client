#include "ro/render/effect_rain_particle.h"

#include <algorithm>

#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LES QUATRE ANGLES DU CARRÉ, dans l'ordre où le client pousse ses coins.
//
// ⚠ CE SONT DES INDEX FIXES DANS SES TABLES, non quatre tables à part : voir
// l'en-tête. Les écrire en degrés est la façon de le dire une fois pour toutes.
constexpr std::array<std::int32_t, 4> kCornerDegrees = {45, 135, 225, 315};

// LE MÉLANGE : zéro est l'ADDITION, dans la numérotation de la voie A.
constexpr std::int16_t kAddedBlend = 0;

// LES TROIS MARQUES QUE LE CLIENT NOMME, et ce que chacune retranche au vert et
// au bleu. Le rouge y reste plein : c'est un grain qui rougit, pas qui s'éteint.
constexpr std::uint8_t kMarkOfWhiteGrain = 0;
constexpr std::uint8_t kMarkOfPinkGrain = 1;
constexpr std::uint8_t kMarkOfRedGrain = 2;
constexpr std::uint8_t kPinkSide = 160;
constexpr std::uint8_t kRedSide = 90;
constexpr std::uint8_t kFullChannel = 255;

// UN CARRÉ posé à plat, tourné vers la caméra, puis porté à l'ancre.
[[nodiscard]] std::array<glm::vec3, 4> AnchoredSquare(float radius, glm::vec3 anchor,
                                                      std::int16_t turn_degrees,
                                                      std::int16_t tilt_degrees) {
  std::array<glm::vec3, 4> corners{};
  for (std::size_t corner = 0; corner < corners.size(); ++corner) {
    corners[corner] = {radius * CosOfDegrees(kCornerDegrees[corner]), 0.0F,
                       radius * SinOfDegrees(kCornerDegrees[corner])};
  }
  // ⚠ LES COINS SE TOURNENT AVANT D'ÊTRE PORTÉS À L'ANCRE. Le client tourne un
  // carré centré sur l'origine, puis ajoute l'ancre aux quatre. Porter d'abord
  // ferait tourner le grain AUTOUR de son nœud au lieu de sur lui-même.
  //
  // ⚠ ET L'ANCRE EST DANS LE REPÈRE DU CLIENT, comme les coins tournés : le grain
  // y naît soixante à quatre-vingts unités AU-DESSUS de son nœud, à une
  // ordonnée négative, et redescend vers lui. Coins tournés par les angles de
  // la caméra du client et ancre reviennent ENSEMBLE au nôtre, par `S` entier.
  RotateEffectQuadCorners(corners, turn_degrees, tilt_degrees);
  for (glm::vec3& corner : corners) {
    corner = EffectPointFromClient(corner + anchor);
  }
  return corners;
}

}  // namespace

bool EffectRainParticleTintFor(std::uint8_t mark, std::array<std::uint8_t, 3>& tint) {
  switch (mark) {
    case kMarkOfWhiteGrain:
      tint = {kFullChannel, kFullChannel, kFullChannel};
      return true;
    case kMarkOfPinkGrain:
      tint = {kFullChannel, kPinkSide, kPinkSide};
      return true;
    case kMarkOfRedGrain:
      tint = {kFullChannel, kRedSide, kRedSide};
      return true;
    default:
      // ⚠ LE CLIENT N'A PAS DE CAS PAR DÉFAUT : son choix ne couvre que ces
      // trois marques, et il saute alors l'émission du grain. Lui inventer une
      // teinte montrerait un grain que le client ne montre pas.
      return false;
  }
}

std::vector<EffectWorldQuad> ComposeRainParticleQuads(const EffectTrack& track,
                                                      std::int16_t turn_degrees,
                                                      std::int16_t tilt_degrees,
                                                      glm::vec3 node_origin,
                                                      const EffectOwnerTint& owner) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active) {
    return quads;
  }
  // ⚠ L'ALPHA SE BORNE À ZÉRO PAR LE BAS ET LA PISTE SE DESSINE QUAND MÊME : le
  // client n'en fait pas une condition d'émission. C'est le MOUVEMENT qui éteint
  // la piste quand l'alpha passe sous zéro, pas la figure.
  const auto alpha = static_cast<std::uint8_t>(std::clamp<std::int16_t>(track.alpha, 0, 255));

  std::array<std::uint8_t, 3> tint_color{};
  if (EffectRainParticleTintFor(track.mark[kMarkOfRainParticleTint], tint_color)) {
    EffectParticleQuadRequest grain;
    grain.corners =
        AnchoredSquare(track.radius_at_anchor, track.anchor, turn_degrees, tilt_degrees);
    grain.alpha = alpha;
    grain.color = tint_color;
    grain.blend_mode = kAddedBlend;
    grain.depth = EffectQuadDepth::kFrontmost;
    quads.push_back(ComposeEffectParticleQuad(grain, node_origin, owner));
  }

  // ⚠ LE GRAIN ET LE HALO PASSENT DEVANT TOUT : voir `EffectQuadDepth`.
  // ⚠ LE HALO PARAÎT TOUJOURS, marque connue ou non : le client l'émet HORS du
  // choix de teinte. C'est lui qui donne son bord rouge au grain, et c'est le
  // seul quad d'une piste dont la marque sortirait des trois nommées.
  EffectParticleQuadRequest halo;
  halo.corners = AnchoredSquare(track.radius_at_anchor * kRainParticleHaloScale, track.anchor,
                                turn_degrees, tilt_degrees);
  halo.alpha = alpha;
  halo.color = kRainParticleHaloTint;
  halo.blend_mode = kAddedBlend;
  halo.depth = EffectQuadDepth::kFrontmost;
  quads.push_back(ComposeEffectParticleQuad(halo, node_origin, owner));
  return quads;
}

}  // namespace ro::render
