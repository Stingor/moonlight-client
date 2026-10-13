#include "ro/render/effect_scene.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "ro/formats/str.h"
#include "ro/render/effect_node.h"

namespace ro::render {
namespace {

// La couleur du fichier va de zéro à 255, en flottant.
constexpr float kMaxColor = 255.0F;

std::uint8_t Clamped(float channel) {
  return static_cast<std::uint8_t>(std::clamp(channel, 0.0F, kMaxColor));
}

// Le quart de cercle qu'il faut pour passer des degrés aux radians.
float Radians(float degrees) { return degrees * std::numbers::pi_v<float> / 180.0F; }

}  // namespace

std::vector<EffectQuad> ComposeEffect(const PreparedEffect& effect, std::int32_t frame) {
  std::vector<EffectQuad> quads;
  ComposeEffect(effect, frame, quads);
  return quads;
}

void ComposeEffect(const PreparedEffect& effect, std::int32_t frame,
                   std::vector<EffectQuad>& quads) {
  quads.clear();
  quads.reserve(effect.layers.size());

  for (const PreparedEffectLayer& layer : effect.layers) {
    if (layer.texture_count == 0) {
      continue;
    }
    const std::optional<EffectLayerFrame> shown = EffectLayerAt(layer, frame, effect.last_frame);
    if (!shown.has_value()) {
      continue;
    }
    // Une couche totalement transparente ne se peint pas : les effets
    // s'ouvrent et se ferment ainsi, et peindre un quad invisible coûte autant
    // qu'un visible.
    if (shown->color[3] <= 0.0F) {
      continue;
    }

    EffectQuad quad;
    quad.texture = layer.first_texture + std::min(shown->texture_index, layer.texture_count - 1);
    quad.uv_min = glm::vec2{shown->uv[0], shown->uv[1]};
    quad.uv_max = glm::vec2{shown->uv[0] + shown->uv[2], shown->uv[1] + shown->uv[3]};
    quad.tint = {Clamped(shown->color[0]), Clamped(shown->color[1]), Clamped(shown->color[2]),
                 Clamped(shown->color[3])};
    quad.blend_source = shown->blend_source;
    quad.blend_destination = shown->blend_destination;

    // Les quatre ABSCISSES viennent avant les quatre ordonnées, et l'ordre des
    // coins est celui d'un quad de sprite. Le centre de la couche est décalé
    // de l'origine d'écran du format : c'est ce qui ramène des positions
    // absolues autour du point où l'effet est joué.
    const float center_x = shown->offset_x - formats::kStrOriginX;
    const float center_y = shown->offset_y - formats::kStrOriginY;
    // L'ordonnée DESCEND ici comme dans le fichier : l'angle tourne donc dans
    // le sens des aiguilles d'une montre, et le sinus ne change pas de signe.
    const float angle = Radians(shown->angle_degrees);
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    for (std::size_t corner = 0; corner < quad.corners.size(); ++corner) {
      const float x = shown->corners[corner];
      const float y = shown->corners[corner + 4];
      quad.corners[corner] =
          glm::vec2{center_x + x * cosine - y * sine, center_y + x * sine + y * cosine};
    }
    quads.push_back(quad);
  }
}

EffectQuad ComposeEffectNodeBillboard(const EffectNode& node, glm::vec2 anchor) {
  const float spin = Radians(node.spin_degrees);
  const float cosine = std::cos(spin);
  const float sine = std::sin(spin);

  // L'ANCRAGE : le quad est centré sur son point, ou il y est PENDU par son
  // bord haut. Le client obtient le second en doublant la demi-hauteur vers le
  // bas plutôt qu'en décalant le centre.
  const bool hangs = (node.flags & kEffectNodeAnchorsAtTop) != 0;
  const float half_height = node.half_height.half_size;
  const float top = hangs ? 0.0F : half_height;
  const float bottom = hangs ? -2.0F * half_height : -half_height;
  const float half_width = node.half_width.half_size;

  const glm::vec2 center = anchor + glm::vec2{node.orbit_x, node.orbit_y};
  const auto place = [&](float along_u, float along_v) {
    return center + glm::vec2{along_u * sine - along_v * cosine, along_u * cosine + along_v * sine};
  };

  EffectQuad quad;
  // L'ordre des coins est celui d'un quad de sprite, et il suit la texture :
  // haut-gauche en (0,0), puis dans le sens des aiguilles.
  quad.corners = {place(-half_width, bottom), place(half_width, bottom), place(half_width, top),
                  place(-half_width, top)};
  quad.uv_min = glm::vec2{0.0F, 0.0F};
  quad.uv_max = glm::vec2{1.0F, 1.0F};
  quad.tint = {Clamped(node.color[0]), Clamped(node.color[1]), Clamped(node.color[2]),
               EffectNodeAlpha(node)};
  quad.blend_source = kEffectNodeBlendSource;
  quad.blend_destination = kEffectNodeBlendDestination;
  quad.texture = static_cast<std::size_t>(std::max(node.frame, 0));
  return quad;
}

}  // namespace ro::render
