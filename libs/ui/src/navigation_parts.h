#pragma once

#include "ro/ui/navigation_window.h"

namespace ro::ui {

// Le côté de l'aperçu du détail, en pixels d'interface.
inline constexpr float kNavigationPreviewPixels = 150.0F;

// Des pixels de référence, à l'échelle de l'interface.
[[nodiscard]] float NavigationPx(float pixels);

// Le groupe passe-t-il le filtre des pastilles ?
[[nodiscard]] bool NavigationGroupShown(NavigationFilter filter, NavigationKind kind);

// L'entrée sélectionnée, ou nulle quand la sélection ne désigne plus rien.
[[nodiscard]] const NavigationEntryView* NavigationSelection(const NavigationWindowState& state,
                                                             const NavigationView& view);

// Le plan d'une map, inscrit dans un carré sans déformation ; « pas de
// miniature » quand l'image manque.
void DrawNavigationMapThumbnail(const SkinImage& image, float side);

// La liste à gauche, le détail à droite, et le menu d'une ligne — ouvert HORS
// des deux volets : une ligne clique sous leur pile d'ids, et un surgissant
// ouvert là ne serait pas retrouvé.
void DrawNavigationResults(NavigationWindowState& state, const NavigationView& view,
                           NavigationAnswer& answer);
void DrawNavigationDetail(const NavigationWindowState& state, const NavigationView& view,
                          const SkinArt& art, NavigationAnswer& answer);
void DrawNavigationRowMenu(NavigationWindowState& state, const NavigationView& view,
                           NavigationAnswer& answer);

}  // namespace ro::ui
