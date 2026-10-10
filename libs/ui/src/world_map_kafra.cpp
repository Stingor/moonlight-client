// Le service de téléportation des Kafra sur la carte du monde. Voir
// `world_map_flight.h`.
//
// Les places suivent l'image ; les tailles — la largeur d'une flèche, sa
// pointe — restent en pixels d'écran, comme chez le client d'origine.

#include <algorithm>
#include <cmath>
#include <vector>

#include "world_map_flight.h"

namespace ro::ui {
namespace {

// UNE FLÈCHE : un corps de dix pixels de large, une pointe de vingt.
constexpr float kArrowWidth = 10.0F;
constexpr float kArrowHead = 20.0F;
constexpr ImU32 kArrowColor = IM_COL32(255, 255, 255, 0xAA);
// Les flèches poussent en une seconde.
constexpr float kGrowSeconds = 1.0F;
// La Kafra ne monte jamais plus haut que six pixels sous le haut de l'image,
// plus sa hauteur.
constexpr float kSpriteTopRoom = 6.0F;

[[nodiscard]] ImVec2 ToScreen(const WorldMapFit& fit, ImVec2 point) {
  return {fit.origin.x + point.x * fit.scale, fit.origin.y + point.y * fit.scale};
}

[[nodiscard]] WorldMapRect RectOnScreen(const WorldMapFit& fit, const WorldMapRect& rect) {
  const ImVec2 from = ToScreen(fit, ImVec2(rect.left, rect.top));
  const ImVec2 to = ToScreen(fit, ImVec2(rect.right, rect.bottom));
  return WorldMapRect{.left = from.x, .top = from.y, .right = to.x, .bottom = to.y};
}

// Avance un bout de flèche pixel par pixel tant qu'il est dans son rectangle :
// c'est le bord que la flèche touche.
[[nodiscard]] ImVec2 LeaveBox(ImVec2 point, ImVec2 step, const WorldMapRect& box) {
  if (point.x <= box.left) {
    return point;
  }
  do {
    if (box.right <= point.x || point.y <= box.top || box.bottom <= point.y) {
      break;
    }
    point = ImVec2(point.x + step.x, point.y + step.y);
  } while (point.x > box.left);
  return point;
}

void PaintQuad(ImDrawList* into, const SkinImage& image, const WorldMapQuad& quad) {
  if (image.usable()) {
    into->AddImageQuad(image.id, quad.corners[0], quad.corners[1], quad.corners[3], quad.corners[2],
                       ImVec2(0.0F, 0.0F), ImVec2(1.0F, 0.0F), ImVec2(1.0F, 1.0F),
                       ImVec2(0.0F, 1.0F), quad.color);
  }
}

// LA POINTE : un triangle, le haut droit de l'image d'un côté de la base, le bas
// gauche de l'autre, le bas droit à la pointe.
void PaintHead(ImDrawList* into, const SkinImage& image, const std::array<ImVec2, 3>& corners,
               ImU32 color) {
  if (!image.usable()) {
    return;
  }
  into->PushTexture(image.id);
  into->PrimReserve(3, 3);
  into->PrimVtx(corners[0], ImVec2(1.0F, 0.0F), color);
  into->PrimVtx(corners[1], ImVec2(0.0F, 1.0F), color);
  into->PrimVtx(corners[2], ImVec2(1.0F, 1.0F), color);
  into->PopTexture();
}

}  // namespace

std::optional<WorldMapArrow> MakeWorldMapArrow(ImVec2 from, ImVec2 to, const WorldMapRect& from_box,
                                               const WorldMapRect& to_box, float progress,
                                               ImU32 color) {
  const float length = std::hypot(to.x - from.x, to.y - from.y);
  if (length <= 0.0F) {
    return std::nullopt;
  }
  const ImVec2 along((to.x - from.x) / length, (to.y - from.y) / length);
  const ImVec2 back(-along.x, -along.y);
  const ImVec2 across(-along.y, along.x);
  const ImVec2 width(across.x * kArrowWidth, across.y * kArrowWidth);
  // LES DEUX BORDS DU CORPS, chacun de bord de rectangle à bord de rectangle.
  const ImVec2 start = LeaveBox(from, along, from_box);
  const ImVec2 end = LeaveBox(to, back, to_box);
  const ImVec2 start_side = LeaveBox(ImVec2(from.x + width.x, from.y + width.y), along, from_box);
  const ImVec2 end_side = LeaveBox(ImVec2(to.x + width.x, to.y + width.y), back, to_box);
  const ImVec2 reached(start.x + (end.x - start.x) * progress,
                       start.y + (end.y - start.y) * progress);

  WorldMapArrow arrow;
  // Achevée, la flèche prend l'autre bord exact ; en poussant, sa largeur.
  const ImVec2 reached_side =
      progress >= 1.0F ? end_side : ImVec2(reached.x + width.x, reached.y + width.y);
  arrow.body = WorldMapQuad{.corners = {start, start_side, reached, reached_side}, .color = color};
  if (progress < 1.0F) {
    const ImVec2 base(reached.x + width.x * 0.5F - along.x * kArrowWidth,
                      reached.y + width.y * 0.5F - along.y * kArrowWidth);
    const float half = kArrowHead * 0.5F;
    arrow.head =
        std::array<ImVec2, 3>{ImVec2(base.x - across.x * half, base.y - across.y * half),
                              ImVec2(base.x + across.x * half, base.y + across.y * half),
                              ImVec2(base.x + along.x * kArrowHead, base.y + along.y * kArrowHead)};
  }
  return arrow;
}

void DrawWorldMapKafra(ImDrawList* into, const WorldMapFit& fit, const WorldMapView& view,
                       WorldMapWindowState& state, std::string_view hovered_map) {
  // LA VILLE DÉSIGNÉE, si elle a une Kafra. Sa vue est celle de l'image
  // précédente : une autre ville attend la sienne.
  if (!view.kafra.has_value() || hovered_map.empty() || view.kafra->town_map != hovered_map) {
    state.kafra_town.clear();
    return;
  }
  const WorldMapKafraView& kafra = *view.kafra;
  if (state.kafra_town != kafra.town_map) {
    state.kafra_town = std::string(kafra.town_map);
    state.kafra_grow = 0.0F;
  }
  state.kafra_grow = std::min(state.kafra_grow + ImGui::GetIO().DeltaTime / kGrowSeconds, 1.0F);

  const WorldMapRect town = RectOnScreen(fit, kafra.town);
  std::vector<WorldMapAirStop> stops;
  for (const WorldMapKafraLeg& leg : kafra.legs) {
    stops.push_back(leg.stop);
    const auto arrow =
        MakeWorldMapArrow(ToScreen(fit, leg.from), ToScreen(fit, leg.to), town,
                          RectOnScreen(fit, leg.stop.rect), state.kafra_grow, kArrowColor);
    if (!arrow.has_value()) {
      continue;
    }
    PaintQuad(into, view.kafra_arrow_body, arrow->body);
    if (arrow->head.has_value()) {
      PaintHead(into, view.kafra_arrow_head, *arrow->head, kArrowColor);
    }
  }
  // LA KAFRA, les pieds sur le coin haut droit de la ville.
  if (kafra.sprite.usable()) {
    const float feet_y = std::max(town.top, fit.origin.y + kafra.sprite_height + kSpriteTopRoom);
    const ImVec2 corner(town.right - kafra.sprite_feet.x, feet_y - kafra.sprite_feet.y);
    into->AddImage(kafra.sprite.id, corner,
                   ImVec2(corner.x + kafra.sprite.width, corner.y + kafra.sprite.height));
  }
  LabelWorldMapStops(into, fit, stops, hovered_map);
}

}  // namespace ro::ui
