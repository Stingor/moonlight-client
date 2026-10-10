#include <algorithm>
#include <cmath>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/minimap_window.h"
#include "ro/ui/skin_widgets.h"

namespace ro::ui {
namespace {

// Les bornes des curseurs : une carte à moins d'un dixième
// d'opacité ne se voit plus, et l'on croirait la minimap cassée.
constexpr int kBackdropLeastPercent = 0;
constexpr int kMapLeastPercent = 10;
constexpr int kMostPercent = 100;
constexpr int kArrowLeastHalfPx = 2;
constexpr int kArrowMostHalfPx = 24;

}  // namespace

MinimapSpan CenteredMinimapSpan(float center, float span) {
  if (span >= 1.0F) {
    return {};
  }
  MinimapSpan window{.low = center - span * 0.5F, .high = center + span * 0.5F};
  if (window.low < 0.0F) {
    window = {.low = 0.0F, .high = span};
  }
  if (window.high > 1.0F) {
    window = {.low = 1.0F - span, .high = 1.0F};
  }
  return window;
}

float StepMinimapZoom(float zoom, float notches) {
  // ⚠ `!(zoom >= least)` ATTRAPE AUSSI UN NaN venu d'un fichier de réglages.
  const float from = !(zoom >= kMinimapZoomLeast) ? kMinimapZoomLeast : zoom;
  return std::clamp(from * std::pow(kMinimapZoomStep, notches), kMinimapZoomLeast,
                    kMinimapZoomMost);
}

ImVec2 FitMinimapSquare(ImVec2 desired, const MinimapSquareFit& fit) {
  const float from_width = desired.x - fit.pad_x;
  const float from_height = desired.y - fit.pad_y - fit.extra_height;
  // 🔴 LE PLAFOND S'APPLIQUE ICI AUSSI : ImGui borne la taille AVANT d'appeler
  // la contrainte, puis retient ce qu'elle rend.
  float side = (from_width + from_height) * 0.5F;
  side = std::min(side, fit.most_side);
  side = std::max(side, kMinimapLeastSide);
  return {side + fit.pad_x, side + fit.pad_y + fit.extra_height};
}

bool DrawMinimapSettings(MinimapSettings& settings, const SkinArt& art) {
  bool changed = false;

  changed |= SkinCheckBox(core::Tr("Traverser les clics"), &settings.click_through, art);
  ImGui::SameLine();
  SkinHelpMarker(
      core::Tr("Décoché : la carte est cliquable — la glisser la déplace, les poignées des deux "
               "coins du bas la redimensionnent en gardant son carré, la molette zoome, et le "
               "bouton de réglages reste affiché.\n\nCoché : la souris passe au travers, la carte "
               "n'est plus qu'un décor. Maintenir Maj au-dessus la réveille alors le temps du "
               "geste, bouton de réglages compris."));

  changed |= SkinSliderInt(core::Tr("Opacité du fond"), &settings.backdrop_percent,
                           kBackdropLeastPercent, kMostPercent, art, "%d%%");
  changed |= SkinSliderInt(core::Tr("Opacité de la carte"), &settings.map_percent, kMapLeastPercent,
                           kMostPercent, art, "%d%%");
  changed |= SkinCheckBox(core::Tr("Lisser l'image"), &settings.smooth, art);
  ImGui::SameLine();
  SkinHelpMarker(
      core::Tr("La carte est un petit bitmap très agrandi : lissée elle est plus "
               "douce, nette elle garde ses pixels."));

  ImGui::SeparatorText(core::Tr("Personnage"));
  changed |= SkinSliderInt(core::Tr("Taille de la flèche"), &settings.arrow_half_px,
                           kArrowLeastHalfPx, kArrowMostHalfPx, art, "%d px");
  changed |= SkinColorSwatch(core::Tr("Teinte de la flèche"), &settings.arrow_tint, art,
                             /*with_alpha=*/false)
                 .changed;
  ImGui::SameLine();
  SkinHelpMarker(
      core::Tr("Blanc = la flèche du jeu telle quelle.\nToute autre couleur la teinte."));
  changed |= SkinCheckBox(core::Tr("Afficher « carte,x,y »"), &settings.show_coords, art);
  changed |=
      SkinCheckBox(core::Tr("Afficher le nom détaillé du lieu"), &settings.show_map_name, art);
  ImGui::SameLine();
  SkinHelpMarker(
      core::Tr("Le nom lisible du lieu — « PvP : Room Copass » pour « pvp_n_3-5 ». "
               "Plus long que la minimap, il passe à la ligne."));

  ImGui::SeparatorText(core::Tr("Marqueurs"));
  changed |= SkinCheckBox(core::Tr("PNJ et commodités"), &settings.show_town, art);
  ImGui::SameLine();
  SkinHelpMarker(
      core::Tr("Kafra, guides, marchands, forge, auberge, salon de coiffure "
               "— la liste que le jeu déclare pour cette carte. Clic : y être guidé."));
  changed |= SkinCheckBox(core::Tr("Portails"), &settings.show_portals, art);
  ImGui::SameLine();
  SkinHelpMarker(
      core::Tr("Les sorties de la carte, libellées de leur destination. "
               "Clic : y être guidé."));
  changed |= SkinCheckBox(core::Tr("Membres du groupe"), &settings.show_party, art);
  changed |= SkinCheckBox(core::Tr("Membres de la guilde"), &settings.show_guild, art);
  changed |= SkinCheckBox(core::Tr("Quêtes"), &settings.show_quests, art);
  changed |= SkinCheckBox(core::Tr("Boss (Convex Mirror)"), &settings.show_boss, art);
  ImGui::SameLine();
  SkinHelpMarker(
      core::Tr("Le boss révélé par un Convex Mirror. Hors du cadrage, l'icône se "
               "colle au bord de la carte pour en donner la direction ; elle disparaît "
               "tant que le boss est mort (le serveur n'a plus de position à donner)."));
  changed |= SkinCheckBox(core::Tr("Repères du serveur"), &settings.show_viewpoints, art);
  ImGui::SameLine();
  SkinHelpMarker(
      core::Tr("Les croix clignotantes que posent les NPC pour indiquer un lieu "
               "(commande de script « viewpoint »)."));
  ImGui::SameLine();
  SkinHelpMarker(
      core::Tr("Les positions et les couleurs viennent du serveur : un marqueur "
               "n'apparaît que si le serveur a envoyé la position."));
  changed |= SkinCheckBox(core::Tr("Tracé de l'itinéraire"), &settings.show_route, art);
  ImGui::SameLine();
  SkinHelpMarker(
      core::Tr("La ligne ambre du guidage, de votre position jusqu'à la sortie de la "
               "carte ; un losange marque l'arrivée."));

  if (SkinButton(core::Tr("Réinitialiser"), art)) {
    settings = MinimapSettings{};
    changed = true;
  }
  return changed;
}

}  // namespace ro::ui
