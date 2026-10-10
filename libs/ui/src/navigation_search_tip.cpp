// L'INFOBULLE D'UNE RECHERCHE DE NAVIGATION DU CHAT (`<NAVS>`).
//
// ⚠ UN NOM DE PNJ EST UN RÔLE recopié partout (« Kafra Employee ») : ce qui le
// rend reconnaissable est sa silhouette, celle qu'on a vue à l'écran. Elle se
// montre à gauche du plan de sa carte quand les données la connaissent ; sinon,
// le plan reprend toute la place.

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/navigation_window.h"
#include "ro/ui/skin_tooltip.h"

#include "navigation_parts.h"

namespace ro::ui {
namespace {

// Le côté du plan, en pixels avant mise à l'échelle.
constexpr float kPlanSidePixels = 160.0F;

}  // namespace

void DrawNavigationSearchTip(const NavigationSearchTip& tip) {
  if (!BeginSkinTooltip()) {
    return;
  }
  ImGui::TextUnformatted(tip.term.c_str());
  // LE CONTEXTE, nommé puis montré : c'est la réponse à « lequel ? ».
  if (!tip.context_label.empty()) {
    ImGui::TextDisabled("%s", tip.context_label.c_str());
  }
  ImGui::Separator();
  if (!tip.plan_map_name.empty()) {
    if (tip.npc_image.usable()) {
      DrawNavigationMapThumbnail(tip.npc_image,
                                 NavigationPx(static_cast<float>(kNavigationSearchTipNpcSidePx)));
      ImGui::SameLine();
    }
    // ⚠ LE PLAN ET SON NOM INTERNE FORMENT UN BLOC : sans lui, le nom repasserait
    // sous la silhouette.
    ImGui::BeginGroup();
    DrawNavigationMapThumbnail(tip.map_image, NavigationPx(kPlanSidePixels));
    ImGui::TextDisabled("%s", tip.plan_map_name.c_str());
    ImGui::EndGroup();
  }
  ImGui::TextDisabled("%s", core::Tr("Clic : chercher dans la navigation"));
  EndSkinTooltip();
}

}  // namespace ro::ui
