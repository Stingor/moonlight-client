#pragma once

#include <string_view>

#include <imgui.h>

#include "ro/ui/world_map_window.h"

namespace ro::ui {

// Le centre d'un rectangle de l'image, en pixels de l'image.
[[nodiscard]] inline ImVec2 CenterOf(const WorldMapRect& rect) {
  return {(rect.right - rect.left) * 0.5F + rect.left, (rect.bottom - rect.top) * 0.5F + rect.top};
}

// Un point de l'image, sur l'écran.
[[nodiscard]] inline ImVec2 ToScreen(const WorldMapFit& fit, ImVec2 point) {
  return {fit.origin.x + point.x * fit.scale, fit.origin.y + point.y * fit.scale};
}

// Le centre d'un rectangle de l'image, sur l'écran.
[[nodiscard]] inline ImVec2 CenterOnScreen(const WorldMapFit& fit, const WorldMapRect& rect) {
  return ToScreen(fit, CenterOf(rect));
}

// LES MARQUES DE LA CARTE DU MONDE, chacune sa couche, dans l'ordre où la
// fenêtre les pose.

// La recherche de la navigation : la teinte du palier, puis son image.
void DrawWorldMapSearch(ImDrawList* into, const WorldMapFit& fit, const WorldMapView& view);

// La coche sur la carte de l'étape en cours.
void DrawWorldMapStepMark(ImDrawList* into, const WorldMapFit& fit, const WorldMapView& view);

// Le visage du groupe au milieu de chacune de ses cartes.
void DrawWorldMapPartyFaces(ImDrawList* into, const WorldMapFit& fit, const WorldMapView& view);

// Les donjons : le trait de l'étiquette au point, le point, l'étiquette. Celui
// que la souris désigne passe en pleine lumière.
void DrawWorldMapDungeons(ImDrawList* into, const WorldMapFit& fit, const WorldMapView& view,
                          const WorldMapDungeonView* hovered);

// CE QUE LA SOURIS DÉSIGNE : la carte, et la carte du groupe sous elle.
struct WorldMapHover {
  const WorldMapRect* rect = nullptr;
  const WorldMapPartyView* party = nullptr;
  // Son aperçu, quand il est prêt.
  const WorldMapPreview* preview = nullptr;
};

// Le fond du survol : le voile de la carte, l'aperçu et le fond de la liste du
// groupe. Puis, par-dessus les images, leurs lettres et leurs filets.
void DrawWorldMapHover(ImDrawList* into, const WorldMapFit& fit, const WorldMapView& view,
                       const WorldMapHover& hover);
void WriteWorldMapHover(ImDrawList* into, const WorldMapFit& fit, const WorldMapView& view,
                        const WorldMapHover& hover);

// Par-dessus les images : le nom de chaque donjon sur son étiquette — et son
// niveau quand les noms se montrent —, le filet des cartes du groupe, et les
// noms et niveaux des cartes.
void WriteWorldMapDungeonNames(ImDrawList* into, const WorldMapFit& fit, const WorldMapView& view,
                               bool with_levels);
void OutlineWorldMapParty(ImDrawList* into, const WorldMapFit& fit, const WorldMapView& view);
void WriteWorldMapFieldNames(ImDrawList* into, const WorldMapFit& fit, const WorldMapView& view);

// Le filet de ce que la souris désigne.
void OutlineWorldMapRect(ImDrawList* into, const WorldMapFit& fit, const WorldMapRect& rect,
                         ImU32 color);

// UNE LETTRE CERNÉE : le contour d'un pixel dans les quatre directions, puis la
// lettre.
void WriteWorldMapOutlined(ImDrawList* into, float size_pixels, ImVec2 at, ImU32 letter,
                           ImU32 outline, std::string_view text);

}  // namespace ro::ui
