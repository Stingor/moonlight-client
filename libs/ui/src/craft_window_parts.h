#pragma once

// CE QUE LES FENÊTRES D'ATELIER PARTAGENT — fabrication et affinage : les
// mesures, les deux teintes de plus que la palette de fabrication, la
// recherche, le lien d'objet souligné et le journal de session.
//
// En-tête PRIVÉ de la couche : rien hors d'elle n'a à dessiner un atelier.

#include <cstdint>
#include <span>
#include <string_view>

#include <imgui.h>

#include "ro/ui/craft_log_view.h"
#include "ro/ui/skin_art.h"

namespace ro::ui::craft_parts {

// L'icône d'un objet dans une liste, et celle d'un matériau ou d'un minerai.
inline constexpr float kRowIconPixels = 24.0F;
inline constexpr float kLinkIconPixels = 16.0F;

// Le voile d'une liste que le serveur n'honore plus : elle reste lisible.
inline constexpr float kStaleAlpha = 0.5F;

// Une puce dont le texte s'enroule sous lui-même.
void BulletWrapped(const char* text);

// LE LIEN D'UN OBJET : l'icône, le texte coloré, souligné au survol, la main.
// Rend vrai au clic, gauche OU droit. `hovered` dit si on le survole.
[[nodiscard]] bool ItemLink(const char* id, const SkinImage& icon, float icon_pixels,
                            const char* text, ImU32 color, bool* hovered);

// Le journal de session : une boîte à hauteur fixe, collée en bas tant qu'on y
// était déjà. `wrap` enroule les lignes plutôt que de les couper.
void DrawHistory(const char* id, std::span<const CraftLogLineView> lines, float height,
                 bool show_time, bool wrap);

}  // namespace ro::ui::craft_parts
