#include "ro/ui/gauge_fill.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace ro::ui {
namespace {

// En dessous d'un demi-pixel, ImGui peint un coin droit plutôt qu'un arc.
constexpr float kLeastDrawnRounding = 0.5F;
// Ce qu'ImGui retranche à la moitié du plus petit côté pour borner un rayon.
constexpr float kRoundingSideMargin = 1.0F;

// LES REPÈRES DU CERCLE EN DOUZIÈMES DE TOUR, ceux de `PathArcToFast` : zéro
// vers la droite, puis dans le sens des aiguilles d'une montre à l'écran — y
// descend.
constexpr int kTwelfthsRight = 0;
constexpr int kTwelfthsDown = 3;
constexpr int kTwelfthsLeft = 6;
constexpr int kTwelfthsUp = 9;
constexpr int kTwelfthsFullTurn = 12;

// Deux sommets plus proches que cela n'en font qu'un. ⚠ UN SOMMET DOUBLE DONNE
// UNE ARÊTE NULLE, dont le fondu d'ImGui ne sait pas tirer de normale.
constexpr float kSamePointPixels = 1.0e-3F;

// L'axe d'une coupe.
enum class Axis : std::uint8_t { kX, kY };

[[nodiscard]] float Along(ImVec2 point, Axis axis) { return axis == Axis::kX ? point.x : point.y; }

// LA COUPE D'UN POLYGONE CONVEXE PAR UN DEMI-PLAN, à la Sutherland-Hodgman :
// on garde ce qui est du côté `keep_above` de `bound` sur `axis`, et chaque
// arête qui traverse la limite y laisse son point de passage.
void ClipAgainst(const std::vector<ImVec2>& from, std::vector<ImVec2>& into, Axis axis, float bound,
                 bool keep_above) {
  into.clear();
  const std::size_t count = from.size();
  const auto inside = [axis, bound, keep_above](ImVec2 point) {
    return keep_above ? Along(point, axis) >= bound : Along(point, axis) <= bound;
  };
  const auto crossing = [axis, bound](ImVec2 from_point, ImVec2 to_point) {
    const float start = Along(from_point, axis);
    const float part = (bound - start) / (Along(to_point, axis) - start);
    if (axis == Axis::kX) {
      return ImVec2(bound, from_point.y + (to_point.y - from_point.y) * part);
    }
    return ImVec2(from_point.x + (to_point.x - from_point.x) * part, bound);
  };
  for (std::size_t rank = 0; rank < count; ++rank) {
    const ImVec2 previous = from[(rank + count - 1) % count];
    const ImVec2 current = from[rank];
    const bool was_inside = inside(previous);
    if (inside(current)) {
      if (!was_inside) {
        into.push_back(crossing(previous, current));
      }
      into.push_back(current);
    } else if (was_inside) {
      into.push_back(crossing(previous, current));
    }
  }
}

[[nodiscard]] bool SamePoint(ImVec2 left, ImVec2 right) {
  return std::abs(left.x - right.x) < kSamePointPixels &&
         std::abs(left.y - right.y) < kSamePointPixels;
}

// LE CONTOUR DE LA FORME, sans aucune borne sur son rayon : celle d'ImGui le
// recalculerait sur la forme elle-même, et un intérieur perdrait la courbe
// parallèle à son cadre.
void PathRoundedBox(ImDrawList& into, const RoundedBox& shape) {
  const float radius = shape.rounding < kLeastDrawnRounding ? 0.0F : shape.rounding;
  into.PathArcToFast(ImVec2(shape.min.x + radius, shape.min.y + radius), radius, kTwelfthsLeft,
                     kTwelfthsUp);
  into.PathArcToFast(ImVec2(shape.max.x - radius, shape.min.y + radius), radius, kTwelfthsUp,
                     kTwelfthsFullTurn);
  into.PathArcToFast(ImVec2(shape.max.x - radius, shape.max.y - radius), radius, kTwelfthsRight,
                     kTwelfthsDown);
  into.PathArcToFast(ImVec2(shape.min.x + radius, shape.max.y - radius), radius, kTwelfthsDown,
                     kTwelfthsLeft);
}

}  // namespace

float DrawnRounding(ImVec2 min, ImVec2 max, float rounding) {
  if (!(rounding >= kLeastDrawnRounding)) {
    return 0.0F;
  }
  const float shortest = std::min(std::abs(max.x - min.x), std::abs(max.y - min.y));
  const float drawn = std::min(rounding, shortest * 0.5F - kRoundingSideMargin);
  return drawn < kLeastDrawnRounding ? 0.0F : drawn;
}

