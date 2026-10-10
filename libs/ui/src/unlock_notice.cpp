#include "ro/ui/unlock_notice.h"

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"

namespace ro::ui {
namespace {

// L'écart au haut de l'écran, en pixels : sous la bordure, sans couvrir le
// centre où l'on joue.
constexpr float kTopMarginPixels = 48.0F;
// Le fond reste lisible sans masquer ce qu'on est en train de poser dessous.
constexpr float kBackgroundAlpha = 0.8F;
// Le titre de l'avis, plus vif que ses lignes d'aide.
constexpr ImU32 kHeadlineColor = IM_COL32(255, 210, 90, 255);

}  // namespace

UnlockNoticeAnswer DrawUnlockNotice(std::span<const char* const> unlocked) {
  UnlockNoticeAnswer answer;
  const ImGuiIO& io = ImGui::GetIO();
  // ⚠ POSÉE UNE FOIS, PUIS LAISSÉE OÙ LE JOUEUR LA MET : elle couvre peut-être
  // justement ce qu'il veut déplacer.
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5F, kTopMarginPixels), ImGuiCond_FirstUseEver,
                          ImVec2(0.5F, 0.0F));
  ImGui::SetNextWindowBgAlpha(kBackgroundAlpha);
  constexpr ImGuiWindowFlags kFlags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize |
      ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus;
  if (ImGui::Begin("##unlock_notice", nullptr, kFlags)) {
    ImGui::PushStyleColor(ImGuiCol_Text, kHeadlineColor);
    ImGui::TextUnformatted(core::Tr("Interface déverrouillée"));
    ImGui::PopStyleColor();
    for (const char* name : unlocked) {
      ImGui::BulletText("%s", name);
    }
    ImGui::Separator();
    ImGui::TextUnformatted(
        core::Tr("Glisse un élément pour le déplacer : il s'aimante à ses voisins."));
    ImGui::TextUnformatted(
        core::Tr("Ctrl + glisser : les éléments aimantés entre eux se déplacent ensemble."));
    ImGui::Separator();
    // ⚠ TOUJOURS DÉCOCHÉE : la cocher verrouille, et l'avis disparaît avec le
    // dernier élément déverrouillé.
    bool locked = false;
    if (ImGui::Checkbox(core::Tr("Verrouiller l'interface"), &locked) && locked) {
      answer.lock_all = true;
    }
    // ⚠ POUR QUI LAISSE UN ÉLÉMENT OUVERT EXPRÈS : l'avis se tait, les éléments
    // restent déverrouillés.
    bool hidden = false;
    if (ImGui::Checkbox(core::Tr("Ne plus afficher cet avis"), &hidden) && hidden) {
      answer.hide_notice = true;
    }
    if (ImGui::IsItemHovered()) {
      SkinTooltip(core::Tr("Il se rallume dans les options, section Interface."));
    }
  }
  ImGui::End();
  return answer;
}

}  // namespace ro::ui
