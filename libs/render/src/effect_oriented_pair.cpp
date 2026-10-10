#include "ro/render/effect_oriented_pair.h"

#include <cmath>
#include <numbers>

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

[[nodiscard]] float DegreesToRadians(float degrees) {
  return degrees * std::numbers::pi_v<float> / 180.0F;
}

// OÙ LA FIGURE LIT SES DEUX ALPHAS et sa seconde position.
constexpr std::size_t kSlotOfFirstAlpha = 12;
constexpr std::size_t kSlotOfSecondAlpha = 13;
// ⚠ LA PREMIÈRE POSITION VIENT DE LA QUEUE, pas de l'ancre : les cases trois,
// quatre et cinq. La seconde vient de l'ancre de la piste.
constexpr std::size_t kSlotOfTailX = 3;
constexpr std::size_t kSlotOfTailY = 4;
constexpr std::size_t kSlotOfTailZ = 5;

// LE MÉLANGE DU QUAD PÂLE, et celui du quad franc.
constexpr std::int16_t kPaleBlend = 1;
constexpr std::int16_t kPlainBlend = 0;
// LA TEINTE DU QUAD FRANC, par marque : rouge pour la marque un, bleu sinon.
[[nodiscard]] std::array<std::uint8_t, 3> PlainQuadTint(std::uint8_t mark) {
  return mark == 1 ? std::array<std::uint8_t, 3>{255, 50, 50}
                   : std::array<std::uint8_t, 3>{50, 50, 255};
}

}  // namespace

std::vector<EffectWorldQuad> ComposeOrientedPairQuads(const EffectOrientedPairRequest& request,
                                                      glm::vec3 node_origin) {
  std::vector<EffectWorldQuad> quads;
  if (request.alpha <= 0 || request.radius == 0.0F) {
    return quads;
  }
  const bool retourne =
      request.mark == kOrientedPairFlippedMark || request.mark == kOrientedPairFlippedOnceMark;
  // ⚠ LA MARQUE TROIS EST MUETTE À LA SECONDE POSITION. La première la traite
  // comme la deux ; la seconde l'écarte.
  if (request.is_second_position && request.mark == kOrientedPairFlippedOnceMark) {
    return quads;
  }

  // LES QUATRE COINS, à quarante-cinq degrés et par quarts de tour.
  std::array<glm::vec3, 4> corners;
  for (std::size_t corner = 0; corner < corners.size(); ++corner) {
    const float theta = DegreesToRadians(
        static_cast<float>(kOrientedPairFirstCornerDegrees +
                           kOrientedPairDegreesPerCorner * static_cast<std::int32_t>(corner)));
    corners[corner] = {request.radius * std::cos(theta), request.radius * std::sin(theta), 0.0F};
  }
  RotateEffectQuadCorners(corners, request.turn_degrees, request.tilt_degrees);

  // ⚠ LE REPÈRE SE RETOURNE ICI, une seule fois, par `S` entier, après la
  // rotation par les angles de la caméra du client et avant la translation par
  // le centre — qui est déjà dans NOTRE repère.
  std::array<glm::vec3, 4> poses;
  for (std::size_t corner = 0; corner < corners.size(); ++corner) {
    poses[corner] = EffectPointFromClient(corners[corner]) + request.center;
  }

  const auto paint = [&](const std::array<glm::vec3, 4>& order, std::int16_t alpha,
                         const std::array<std::uint8_t, 3>& tint, std::int16_t blend) {
    EffectParticleQuadRequest spawn_request;
    spawn_request.corners = order;
    spawn_request.alpha = static_cast<std::uint8_t>(alpha);
    spawn_request.color = tint;
    spawn_request.blend_mode = blend;
    // ⚠ CETTE FIGURE PASSE TOUJOURS DEVANT TOUT : voir `EffectQuadDepth`.
    spawn_request.depth = EffectQuadDepth::kFrontmost;
    quads.push_back(ComposeEffectParticleQuad(spawn_request, node_origin));
  };

  if (retourne) {
    // ⚠ UN SEUL QUAD, ET SES COINS SONT ÉCHANGÉS DEUX À DEUX : l'image se
    // retourne. Garder l'ordre ordinaire la remettrait à l'endroit, et ces deux
    // marques ne se distingueraient plus des autres.
    paint({poses[1], poses[0], poses[3], poses[2]}, request.alpha, {255, 255, 255}, kPlainBlend);
    return quads;
  }
  // LE QUAD PÂLE D'ABORD — un tiers de l'alpha, mélange un —, puis le franc.
  paint(poses, static_cast<std::int16_t>(request.alpha / kOrientedPairPaleAlphaDivisor),
        {255, 255, 255}, kPaleBlend);
  paint(poses, request.alpha, PlainQuadTint(request.mark), kPlainBlend);
  return quads;
}

std::array<EffectOrientedPairRequest, 2> EffectOrientedPairRequestsOf(const EffectTrack& track,
                                                                      std::int16_t turn_degrees,
                                                                      std::int16_t tilt_degrees) {
  std::array<EffectOrientedPairRequest, 2> requests;
  for (EffectOrientedPairRequest& spawn_request : requests) {
    spawn_request.radius = track.radius_at_anchor;
    spawn_request.turn_degrees = turn_degrees;
    spawn_request.tilt_degrees = tilt_degrees;
    spawn_request.mark = track.mark[0];
  }
  requests[0].center = {track.tail[kSlotOfTailX], track.tail[kSlotOfTailY],
                        track.tail[kSlotOfTailZ]};
  requests[0].alpha = static_cast<std::int16_t>(track.value[kSlotOfFirstAlpha]);
  requests[1].center = track.anchor;
  requests[1].alpha = static_cast<std::int16_t>(track.value[kSlotOfSecondAlpha]);
  requests[1].is_second_position = true;
  if (!track.active) {
    requests[0].alpha = 0;
    requests[1].alpha = 0;
  }
  return requests;
}

}  // namespace ro::render