RoundedBox GaugeInterior(ImVec2 frame_min, ImVec2 frame_max, float frame_rounding,
                         float border_pixels) {
  const float border = std::max(border_pixels, 0.0F);
  RoundedBox interior;
  interior.min = ImVec2(frame_min.x + border, frame_min.y + border);
  interior.max = ImVec2(std::max(frame_max.x - border, interior.min.x),
                        std::max(frame_max.y - border, interior.min.y));
  interior.rounding = std::max(DrawnRounding(frame_min, frame_max, frame_rounding) - border, 0.0F);
  return interior;
}

AxisBox GaugeFillBox(const RoundedBox& interior, float fill, GaugeGrowth growth) {
  // ⚠ `fill == fill` ÉCARTE LE NaN : une division par zéro en amont ne doit pas
  // peindre une jauge pleine.
  const float share = fill == fill ? std::clamp(fill, 0.0F, 1.0F) : 0.0F;
  const float width = (interior.max.x - interior.min.x) * share;
  const float height = (interior.max.y - interior.min.y) * share;
  AxisBox box{interior.min, interior.max};
  switch (growth) {
    case GaugeGrowth::kRightward:
      box.max.x = interior.min.x + width;
      break;
    case GaugeGrowth::kLeftward:
      box.min.x = interior.max.x - width;
      break;
    case GaugeGrowth::kUpward:
      box.min.y = interior.max.y - height;
      break;
    case GaugeGrowth::kDownward:
      box.max.y = interior.min.y + height;
      break;
  }
  return box;
}

float RoundedBoxInsetAt(const RoundedBox& shape, float y) {
  const float radius = shape.rounding;
  if (radius <= 0.0F) {
    return 0.0F;
  }
  const float from_edge = std::min(y - shape.min.y, shape.max.y - y);
  if (from_edge >= radius) {
    return 0.0F;
  }
  const float rise = radius - std::max(from_edge, 0.0F);
  return radius - std::sqrt(std::max(radius * radius - rise * rise, 0.0F));
}

bool PathRoundedBoxWithin(ImDrawList& into, const RoundedBox& shape, const AxisBox& box) {
  into.PathClear();
  const AxisBox kept{ImVec2(std::max(box.min.x, shape.min.x), std::max(box.min.y, shape.min.y)),
                     ImVec2(std::min(box.max.x, shape.max.x), std::min(box.max.y, shape.max.y))};
  if (!(kept.max.x > kept.min.x) || !(kept.max.y > kept.min.y)) {
    return false;
  }

  PathRoundedBox(into, shape);
  std::vector<ImVec2> outline(into._Path.Data, into._Path.Data + into._Path.Size);
  into.PathClear();

  std::vector<ImVec2> cut;
  ClipAgainst(outline, cut, Axis::kX, kept.min.x, /*keep_above=*/true);
  ClipAgainst(cut, outline, Axis::kX, kept.max.x, /*keep_above=*/false);
  ClipAgainst(outline, cut, Axis::kY, kept.min.y, /*keep_above=*/true);
  ClipAgainst(cut, outline, Axis::kY, kept.max.y, /*keep_above=*/false);

  for (const ImVec2& point : outline) {
    if (into._Path.Size > 0 && SamePoint(into._Path.back(), point)) {
      continue;
    }
    into.PathLineTo(point);
  }
  while (into._Path.Size > 1 && SamePoint(into._Path.back(), into._Path.front())) {
    into._Path.pop_back();
  }
  // ⚠ MOINS DE TROIS SOMMETS NE FONT PAS UNE SURFACE.
  constexpr int kLeastPolygonPoints = 3;
  if (into._Path.Size < kLeastPolygonPoints) {
    into.PathClear();
    return false;
  }
  return true;
}

void FillRoundedBoxWithin(ImDrawList& into, const RoundedBox& shape, const AxisBox& box,
                          ImU32 color) {
  if (PathRoundedBoxWithin(into, shape, box)) {
    into.PathFillConvex(color);
  }
}

bool PathGaugeFill(ImDrawList& into, const RoundedBox& interior, float fill, GaugeGrowth growth) {
  return PathRoundedBoxWithin(into, interior, GaugeFillBox(interior, fill, growth));
}

void PaintGaugeFill(ImDrawList& into, const RoundedBox& interior, float fill, GaugeGrowth growth,
                    ImU32 color) {
  FillRoundedBoxWithin(into, interior, GaugeFillBox(interior, fill, growth), color);
}

}  // namespace ro::ui
