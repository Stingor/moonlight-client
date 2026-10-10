#include "ro/render/effect_particle_quad.h"

namespace ro::render {
namespace {

// LES MODES QUE LE CLIENT TESTE NOMMÉMENT à l'émission d'un carré de
// particule, et les facteurs qu'ils posent.
constexpr std::int16_t kModeThatPaintsOpaque = 10;
constexpr std::int16_t kModeThatForcesOrdinaryBlend = 99;

// Les facteurs, dans la numérotation de Direct3D : un pour la source pleine,
// deux pour la destination pleine, cinq pour l'alpha de la source, six pour son
// complément.
constexpr std::int32_t kBlendFactorSourceColor = 2;
constexpr std::int32_t kBlendFactorNone = 1;
constexpr std::int32_t kBlendFactorSourceAlpha = 5;
constexpr std::int32_t kBlendFactorAdd = 2;
constexpr std::int32_t kBlendFactorOrdinary = 6;

// La destination qu'un mode pose de lui-même, quand le propriétaire n'en impose
// aucune : l'ADDITION pour le mode zéro, le mélange ordinaire pour les autres.
[[nodiscard]] std::int32_t DestinationOfMode(std::int16_t blend_mode) {
  return blend_mode != 0 ? kBlendFactorOrdinary : kBlendFactorAdd;
}

// Un canal, écrasé par le propriétaire quand il en pose un.
[[nodiscard]] std::uint8_t ChannelOrOwner(std::uint8_t channel, std::int32_t owner) {
  return owner == kEffectOwnerTintAbsent ? channel : static_cast<std::uint8_t>(owner);
}

}  // namespace

EffectBlendPair EffectParticleBlend(std::int16_t blend_mode, std::int32_t owner_destination) {
  if (blend_mode == kModeThatPaintsOpaque) {
    return {kBlendFactorSourceColor, kBlendFactorNone};
  }
  if (blend_mode == kModeThatForcesOrdinaryBlend) {
    // ⚠ CE MODE IGNORE LE PROPRIÉTAIRE : le client le teste APRÈS avoir lu
    // l'écrasement, et lui donne le pas.
    return {kBlendFactorSourceAlpha, kBlendFactorOrdinary};
  }
  const std::int32_t destination = owner_destination == kEffectOwnerTintAbsent
                                       ? DestinationOfMode(blend_mode)
                                       : owner_destination;
  return {kBlendFactorSourceAlpha, destination};
}

EffectWorldQuad ComposeEffectParticleQuad(const EffectParticleQuadRequest& request,
                                          glm::vec3 node_origin, const EffectOwnerTint& owner) {
  const glm::vec3 anchor = request.absolute ? glm::vec3{0.0F, 0.0F, 0.0F} : node_origin;
  const EffectBlendPair blend = EffectParticleBlend(request.blend_mode, owner.blend_destination);

  EffectWorldQuad quad;
  for (std::size_t corner = 0; corner < quad.corners.size(); ++corner) {
    quad.corners[corner] = anchor + request.corners[corner];
  }
  quad.texture_uv = {glm::vec2{request.u_at_a_and_d, request.v_at_a_and_b},
                     glm::vec2{request.u_at_b_and_c, request.v_at_a_and_b},
                     glm::vec2{request.u_at_b_and_c, request.v_at_c_and_d},
                     glm::vec2{request.u_at_a_and_d, request.v_at_c_and_d}};
  quad.tint = {ChannelOrOwner(request.color[0], owner.red),
               ChannelOrOwner(request.color[1], owner.green),
               ChannelOrOwner(request.color[2], owner.blue), request.alpha};
  quad.blend_source = blend.source;
  quad.blend_destination = blend.destination;
  quad.texture = request.texture;
  quad.depth = request.depth;
  // Le recentrage se fait sur l'origine du nœud, que les coins soient absolus
  // ou non.
  quad.screen_anchor = node_origin;
  return quad;
}

}  // namespace ro::render
