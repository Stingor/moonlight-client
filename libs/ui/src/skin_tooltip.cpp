// La bulle du jeu, avec l'habillage de la fenêtre en cours. Voir l'en-tête.

#include "ro/ui/skin_tooltip.h"

#include <cstdarg>

#include "ro/ui/skinned_window.h"

namespace ro::ui {
namespace {

// L'habillage avec lequel la bulle ouverte l'a été : sa fermeture doit dépiler
// ce que son ouverture a posé, même si la fenêtre en cours a changé entre-temps.
//
// ⚠ UNE SEULE BULLE À LA FOIS, comme chez ImGui : une infobulle ne s'imbrique
// pas dans une autre.
const SkinArt* g_open_art = nullptr;

constexpr int kBareColors = 2;
constexpr int kBareVars = 4;

// Le point d'exclamation du signe, en fractions de son côté : la barre de
// 35 % à 68 % de la hauteur, le point centré à 82 %.
constexpr float kHalf = 0.5F;
constexpr float kMarkBarHalfWidth = 0.06F;
constexpr float kMarkBarTop = 0.35F;
constexpr float kMarkBarBottom = 0.68F;
constexpr float kMarkDotCenter = 0.82F;
constexpr float kMarkDotRadius = 0.07F;

// L'habillage d'origine, celui qu'on lit hors d'une fenêtre habillée.
constexpr SkinLook kOriginalLook{};

}  // namespace

LentSkinArt::LentSkinArt(const SkinArt& art) noexcept {
  LendSkinArt(art.dresses_windows() ? &art : nullptr);
}

const SkinLook& PaintingLook() noexcept {
  const SkinArt* const art = PaintingSkinArt();
  return art != nullptr ? art->look() : kOriginalLook;
}

bool BeginSkinTooltip(float widest) {
  if (const SkinArt* art = PaintingSkinArt(); art != nullptr) {
    if (!BeginSkinnedTooltip(*art, widest)) {
      return false;
    }
    g_open_art = art;
    return true;
  }
  // SANS HABILLAGE : le même corps, blanc à texte noir, et sans cadre.
  ImGui::PushStyleColor(ImGuiCol_PopupBg, kSkinnedTooltipBodyColor);
  ImGui::PushStyleColor(ImGuiCol_Text, kSkinnedTooltipTextColor);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, kSkinnedTooltipRounding);
  // ⚠ SA MARGE ET SON INTERLIGNE À ELLE : ce qui l'entoure peut les avoir mis à
  // zéro pour ses propres cases. Voir `kBareTooltipPaddingPixels`.
  const float scale = ImGui::GetStyle().FontScaleMain;
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      ImVec2(kBareTooltipPaddingPixels * scale, kBareTooltipPaddingPixels * scale));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                      ImVec2(ImGui::GetStyle().ItemSpacing.x, kBareTooltipLineGapPixels * scale));
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(0.0F, 0.0F), ImVec2(widest > 0.0F ? widest : kSkinnedTooltipWidest,
                                 ImGui::GetIO().DisplaySize.y * kSkinnedTooltipScreenShare));
  if (!ImGui::BeginTooltip()) {
    ImGui::PopStyleVar(kBareVars);
    ImGui::PopStyleColor(kBareColors);
    return false;
  }
  g_open_art = nullptr;
  return true;
}

void EndSkinTooltip() {
  if (g_open_art != nullptr) {
    EndSkinnedTooltip(*g_open_art);
    g_open_art = nullptr;
    return;
  }
  ImGui::EndTooltip();
  ImGui::PopStyleVar(kBareVars);
  ImGui::PopStyleColor(kBareColors);
}

void SkinTooltipFormat(const char* format, ...) {
  if (format == nullptr || !BeginSkinTooltip()) {
    return;
  }
  // ⚠ REPLIÉE À LA LARGEUR DE LA BULLE, marges ôtées : une aide longue écrite
  // sur une ligne traverserait sinon l'écran, la bulle ne s'élargissant pas
  // au-delà de sa borne.
  const float wrap = kSkinnedTooltipWidest - 2.0F * ImGui::GetStyle().WindowPadding.x;
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap);
  va_list arguments;
  va_start(arguments, format);
  ImGui::TextV(format, arguments);
  va_end(arguments);
  ImGui::PopTextWrapPos();
  EndSkinTooltip();
}

void PaintWarningMark(ImDrawList& into, ImVec2 top_left, float side) {
  const float middle = top_left.x + side * kHalf;
  const float bottom = top_left.y + side;
  into.AddTriangleFilled(ImVec2(middle, top_left.y), ImVec2(top_left.x + side, bottom),
                         ImVec2(top_left.x, bottom), kSkinWarningFill);
  // LE POINT D'EXCLAMATION : une barre, puis un point, au milieu du triangle.
  const float half_bar = side * kMarkBarHalfWidth;
  into.AddRectFilled(ImVec2(middle - half_bar, top_left.y + side * kMarkBarTop),
                     ImVec2(middle + half_bar, top_left.y + side * kMarkBarBottom),
                     kSkinWarningInk);
  into.AddCircleFilled(ImVec2(middle, top_left.y + side * kMarkDotCenter), side * kMarkDotRadius,
                       kSkinWarningInk);
}

void SkinTooltipWarned(const char* text, const char* warning) {
  const bool has_text = text != nullptr && text[0] != '\0';
  const bool has_warning = warning != nullptr && warning[0] != '\0';
  if ((!has_text && !has_warning) || !BeginSkinTooltip()) {
    return;
  }
  // ⚠ LE BORD DROIT EST LE MÊME POUR LES DEUX TEXTES, relevé en tête de bulle :
  // l'avertissement commence plus à droite, il s'enroule plus tôt.
  const float wrap_right =
      ImGui::GetCursorPosX() + kSkinnedTooltipWidest - 2.0F * ImGui::GetStyle().WindowPadding.x;
  ImGui::PushTextWrapPos(wrap_right);
  if (has_text) {
    ImGui::TextUnformatted(text);
  }
  if (has_warning) {
    const float side = ImGui::GetFontSize();
    if (has_text) {
      ImGui::Dummy(ImVec2(0.0F, side));
    }
    const ImVec2 at = ImGui::GetCursorScreenPos();
    PaintWarningMark(*ImGui::GetWindowDrawList(), at, side);
    ImGui::SetCursorScreenPos(ImVec2(at.x + side + ImGui::GetStyle().ItemSpacing.x, at.y));
    ImGui::TextUnformatted(warning);
  }
  ImGui::PopTextWrapPos();
  EndSkinTooltip();
}

}  // namespace ro::ui
