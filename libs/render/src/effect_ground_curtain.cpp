#include "ro/render/effect_ground_curtain.h"

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LES DEUX BOUTS DU SEGMENT AU SOL : l'ancre d'un côté, deux cases de queue de
// l'autre — la QUATRIÈME et la SIXIÈME. Le client saute celle du milieu, qui
// tiendrait la place d'une hauteur : les trois cases se lisent donc comme un
// point, dont le y ne sert pas.
constexpr std::size_t kSlotOfFarX = 3;
constexpr std::size_t kSlotOfFarZ = 5;
// LA CASE QUI PORTE LE STYLE, et dont la figure tire sa teinte.
constexpr std::size_t kSlotOfStyle = 0;

constexpr std::int16_t kBlendAdditive = 0;
constexpr std::int16_t kBlendBySourceAlpha = 1;

}  // namespace

std::array<std::uint8_t, 3> EffectGroundCurtainColor(float style) {
  if (style == 1.0F) {
    return kGroundCurtainPink;
  }
  if (style == 2.0F) {
    return kGroundCurtainGreen;
  }
  if (style == 3.0F) {
    return kGroundCurtainYellow;
  }
  if (style == 4.0F) {
    return kGroundCurtainSalmon;
  }
  return kGroundCurtainPlain;
}

std::int16_t EffectGroundCurtainBlendMode(float style) {
  const bool blend =
      style >= kGroundCurtainFirstBlendedStyle && style <= kGroundCurtainLastBlendedStyle;
  return blend ? kBlendBySourceAlpha : kBlendAdditive;
}

std::optional<EffectWorldQuad> ComposeGroundCurtainQuad(const EffectTrack& track,
                                                        glm::vec3 anchor) {
  if (!track.active || track.alpha <= 0) {
    return std::nullopt;
  }
  const float style = track.value[kSlotOfStyle];
  // ⚠ NOTRE REPÈRE MONTE, CELUI DU CLIENT DESCEND : là où il retranche la clé
  // pour dresser sa tenture, nous l'ajoutons. Recopier le signe la planterait
  // sous le sol, où rien ne la verrait.
  const float top = track.lift_key;
  const glm::vec3 pres{track.anchor.x, 0.0F, track.anchor.z};
  const glm::vec3 far_point{track.tail[kSlotOfFarX], 0.0F, track.tail[kSlotOfFarZ]};

  EffectParticleQuadRequest request;
  // LES QUATRE COINS EN FAISANT LE TOUR : le pied proche, le pied lointain, la
  // tête lointaine, la tête proche.
  request.corners = {pres, far_point, far_point + glm::vec3{0.0F, top, 0.0F},
                     pres + glm::vec3{0.0F, top, 0.0F}};
  request.alpha = static_cast<std::uint8_t>(track.alpha);
  request.color = EffectGroundCurtainColor(style);
  request.blend_mode = EffectGroundCurtainBlendMode(style);
  // ⚠ L'IMAGE EST RETOURNÉE : le client donne un aux deux premiers coins et
  // zéro aux deux derniers, là où le défaut fait l'inverse.
  request.v_at_a_and_b = 1.0F;
  request.v_at_c_and_d = 0.0F;
  return ComposeEffectParticleQuad(request, anchor);
}

}  // namespace ro::render
