#pragma once

#include <span>
#include <string_view>

#include <imgui.h>

#include "ro/ui/world_map_window.h"

namespace ro::ui {

// L'ITINÉRAIRE de la navigation, par-dessus les teintes de la recherche.
void DrawWorldMapRoute(ImDrawList* into, const WorldMapFit& fit, const WorldMapView& view);

// LES DIRIGEABLES : chaque ligne avance son vol, puis se dessine si la souris
// survole l'une de ses escales.
void DrawWorldMapAirLines(ImDrawList* into, const WorldMapFit& fit, const WorldMapView& view,
                          WorldMapWindowState& state, std::string_view hovered_map);

// LES ÉTIQUETTES D'ESCALES D'UN DIRIGEABLE, une par carte, sauf celle qu'on
// survole : un voile, un filet blanc, et le nom en orangé.
void LabelWorldMapStops(ImDrawList* into, const WorldMapFit& fit,
                        std::span<const WorldMapAirStop> stops, std::string_view hovered_map);

// LE WARP AGENT DE LA VILLE SURVOLÉE : sa figure, et son nom à côté.
void DrawWorldMapWarpAgent(ImDrawList* into, const WorldMapFit& fit, const WorldMapView& view,
                           std::string_view hovered_map);

}  // namespace ro::ui
