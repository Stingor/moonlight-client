#include "ro/render/effect_flying_shard.h"

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

constexpr std::int16_t kBlendAdditive = 0;
constexpr std::int16_t kBlendBySourceAlpha = 1;

// LES TROIS MARQUES QUE LE CLIENT COMPARE, une à une.
constexpr std::uint8_t kMarkGold = 0;
constexpr std::uint8_t kMarkWhite = 1;

}  // namespace

std::array<std::uint8_t, 3> EffectFlyingShardColor(std::uint8_t mark) {
  if (mark == kMarkGold) {
    return kFlyingShardGold;
  }
  if (mark == kMarkWhite) {
    return kFlyingShardWhite;
  }
  if (mark == kFlyingShardMarkInFront) {
    return kFlyingShardSky;
  }
  return kFlyingShardPale;
}

std::int16_t EffectFlyingShardBlendMode(std::uint8_t mark) {
  // ⚠ SEULE LA QUATRIÈME TEINTE SE MÉLANGE PAR L'ALPHA ; les trois autres
  // s'ajoutent. Et c'est la seule des quatre qu'aucun de nos quatre sites ne
  // demande — elle est transcrite parce qu'elle est mesurée.
  return mark >= 3 ? kBlendBySourceAlpha : kBlendAdditive;
}

std::optional<EffectWorldQuad> ComposeFlyingShardQuad(const EffectTrack& track, glm::vec3 anchor) {
  if (!track.active || track.alpha <= 0) {
    return std::nullopt;
  }
  const float half = track.value[kFlyingShardSlotOfHalfWidth];
  // ⚠ LA MARQUE EST UN ÉLANCEMENT, et elle se lit SIGNÉE : le client la prend
  // dans un `char`. Vingt-quatre demi-largeurs de long pour un éclat, seize
  // pour l'autre — c'est ce qui en fait un trait et non un carré.
  const auto elancement =
      static_cast<float>(static_cast<std::int8_t>(track.mark[kFlyingShardMarkOfLength]));
  const float far_point = track.radius_at_anchor + elancement * half;
  const float pres = track.radius_at_anchor - elancement * half;

  // LES DEUX ANGLES : l'un oriente le barreau DANS son plan, l'autre dit où ce
  // plan se dresse autour du nœud.
  const float le_long_x = CosOfDegrees(track.degrees);
  const float le_long_y = SinOfDegrees(track.degrees);
  const float plan_x = CosOfDegrees(track.tilt_degrees);
  const float plan_z = SinOfDegrees(track.tilt_degrees);

  const auto point = [&](float radius, float cote) {
    const float au_large = le_long_x * radius + cote * half * le_long_y;
    const float upward = le_long_y * radius - cote * half * le_long_x;
    // ⚠ NOTRE REPÈRE MONTE, CELUI DU CLIENT DESCEND : seule la hauteur change
    // de signe, la distance au centre reste ce qu'elle est.
    return glm::vec3{au_large * plan_x, -upward, au_large * plan_z};
  };

  EffectParticleQuadRequest request;
  // LES QUATRE COINS EN FAISANT LE TOUR : les deux du bout lointain, puis les
  // deux du bout proche.
  request.corners = {point(far_point, 1.0F), point(far_point, -1.0F), point(pres, -1.0F),
                     point(pres, 1.0F)};
  request.alpha = static_cast<std::uint8_t>(track.alpha);
  request.color = EffectFlyingShardColor(track.mark[kFlyingShardMarkOfColor]);
  request.blend_mode = EffectFlyingShardBlendMode(track.mark[kFlyingShardMarkOfColor]);
  // ⚠ L'IMAGE EST RETOURNÉE : le client donne un aux deux premiers coins et
  // zéro aux deux derniers, là où le défaut fait l'inverse.
  request.v_at_a_and_b = 1.0F;
  request.v_at_c_and_d = 0.0F;
  request.depth = track.mark[kFlyingShardMarkOfColor] == kFlyingShardMarkInFront
                      ? EffectQuadDepth::kFrontmost
                      : EffectQuadDepth::kProjected;
  return ComposeEffectParticleQuad(request, anchor);
}

}  // namespace ro::render
