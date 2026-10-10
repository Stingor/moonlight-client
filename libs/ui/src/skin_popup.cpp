#include "ro/ui/skin_popup.h"

#include <array>
#include <tuple>
#include <utility>

#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_tooltip.h"

namespace ro::ui {
namespace {

// Le texte, posé DANS le surgissant, et retiré avant sa fermeture.
constexpr int kTextColors = 2;

// LE CORPS DU SURGISSANT, posé AVANT son ouverture — c'est `Begin` qui peint
// le fond et le liseré — et retiré APRÈS sa fermeture : ImGui exige qu'une
// fenêtre retrouve en se fermant la pile qu'elle avait en s'ouvrant.
constexpr auto kBodyColors = std::to_array<std::pair<ImGuiCol, ImU32>>({
    {ImGuiCol_PopupBg, kSkinPopupColor},
    {ImGuiCol_Border, kSkinBorderColor},
    {ImGuiCol_Separator, kSkinBorderColor},
    {ImGuiCol_Header, kSkinSelectionColor},
    {ImGuiCol_HeaderHovered, kSkinSelectionHoverColor},
    {ImGuiCol_HeaderActive, kSkinSelectionActiveColor},
});

// LE CORPS DANS UNE FENÊTRE HABILLÉE : celui que le joueur a réglé — « Menus
// et listes », la bordure, la sélection —, qu'il soit sombre ou clair.
[[nodiscard]] auto BodyOf(const SkinLook& look) {
  return std::to_array<std::pair<ImGuiCol, ImU32>>({
      {ImGuiCol_PopupBg, look.popup},
      {ImGuiCol_Border, look.border},
      {ImGuiCol_Separator, look.border},
      {ImGuiCol_Header, look.selection},
      {ImGuiCol_HeaderHovered, look.SelectionHover()},
      {ImGuiCol_HeaderActive, look.SelectionActive()},
  });
}

// ⚠ LE MÊME COMPTE DANS LES DEUX CAS : la fermeture dépile sans savoir lequel.
static_assert(std::tuple_size_v<decltype(BodyOf(SkinLook{}))> == kBodyColors.size());

// Le corps : celui de l'habillage de la fenêtre en cours. Hors d'une fenêtre
// habillée, celui du jeu si l'endroit en impose un sombre, sinon celui de
// l'endroit, reposé tel quel.
//
// ⚠ LUES PAR `GetStyleColorVec4`, PAS PAR `GetColorU32` : celle-ci applique
// l'opacité globale, qu'une zone grisée abaisse — reposée ainsi, la couleur
// la subirait deux fois.
void PushBody() {
  if (const SkinArt* art = PaintingSkinArt(); art != nullptr) {
    for (const auto& [which, color] : BodyOf(art->look())) {
      ImGui::PushStyleColor(which, color);
    }
    return;
  }
  const bool dark = SkinPopupNeedsBody(
      ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_PopupBg)));
  for (const auto& [which, skin] : kBodyColors) {
    if (dark) {
      ImGui::PushStyleColor(which, skin);
    } else {
      ImGui::PushStyleColor(which, ImGui::GetStyleColorVec4(which));
    }
  }
}

void PopBody() { ImGui::PopStyleColor(static_cast<int>(kBodyColors.size())); }

// Le texte : celui de l'habillage de la fenêtre en cours, sinon celui du jeu —
// le corps est alors le sien, ou un corps clair posé exprès.
void PushSkinText() {
  const SkinArt* art = PaintingSkinArt();
  ImGui::PushStyleColor(ImGuiCol_Text, art != nullptr ? art->look().text : kSkinTextColor);
  ImGui::PushStyleColor(ImGuiCol_TextDisabled,
                        art != nullptr ? art->look().faint_text : kSkinFaintTextColor);
}

// Une luminance au-dessous de laquelle un fond est sombre, sur 255 : le texte
// de l'habillage, presque noir, ne s'y lirait pas.
constexpr int kDarkestLightBody = 128;
// Les poids de la luminance perçue, en millièmes (Rec. 601).
constexpr int kRedWeight = 299;
constexpr int kGreenWeight = 587;
constexpr int kBlueWeight = 114;
constexpr int kWeightTotal = 1000;
constexpr unsigned kChannelMask = 0xFFU;

}  // namespace

bool SkinPopupNeedsBody(ImU32 ambient) {
  const auto channel = [ambient](int shift) {
    return static_cast<int>((ambient >> static_cast<unsigned>(shift)) & kChannelMask);
  };
  const int luminance =
      (channel(IM_COL32_R_SHIFT) * kRedWeight + channel(IM_COL32_G_SHIFT) * kGreenWeight +
       channel(IM_COL32_B_SHIFT) * kBlueWeight) /
      kWeightTotal;
  return luminance < kDarkestLightBody;
}

bool BeginSkinPopup(const char* id, ImGuiWindowFlags flags) {
  PushBody();
  const bool open = ImGui::BeginPopup(id, flags);
  if (!open) {
    PopBody();
    return false;
  }
  PushSkinText();
  return true;
}

bool BeginSkinPopupContextItem(const char* id, ImGuiPopupFlags flags) {
  PushBody();
  const bool open = ImGui::BeginPopupContextItem(id, flags);
  if (!open) {
    PopBody();
    return false;
  }
  PushSkinText();
  return true;
}

void EndSkinPopup() {
  ImGui::PopStyleColor(kTextColors);
  ImGui::EndPopup();
  PopBody();
}

}  // namespace ro::ui
