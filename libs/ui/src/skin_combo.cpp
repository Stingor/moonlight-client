// LA LISTE DÉROULANTE DU JEU : un champ bordé, et le bouton flèche du client. La
// molette au-dessus du champ fermé en fait défiler les entrées ; déroulée,
// `SkinCombo` se parcourt aux touches de liste (`list_keys.h`).
//
// ⚠ ELLE S'EMPLOIE COMME CELLE D'IMGUI — l'appelant pose ses `Selectable` entre
// l'ouverture et la fermeture — et `EndSkinCombo` NE S'APPELLE QUE si l'ouverture
// a rendu vrai : elle dépile ce que l'ouverture a posé.

#include <algorithm>
#include <array>
#include <cstdio>

#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/ui/list_keys.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/wheel_gate.h"

#include "ui_scale.h"

namespace ro::ui {
namespace {

// Le nom de la liste ouverte. Un seul suffit : une liste déroulée en cache une
// autre, et ImGui n'en tient qu'une à la fois.
constexpr const char* kPopupId = "##liste";
// La marge du texte dans le champ.
constexpr float kFieldPaddingPixels = 4.0F;
// Le côté du bouton flèche, quand l'art manque.
constexpr float kArrowSidePixels = 16.0F;
// Le triangle de repli, à défaut de bouton.
constexpr float kFallbackArrowHalfWidth = 3.0F;
constexpr float kFallbackArrowRise = 2.0F;
constexpr float kFallbackArrowDrop = 3.0F;

// Le bouton flèche : la largeur de son art, ou celle du repli.
[[nodiscard]] float ArrowWidth(const SkinArt& art) {
  const SkinImage& resting = art.Get(render::SkinPiece::kComboArrow);
  return (resting.usable() ? resting.width : kArrowSidePixels) * Scale();
}

}  // namespace

float SkinComboChromeWidth(const SkinArt& art) {
  return ArrowWidth(art) + kFieldPaddingPixels * Scale() * 2.0F;
}

bool BeginSkinCombo(const char* label, const char* shown, const SkinArt& art, int* wheel_step) {
  const SkinImage& resting = art.Get(render::SkinPiece::kComboArrow);
  const SkinImage& over = art.Get(render::SkinPiece::kComboArrowHover);
  const SkinImage& pressed = art.Get(render::SkinPiece::kComboArrowPressed);
  const float scale = Scale();
  const float arrow_width = ArrowWidth(art);
  // ⚠ SANS ART, `GetFrameHeight` SUIT DÉJÀ L'ÉCHELLE — police et marges y sont
  // toutes deux mises à l'échelle. La repasser par le facteur la doublerait.
  const float height = resting.usable() ? resting.height * scale : ImGui::GetFrameHeight();
  const float width = ImGui::CalcItemWidth();

  ImGui::PushID(label);
  const ImVec2 top_left = ImGui::GetCursorScreenPos();
  const bool clicked = ImGui::InvisibleButton("##champ", ImVec2(width, height));
  const bool hovered = ImGui::IsItemHovered();
  const bool held = ImGui::IsItemActive();
  // La molette se lit sur le CHAMP, juste après lui : plus bas, le dernier
  // élément serait le libellé.
  if (wheel_step != nullptr) {
    const float wheel = WheelOverItem();
    *wheel_step = wheel > 0.0F ? -1 : (wheel < 0.0F ? 1 : 0);
  }
  const ImVec2 bottom_right(top_left.x + width, top_left.y + height);
  const ImVec2 arrow_at(bottom_right.x - arrow_width, top_left.y);

  ImDrawList* into = ImGui::GetWindowDrawList();
  // Le champ : un fond de saisie et une bordure. La boîte du client n'est pas
  // autre chose.
  into->AddRectFilled(top_left, ImVec2(arrow_at.x, bottom_right.y), art.look().input);
  into->AddRect(top_left, bottom_right, art.look().border);

  const SkinImage& face = held ? pressed : (hovered ? over : resting);
  if (face.usable()) {
    // ⚠ LA FLÈCHE EST UN BOUTON, et prend les teintes des boutons.
    into->AddImage(
        face.id, arrow_at, bottom_right, ImVec2(0.0F, 0.0F), ImVec2(1.0F, 1.0F),
        PaintedTint(held || hovered ? art.look().button_hover_tint : art.look().button_tint,
                    (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) != 0));
  } else {
    // Un triangle vers le bas : moins joli, et parfaitement compréhensible.
    const float middle_x = (arrow_at.x + bottom_right.x) * 0.5F;
    const float middle_y = (top_left.y + bottom_right.y) * 0.5F;
    into->AddTriangleFilled(
        ImVec2(middle_x - kFallbackArrowHalfWidth * scale, middle_y - kFallbackArrowRise * scale),
        ImVec2(middle_x + kFallbackArrowHalfWidth * scale, middle_y - kFallbackArrowRise * scale),
        ImVec2(middle_x, middle_y + kFallbackArrowDrop * scale), art.look().text);
  }

  if (shown != nullptr && shown[0] != '\0') {
    const char* end = ImGui::FindRenderedTextEnd(shown);
    const float text_height = ImGui::CalcTextSize(shown, end).y;
    // Découpé au champ : un libellé plus long que la boîte ne doit pas courir
    // par-dessus la flèche.
    into->PushClipRect(top_left, ImVec2(arrow_at.x, bottom_right.y), true);
    into->AddText(ImVec2(top_left.x + kFieldPaddingPixels * scale,
                         top_left.y + (height - text_height) * 0.5F),
                  art.look().text, shown, end);
    into->PopClipRect();
  }

  // Le libellé à droite, comme une liste d'ImGui. ⚠ EN VRAI ÉLÉMENT, pour que la
  // disposition lui réserve sa largeur : un `SameLine` de l'appelant démarrerait
  // sinon PAR-DESSUS lui.
  const char* label_end = ImGui::FindRenderedTextEnd(label);
  if (label != label_end) {
    const ImGuiStyle& style = ImGui::GetStyle();
    ImGui::SameLine(0.0F, style.ItemInnerSpacing.x);
    ImGui::SetCursorScreenPos(ImVec2(bottom_right.x + style.ItemInnerSpacing.x,
                                     top_left.y + (height - ImGui::GetTextLineHeight()) * 0.5F));
    ImGui::TextUnformatted(label, label_end);
  }

  if (clicked) {
    ImGui::OpenPopup(kPopupId);
  }

  // ⚠ LES CONTRAINTES AVANT LE PLACEMENT : c'est la taille CONTRAINTE que le
  // placement doit connaître. Même ordre que `BeginComboPopup`.
  ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0F), ImVec2(FLT_MAX, FLT_MAX));

  // ⚠ SOUS LE CHAMP QUAND IL Y A LA PLACE, AU-DESSUS SINON — et un
  // `SetNextWindowPos` en dur ne peut pas le faire : `Begin` ne recadre QUE les
  // positions qu'il a choisies lui-même. Une liste ouverte depuis une fenêtre
  // posée en bas d'écran se déplierait dans le vide. On reprend donc la recette
  // d'ImGui : lire la taille que la liste AURA, puis lui chercher un côté.
  const ImRect field(top_left, bottom_right);
  std::array<char, 32> popup_name{};
  ImFormatString(popup_name.data(), popup_name.size(), "##Popup_%08x",
                 ImGui::GetCurrentWindow()->GetID(kPopupId));
  ImGuiWindow* popup = ImGui::FindWindowByName(popup_name.data());
  if (popup != nullptr && popup->WasActive) {
    const ImVec2 expected = ImGui::CalcWindowNextAutoFitSize(popup);
    // Remis au bas à CHAQUE image, comme ImGui : sans cela le côté retenu la
    // fois d'avant est retenté d'abord, et la liste reste collée en haut.
    popup->AutoPosLastDirection = ImGuiDir_Down;
    ImGui::SetNextWindowPos(ImGui::FindBestWindowPosForPopupEx(
        field.GetBL(), expected, &popup->AutoPosLastDirection,
        ImGui::GetPopupAllowedExtentRect(popup), field, ImGuiPopupPositionPolicy_ComboBox));
  } else {
    // Première ouverture : la liste n'a pas encore de fenêtre. On la pose sous
    // le champ — invisible de toute façon, ImGui sautant l'image où il mesure.
    ImGui::SetNextWindowPos(ImVec2(top_left.x, bottom_right.y));
  }

  // ⚠ LA LISTE OUVERTE EST UN MENU : elle prend « Menus et listes », la
  // sélection et le texte de l'habillage, que pose le menu lui-même. Seule
  // l'entrée choisie garde le bleu franc de l'onglet actif, posé dedans.
  const bool open = BeginSkinPopup(kPopupId);
  if (!open) {
    ImGui::PopID();
    return false;
  }
  ImGui::PushStyleColor(ImGuiCol_Header, art.look().active_tab);
  return true;
}

