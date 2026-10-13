#include "ro/ui/link_notice.h"

#include <cmath>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/modal_box.h"

namespace ro::ui {
namespace {

// Le fond du bandeau : sombre et translucide, pour que le monde reste lisible
// dessous ; le texte, blanc sur une ombre noire, comme les autres bandeaux.
constexpr ImU32 kBannerBackground = IM_COL32(16, 18, 24, 190);
constexpr ImU32 kBannerText = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kBannerShadow = IM_COL32(0, 0, 0, 255);
constexpr float kBannerRounding = 4.0F;
// Le décalage de l'ombre, en pixels d'interface.
constexpr float kShadowOffsetPixels = 1.0F;

// LE BANDEAU, centré en haut, sur la liste de premier plan : il ne prend ni
// clic ni touche, et ne se range derrière aucune fenêtre.
void DrawUnsteadyBanner() {
  const char* text = core::Tr("Le serveur ne répond plus…");
  const float scale = ImGui::GetStyle().FontScaleMain;
  const ImVec2 size = ImGui::CalcTextSize(text);
  const float padding = kLinkBannerPaddingPixels * scale;
  const float width = ImGui::GetIO().DisplaySize.x;
  const ImVec2 at(std::trunc((width - size.x) * 0.5F), std::trunc(kLinkBannerTopPixels * scale));
  ImDrawList* into = ImGui::GetForegroundDrawList();
  into->AddRectFilled(ImVec2(at.x - padding, at.y - padding),
                      ImVec2(at.x + size.x + padding, at.y + size.y + padding), kBannerBackground,
                      kBannerRounding * scale);
  const float shadow = kShadowOffsetPixels * scale;
  into->AddText(ImVec2(at.x + shadow, at.y + shadow), kBannerShadow, text);
  into->AddText(at, kBannerText, text);
}

}  // namespace

void DrawLinkNotice(LinkNoticeState& state, const SkinArt& art) {
  if (state.unsteady && state.text.empty()) {
    DrawUnsteadyBanner();
  }
  if (state.text.empty() || state.acknowledged) {
    return;
  }
  if (!ImGui::IsPopupOpen(kLinkLossBoxId)) {
    AskModal(kLinkLossBoxId);
  }
  if (DrawNoticeBox(kLinkLossBoxId, state.text.c_str(), core::Tr("OK"), art)) {
    state.acknowledged = true;
  }
}

}  // namespace ro::ui
