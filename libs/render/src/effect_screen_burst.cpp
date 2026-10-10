#include "ro/render/effect_screen_burst.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "ro/render/map_effect_math.h"

namespace ro::render {
namespace {

// LES DEUX FIGURES INDEXENT DES TABLES DE DEGRÉS ENTIERS, comme partout dans le
// client : l'angle se TRONQUE avant d'être employé, il ne s'arrondit pas, et un
// négatif s'y lit un degré de travers — voir `NativeTableRadians`.
[[nodiscard]] float CosOfTruncatedDegrees(float degrees) {
  return std::cos(NativeTableRadians(static_cast<std::int32_t>(std::trunc(degrees))));
}

[[nodiscard]] float SinOfTruncatedDegrees(float degrees) {
  return std::sin(NativeTableRadians(static_cast<std::int32_t>(std::trunc(degrees))));
}

[[nodiscard]] std::array<std::uint8_t, 4> TintOf(const EffectScreenNode& node) {
  return {static_cast<std::uint8_t>(std::clamp(node.color[0], 0, 255)),
          static_cast<std::uint8_t>(std::clamp(node.color[1], 0, 255)),
          static_cast<std::uint8_t>(std::clamp(node.color[2], 0, 255)),
          static_cast<std::uint8_t>(std::clamp(static_cast<std::int32_t>(node.alpha), 0, 255))};
}

// ⚠ UN quand rien ne dit autre chose, et c'est ce que les deux dessinateurs
// passent en dur à la primitive. Ils ne consultent pas le mode du nœud.
constexpr std::int32_t kScreenBurstBlendMode = 1;

// ⚠ ET LA PREMIÈRE IMAGE, TOUJOURS. Les deux déréférencent la liste de textures
// du nœud une seule fois, sans index : ces figures n'animent rien, au contraire
// du quad dont le mouvement avance l'image.
constexpr std::size_t kScreenBurstTexture = 0;

// UN POINT DE LA COURONNE : l'abscisse gagne le cosinus, l'ordonnée PERD le
// sinus — le repère de l'écran descend.
[[nodiscard]] glm::vec2 RingPoint(glm::vec2 centre, float degrees, float radius) {
  return {centre.x + CosOfTruncatedDegrees(degrees) * radius,
          centre.y - SinOfTruncatedDegrees(degrees) * radius};
}

}  // namespace

glm::vec2 EffectScreenBurstCenter(const EffectScreenNode& node, glm::vec2 owner_screen_position) {
  if (HasEffectScreenFlag(node.flags, EffectScreenFlag::kScreenAbsolute)) {
    return node.screen_offset;
  }
  return owner_screen_position + node.screen_offset;
}

std::vector<EffectScreenQuad> ComposeScreenRingQuads(const EffectScreenNode& node,
                                                     glm::vec2 owner_screen_position) {
  std::vector<EffectScreenQuad> bands;
  const float step = node.angle_degrees;
  if (step < kScreenRingSmallestStep) {
    return bands;
  }
  const glm::vec2 centre = EffectScreenBurstCenter(node, owner_screen_position);
  const float outer_radius = node.outer_radius;
  // ⚠ LA LARGEUR SE RETRANCHE AU RAYON : c'est une ÉPAISSEUR, pas un second
  // rayon. Quand elle vaut le rayon entier, le bord intérieur tombe au centre
  // et la couronne devient un disque plein.
  const float inner_radius = outer_radius - node.band_width;
  const std::array<std::uint8_t, 4> tint = TintOf(node);

  float abscisse = 0.0F;
  for (float angle = 0.0F; angle <= kScreenRingFullTurnDegrees - step; angle += step) {
    const float next = angle + step;
    EffectScreenQuad band;
    // EN CYCLE : le bord intérieur à l'angle, puis au suivant, puis le bord
    // extérieur au suivant et à l'angle. Recopier l'ordre des deux triangles du
    // client y ferait un papillon.
    band.corners = {RingPoint(centre, angle, inner_radius), RingPoint(centre, next, inner_radius),
                    RingPoint(centre, next, outer_radius), RingPoint(centre, angle, outer_radius)};
    // ⚠ L'ORDONNÉE DE TEXTURE VAUT UN AU BORD INTÉRIEUR et zéro à l'extérieur —
    // l'inverse de ce que la lecture d'un anneau suggère. L'image est donc vue
    // du centre vers le dehors.
    band.texture_uv = {
        glm::vec2{abscisse, 1.0F}, glm::vec2{abscisse + kScreenRingTextureStep, 1.0F},
        glm::vec2{abscisse + kScreenRingTextureStep, 0.0F}, glm::vec2{abscisse, 0.0F}};
    band.tint = tint;
    band.blend_mode = kScreenBurstBlendMode;
    band.texture = kScreenBurstTexture;
    bands.push_back(band);

    // ⚠ L'ABSCISSE REBOUCLE À UN, pas au tour : le client la remet à zéro dès
    // qu'elle l'atteint, si bien que l'image se répète tous les quatre segments
    // quel que soit le pas.
    abscisse += kScreenRingTextureStep;
    if (abscisse >= 1.0F) {
      abscisse = 0.0F;
    }
  }
  return bands;
}

EffectScreenQuad ComposeScreenWedgeQuad(const EffectScreenNode& node,
                                        glm::vec2 owner_screen_position) {
  const glm::vec2 centre = EffectScreenBurstCenter(node, owner_screen_position);
  const float left_edge_degrees = node.turn_degrees - node.angle_degrees;
  const float right_edge_degrees = node.turn_degrees + node.angle_degrees;
  const float radius = node.half_height;

  // 🔴 COSINUS EN ABSCISSE, SINUS EN ORDONNÉE, ET L'ORDONNÉE PERD LE SIEN :
  // l'éclat pointe vers la DROITE à l'angle zéro, comme un point de la couronne.
  // Le dessinateur lit deux tables dont les noms sont échangés — celle qu'on
  // croirait du sinus rend le cosinus —, et les prendre au mot le faisait
  // pointer vers le haut, un quart de tour trop tôt.
  const auto on_arc = [centre, radius](float degrees) {
    return glm::vec2{centre.x + CosOfTruncatedDegrees(degrees) * radius,
                     centre.y - SinOfTruncatedDegrees(degrees) * radius};
  };

  EffectScreenQuad shard;
  // LE QUATRIÈME COIN REPREND LA POINTE : le second triangle est d'aire nulle,
  // et la figure reste le triangle que le client soumet.
  shard.corners = {centre, on_arc(left_edge_degrees), on_arc(right_edge_degrees), centre};
  shard.texture_uv = {glm::vec2{kScreenWedgeTipU, kScreenWedgeTipV},
                      glm::vec2{1.0F, kScreenWedgeBaseV}, glm::vec2{0.0F, kScreenWedgeBaseV},
                      glm::vec2{kScreenWedgeTipU, kScreenWedgeTipV}};
  shard.tint = TintOf(node);
  shard.blend_mode = kScreenBurstBlendMode;
  shard.texture = kScreenBurstTexture;
  return shard;
}

}  // namespace ro::render
