// L'itinéraire et les dirigeables de la carte du monde. Voir l'en-tête public.
//
// LES PLACES SUIVENT L'IMAGE, LES TAILLES RESTENT EN PIXELS D'ÉCRAN : le
// client d'origine étire l'image du monde sur l'écran, et garde la largeur
// d'un trait, la taille de l'appareil et sa vitesse.

#include "world_map_flight.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

#include "world_map_marks.h"

namespace ro::ui {
namespace {

// L'ITINÉRAIRE : un trait de dix pixels d'escale en escale, et la marque
// d'arrivée posée au-dessus et à gauche du centre de la destination.
constexpr float kRouteHalfWidth = 5.0F;
constexpr ImU32 kRouteLineColor = IM_COL32(255, 255, 255, 0x99);
constexpr ImU32 kRouteGoalColor = IM_COL32(255, 255, 255, 0xCC);
// Une teinte se peint rentrée d'un pixel d'écran, comme celles de la recherche.
constexpr float kTintInsetPixels = 1.0F;

// LE CIRCUIT D'UN DIRIGEABLE : un trait de quatre pixels.
constexpr float kAirHalfWidth = 2.0F;
constexpr ImU32 kAirLineColor = IM_COL32(255, 255, 255, 0xAA);
// L'appareil avance de deux cents pixels par seconde.
constexpr float kAirSpeedPixelsPerSecond = 200.0F;

// LA TRACE : des empreintes de seize pixels sur seize, espacées de huit, posées
// à six pixels à gauche du trait. Seules celles qui sont à moins de cent pixels
// de l'appareil se voient, chacune plus nette que la précédente.
constexpr float kTrailStep = 16.0F;
constexpr float kTrailWidth = 16.0F;
constexpr float kTrailGapShare = 0.5F;
constexpr float kTrailSideOffset = 6.0F;
constexpr float kTrailReach = 100.0F;
constexpr ImU32 kTrailFirstColor = IM_COL32(255, 255, 255, 0x33);
// ⚠ L'OPACITÉ MONTE PAR ADDITION SUR LA COULEUR ENTIÈRE, sans plafond : une
// sixième empreinte repasserait presque transparente.
constexpr ImU32 kTrailAlphaStep = IM_COL32(0, 0, 0, 0x33);
// L'APPAREIL : un carré de quarante pixels posé devant la dernière empreinte.
constexpr float kAirshipHalfSize = 20.0F;

// L'ÉTIQUETTE D'UNE ESCALE : un voile sur sa carte, un filet blanc aux coins
// arrondis, et son nom en orangé.
constexpr ImU32 kStopVeil = IM_COL32(0, 0, 0, 0x66);
constexpr ImU32 kStopEdge = IM_COL32(255, 255, 255, 255);
constexpr float kStopRounding = 3.0F;
constexpr ImU32 kStopTextColor = IM_COL32(255, 210, 0, 255);
constexpr ImU32 kStopTextOutline = IM_COL32(0, 0, 0, 255);
constexpr ImVec2 kStopTextOffset(2.0F, 5.0F);
constexpr float kStopFontPixels = 12.0F;

[[nodiscard]] float Distance(ImVec2 from, ImVec2 to) {
  return std::hypot(to.x - from.x, to.y - from.y);
}

[[nodiscard]] ImVec2 Plus(ImVec2 point, ImVec2 offset, float times = 1.0F) {
  return {point.x + offset.x * times, point.y + offset.y * times};
}

// Le quadrilatère, déjà sur l'écran, l'image dans l'ordre de ses coins.
void PaintQuad(ImDrawList* into, const SkinImage& image, const WorldMapQuad& quad) {
  if (!image.usable()) {
    return;
  }
  into->AddImageQuad(image.id, quad.corners[0], quad.corners[1], quad.corners[3], quad.corners[2],
                     ImVec2(0.0F, 0.0F), ImVec2(1.0F, 0.0F), ImVec2(1.0F, 1.0F), ImVec2(0.0F, 1.0F),
                     quad.color);
}

void PaintBand(ImDrawList* into, const SkinImage& image, ImVec2 from, ImVec2 to, float half_width,
               ImU32 color) {
  if (const auto band = WorldMapBand(from, to, half_width, color); band.has_value()) {
    PaintQuad(into, image, *band);
  }
}

}  // namespace

void LabelWorldMapStops(ImDrawList* into, const WorldMapFit& fit,
                        std::span<const WorldMapAirStop> shown, std::string_view hovered_map) {
  std::map<std::string_view, const WorldMapAirStop*> stops;
  for (const WorldMapAirStop& stop : shown) {
    stops.emplace(stop.map_name, &stop);
  }
  for (const auto& [map_name, stop] : stops) {
    if (map_name == hovered_map) {
      continue;
    }
    const ImVec2 from = ToScreen(fit, ImVec2(stop->rect.left, stop->rect.top));
    const ImVec2 to = ToScreen(fit, ImVec2(stop->rect.right, stop->rect.bottom));
    into->AddRectFilled(from, to, kStopVeil);
    into->AddRect(from, to, kStopEdge, kStopRounding);
    const std::string_view name = stop->title.empty() ? stop->map_name : stop->title;
    WriteWorldMapOutlined(into, kStopFontPixels, Plus(from, kStopTextOffset), kStopTextColor,
                          kStopTextOutline, name);
  }
}

std::optional<WorldMapQuad> WorldMapBand(ImVec2 from, ImVec2 to, float half_width, ImU32 color) {
  const float length = Distance(from, to);
  if (length <= 0.0F) {
    return std::nullopt;
  }
  // En travers : le sens du trait tourné d'un quart de tour.
  const ImVec2 across(-(to.y - from.y) / length * half_width,
                      (to.x - from.x) / length * half_width);
  return WorldMapQuad{.corners = {Plus(from, across, -1.0F), Plus(from, across),
                                  Plus(to, across, -1.0F), Plus(to, across)},
                      .color = color};
}

std::optional<float> AdvanceWorldMapAir(WorldMapAirState& state, const WorldMapAirLine& line,
                                        std::string_view hovered_map, float elapsed_seconds,
                                        float pixels_per_unit) {
  const auto tour = std::ranges::find(line.tours, hovered_map, &WorldMapAirTour::start);
  if (hovered_map.empty() || tour == line.tours.end()) {
    // Au repos. La longueur de l'étape reste : la première image du vol suivant
    // la remesure.
    state.tour = -1;
    state.leg = 0;
    state.traveled = 0.0F;
    return std::nullopt;
  }
  // LE CIRCUIT SE CHOISIT EN ARRIVANT, et se garde d'une escale à l'autre.
  if (state.tour < 0 || static_cast<std::size_t>(state.tour) >= line.tours.size()) {
    state.tour = static_cast<int>(tour - line.tours.begin());
  }
  const std::span<const WorldMapAirStop> stops =
      line.tours[static_cast<std::size_t>(state.tour)].stops;
  if (stops.size() < 2) {
    return std::nullopt;
  }
  const std::size_t legs = stops.size() - 1;
  if (state.leg >= legs) {
    state.leg = 0;
  }
  if (state.traveled > state.leg_length) {
    state.leg = state.leg + 1 >= legs ? 0 : state.leg + 1;
    state.traveled = 0.0F;
  }
  if (state.traveled <= 0.0F) {
    state.leg_length =
        Distance(CenterOf(stops[state.leg].rect), CenterOf(stops[state.leg + 1].rect)) *
        pixels_per_unit;
  }
  state.traveled += elapsed_seconds * kAirSpeedPixelsPerSecond;
  // Deux escales au même point : l'étape est faite dès qu'elle commence.
  return state.leg_length > 0.0F ? state.traveled / state.leg_length : 1.0F;
}

WorldMapFlight FlyWorldMapLeg(ImVec2 from, ImVec2 to, float progress) {
  WorldMapFlight flight;
  const float length = Distance(from, to);
  if (length <= 0.0F) {
    return flight;
  }
  const ImVec2 along((to.x - from.x) / length, (to.y - from.y) / length);
  const ImVec2 across(-along.y, along.x);
  const ImVec2 step(along.x * kTrailStep, along.y * kTrailStep);
  const ImVec2 width(across.x * kTrailWidth, across.y * kTrailWidth);
  const float reached = length * progress;
  const ImVec2 airship_at = Plus(from, along, reached);

  ImVec2 start = Plus(from, across, -kTrailSideOffset);
  ImVec2 end = Plus(start, step);
  ImU32 color = kTrailFirstColor;
  while (reached > Distance(from, end)) {
    if (Distance(airship_at, start) < kTrailReach) {
      flight.trail.push_back(WorldMapQuad{
          .corners = {start, Plus(start, width), end, Plus(end, width)}, .color = color});
      color += kTrailAlphaStep;
    }
    start = Plus(end, step, kTrailGapShare);
    end = Plus(start, step);
  }
  if (progress < 1.0F) {
    // Posé devant la première empreinte à venir, le nez en avant.
    const ImVec2 base = Plus(Plus(end, width, 0.5F), along, -kTrailWidth);
    const ImVec2 front = Plus(base, along, 2.0F * kAirshipHalfSize);
    flight.airship = WorldMapQuad{
        .corners = {Plus(front, across, kAirshipHalfSize), Plus(front, across, -kAirshipHalfSize),
                    Plus(base, across, kAirshipHalfSize), Plus(base, across, -kAirshipHalfSize)},
        .color = IM_COL32_WHITE};
  }
  return flight;
}

void DrawWorldMapRoute(ImDrawList* into, const WorldMapFit& fit, const WorldMapView& view) {
  for (std::size_t rank = 0; rank < view.route.size(); ++rank) {
    const WorldMapRouteStep& step = view.route[rank];
    if (!step.shown) {
      continue;
    }
    const ImVec2 from = ToScreen(fit, ImVec2(step.rect.left, step.rect.top));
    const ImVec2 to = ToScreen(fit, ImVec2(step.rect.right, step.rect.bottom));
    into->AddRectFilled(ImVec2(from.x + kTintInsetPixels, from.y + kTintInsetPixels),
                        ImVec2(to.x - kTintInsetPixels, to.y - kTintInsetPixels), view.route_color);
    if (rank > 0 && view.route[rank - 1].shown) {
      PaintBand(into, view.route_line, CenterOnScreen(fit, view.route[rank - 1].rect),
                CenterOnScreen(fit, step.rect), kRouteHalfWidth, kRouteLineColor);
    }
    if (rank + 1 == view.route.size() && view.route_reaches_goal && view.route_goal.usable()) {
      const ImVec2 center = CenterOnScreen(fit, step.rect);
      const ImVec2 corner(center.x - view.route_goal.width, center.y - view.route_goal.height);
      into->AddImage(view.route_goal.id, corner, center, ImVec2(0.0F, 0.0F), ImVec2(1.0F, 1.0F),
                     kRouteGoalColor);
    }
  }
}

void DrawWorldMapAirLines(ImDrawList* into, const WorldMapFit& fit, const WorldMapView& view,
                          WorldMapWindowState& state, std::string_view hovered_map) {
  const std::size_t lines = std::min(view.air_lines.size(), state.air.size());
  for (std::size_t rank = 0; rank < lines; ++rank) {
    WorldMapAirState& air = state.air[rank];
    const WorldMapAirLine& line = view.air_lines[rank];
    const std::optional<float> progress =
        AdvanceWorldMapAir(air, line, hovered_map, ImGui::GetIO().DeltaTime, fit.scale);
    if (!progress.has_value()) {
      continue;
    }
    const WorldMapAirTour& tour = line.tours[static_cast<std::size_t>(air.tour)];
    for (std::size_t leg = 0; leg + 1 < tour.stops.size(); ++leg) {
      PaintBand(into, view.air_line, CenterOnScreen(fit, tour.stops[leg].rect),
                CenterOnScreen(fit, tour.stops[leg + 1].rect), kAirHalfWidth, kAirLineColor);
    }
    const WorldMapFlight flight =
        FlyWorldMapLeg(CenterOnScreen(fit, tour.stops[air.leg].rect),
                       CenterOnScreen(fit, tour.stops[air.leg + 1].rect), *progress);
    for (const WorldMapQuad& footprint : flight.trail) {
      PaintQuad(into, view.air_trail, footprint);
    }
    if (flight.airship.has_value()) {
      PaintQuad(into, view.airship, *flight.airship);
    }
    LabelWorldMapStops(into, fit, tour.stops, hovered_map);
  }
}

}  // namespace ro::ui