void EndSkinCombo() {
  ImGui::PopStyleColor();
  EndSkinPopup();
  ImGui::PopID();
}

int SkinComboStepped(int rank, int step, int count) {
  if (step == 0 || count <= 0) {
    return rank;
  }
  return std::clamp(rank + step, 0, count - 1);
}

bool SkinCombo(const char* label, int* chosen, std::span<const char* const> items,
               const SkinArt& art, const char* closed_text) {
  if (chosen == nullptr || items.empty()) {
    return false;
  }
  const int count = static_cast<int>(items.size());
  *chosen = std::clamp(*chosen, 0, count - 1);
  bool changed = false;
  int step = 0;
  // ⚠ LES LIBELLÉS SE TRADUISENT ICI, ET C'EST LE SEUL ENDROIT POSSIBLE. Les
  // appelants passent une table STATIQUE : une traduction posée à sa définition
  // serait évaluée avant le catalogue et resterait française pour toujours. À la
  // lecture, un seul point de code rend traduisibles toutes les listes.
  const char* shown =
      closed_text != nullptr ? closed_text : core::Tr(items[static_cast<std::size_t>(*chosen)]);
  const bool open = BeginSkinCombo(label, shown, art, &step);
  // Un cran en butée n'est pas un changement.
  if (const int stepped = SkinComboStepped(*chosen, step, count); stepped != *chosen) {
    *chosen = stepped;
    changed = true;
  }
  if (open) {
    // LE CLAVIER DE LA LISTE DÉROULÉE : le choix suit la touche, comme sous
    // la molette, et Entrée referme.
    ComboKeys keys(*chosen, count);
    for (int rank = 0; rank < count; ++rank) {
      const bool current = keys.marked() == rank;
      // ⚠ VRAI SEULEMENT SUR UN VRAI CHANGEMENT. Posé dans le corps du `if`, ce
      // drapeau serait vrai à chaque image où la liste est ouverte : un appelant
      // qui écrit `if (changed) Enregistrer()` réécrirait son fichier de
      // réglages soixante fois par seconde. Recliquer l'entrée déjà choisie n'est
      // pas un changement non plus.
      if (keys.Took(rank,
                    ImGui::Selectable(core::Tr(items[static_cast<std::size_t>(rank)]), current) &&
                        !current)) {
        *chosen = rank;
        changed = true;
      }
    }
    EndSkinCombo();
  }
  return changed;
}

}  // namespace ro::ui
