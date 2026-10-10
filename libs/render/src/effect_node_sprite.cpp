#include "ro/render/effect_node_sprite.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "ro/render/effect_node.h"
#include "ro/render/map_effect_math.h"

namespace ro::render {
namespace {

[[nodiscard]] std::uint8_t Clamped(float channel) {
  return static_cast<std::uint8_t>(std::clamp(channel, 0.0F, kEffectNodeChannelMax));
}

// Le π du client, à six décimales, et le demi-tour : son angle réel se convertit
// par `degrés / 180 × π`, dans cet ordre.
constexpr float kClientPi = 3.141592F;
constexpr float kHalfTurnDegrees = 180.0F;

}  // namespace

std::vector<SpriteQuad> ComposeEffectNodeSprite(const EffectNode& node,
                                                const formats::SprSprite& sprite,
                                                const formats::ActAnimation& animation) {
  const auto action_index = static_cast<std::size_t>(std::max(node.action, 0));
  const auto frame_index = static_cast<std::size_t>(std::max(node.frame, 0));
  // 🔴 L'ANGLE DE CHAQUE CALQUE TOURNE SON QUAD, et celui du nœud s'y ajoute plus
  // bas. C'est l'inverse d'un acteur, dont le jeu redresse les calques : un effet
  // les dessine tournés, et les gouttelettes d'Aqua Benedicta — vingt calques
  // tournés chacun de son angle — sortiraient sinon toutes droites. Ce qui n'est
  // pas un nœud d'effet garde la règle de l'acteur : voir
  // `EffectNode::draws_layer_angles`.
  std::vector<SpriteQuad> quads = BuildFrameQuads(sprite, animation, action_index, frame_index,
                                                  /*apply_rotation=*/node.draws_layer_angles);

  // L'ÉCHELLE DU NŒUD multiplie tout : le décalage d'un calque comme sa taille.
  // Le client la range à part de celle des calques et les compose ; la poser
  // sur les coins déjà composés revient au même et garde le sprite ignorant de
  // l'effet qui le porte.
  const std::array<std::uint8_t, 4> tint = {Clamped(node.color[0]), Clamped(node.color[1]),
                                            Clamped(node.color[2]), EffectNodeAlpha(node)};
  const bool keeps_layer_color = (node.flags & kEffectNodeKeepsLayerColor) != 0;

  // LA ROTATION D'ÉCRAN, quand le nœud se dessine tourné. Les coins sont en
  // pixels de sprite, l'ordonnée vers le BAS : un angle positif tourne donc
  // l'image dans le sens des aiguilles d'une montre à l'écran, et c'est la
  // convention de `ScreenRotationOfDirection`.
  //
  // 🔴 AUTOUR DU CENTRE DU QUAD, ET NON DE L'ANCRE DU SPRITE. C'est la file de
  // rendu du client qui tourne, pas le calcul des coins : sa branche tournée
  // prend la demi-hauteur et la demi-largeur du quad, en compose le CENTRE —
  // `(haut + demi_hauteur)`,
  // `(gauche + demi_largeur)` —, puis pose les quatre coins à ce centre plus ou
  // moins la demi-étendue tournée.
  //
  // ⚠ ET UN QUAD N'EST PAS CENTRÉ SUR SON ANCRE. Le calque de `fireball` est à
  // seize pixels sous elle : tourner autour de l'ancre enverrait ce centre sur
  // le côté, donc REMONTERAIT la boule de seize pixels fois son échelle — trois
  // unités du monde, un cinquième de sa hauteur de naissance — et la décalerait
  // d'autant latéralement.
  //
  // ⚠ ET L'ANGLE SE TRONQUE AU DEGRÉ, lu dans les tables du client — un degré de
  // travers quand il est négatif : voir `NativeTableRadians`. SAUF pour le nœud
  // que son dessinateur tourne d'un angle RÉEL, la munition en sprite d'un
  // monstre : voir `EffectNode::spins_by_exact_degrees`.
  const float radians = node.spins_by_exact_degrees
                            ? node.spin_degrees / kHalfTurnDegrees * kClientPi
                            : NativeTableRadians(static_cast<std::int32_t>(node.spin_degrees));
  const float cosinus = std::cos(radians);
  const float sinus = std::sin(radians);

  for (SpriteQuad& quad : quads) {
    for (glm::vec2& corner : quad.corners) {
      corner *= node.sprite_scale;
    }
    if (node.draws_rotated) {
      glm::vec2 centre{0.0F, 0.0F};
      for (const glm::vec2& corner : quad.corners) {
        centre += corner;
      }
      centre /= static_cast<float>(quad.corners.size());
      for (glm::vec2& corner : quad.corners) {
        const glm::vec2 offset = corner - centre;
        corner = centre + glm::vec2{offset.x * cosinus - offset.y * sinus,
                                    offset.x * sinus + offset.y * cosinus};
      }
    }
    if (!keeps_layer_color) {
      quad.tint = tint;
    }
  }
  return quads;
}

}  // namespace ro::render
