#include "ro/render/actor_pick.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include <glm/gtc/constants.hpp>

#include "ro/render/sprite_billboard.h"

namespace ro::render {
namespace {

// Élargit un intervalle trop court de part et d'autre, jusqu'à la taille
// donnée — ce que le client fait, en largeur puis en hauteur.
void Widen(float& low, float& high, float minimum) {
  const float width = high - low;
  if (width < minimum) {
    const float margin = (minimum - width) * 0.5F;
    low -= margin;
    high += margin;
  }
}

// LE CARRÉ D'UN ACTEUR EN MODÈLE : son centre passé dans la vue, et un
// demi-côté de `√(côté² / 2)` de part et d'autre, parallèle à l'écran. Le
// client ne regarde pas le modèle lui-même : c'est une boîte fixe autour de son
// origine.
std::optional<ScreenRect> ModelScreenBounds(const Camera& camera, float aspect_ratio,
                                            const DrawnActor& drawn, float viewport_width,
                                            float viewport_height) {
  const float half = std::sqrt(drawn.model_pick_side * drawn.model_pick_side * 0.5F);
  const glm::mat4 view = camera.ViewMatrix();
  const glm::mat4 projection = camera.ProjectionMatrix(aspect_ratio);
  const glm::vec4 center = view * glm::vec4(drawn.model_pick_center, 1.0F);
  ScreenRect rect;
  rect.min = glm::vec2{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
  rect.max = glm::vec2{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
  for (const glm::vec2 corner : {glm::vec2{-half, -half}, glm::vec2{half, half}}) {
    const glm::vec4 clip = projection * (center + glm::vec4(corner, 0.0F, 0.0F));
    if (clip.w <= 0.0F) {
      return std::nullopt;
    }
    const glm::vec2 normalized = glm::vec2(clip) / clip.w;
    const glm::vec2 screen{(normalized.x + 1.0F) * 0.5F * viewport_width,
                           (1.0F - normalized.y) * 0.5F * viewport_height};
    rect.min = glm::min(rect.min, screen);
    rect.max = glm::max(rect.max, screen);
  }
  return rect;
}

// Combien de points dessinent le bord d'un disque au sol. Assez pour que le
// polygone ne s'écarte du cercle que d'un pour cent de son rayon.
constexpr std::size_t kGroundDiscOutlinePoints = 24;

using GroundDiscOutline = std::array<glm::vec2, kGroundDiscOutlinePoints>;

// LE BORD DU DISQUE AU SOL, projeté à l'écran. Rien si un point du bord passe
// derrière l'œil : un disque à demi dans le dos n'a pas de contour d'écran.
std::optional<GroundDiscOutline> ProjectGroundDisc(const Camera& camera, float aspect_ratio,
                                                   const DrawnActor& drawn, float viewport_width,
                                                   float viewport_height) {
  GroundDiscOutline outline;
  for (std::size_t index = 0; index < outline.size(); ++index) {
    const float angle =
        glm::two_pi<float>() * static_cast<float>(index) / static_cast<float>(outline.size());
    const glm::vec3 edge =
        drawn.ground_position +
        glm::vec3(std::cos(angle), 0.0F, std::sin(angle)) * drawn.ground_pick_radius;
    const std::optional<glm::vec2> projected =
        ProjectToScreen(camera, aspect_ratio, edge, viewport_width, viewport_height);
    if (!projected.has_value()) {
      return std::nullopt;
    }
    outline[index] = *projected;
  }
  return outline;
}

ScreenRect BoundsOf(const GroundDiscOutline& outline) {
  ScreenRect rect{outline.front(), outline.front()};
  for (const glm::vec2 point : outline) {
    rect.min = glm::min(rect.min, point);
    rect.max = glm::max(rect.max, point);
  }
  return rect;
}

// Un disque vu en perspective reste convexe : le point est dedans s'il se
// trouve du même côté de chaque arête.
bool InsideConvex(const GroundDiscOutline& outline, glm::vec2 point) {
  bool any_positive = false;
  bool any_negative = false;
  for (std::size_t index = 0; index < outline.size(); ++index) {
    const glm::vec2 from = outline[index];
    const glm::vec2 to = outline[(index + 1) % outline.size()];
    const float side = (to.x - from.x) * (point.y - from.y) - (to.y - from.y) * (point.x - from.x);
    any_positive = any_positive || side > 0.0F;
    any_negative = any_negative || side < 0.0F;
  }
  return !(any_positive && any_negative);
}

// Le pixel tombe-t-il sur l'acteur posé au sol ? Dans son disque projeté, ou,
// si ce disque est plus étroit que le plancher, dans son rectangle élargi.
bool GroundDiscContains(const Camera& camera, float aspect_ratio, const DrawnActor& drawn,
                        glm::vec2 pointer, float viewport_width, float viewport_height) {
  const std::optional<GroundDiscOutline> outline =
      ProjectGroundDisc(camera, aspect_ratio, drawn, viewport_width, viewport_height);
  if (!outline.has_value()) {
    return false;
  }
  ScreenRect rect = BoundsOf(*outline);
  const float floor = drawn.pick_floor_pixels;
  if (rect.max.x - rect.min.x >= floor && rect.max.y - rect.min.y >= floor) {
    return InsideConvex(*outline, pointer);
  }
  Widen(rect.min.x, rect.max.x, floor);
  Widen(rect.min.y, rect.max.y, floor);
  return rect.Contains(pointer);
}

// Élargit un côté comme le client : la longueur tronquée à l'entier, la moitié
// entière du manque de part et d'autre.
void WidenWhole(float& low, float& high, int floor) {
  const auto length = static_cast<int>(high - low);
  if (length < floor) {
    const auto margin = static_cast<float>((floor - length) / 2);
    low -= margin;
    high += margin;
  }
}

// Réduit un côté autour de son milieu à la part donnée, sans descendre sous un
// demi-pixel de chaque côté : le rectangle reste bien ordonné.
constexpr float kLeastHalfSidePixels = 0.5F;

void Shrink(float& low, float& high, float share) {
  const float middle = (low + high) * 0.5F;
  const float half = std::max((high - low) * 0.5F * share, kLeastHalfSidePixels);
  low = middle - half;
  high = middle + half;
}

// Un rectangle de sprite fait de quatre pixels de large, soit deux de chaque
// côté du point, et deux de haut.
constexpr float kGroundUnitHalfWidthSpritePixels = 2.0F;
constexpr float kGroundUnitHeightSpritePixels = 2.0F;

// L'emprise des calques peints dont la partie est dans `counted_parts`. Rien si
// aucun ne l'est, ou si un coin passe derrière l'œil.
std::optional<ScreenRect> PaintedPartsScreenBounds(const Camera& camera, float aspect_ratio,
                                                   const DrawnActor& drawn,
                                                   ActorPartSet counted_parts, float viewport_width,
                                                   float viewport_height) {
  if (drawn.actor == nullptr || drawn.actor->quads.empty()) {
    return std::nullopt;
  }
  const BillboardBasis basis = MakeBillboardBasis(camera);
  ScreenRect rect;
  rect.min = glm::vec2{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
  rect.max = glm::vec2{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
  bool any_drawn = false;
  for (std::size_t index = 0; index < drawn.actor->quads.size(); ++index) {
    // ⚠ SEUL CE QUI EST PEINT COMPTE, et c'est la règle du client : il
    // n'accumule son rectangle que sur les calques qu'il a réellement soumis.
    // Un calque dont l'image n'existe pas — une pièce absente, une image de
    // taille nulle — porte quand même le décalage de son .act, et le compter
    // étire l'emprise jusque-là sur du vide.
    if (index >= drawn.actor->sources.size() || !HasImage(drawn.actor->sources[index])) {
      continue;
    }
    if ((counted_parts & PartBit(PartOfQuad(*drawn.actor, index))) == 0) {
      continue;
    }
    SpriteQuad quad = drawn.actor->quads[index];
    // Le rectangle suit ce qui est DESSINÉ : un corps rapetissé s'attrape
    // petit, un corps agrandi s'attrape grand.
    for (glm::vec2& corner : quad.corners) {
      corner *= drawn.scale;
    }
    any_drawn = true;
    const std::array<glm::vec3, 4> corners = BillboardCorners(quad, drawn.ground_position, basis);
    for (const glm::vec3& corner : corners) {
      const std::optional<glm::vec2> projected =
          ProjectToScreen(camera, aspect_ratio, corner, viewport_width, viewport_height);
      if (!projected.has_value()) {
        return std::nullopt;
      }
      rect.min = glm::min(rect.min, *projected);
      rect.max = glm::max(rect.max, *projected);
    }
  }
  // Aucun calque compté : l'acteur n'occupe RIEN, et rendre le rectangle
  // sentinelle — infini à l'envers — le rendrait attrapable partout.
  if (!any_drawn) {
    return std::nullopt;
  }
  return rect;
}

// Ce qui porte une parole : le corps et la tête, sans rien de ce qu'on enfile.
constexpr ActorPartSet kSpeakerParts = PartBit(ActorPart::kBody) | PartBit(ActorPart::kHead);

}  // namespace

float PickFloorPixels(float factor, float viewport_width, int shift) {
  const auto whole = static_cast<int>(factor * viewport_width / kPickFloorReferenceWidth);
  const int kept = std::clamp(shift, 0, kLargestPickFloorShift);
  return static_cast<float>(std::max(1, whole >> kept));
}

void WidenToWholeFloor(ScreenRect& rect, float floor_pixels) {
  const auto floor = static_cast<int>(floor_pixels);
  WidenWhole(rect.min.x, rect.max.x, floor);
  WidenWhole(rect.min.y, rect.max.y, floor);
}

std::optional<ScreenRect> GroundUnitScreenRect(const Camera& camera, float aspect_ratio,
                                               const glm::vec3& ground_point, float floor_pixels,
                                               float viewport_width, float viewport_height) {
  const std::optional<glm::vec2> foot =
      ProjectToScreen(camera, aspect_ratio, ground_point, viewport_width, viewport_height);
  if (!foot.has_value()) {
    return std::nullopt;
  }
  // L'échelle d'un pixel de sprite à ce recul, puis celle de la fenêtre : en
  // largeur rapportée à 640, en hauteur à 480.
  const float sprite_scale = ApparentScale(camera);
  const float half_width =
      kGroundUnitHalfWidthSpritePixels * sprite_scale * viewport_width / kPickFloorReferenceWidth;
  const float height =
      kGroundUnitHeightSpritePixels * sprite_scale * viewport_height / kPickHeightReferenceHeight;
  ScreenRect rect{.min = {foot->x - half_width, foot->y - height},
                  .max = {foot->x + half_width, foot->y}};
  WidenToWholeFloor(rect, floor_pixels);
  return rect;
}

std::optional<glm::vec2> ProjectToScreen(const Camera& camera, float aspect_ratio,
                                         const glm::vec3& world_point, float viewport_width,
                                         float viewport_height) {
  const glm::vec4 clip =
      camera.ProjectionMatrix(aspect_ratio) * camera.ViewMatrix() * glm::vec4(world_point, 1.0F);
  if (clip.w <= 0.0F) {
    return std::nullopt;
  }
  const glm::vec2 normalized = glm::vec2(clip) / clip.w;
  // Coordonnées normalisées : -1 à +1, l'axe des Y vers le HAUT. La fenêtre
  // compte le sien vers le bas, d'où le retournement — le même que dans
  // MakeScreenRay, dont ceci est l'inverse.
  return glm::vec2{(normalized.x + 1.0F) * 0.5F * viewport_width,
                   (1.0F - normalized.y) * 0.5F * viewport_height};
}

std::optional<ScreenRect> ActorScreenBounds(const Camera& camera, float aspect_ratio,
                                            const DrawnActor& drawn, float viewport_width,
                                            float viewport_height) {
  if (drawn.model_pick_side > 0.0F) {
    return ModelScreenBounds(camera, aspect_ratio, drawn, viewport_width, viewport_height);
  }
  return PaintedPartsScreenBounds(camera, aspect_ratio, drawn, kAllActorParts, viewport_width,
                                  viewport_height);
}

std::optional<ScreenRect> ActorSpeakerScreenBounds(const Camera& camera, float aspect_ratio,
                                                   const DrawnActor& drawn, float viewport_width,
                                                   float viewport_height) {
  if (drawn.model_pick_side > 0.0F) {
    return ModelScreenBounds(camera, aspect_ratio, drawn, viewport_width, viewport_height);
  }
  std::optional<ScreenRect> speaker = PaintedPartsScreenBounds(
      camera, aspect_ratio, drawn, kSpeakerParts, viewport_width, viewport_height);
  if (speaker.has_value()) {
    return speaker;
  }
  return PaintedPartsScreenBounds(camera, aspect_ratio, drawn, kAllActorParts, viewport_width,
                                  viewport_height);
}

std::optional<float> ActorSpriteScreenScale(const Camera& camera, float aspect_ratio,
                                            const DrawnActor& drawn, float viewport_width,
                                            float viewport_height) {
  if (drawn.model_pick_side > 0.0F || drawn.actor == nullptr || drawn.scale <= 0.0F) {
    return std::nullopt;
  }
  // LA HAUTEUR PEINTE EN PIXELS DE SPRITE, sur les mêmes calques que l'emprise.
  float top = std::numeric_limits<float>::max();
  float bottom = std::numeric_limits<float>::lowest();
  for (std::size_t index = 0; index < drawn.actor->quads.size(); ++index) {
    if (index >= drawn.actor->sources.size() || !HasImage(drawn.actor->sources[index])) {
      continue;
    }
    for (const glm::vec2& corner : drawn.actor->quads[index].corners) {
      top = std::min(top, corner.y);
      bottom = std::max(bottom, corner.y);
    }
  }
  const std::optional<ScreenRect> on_screen =
      ActorScreenBounds(camera, aspect_ratio, drawn, viewport_width, viewport_height);
  if (!on_screen.has_value() || bottom <= top) {
    return std::nullopt;
  }
  return (on_screen->max.y - on_screen->min.y) / ((bottom - top) * drawn.scale);
}

std::optional<ScreenRect> ActorScreenRect(const Camera& camera, float aspect_ratio,
                                          const DrawnActor& drawn, float viewport_width,
                                          float viewport_height) {
  std::optional<ScreenRect> rect =
      ActorScreenBounds(camera, aspect_ratio, drawn, viewport_width, viewport_height);
  if (!rect.has_value()) {
    return rect;
  }
  // Un acteur minuscule reste attrapable : c'est ce que le client garantit par
  // une taille plancher.
  Widen(rect->min.x, rect->max.x, drawn.pick_floor_pixels);
  Widen(rect->min.y, rect->max.y, drawn.pick_floor_pixels);
  // Puis la part qu'il en garde, APRÈS l'élargissement : un acteur qu'on ne
  // peut pas viser ne retrouve pas son plancher par l'autre bout.
  if (drawn.pick_share < 1.0F) {
    Shrink(rect->min.x, rect->max.x, drawn.pick_share);
    Shrink(rect->min.y, rect->max.y, drawn.pick_share);
  }
  return rect;
}

std::optional<std::size_t> PickActor(const Camera& camera, float aspect_ratio,
                                     std::span<const DrawnActor> actors, float screen_x,
                                     float screen_y, float viewport_width, float viewport_height) {
  const glm::vec3 eye = camera.Position();
  const glm::vec2 pointer{screen_x, screen_y};
  std::optional<std::size_t> picked;
  float nearest = std::numeric_limits<float>::max();
  // Ce qui est posé au sol ne sert que si rien d'autre ne tient le pixel.
  std::optional<std::size_t> picked_on_ground;
  float nearest_on_ground = std::numeric_limits<float>::max();
  for (std::size_t index = 0; index < actors.size(); ++index) {
    if (actors[index].ground_pick_radius > 0.0F) {
      if (!GroundDiscContains(camera, aspect_ratio, actors[index], pointer, viewport_width,
                              viewport_height)) {
        continue;
      }
      const float depth = glm::length(actors[index].ground_position - eye);
      if (depth < nearest_on_ground) {
        nearest_on_ground = depth;
        picked_on_ground = index;
      }
      continue;
    }
    const std::optional<ScreenRect> rect =
        ActorScreenRect(camera, aspect_ratio, actors[index], viewport_width, viewport_height);
    if (!rect.has_value() || !rect->Contains(pointer)) {
      continue;
    }
    const float depth = glm::length(actors[index].ground_position - eye) + actors[index].pick_bias;
    if (depth < nearest) {
      nearest = depth;
      picked = index;
    }
  }
  return picked.has_value() ? picked : picked_on_ground;
}

}  // namespace ro::render
